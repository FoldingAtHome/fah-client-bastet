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

#include "Group.h"
#include "App.h"
#include "OS.h"
#include "Config.h"
#include "GPUResources.h"

#include <cbang/log/Logger.h>


using namespace std;
using namespace cb;
using namespace FAH::Client;


#undef CBANG_LOG_PREFIX
#define CBANG_LOG_PREFIX (name.empty() ? "Default" : name) << ":"


Group::Group(App &app, const string &name) :
  app(app), name(name),
  event(app.getEventBase().newEvent(this, &Group::update, 0)) {
  auto data = app.getDB("groups").getJSON(name, new JSON::Dict);
  config    = new Config(app, data);

  insert("config", config);

  triggerUpdate();
}


Group::Units Group::units() const {return Units(*app.getUnits(), name);}


void Group::setState(const JSON::Value &msg) {
  config->setState(msg);
  triggerUpdate();
}


bool Group::waitForIdle() const {
  return config->getOnIdle() && !app.getOS().isSystemIdle();
}


bool Group::isActive() const {
  for (auto unit: units())
    if (!unit->isPaused()) return true;

  return false;
}


bool Group::hasUnrunWUs() const {
  for (auto unit: units())
    if (!unit->hasRun()) return true;

  return false;
}


void Group::triggerUpdate(bool updateUnits) {
  if (!event->isPending()) event->activate();

  if (updateUnits)
    for (auto unit: units())
      unit->triggerNext();
}


void Group::shutdown(function<void ()> cb) {
  shutdownCB = cb;
  triggerUpdate();
}


void Group::unitComplete(bool success) {
  if (success) {
    failures = 0;
    setWait(0);

  } else {
    failures++;
    setWait(pow(2, std::min(failures, 10U)));
  }

  triggerUpdate();
}



void Group::save()   {app.getDB("groups").set(name, config->toString());}
void Group::remove() {app.getDB("groups").unset(name);}


void Group::notify(const list<JSON::ValuePtr> &change) {
  // Automatically save changes to config
  bool isConfig = !change.empty() && change.front()->getString() == "config";

  if (isConfig) {
    save();
    triggerUpdate();
  }
}


void Group::update() {
  // Remove completed units
  std::set<string> completed;
  for (auto unit: units())
    if (unit->getState() == UnitState::UNIT_DONE)
      completed.insert(unit->getID());

  for (auto &id: completed)
    app.getUnits()->removeUnit(id);

  // Handle graceful shutdown
  if (app.shouldQuit()) {
    for (auto unit: units())
      if (unit->isRunning())
        return event->add(1); // Check again later

    if (shutdownCB) {
      // Save state to DB
      for (auto unit: units())
        unit->save();

      shutdownCB();
      shutdownCB = 0;
    }

    return;
  }

  // Handle finish
  if (config->getFinish() && !isActive())
    config->setPaused(true);

  // No further action if paused or idle
  if (config->getPaused() || waitForIdle())
    return setWait(0); // Pausing clears wait timer

  // Wait on failures
  auto now = Time::now();
  if (now < waitUntil) return event->add(waitUntil - now);

  // Allocate resources
  unsigned         remainingCPUs = config->getCPUs();
  std::set<string> remainingGPUs = config->getGPUs();
  std::set<string> enabledWUs;

  // Allocate GPUs with minimum CPU requirements
  for (auto unit: units()) {
    if (UNIT_RUN < unit->getState()) continue;

    auto unitGPUs = unit->getGPUs();
    if (unitGPUs.empty()) continue;

    uint32_t minCPUs = unit->getMinCPUs();
    bool     runable = minCPUs <= remainingCPUs || minCPUs < 2;

    std::set<string> gpusWithWU = remainingGPUs;
    for (auto id: unitGPUs) runable &= gpusWithWU.erase(id) != 0;

    if (runable) {
      remainingGPUs = gpusWithWU;
      remainingCPUs -= min(remainingCPUs, minCPUs); // Allocate minimum CPUs
      enabledWUs.insert(unit->getID());
    }
  }

  // Allocate extra CPUs to enabled GPU WUs
  for (auto unit: units()) {
    // GPU WUs that were enabled above
    if (!enabledWUs.count(unit->getID())) continue;

    uint32_t minCPUs = unit->getMinCPUs();
    uint32_t maxCPUs = unit->getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs + minCPUs);

    unit->setCPUs(cpus);
    // minCPUs was subtracted above or remainingCPUs is already zero
    remainingCPUs -= min(remainingCPUs, cpus - minCPUs);
  }

  // Allocate remaining CPUs to existing CPU WUs
  for (auto unit: units()) {
    if (unit->hasGPUs() || remainingCPUs < unit->getMinCPUs() ||
        UNIT_RUN < unit->getState()) continue;

    uint32_t maxCPUs = unit->getMaxCPUs();
    uint32_t cpus    = min(maxCPUs, remainingCPUs);

    unit->setCPUs(cpus);
    remainingCPUs -= cpus;
    enabledWUs.insert(unit->getID());
  }

  // Start and stop WUs
  unsigned wuCount = 0;
  for (auto unit: units()) {
    wuCount++;
    bool pause = unit->atRunState() && !enabledWUs.count(unit->getID());
    unit->setPause(pause);
    LOG_DEBUG(3, "Unit " << unit->getID() << (pause ? " paused" : " enabled"));
  }

  // Report allocation status
  LOG_DEBUG(1, "Remaining CPUs: " << remainingCPUs << ", Remaining GPUs: "
            << remainingGPUs.size() << ", Active WUs: " << enabledWUs.size());

  // Do not add WUs when finishing, if any WUs have not run yet or GPU resources
  // have not yet been loaded.
  if (config->getFinish() || hasUnrunWUs() || !app.getGPUs().isLoaded())
    return;

  // Add new WU if we don't already have too many and there are some resources
  const unsigned maxWUs = config->getGPUs().size() + config->getCPUs() / 64 + 3;
  if (wuCount < maxWUs && (remainingCPUs || remainingGPUs.size())) {
    SmartPointer<Unit> unit =
      new Unit(app, name, app.getNextWUID(), remainingCPUs, remainingGPUs);
    app.getUnits()->add(unit);
    LOG_INFO(1, "Added new work unit: cpus:" << remainingCPUs << " gpus:"
             << String::join(remainingGPUs, ","));
    triggerUpdate();
  }
}


void Group::setWait(double delay) {
  waitUntil = Time::now() + delay;
  // TODO report wait to frontend
}