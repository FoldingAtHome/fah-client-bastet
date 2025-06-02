/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2024, foldingathome.org
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


Config::Config(App &app, const JSON::ValuePtr &defaults) :
  app(app), defaults(defaults) {merge(*defaults);}


void Config::load(const JSON::Value &config) {
  for (auto it = config.begin(); it != config.end(); it++)
    if (defaults->has(it.key())) insert(it.key(), *it);
}


void Config::load(const Options &opts) {
  for (auto it = begin(); it != end(); it++) {
    string _key = it.key();
    string key  = String::replace(_key, "_", "-");

    if (opts.has(key) && !opts[key].isDefault() && opts[key].isSet())
      switch (opts[key].getType()) {
      case Option::TYPE_INTEGER: insert(_key, opts[key].toInteger()); break;
      case Option::TYPE_DOUBLE:  insert(_key, opts[key].toDouble());  break;
      case Option::TYPE_BOOLEAN: insert(_key, opts[key].toBoolean()); break;
      default:                   insert(_key, opts[key]);             break;
      }
  }
}


void Config::configure(const JSON::Value &config) {
  for (auto it = config.begin(); it != config.end(); it++)
    if (has(it.key())) insert(it.key(), *it);
}


void Config::setState(const JSON::Value &msg) {
  string state = msg.getString("state");

  if      (state == "pause")  setPaused(true);
  else if (state == "fold")   setPaused(false);
  else if (state == "finish") insertBoolean("finish", true);
  else LOG_WARNING("Unsupported config state '" << state << "'");
}


bool Config::getOnIdle()    const {return getBoolean("on_idle");}
bool Config::getOnBattery() const {return getBoolean("on_battery");}
bool Config::getKeepAwake() const {return getBoolean("keep_awake");}


void Config::setPaused(bool paused) {
  insertBoolean("paused", paused);
  insertBoolean("finish", false);
}


bool Config::getPaused() const {return getBoolean("paused");}
bool Config::getFinish() const {return getBoolean("finish");}


string Config::getUsername() const {
  string user = getString("user", "Anonymous");
  return user.empty() ? "Anonymous" : user;
}


string Config::getPasskey() const {return getString("passkey", "");}
uint32_t Config::getTeam() const {return getU32("team", 0);}


uint64_t Config::getProjectKey(const std::set<string> &gpus) const {
  return getU64("key", 0);
}


bool Config::getBeta(const std::set<string> &gpus) const {
  return getBoolean("beta", false);
}


uint32_t Config::getCPUs() const {
  uint32_t maxCPUs = SystemInfo::instance().getCPUCount();
  uint32_t cpus    = getU32("cpus");
  return maxCPUs < cpus ? maxCPUs : cpus;
}


std::set<string> Config::getGPUs() const {
  std::set<string> gpus;

  for (auto &v: app.getGPUs()) {
    auto &gpu = *v.cast<GPUResource>();
    if (gpu.isSupported(*this)) gpus.insert(gpu.getID());
  }

  return gpus;
}


bool Config::isGPUEnabled(const string &id) const {
  auto &gpus = *get("gpus");
  if (!gpus.has(id)) return false;
  return gpus.get(id)->getBoolean("enabled", false);
}


bool Config::isComputeDeviceEnabled(const string &type) const {
  return getBoolean(type, true);
}


void Config::disableGPU(const string &id) {
  auto &gpus = *get("gpus");
  if (gpus.has(id)) gpus.erase(id);
}


JSON::Value::iterator Config::insert(
  const string &key, const JSON::ValuePtr &value) {
  if (!defaults->has(key)) {
    LOG_WARNING("Ignoring unsupported config key '" << key << "'");
    return end();
  }

  if (defaults->get(key)->getType() != value->getType()) {
    LOG_WARNING("Ignoring config key '" << key << "' with wrong type "
                << value->getType());
    return end();
  }

  return Super_T::insert(key, value);
}
