/******************************************************************************\

                 This file is part of the Folding@home Client.

          The fahclient runs Folding@home protein folding simulations.
                   Copyright (c) 2001-2019, foldingathome.org
                              All rights reserved.

      This program is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
       the Free Software Foundation; either version 2 of the License, or
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

#include "Server.h"
#include "App.h"
#include "ComputeResources.h"

#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Server::Server(App &app) :
  Event::WebServer(app.getOptions(), app.getEventBase()), app(app) {
  app.getOptions()["http-addresses"].setDefault("127.0.0.1:7396");

  addMember(this, &Server::corsCB);
  addMember(HTTP_GET, "/api/info", this, &Server::infoCB);
}



bool Server::corsCB(Event::Request &req) {
  req.outSet("Access-Control-Allow-Origin",
             "https://console.foldingathome.org");
  req.outSet("Access-Control-Allow-Methods", "POST,PUT,GET,OPTIONS,DELETE");
  req.outSet("Access-Control-Allow-Credentials", "true");
  req.outSet("Access-Control-Allow-Headers",
             "DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,"
             "Content-Type,Range,Set-Cookie,Authorization");

  if (req.getMethod() == HTTP_OPTIONS) {
    req.reply();
    return true;
  }

  return false;
}


void Server::infoCB(Event::Request &req, const JSON::ValuePtr &msg,
                    JSON::Sink &sink) {
  sink.beginDict();

  app.writeSystemInfo(sink);

  auto &config = app.getDB("config");
  sink.insertDict("config");
  sink.insert("user", config.getString("user", "anonymous"));
  sink.insert("team", config.getInteger("team", 0));
  if (config.has("passkey"))
    sink.insert("passkey", config.getString("passkey"));
  sink.insert("cause", config.getString("cause", "any"));
  sink.insert("power", config.getString("power", "medium"));
  sink.insertBoolean("paused", config.getBoolean("paused", false));
  sink.insert("max-cpus", config.getInteger("max-cpus", 0));
  sink.insertBoolean("on-idle", config.getBoolean("on-idle", true));
  sink.endDict();

  sink.beginInsert("resources");
  app.getComputeResources().write(sink);

  sink.endDict();
}
