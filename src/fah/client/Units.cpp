/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2022, foldingathome.org
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

#include <cbang/Catch.h>
#include <cbang/json/JSON.h>
#include <cbang/log/Logger.h>

#include <algorithm>
#include <set>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Units::Units(App &app) :
  app(app), event(app.getEventBase().newEvent(this, &Units::update, 0)) {
  app.getEventBase().newEvent(this, &Units::load, 0)->activate();
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


void Units::add(const SmartPointer<Unit> &unit) {
  append(unit);
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
    waitUntil = 0;

  } else {
    failures++;
    waitUntil = lastWU + pow(2, std::min(failures, 10U));
  }

  triggerUpdate();
}


void Units::update() {
  // First load the already existing WUs
  if (!isConfigLoaded) return;

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
  if (app.getConfig().getFinish()) {
    bool running = false;

    for (unsigned i = 0; i < size() && !running; i++)
      running = getUnit(i).isPaused();

    if (!running) app.getConfig().setPaused(true);
  }

  // No further action if paused or idle
  if (app.getConfig().getPaused() || app.getOS().shouldIdle()) return;

  // Wait on failures
  // TODO report wait time to remote clients
  auto now = Time::now();
  if (now < waitUntil) return event->add(waitUntil - now);

  // Allocate resources
  unsigned           remainingCPUs = app.getConfig().getCPUs();
  std::set<string>   remainingGPUs;
  std::set<unsigned> enabledWUs;

  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (gpu.isEnabled()) remainingGPUs.insert(gpu.getID());
  }

  // Allocate GPUs with minimum CPU requirements
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);
    if (!unit.atRunState()) continue;

    auto &unitGPUs = *unit.getGPUs();
    if (unitGPUs.empty()) continue;

    uint32_t minCPUs = unit.getMinCPUs();
    bool     runable = minCPUs <= remainingCPUs;

    std::set<string> gpusWithWU = remainingGPUs;
    for (unsigned j = 0; j < unitGPUs.size() && runable; j++)
      runable = gpusWithWU.erase(unitGPUs.getString(j));

    if (runable) {
      remainingGPUs = gpusWithWU;
      remainingCPUs -= minCPUs; // Initially allocate only minimum CPUs
      enabledWUs.insert(i);
    }
  }

  // Reserve one CPU for any unused GPUs
  uint32_t reservedCPUs = min(remainingCPUs, (uint32_t)remainingGPUs.size());
  remainingCPUs -= reservedCPUs;

  // Allocate extra CPUs to GPU WUs
  for (unsigned i = 0; i < size(); i++) {
    if (!enabledWUs.count(i)) continue; // GPU WUs that were enabled above
    auto &unit = getUnit(i);

    uint32_t minCPUs = unit.getMinCPUs();
    uint32_t maxCPUs = unit.getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs + minCPUs);

    unit.setCPUs(cpus);
    remainingCPUs -= cpus - minCPUs; // Minimum CPUs subtracted above
  }

  // Allocate remaining CPUs to existing CPU WUs
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);
    if (!unit.getGPUs()->empty() || remainingCPUs < unit.getMinCPUs()) continue;
    if (!unit.atRunState()) continue;

    uint32_t maxCPUs = unit.getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs);

    unit.setCPUs(cpus);
    remainingCPUs -= cpus;
    enabledWUs.insert(i);
  }

  // Restore reserved CPUs
  remainingCPUs += reservedCPUs;

  // Start and stop WUs
  for (unsigned i = 0; i < size(); i++)
    getUnit(i).setPause(getUnit(i).atRunState() && !enabledWUs.count(i));

  // Report allocation status
  LOG_DEBUG(1, "Remaining CPUs: " << remainingCPUs << ", Remaining GPUs: "
            << remainingGPUs.size() << ", Active WUs: " << enabledWUs.size());

#if 0
  // Dump WU with different resource allocation if not yet running
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = getUnit(i);
    if (unit.hasRun()) continue;

    if (remainingCPUs != unit.getCPUs() ||
        remainingGPUs.size() != unit.getGPUs()->size()) unit.dumpWU();

    else
      for (auto id : remainingGPUs)
        if (!unit.hasGPU(id)) {
          unit.dumpWU();
          break;
        }
  }
#endif

  // Do not add WUs when finishing
  if (app.getConfig().getFinish()) return;

  // Do not add WUs if any still have not run
  for (unsigned i = 0; i < size(); i++)
    if (!getUnit(i).hasRun()) return;

  // Add new WU if we don't have too many WUs.
  // Assume all WUs need at least one CPU
  const unsigned maxWUs = allGPUs.size() + 6;
  if (size() < maxWUs && remainingCPUs) {
    app.getDB("config").set("wus", ++wus);
    add(new Unit(app, wus, remainingCPUs, remainingGPUs));
    LOG_INFO(1, "Added new work unit");
  }

  lastWU = Time::now();
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


void Units::load() {
  wus = app.getDB("config").getInteger("wus", 0);

  app.getDB("units").foreach(
    [this] (const string &id, const string &data) {
      LOG_INFO(3, "Loading work unit " << id);
      try {
        add(new Unit(app, JSON::Reader::parseString(data)));
      } CATCH_ERROR;
    });

  isConfigLoaded = true;
  LOG_INFO(3, "Loaded " << size() << " wus.");

  triggerUpdate();
}
