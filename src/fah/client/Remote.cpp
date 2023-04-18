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

#include "Remote.h"

#include "App.h"
#include "Server.h"
#include "Units.h"
#include "Config.h"
#include "ResourceGroup.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Remote::Remote(App &app, ResourceGroup &group, Event::RequestMethod method,
               const URI &uri, const Version &version) :
  Event::JSONWebsocket(method, uri, version), app(app), group(group) {}


Remote::~Remote() {if (logEvent.isSet()) logEvent->del();}


void Remote::sendViz() {
  if (vizUnitID.empty()) return;

  auto &unit    = group.getUnits()->getUnit(vizUnitID);
  auto topology = unit.getTopology();
  auto frames   = unit.getFrames();

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


void Remote::sendLog() {
  if (!followLog) return;

  if (logEvent.isNull())
    logEvent = app.getEventBase().newEvent(this, &Remote::sendLog, 0);

  if (log.isNull()) {
    try {
      string filename = app.getOptions()["log"];
      log = SystemUtilities::open(filename, ios::in);

      // Check offset bounds
      streamsize len = SystemUtilities::getFileSize(filename);
      if (len < logOffset) logOffset = len;
      if (logOffset < -len) logOffset = -len;

      log->seekg(logOffset, logOffset < 0 ? ios::end : ios::beg);
      sendLog();
    } CATCH_ERROR;

    return;
  }

  try {
    for (int i = 0; i < 64; i++) {
      const unsigned size = 4096;
      char buffer[size];

      log->getline(buffer, size);
      streamsize count = log->gcount();

      bool done = false;
      if (log->eof() && !log->bad()) {
        log->clear();
        done = true;
      }

      if (count) {
        if (buffer[count - 1] == '\r') buffer[count - 1] = 0;

        SmartPointer<JSON::List> changes = new JSON::List;
        changes->append("log");
        changes->append(-1);
        changes->append(buffer);
        sendChanges(changes);
      }

      logOffset = log->tellg();

      if (done) {
        logEvent->add(1);
        return;
      }
    }

    logEvent->activate();
    return;
  } CATCH_ERROR;

  log.release();
  logEvent->add(5);
}


void Remote::sendChanges(const JSON::ValuePtr &changes) {
  send(*changes);

  // Check for viz frame changes: ["units", <unit index>, "frames", #]
  if (changes->size() == 4 && changes->getString(0) == "units" &&
      changes->getString(2) == "frames") {
    unsigned unitIndex = changes->getU32(1);
    string   unitID    = group.getUnits()->getUnit(unitIndex).getID();

    if (vizUnitID == unitID) sendViz();
  }
}


void Remote::send(const JSON::Value &msg) {
  pingEvent->add(15);
  Event::JSONWebsocket::send(msg);
}


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "'" << group.getName() << "' msg: " << *msg);

  string cmd  = msg->getString("cmd",  "");
  string unit = msg->getString("unit", "");

  if (cmd == "viz") {
    vizUnitID = unit;
    vizFrame  = msg->getU32("frame", 0);
    sendViz();
  }

  if (cmd == "log") {
    followLog = msg->getBoolean("enable", false);
    logOffset = msg->getS64("offset", -(1 << 17));
    sendLog();
  }

  if (cmd == "dump")    group.getUnits()->dump(unit);
  if (cmd == "finish")  group.getConfig()->setFinish(true);
  if (cmd == "pause")   group.getConfig()->setPaused(true);
  if (cmd == "unpause") group.getConfig()->setPaused(false);

  if (cmd == "config") {
    group.getConfig()->update(*msg->get("config"));
    if (group.getName().empty()) app.updateGroups();
    app.updateResources();
  }

  group.getUnits()->triggerUpdate(true);
}


void Remote::onOpen() {
  LOG_DEBUG(3, group.getName() << ":New client from " << getClientIP());
  pingEvent = app.getEventBase().newEvent(this, &Remote::sendPing, 0);
  send(group);
}


void Remote::onComplete() {
  LOG_DEBUG(3, group.getName() << ":Closing client from " << getClientIP());
  if (logEvent.isSet())  logEvent->del();
  if (pingEvent.isSet()) pingEvent->del();
  group.remove(*this);
}


void Remote::sendPing() {send(JSON::String("ping"));}
