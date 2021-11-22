/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2020, foldingathome.org
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
#include "GPUResources.h"

#include <cbang/Catch.h>
#include <cbang/json/JSON.h>
#include <cbang/log/Logger.h>

#include <algorithm>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Units::Units(App &app) :
  Event::Scheduler<Units>(app.getEventBase()), app(app) {
  schedule(&Units::load);
}


void Units::unitComplete(bool success) {
  if (success) {
    failures = 0;
    waitUntil = 0;

  } else {
    failures++;
    waitUntil = lastWU + pow(2, std::max(failures, 10U));
  }

  triggerUpdate();
}


void Units::update() {
  // First load the already existing WUs
  if (!isConfigLoaded) return;

  // Remove completed units
  for (unsigned i = 0; i < size();) {
    auto &unit = *get(i).cast<Unit>();
    if (unit.getState() == UnitState::UNIT_DONE) erase(i);
    else i++;
  }

  // Wait on failures
  if (Time::now() < waitUntil)
    return schedule(&Units::update, waitUntil - Time::now());

  if (isPaused()) return;

  // Find best fit
  state_t best;
  best.cpus = app.getConfig().getCPUs();

  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (gpu.isEnabled()) best.gpus.insert(gpu.getID());
  }

  best = findBestFit(best, 0);

  // Start/stop WUs
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();
    if (!best.wus.count(i)) unit.setPause(true, "Resources not available.");
    else unit.setPause(false);
  }

  LOG_DEBUG(1, "Remaining CPUs: " << best.cpus << ", Remaining GPUs: "
            << best.gpus.size());

  // Do not add WUs if any have not reached the CORE state
  for (unsigned i = 0; i < size(); i++)
    if (get(i).cast<Unit>()->getState() < UnitState::UNIT_CORE) return;

  // Add new WU if we don't have too many WUs.
  // Assume all WUs need at least one CPU
  const unsigned maxWUs = allGPUs.size() + 6;
  if (size() < maxWUs && best.cpus) {
    app.getDB("config").set("wus", ++wus);
    add(new Unit(app, wus, best.cpus, best.gpus, getProjectKey()));
    LOG_INFO(1, "Added new work unit");
  }

  lastWU = Time::now();
}


void Units::triggerUpdate() {schedule(&Units::update);}


void Units::add(const SmartPointer<Unit> &unit) {
  append(unit);
  unit->triggerNext();
}


bool Units::isPaused() const {
  return app.getDB("config").getBoolean("paused", false);
}


void Units::setPause(bool pause) {
  app.getConfig().setPaused(pause);

  if (pause)
    for (unsigned i = 0; i < size(); i++)
      get(i).cast<Unit>()->setPause(true, "Paused by user.");

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

  if (isPaused()) setPause(true);
  else triggerUpdate();
}


uint64_t Units::getProjectKey() const {
  uint64_t key = app.getConfig().getProjectKey();

  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();
    if (key == unit.getProjectKey()) return 0;
  }

  return key;
}


bool Units::isBetter(const state_t &a, const state_t &b) {
  if (a.gpus.size() < b.gpus.size()) return true;
  if (a.gpus.size() == b.gpus.size() && a.cpus < b.cpus) return true;
  else if (a.cpus == b.cpus && a.wus.size() < b.wus.size()) return true;

  // TODO some GPUs are better, we could look at GPU type & species
  // TODO running WUs are preferable to non-running WUs
  // TODO WUs closer to completion are preferable

  return false;
}


Units::state_t Units::findBestFit(const state_t &current, unsigned i) const {
  state_t best = current;

  for (unsigned j = i; j < size(); j++) {
    auto &unit = *get(j).cast<Unit>();
    state_t next = current;

    // Check and remove CPUs
    if (current.cpus < unit.getCPUs()) continue;
    next.cpus -= unit.getCPUs();

    // Check and remove GPUs
    auto &unitGPUs = *unit.getGPUs();
    bool haveGPUs = true;

    for (unsigned k = 0; k < unitGPUs.size() && haveGPUs; k++)
      haveGPUs = next.gpus.erase(unitGPUs.getString(k));

    if (!haveGPUs) continue;

    // TODO It would be more optimal to skip WU states which are equivalent

    // Find best WU set containing this WU
    next.wus.insert(j);
    next = findBestFit(next, j + 1);

    // Keep it if it's better
    if (isBetter(next, best)) best = next;
  }

  return best;
}
