/******************************************************************************\

                  This file is part of the Folding@home Client.

           The fahclient runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2019, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
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

using namespace FAH::Client;
using namespace cb;
using namespace std;


Units::Units(App &app) :
  Event::Scheduler<Units>(app.getEventBase()), app(app) {
  schedule(&Units::load);
}


void Units::update() {
  // Remove completed units
  for (unsigned i = 0; i < size();) {
    auto &unit = *get(i).cast<Unit>();
    if (unit.getState() == UnitState::UNIT_DONE) erase(i);
    else i++;
  }

  // Get CPUs
  int32_t cpus = app.getConfig().getCPUs();

  // Get GPUs
  std::set<string> gpus;
  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (gpu.isEnabled()) gpus.insert(gpu.getID());
  }

  // Remove used resources
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();

    cpus -= unit.getCPUs();

    auto &unitGPUs = *unit.getGPUs();
    for (unsigned j = 0; j < unitGPUs.size(); j++)
      gpus.erase(unitGPUs.getString(j));
  }

  // Create new unit
  if (0 < cpus) {
    add(new Unit(app, cpus, gpus));
    LOG_INFO(1, "Added new work unit");
  }
}


void Units::add(const SmartPointer<Unit> &unit) {
  append(unit);
  unit->schedule(&Unit::next);
}


void Units::setPause(bool pause, const string unitID) {
  for (unsigned i = 0; i < size(); i++) {
    auto &unit = *get(i).cast<Unit>();

    if (unitID.empty() || unitID == unit.getID())
      unit.setPause(pause);
  }
}


void Units::load() {
  app.getDB("units").foreach(
    [this] (const string &id, const string &data) {
      LOG_INFO(3, "Loading work unit " << id);
      try {
        add(new Unit(app, JSON::Reader::parseString(data)));
      } CATCH_ERROR;
    });

  if (empty()) update();
}
