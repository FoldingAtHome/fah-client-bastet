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

  update();
}


void Units::update() {
  // First load the already existing wus
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

  // Get CPUs
  uint32_t cpus = app.getConfig().getCPUs();

  // Get GPUs
  std::set<string> gpus;
  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (gpu.isEnabled()) gpus.insert(gpu.getID());
  }

  uint64_t maxWUs = gpus.size() + 6;
  uint32_t allocatedCPUs = app.getConfig().getCPUs();
  std::set<int> processedUnits;

  // Remove used resources for gpu units
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();
    auto &unitGPUs = *unit.getGPUs();

    if(0 < unitGPUs.size()) {
      cpus -= unit.getCPUs();
      processedUnits.insert(i);
      bool resourcesAvailable = true;
      for (unsigned j = 0; j < unitGPUs.size(); j++) {
        if(!gpus.count(unitGPUs.getString(j))) resourcesAvailable = false;
        gpus.erase(unitGPUs.getString(j));
      }

      if(!resourcesAvailable) unit.setPause(true, "resources");
      else if(unit.getPauseMessage() != "user") unit.setPause(false);
    }
    else if(allocatedCPUs < unit.getCPUs()) unit.setPause(true, "resources");
  }

  while(processedUnits.size() != size())
  {
    int32_t index = -1;
    for (unsigned i = 0; i < size(); i++) {
      auto &unit = *get(i).cast<Unit>();
      uint32_t mostCPUs = index == -1 ? 0: (*get(index).cast<Unit>()).getCPUs();
      if(unit.getCPUs() > mostCPUs && !processedUnits.count(i)) {
        index = i;
      }
    }
    if(!processedUnits.count(index)) {
      auto &unit = *get(index).cast<Unit>();
      if(unit.getCPUs() <= cpus) {
        if(unit.getPauseMessage() != "user") unit.setPause(false);
        cpus -= unit.getCPUs();
      }
      else unit.setPause(true, "resources");
      processedUnits.insert(index);
    }
  }

  // Create new gpu unit(s)
  for(auto it = gpus.begin(); it != gpus.end() && 0 < cpus && size() < maxWUs; it++) {
    app.getDB("config").set("wus", ++wus);
    std::set<string> assignGPUs;
    assignGPUs.insert(*it);
    add(new Unit(app, wus, 1, assignGPUs, getProjectKey()));
    LOG_INFO(1, "Added new gpu work unit");
    gpus.erase(*it);
    cpus--;
  }

  // Create new cpu unit(s)
  if(0 < cpus && size() < maxWUs)
  {
    app.getDB("config").set("wus", ++wus);
    add(new Unit(app, wus, cpus, gpus, getProjectKey()));
    cpus -= cpus;
    LOG_INFO(1, "Added new cpu work unit");
  }
  lastWU = Time::now();
}


void Units::add(const SmartPointer<Unit> &unit) {
  append(unit);
  unit->triggerNext();
}


void Units::setPause(bool pause, const string unitID) {
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();

    if (unitID.empty() || unitID == unit.getID()) {
      if(pause) unit.setPause(pause, "user");
      else unit.setPause(pause);
    }
  }
}

uint64_t Units::getProjectKey() const {
  uint64_t key = app.getConfig().getProjectKey();
  for(unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();
    if(key == unit.getProjectKey()) return 0;
  }
  return key;
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
  LOG_INFO(3, "Loaded already existing wus: " << wus);

  if (empty()) update();
}