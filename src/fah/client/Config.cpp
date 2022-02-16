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
#include <cbang/log/Logger.h>
#include <cbang/json/Reader.h>
#include <cbang/os/SystemInfo.h>

#include <cbang/config/MinMaxConstraint.h>
#include <cbang/config/MinConstraint.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Config::Config(App &app) : app(app) {
  unsigned cpus = SystemInfo::instance().getCPUCount();
  if (1 < cpus) cpus--; // Reserve one CPU by default

  // Defaults
  insert("user", "Anonymous");
  insert("team", 0);
  insert("passkey", "");
  insert("checkpoint", 15);
  insertBoolean("on_idle", false);
  insertBoolean("paused", false);
  insertBoolean("finish", false);
  insert("cause", "any");
  insert("cpus", cpus);
  insertDict("gpus");

  SmartPointer<Option> opt;
  auto &options = app.getOptions();

  options.pushCategory("User Information");
  options.add("user", "Your user name.")->setDefault("Anonymous");
  options.add("team", "Your team number.",
              new MinMaxConstraint<int64_t>(0, 2147483647))->setDefault(0);
  opt = options.add("passkey", "Your passkey.");
  opt->setDefault("");
  opt->setObscured();
  options.popCategory();

  options.pushCategory("Project Settings");
  options.add("project-key", "Key for access to restricted testing projects."
              )->setDefault(0);
  options.alias("project-key", "key");
  options.add("cause", "The cause you prefer to support.")->setDefault("any");
  options.popCategory();

  options.pushCategory("Resource Settings");
  options.add("cpus", "Number of cpus FAH client will use.",
              new MaxConstraint<int64_t>(cpus))->setDefault(cpus);
  options.popCategory();
}


void Config::init() {
  // Load options from config file
  auto &options = app.getOptions();
  std::set<string> keys = {"user", "passkey", "team", "key", "cause", "cpus"};
  for (auto key : keys)
    if (options.has(key) && !options[key].isDefault())
      insert(key, options[key]);

  // Load saved data
  auto &db = app.getDB("config");

  try {
    if (db.has("config"))
      merge(*JSON::Reader::parseString(db.getString("config")));
  } CATCH_ERROR;
}


bool Config::getOnIdle() const {return getBoolean("on_idle");}
void Config::setOnIdle(bool onIdle) {insertBoolean("on_idle", onIdle);}


void Config::setPaused(bool paused) {
  insertBoolean("paused", paused);
  insertBoolean("finish", false);
}


bool Config::getPaused() const {return getBoolean("paused");}
void Config::setFinish(bool finish) {insertBoolean("finish", finish);}
bool Config::getFinish() const {return getBoolean("finish");}
uint64_t Config::getProjectKey() const {return getU64("key", 0);}


uint32_t Config::getCPUs() const {
  uint32_t maxCPUs = SystemInfo::instance().getCPUCount();
  uint32_t cpus = getU32("cpus");
  return maxCPUs < cpus ? maxCPUs : cpus;
}


ProcessPriority Config::getCorePriority() {
  return ProcessPriority::parse(getString("priority", "idle"));
}


JSON::ValuePtr Config::getGPU(const string &id) {
  auto &gpus = *get("gpus");

  if (!gpus.has(id)) {
    JSON::ValuePtr gpu = new JSON::Dict;
    gpu->insertBoolean("enabled", false);
    gpus.insert(id, gpu);
  }

  return gpus.get(id);
}
