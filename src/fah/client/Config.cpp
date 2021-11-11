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

#include "Config.h"
#include "App.h"

#include <cbang/Catch.h>
#include <cbang/json/Reader.h>
#include <cbang/os/SystemInfo.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Config::Config(App &app) : app(app) {
  unsigned cpus = SystemInfo::instance().getCPUCount();
  if (1 < cpus) cpus--; // Reserve one CPU by default

  // Defaults
  insert("checkpoint", 15);
  insertBoolean("on_idle", true);
  insertBoolean("paused", false);
  insert("power", "medium");
  insert("cause", "any");
  insert("cpus", cpus);
  insert("options", "");
  insertDict("gpus");
}


void Config::init() {
  // Load saved data
  auto &db = app.getDB("config");

  try {
    if (db.has("config"))
      merge(*JSON::Reader::parseString(db.getString("config")));
  } CATCH_ERROR;
}


bool Config::getOnIdle() const {return getBoolean("on_idle");}
void Config::setOnIdle(bool onIdle) {insertBoolean("on_idle", onIdle);}
void Config::setPaused(bool paused) {insertBoolean("paused", paused);}
bool Config::getPaused() const {return getBoolean("paused");}


void Config::setPower(Power power) {
  insert("power", String::toLower(power.toString()));
}


void Config::update() {
  // Update the cpus
  unsigned maxCPUs = SystemInfo::instance().getCPUCount();
  if(1 < maxCPUs) maxCPUs--;
  unsigned cpus;
  switch (getPower())
  {
    case POWER_LIGHT:
      cpus = maxCPUs > 1 ? floor(maxCPUs*0.3) : maxCPUs;
      break;
    case POWER_MEDIUM:
      cpus = maxCPUs > 1 ? floor(maxCPUs*0.7) : maxCPUs;
      break;
    case POWER_CUSTOM:
      cpus = getU32("cpus", maxCPUs);
      break;
    case POWER_FULL:
    default:
      cpus = maxCPUs;
      break;
  }

  unsigned currentCPUs = getU32("cpus", maxCPUs);
  if(currentCPUs != cpus) insert("cpus", cpus);
}

Power Config::getPower() const {return Power::parse(getString("power"));}

uint64_t Config::getProjectKey() const {return getU64("key", 0);}

uint32_t Config::getCPUs() const {
  int32_t maxCPUs = SystemInfo::instance().getCPUCount();
  if(1 < maxCPUs) maxCPUs--;
  int32_t cpus = getU32("cpus", maxCPUs);
  return maxCPUs < cpus ? maxCPUs : cpus;
}


ProcessPriority Config::getCorePriority() {
  return ProcessPriority::parse(getString("priority", "idle"));
}


JSON::ValuePtr Config::getGPU(const string &id) {
  auto &gpus = *get("gpus");

  if (!gpus.has(id)) {
    JSON::ValuePtr gpu = new JSON::Dict;
    gpu->insert("options", "");
    gpu->insertBoolean("enabled", false);
    gpus.insert(id, gpu);
  }

  return gpus.get(id);
}
