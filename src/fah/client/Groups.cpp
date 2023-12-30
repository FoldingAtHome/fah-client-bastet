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

#include "Groups.h"
#include "App.h"
#include "Config.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>

using namespace std;
using namespace cb;
using namespace FAH::Client;


Groups::Groups(App &app) : app(app) {
  LOG_INFO(3, "Loading default group");
  getGroup(""); // Default group

  app.getDB("groups").foreach(
    [this] (const string &name, const string &dataStr) {
      try {
        if (!name.empty()) getGroup(name);
      } CATCH_ERROR;
    });
}


const Group &Groups::getGroup(const string &name) const {
  return *get(name).cast<Group>();
}


Group &Groups::getGroup(const string &name) {
  if (!has(name)) {
    LOG_INFO(3, "Loading " << (name.empty() ? "default" : name)
             << " resource group");

    SmartPointer<Group> group = new Group(app, name);
    insert(name, group);
    group->save();
  }

  return *get(name).cast<Group>();
}


void Groups::delGroup(const string &name) {
  if (name.empty()) THROW("Cannot delete default group");

  if (!has(name)) return;

  LOG_INFO(1, "Deleting resource group " << name);

  // Migrate units
  auto &group = getGroup(name);
  auto &root  = getGroup("");

  for (auto unit: group.units())
    unit->setGroup(&root);

  // Remove from DB
  group.remove();

  // Remove
  erase(name);
}


void Groups::configure(const JSON::Value &configs) {
  // Delete removed groups
  std::set<string> remove;
  for (auto &name: keys())
    if (!name.empty() && !configs.has(name))
      remove.insert(name);

  for (auto &name: remove) delGroup(name);

  // Configure groups
  for (auto &name: configs.keys())
    getGroup(name).getConfig().configure(*configs.get(name));
}


void Groups::triggerUpdate() {
  for (auto &name: keys())
    getGroup(name).triggerUpdate();
}


void Groups::setState(const JSON::Value &msg) {
  if (msg.hasString("group"))
    getGroup(msg.getString("group")).getConfig().setState(msg);

  else
    for (auto &name: keys())
      getGroup(name).getConfig().setState(msg);
}


bool Groups::getPaused() const {
  for (auto &name: keys())
    if (!getGroup(name).getConfig().getPaused())
      return false;

  return true;
}
