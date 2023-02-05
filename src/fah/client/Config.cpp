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

#include "Config.h"
#include "App.h"
#include "GPUResources.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/json/Reader.h>
#include <cbang/os/SystemInfo.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Config::Config(App &app, const JSON::ValuePtr &config) : app(app) {
  unsigned cpus = SystemInfo::instance().getCPUCount();
  if (1 < cpus) cpus--; // Reserve one CPU by default

  unsigned pcount = SystemInfo::instance().getPerformanceCPUCount();
  if (0 < pcount && pcount < cpus) cpus = pcount;

  // Defaults
  insert("user", "Anonymous");
  insert("team", 0);
  insert("passkey", "");
  insertBoolean("fold_anon", false);
  insertBoolean("on_idle", false);
  insertBoolean("paused", false);
  insertBoolean("finish", false);
  insert("cause", "any");
  insert("cpus", cpus);
  insertDict("gpus");
  insertList("peers");

  // Load options from config file
  auto &options = app.getOptions();
  std::set<string> keys = {
    "user", "team", "passkey", "fold-anon", "on-idle", "key", "cause", "cpus"};
  for (auto key : keys) {
    string _key = String::replace(key, "-", "_");

    if (options.has(key) && !options[key].isDefault() && options[key].isSet())
      switch (options[key].getType()) {
      case Option::INTEGER_TYPE: insert(_key, options[key].toInteger()); break;
      case Option::DOUBLE_TYPE:  insert(_key, options[key].toDouble());  break;
      case Option::BOOLEAN_TYPE: insert(_key, options[key].toBoolean()); break;
      default:                   insert(_key, options[key]);             break;
      }
  }

  // Load config data
  merge(*config);
}


void Config::update(const JSON::Value &config) {
  for (unsigned i = 0; i < config.size(); i++)
    insert(config.keyAt(i), config.get(i));
}


void Config::setOnIdle(bool onIdle) {insertBoolean("on_idle", onIdle);}
bool Config::getOnIdle() const {return getBoolean("on_idle");}
void Config::setFoldAnon(bool foldAnon) {insertBoolean("fold_anon", foldAnon);}
bool Config::getFoldAnon() const {return getBoolean("fold_anon");}


bool Config::waitForConfig() const {
  return !getFoldAnon() && String::toLower(getUsername()) == "anonymous" &&
    !getTeam() && getPasskey().empty();
}


void Config::setPaused(bool paused) {
  insertBoolean("paused", paused);
  insertBoolean("finish", false);
}


bool Config::getPaused() const {return getBoolean("paused");}
void Config::setFinish(bool finish) {insertBoolean("finish", finish);}
bool Config::getFinish() const {return getBoolean("finish");}


string Config::getUsername() const {
  if (getFoldAnon()) return "Anonymous";
  string user = getString("user", "Anonymous");
  return user.empty() ? "Anonymous" : user;
}


string Config::getPasskey() const {
  return getFoldAnon() ? "" : getString("passkey", "");
}


uint32_t Config::getTeam() const {
  return getFoldAnon() ? 0 : getU32("team", 0);
}


uint64_t Config::getProjectKey() const {return getU64("key", 0);}


uint32_t Config::getCPUs() const {
  uint32_t maxCPUs = SystemInfo::instance().getCPUCount();
  uint32_t cpus    = getU32("cpus");
  return maxCPUs < cpus ? maxCPUs : cpus;
}


ProcessPriority Config::getCorePriority() const {
  return ProcessPriority::parse(getString("priority", "idle"));
}


std::set<string> Config::getGPUs() const {
  std::set<string> gpus;

  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (isGPUEnabled(gpu.getID())) gpus.insert(gpu.getID());
  }

  return gpus;
}


bool Config::isGPUEnabled(const string &id) const {
  auto &gpus = *get("gpus");
  if (!gpus.has(id)) return false;
  return gpus.get(id)->getBoolean("enabled", false);
}


void Config::disableGPU(const string &id) {
  auto &gpus = *get("gpus");
  if (gpus.has(id)) gpus.erase(id);
}


unsigned Config::insert(const string &key, const JSON::ValuePtr &value) {
  if (key == "user" && (!value->isString() || value->getString().empty()))
    return JSON::ObservableDict::insert(key, "Anonymous");

  if (key == "team" && !value->isU32())
    return JSON::ObservableDict::insert(key, 0);

  if (key == "key" && !value->isU64())
    return JSON::ObservableDict::insert(key, 0);

  return JSON::ObservableDict::insert(key, value);
}
