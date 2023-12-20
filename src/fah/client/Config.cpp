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
#include <cbang/util/Resource.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;

namespace FAH {namespace Client {extern const DirectoryResource resource0;}}


Config::Config(App &app, const JSON::ValuePtr &config) : app(app) {
  // Load defaults
  auto &r  = FAH::Client::resource0.get("config.json");
  defaults = JSON::Reader::parseString(r.toString());
  merge(*defaults);

  // Load config data
  for (unsigned i = 0; i < config->size(); i++) {
    string key = config->keyAt(i);
    if (defaults->has(key)) insert(key, config->get(i));
  }
}


void Config::load(const Options &opts) {
  for (unsigned i = 0; i < size(); i++) {
    string _key = keyAt(i);
    string key  = String::replace(_key, "_", "-");

    if (opts.has(key) && !opts[key].isDefault() && opts[key].isSet())
      switch (opts[key].getType()) {
      case Option::INTEGER_TYPE: insert(_key, opts[key].toInteger()); break;
      case Option::DOUBLE_TYPE:  insert(_key, opts[key].toDouble());  break;
      case Option::BOOLEAN_TYPE: insert(_key, opts[key].toBoolean()); break;
      default:                   insert(_key, opts[key]);             break;
      }
  }
}


void Config::configure(const JSON::Value &config) {
  for (unsigned i = 0; i < config.size(); i++)
    if (has(config.keyAt(i)))
      insert(config.keyAt(i), config.get(i));
}


void Config::setState(const JSON::Value &msg) {
  string state = msg.getString("state");

  if (state == "pause")  return setPaused(true);
  if (state == "fold")   return setPaused(false);
  if (state == "finish") return setFinish(true);

  LOG_WARNING("Unsupported config state '" << state << "'");
}


void Config::setOnIdle(bool onIdle) {insertBoolean("on_idle", onIdle);}
bool Config::getOnIdle() const {return getBoolean("on_idle");}


void Config::setPaused(bool paused) {
  insertBoolean("paused", paused);
  insertBoolean("finish", false);
}


bool Config::getPaused() const {return getBoolean("paused");}
void Config::setFinish(bool finish) {insertBoolean("finish", finish);}
bool Config::getFinish() const {return getBoolean("finish");}


string Config::getUsername() const {
  string user = getString("user", "Anonymous");
  return user.empty() ? "Anonymous" : user;
}


void Config::setUsername(const string &user) {insert("user", user);}
string Config::getPasskey() const {return getString("passkey", "");}
void Config::setPasskey(const string &passkey) {insert("passkey", passkey);}
uint32_t Config::getTeam() const {return getU32("team", 0);}
void Config::setTeam(uint32_t team) {insert("team", team);}


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

  auto &allGPUs = app.getGPUs();
  for (unsigned i = 0; i < allGPUs.size(); i++) {
    auto &gpu = *allGPUs.get(i).cast<GPUResource>();
    if (gpu.getBoolean("supported") && isGPUEnabled(gpu.getID()))
      gpus.insert(gpu.getID());
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


void Config::setAccountData(const JSON::ValuePtr &data) {
  setUsername(data->getString("user", "Anonymous"));
  setPasskey(data->getString("passkey", ""));
  setTeam(data->getU32("team", 0));
  insert("cause", data->getString("cause", "any"));
}


int Config::insert(const string &key, const JSON::ValuePtr &value) {
  if (!defaults->has(key)) {
    LOG_WARNING("Ignoring unsupported config key '" << key << "'");
    return -1;
  }

  if (defaults->get(key)->getType() != value->getType()) {
    LOG_WARNING("Ignoring config key '" << key << "' with wrong type");
    return -1;
  }

  return Super_T::insert(key, value);
}
