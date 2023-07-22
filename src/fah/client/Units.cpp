/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2023, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 3 of the License, or
                       (at your option) any later version.

         This program is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
                   GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
           51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

                  For information regarding this software email:
                                 Joseph Coffland
                          joseph@cauldrondevelopment.com

\******************************************************************************/

#include "Units.h"

#include "App.h"
#include "Unit.h"
#include "Config.h"
#include "OS.h"
#include "GPUResources.h"
#include "ResourceGroup.h"

#include <cbang/Catch.h>
#include <cbang/json/JSON.h>
#include <cbang/log/Logger.h>

#include <algorithm>
#include <set>

using namespace FAH::Client;
using namespace cb;
using namespace std;

#undef CBANG_LOG_PREFIX
#define CBANG_LOG_PREFIX << group.getName() << ':'


Units::Units(App &app, ResourceGroup &group,
             const SmartPointer<Config> &config) :
  app(app), group(group), config(config),
  event(app.getEventBase().newEvent(this, &Units::update, 0)) {
  triggerUpdate();
}


bool Units::isActive() const {
  for (unsigned i = 0; i < size(); i++)
    if (!getUnit(i).isPaused()) return true;

  return false;
}


bool Units::hasFailure() const {
  for (unsigned i = 0; i < size(); i++)
    if (getUnit(i).getRetries()) return true;

  return false;
}


bool Units::hasUnrunWUs() const {
  for (unsigned i = 0; i < size(); i++)
    if (!getUnit(i).hasRun()) return true;

  return false;
}


bool Units::waitForIdle() const {
  return getConfig().getOnIdle() && !app.getOS().isSystemIdle();
}


void Units::add(const SmartPointer<Unit> &unit) {
  append(unit);
  unit->setUnits(this);
  unit->triggerNext();
}


unsigned Units::getUnitIndex(const std::string &id) const {
  for (unsigned i = 0; i < size(); i++)
    if (getUnit(i).getID() == id) return i;

  THROW("Unit " << id << " not found.");
}


Unit &Units::getUnit(unsigned index) const {
  if (size() <= index) THROW("Invalid unit index " << index);
  return get(index)->cast<Unit>();
}


Unit &Units::getUnit(const string &id) const {return getUnit(getUnitIndex(id));}


SmartPointer<Unit> Units::removeUnit(unsigned index) {
  if (size() <= index) THROW("Invalid unit index " << index);
  auto unit = get(index).cast<Unit>();
  erase(index);
  unit->setUnits(0);
  return unit;
}


void Units::dump(const string &unitID) {
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);

    if (unitID.empty() || unit.getID() == unitID)
      unit.dumpWU();
  }
}


void Units::unitComplete(bool success) {
  if (success) {
    failures = 0;
    setWait(0);

  } else {
    failures++;
    setWait(pow(2, std::min(failures, 10U)));
  }

  triggerUpdate();
}


void Units::update() {
  // Remove completed units
  for (unsigned i = 0; i < size();)
    if (getUnit(i).getState() == UnitState::UNIT_DONE) erase(i);
    else i++;

  // Handle graceful shutdown
  if (app.shouldQuit()) {
    for (unsigned i = 0; i < size(); i++)
      if (getUnit(i).isRunning())
        return event->add(1); // Check again later

    if (shutdownCB) {
      // Save state to DB
      for (unsigned i = 0; i < size(); i++)
        getUnit(i).save();

      shutdownCB();
      shutdownCB = 0;
    }

    return;
  }

  // Handle finish
  if (config->getFinish()) {
    bool running = false;

    for (unsigned i = 0; i < size() && !running; i++)
      running = !getUnit(i).isPaused();

    if (!running) config->setPaused(true);
  }

  // No further action if paused or idle
  if (config->getPaused() || config->waitForConfig() || waitForIdle())
    return setWait(0); // Pausing clears wait timer

  // Wait on failures
  auto now = Time::now();
  if (now < waitUntil) return event->add(waitUntil - now);

  // Allocate resources
  unsigned           remainingCPUs = config->getCPUs();
  std::set<string>   remainingGPUs = config->getGPUs();
  std::set<unsigned> enabledWUs;

  // Allocate GPUs with minimum CPU requirements
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);
    if (UNIT_RUN < unit.getState()) continue;

    auto unitGPUs = unit.getGPUs();
    if (unitGPUs.empty()) continue;

    uint32_t minCPUs = unit.getMinCPUs();
    bool     runable = minCPUs <= remainingCPUs || minCPUs < 2;

    std::set<string> gpusWithWU = remainingGPUs;
    for (auto id: unitGPUs) runable &= gpusWithWU.erase(id);

    if (runable) {
      remainingGPUs = gpusWithWU;
      remainingCPUs -= min(remainingCPUs, minCPUs); // Allocate minimum CPUs
      enabledWUs.insert(i);
    }
  }

  // Allocate extra CPUs to enabled GPU WUs
  for (unsigned i = 0; i < size(); i++) {
    if (!enabledWUs.count(i)) continue; // GPU WUs that were enabled above
    auto &unit = getUnit(i);

    uint32_t minCPUs = unit.getMinCPUs();
    uint32_t maxCPUs = unit.getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs + minCPUs);

    unit.setCPUs(cpus);
    // minCPUs was subtracted above or remainingCPUs is already zero
    remainingCPUs -= min(remainingCPUs, cpus - minCPUs);
  }

  // Allocate remaining CPUs to existing CPU WUs
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);
    if (unit.hasGPUs() || remainingCPUs < unit.getMinCPUs() ||
        UNIT_RUN < unit.getState()) continue;

    uint32_t maxCPUs = unit.getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs);

    unit.setCPUs(cpus);
    remainingCPUs -= cpus;
    enabledWUs.insert(i);
  }

  // Start and stop WUs
  for (unsigned i = 0; i < size(); i++)
    getUnit(i).setPause(getUnit(i).atRunState() && !enabledWUs.count(i));

  // Report allocation status
  LOG_DEBUG(1, "Remaining CPUs: " << remainingCPUs << ", Remaining GPUs: "
            << remainingGPUs.size() << ", Active WUs: " << enabledWUs.size());

  // Do not add WUs when finishing, if any WUs have not run yet or GPU resources
  // have not yet been loaded.
  if (config->getFinish() || hasUnrunWUs() || !app.getGPUs().isLoaded())
    return;

  // Add new WU if we don't already have too many and there are some resources
  const unsigned maxWUs = config->getGPUs().size() + config->getCPUs() / 64 + 3;
  if (size() < maxWUs && (remainingCPUs || remainingGPUs.size())) {
    add(new Unit(app, app.getNextWUID(), group.getName(), remainingCPUs,
                 remainingGPUs));
    LOG_INFO(1, "Added new work unit: cpus:" << remainingCPUs << " gpus:"
             << String::join(remainingGPUs, ","));
    triggerUpdate();
  }
}


void Units::triggerUpdate(bool updateUnits) {
  if (!event->isPending()) event->activate();

  if (updateUnits)
    for (unsigned i = 0; i < size(); i++)
      getUnit(i).triggerNext();
}


void Units::shutdown(function<void ()> cb) {
  shutdownCB = cb;
  triggerUpdate();
}


void Units::setWait(double delay) {
  waitUntil = Time::now() + delay;
  // TODO report wait to frontend
}
