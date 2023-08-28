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
#include "Account.h"
#include "Units.h"
#include "Config.h"

#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Remote::Remote(App &app) : app(app) {}
Remote::~Remote() {if (logEvent.isSet()) logEvent->del();}


void Remote::sendViz() {
  if (vizUnitID.empty()) return;

  auto &unit    = app.getUnits().getUnit(vizUnitID);
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


void Remote::readLogToNextLine() {
  const unsigned size = 1024;
  char buffer[size];

  do {
    log->clear();
    log->getline(buffer, size);
  } while (log->rdstate() & ios::failbit && log->gcount());
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
      readLogToNextLine();
      sendLog();
    } CATCH_ERROR;

    return;
  }

  try {
    bool done = false;
    SmartPointer<JSON::List> lines = new JSON::List;

    for (int i = 0; i < 256; i++) {
      const unsigned size = 1024;
      char buffer[size];

      log->getline(buffer, size);
      streamsize count = log->gcount();
      auto state = log->rdstate();

      if (state & ios::eofbit) {
        log->clear();
        done = true;
      }

      if (count) {
        if (buffer[count - 1] == '\r') buffer[count - 1] = 0;
        lines->append(buffer);
      }

      // Handle incomplete long lines
      if ((state & ios::failbit) && count)
        readLogToNextLine();

      logOffset = log->tellg();

      if (state & ios::badbit) {
        done = true;
        log.release();
      }

      if (done) {
        logEvent->add(1);
        break;
      }
    }

    if (!done) logEvent->activate();

    if (!lines->empty()) {
      SmartPointer<JSON::List> changes = new JSON::List;
      changes->append("log");
      changes->append(-2);
      changes->append(lines);
      sendChanges(changes);
    }

    return;
  } CATCH_ERROR;

  log.release();
  logEvent->add(5);
}


void Remote::sendChanges(const JSON::ValuePtr &changes) {
  send(changes);

  // Check for viz frame changes: ["units", <unit index>, "frames", #]
  if (changes->size() == 4 && changes->getString(0) == "units" &&
      changes->getString(2) == "frames") sendViz();
}


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "msg: " << *msg);

  string cmd  = msg->getString("cmd",  "");
  string unit = msg->getString("unit", "");

  if      (cmd == "dump")    app.getUnits().dump(unit);
  else if (cmd == "finish")  app.getConfig().setFinish(true);
  else if (cmd == "pause")   app.getConfig().setPaused(true);
  else if (cmd == "unpause") app.getConfig().setPaused(false);
  else if (cmd == "config")  app.getConfig().update(*msg->get("config"));
  else if (cmd == "reset")   app.getAccount().reset();
  else if (cmd == "unlink")  app.getAccount().setToken("");
  else if (cmd == "link") {
    app.getAccount().setToken(msg->getString("token"));
    app.getAccount().setMachName(msg->getString("name"));

  } else if (cmd == "viz") {
    vizUnitID = unit;
    vizFrame  = msg->getU32("frame", 0);
    sendViz();

  } else if (cmd == "log") {
    followLog = msg->getBoolean("enable", false);
    logOffset = msg->getS64("offset", -(1 << 17));
    sendLog();

  } else {
    LOG_WARNING("Received unsupported remote command '" << cmd << "'");
    return;
  }

  app.getUnits().triggerUpdate(true);
}


void Remote::onOpen() {
  LOG_DEBUG(3, "New client " << getName());
  send(SmartPointer<JSON::Value>::Phony(&app));
}


void Remote::onComplete() {
  LOG_DEBUG(3, "Closing client " << getName());
  if (logEvent.isSet()) logEvent->del();
  app.remove(*this);
}
