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

#include "Remote.h"

#include "App.h"
#include "Server.h"
#include "Units.h"
#include "Config.h"

#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Remote::Remote(App &app, Event::RequestMethod method, const URI &uri,
               const Version &version) :
  Event::JSONWebsocket(method, uri, version), app(app) {}


void Remote::sendViz() {
  if (vizUnitID.empty()) return;

  auto unitIndex = app.getUnits().getUnitIndex(vizUnitID);
  auto &unit = app.getUnits().getUnit(unitIndex);
  auto topology = unit.getTopology();
  auto frames = unit.getFrames();

  if (topology.isNull()) return;

  // Send topology
  if (!vizFrame) {
    SmartPointer<JSON::List> changes = new JSON::List;
    changes->append("viz");
    changes->append(vizUnitID);
    changes->append("topology");
    changes->append(topology);
    sendChanges(changes);
  }

  // Send frames
  while (vizFrame < frames.size() - 1) {
    SmartPointer<JSON::List> changes = new JSON::List;
    changes->append("viz");
    changes->append(vizUnitID);
    changes->append("frames");
    changes->append(vizFrame);
    changes->append(frames[vizFrame]);
    sendChanges(changes);
    vizFrame++;
  }
}


void Remote::sendChanges(const JSON::ValuePtr &changes) {
  send(*changes);

  // Check for viz frame changes: ["units", <unit index>, "frames", #]
  if (changes->size() == 4 && changes->getString(0) == "units" &&
      changes->getString(2) == "frames") {
    unsigned unitIndex = changes->getU32(1);
    string unitID = app.getUnits().getUnit(unitIndex).getID();

    if (vizUnitID == unitID) sendViz();
  }
}


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "msg: " << *msg);

  string cmd = msg->getString("cmd", "");

  if (cmd == "viz") {
    vizUnitID = msg->getString("unit", "");
    vizFrame = msg->getU32("frame", 0);
    sendViz();
  }

  if (cmd == "pause")   app.getConfig().setPaused(true);
  if (cmd == "unpause") app.getConfig().setPaused(false);
  if (cmd == "config")  app.getConfig().merge(*msg->get("config"));

  app.getUnits().triggerUpdate(true);
}


void Remote::onOpen() {send(app.getServer());}
void Remote::onComplete() {app.getServer().remove(*this);}
