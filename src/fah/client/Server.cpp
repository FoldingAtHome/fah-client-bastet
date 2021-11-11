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

#include "Server.h"
#include "App.h"
#include "Remote.h"
#include "Config.h"
#include "Units.h"

#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Server::Server(App &app) :
  Event::WebServer(app.getOptions(), app.getEventBase()), app(app) {
  app.getOptions()["http-addresses"].setDefault("127.0.0.1:7396");

  addMember(this, &Server::corsCB);
}


bool Server::corsCB(Event::Request &req) {
  if (req.inHas("Origin")) {
    string origin = req.inGet("Origin");

    if (allowedOrigins.find(origin) == allowedOrigins.end())
      THROWX("Access denied by Origin", HTTP_UNAUTHORIZED);

    req.outSet("Access-Control-Allow-Origin", origin);
    req.outSet("Access-Control-Allow-Methods", "POST,PUT,GET,OPTIONS,DELETE");
    req.outSet("Access-Control-Allow-Credentials", "true");
    req.outSet("Access-Control-Allow-Headers",
               "DNT,User-Agent,X-Requested-With,"
               "If-Modified-Since,Cache-Control,Content-Type,Range,"
               "Set-Cookie,Authorization");
    req.outSet("Vary", "Origin");
  }

  if (req.getMethod() == HTTP_OPTIONS) {
    req.reply();
    return true;
  }

  return false;
}


void Server::broadcast(const JSON::ValuePtr &msg) {
  LOG_DEBUG(5, __func__ << ' ' << *msg);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    auto &ws = **it;
    if (ws.isActive()) ws.send(*msg);
  }
}


void Server::remove(Remote &remote) {
  for (auto it = clients.begin(); it != clients.end(); it++)
    if (*it == &remote) {
      clients.erase(it);
      break;
    }
}


void Server::init() {
  auto &options = app.getOptions();

  // Allowed orgins
  auto origins = options["allowed-origins"].toStrings();
  for (unsigned i = 0; i < origins.size(); i++)
    allowedOrigins.insert(origins[i]);

  // Web root
  if (options["web-root"].hasValue())
    addHandler("/.*", options["web-root"]);

  // Init
  WebServer::init();
}


SmartPointer<Event::Request>
Server::createRequest(Event::RequestMethod method, const URI &uri,
                      const Version &version) {
  if (method == HTTP_GET && uri.getPath() == "/api/websocket") {
    LOG_DEBUG(3, "New websocket client");
    clients.push_back(new Remote(app, method, uri, version));
    return clients.back();
  }

  return Event::WebServer::createRequest(method, uri, version);
}


bool Server::handleRequest(const SmartPointer<Event::Request> &req) {
  if (req->isWebsocket()) return true;
  return Event::WebServer::handleRequest(req);
}


void Server::notify(list<JSON::ValuePtr> &change) {
  SmartPointer<JSON::List> list = new JSON::List(change.begin(), change.end());
  broadcast(list);

  // Automatically save changes to config
  if (!change.empty() && change.front()->getString() == "config") {
    app.getDB("config").set("config", app.getConfig());
    app.getUnits().update();
  }
}
