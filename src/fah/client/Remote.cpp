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

#include "Remote.h"

#include "App.h"
#include "Server.h"
#include "Account.h"
#include "Units.h"
#include "Unit.h"
#include "Config.h"
#include "Group.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Remote::Remote(App &app) : app(app) {}
Remote::~Remote() {}


void Remote::sendViz() {
  if (vizUnitID.empty()) return;

  auto unit = app.getUnits()->findUnit(vizUnitID);
  if (unit.isNull()) {
    vizUnitID = "";
    return;
  }

  auto topology = unit->getTopology();
  auto frames   = unit->getFrames();

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
  while (vizFrame < frames.size()) {
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


void Remote::sendWUs() {
  if (!sendWUsEnabled) return;

  auto changes = SmartPtr(new JSON::List);
  auto wus     = SmartPtr(new JSON::List);
  changes->append("wus");
  changes->append(wus);

  app.getDB("wu_log", true)
    .foreach([&wus] (const string &id, const string &data) {
      try {
        wus->append(JSON::parse(data));
      } CATCH_ERROR;
    }, 10000);


  sendChanges(changes);
}


void Remote::logWU(const Unit &wu) {
  if (!sendWUsEnabled) return;

  auto changes = SmartPtr(new JSON::List);
  auto wus     = SmartPtr(new JSON::List);
  changes->append("wus");
  changes->append(-2);
  changes->append(wus);
  wus->append(wu.copy(true));

  sendChanges(changes);
}


void Remote::sendChanges(const JSON::ValuePtr &changes) {
  send(changes);

  // Check for viz frame changes: ["units", <unit index>, "frames", #]
  if (changes->size() == 4 && changes->getString(0) == "units" &&
      changes->getString(2) == "frames") sendViz();
}


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "msg: " << *msg);

  string cmd = msg->getString("cmd", "");

  if (cmd == "dump")
    app.getUnits()->getUnit(msg->getString("unit", ""))->dumpWU();
  else if (cmd == "finish")  app.setState("finish"); // Deprecated
  else if (cmd == "pause")   app.setState("pause");  // Deprecated
  else if (cmd == "unpause") app.setState("fold");   // Deprecated
  else if (cmd == "state")   app.setState(*msg);
  else if (cmd == "config")  app.configure(*msg);
  else if (cmd == "restart") app.getAccount().restart();
  else if (cmd == "link")
    app.getAccount().link(msg->getString("token"), msg->getString("name"));

  else if (cmd == "viz") {
    vizUnitID = msg->getString("unit", "");
    vizFrame  = msg->getU32("frame", 0);
    sendViz();

  } else if (cmd == "log") {
    if (msg->getBoolean("enable", false))
      app.getLogTracker().add(SmartPhony(this), lastLogLine);
    else app.getLogTracker().remove(SmartPhony(this));

  } else if (cmd == "wus") {
    sendWUsEnabled = msg->getBoolean("enable", false);
    sendWUs();

  } else {
    LOG_WARNING("Received unsupported remote command '" << cmd << "'");
    return;
  }
}


void Remote::onOpen() {
  LOG_DEBUG(3, "New client " << getName());
  send(SmartPointer<JSON::Value>::Phony(&app));
}


void Remote::onComplete() {
  LOG_DEBUG(3, "Closing client " << getName());
  app.getLogTracker().remove(SmartPhony(this));
  app.remove(*this);
}


void Remote::logUpdate(const SmartPointer<JSON::List> &lines, uint64_t last) {
  bool restart = !lastLogLine || lastLogLine < last - lines->size();
  lastLogLine = last;

  SmartPointer<JSON::List> changes = new JSON::List;

  changes->append("log");
  if (!restart) changes->append(-2);
  changes->append(lines);
  sendChanges(changes);
}
