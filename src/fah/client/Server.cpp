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

#include "Server.h"
#include "App.h"
#include "WebsocketRemote.h"

#include <cbang/Info.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Server::Server(App &app) :
  Event::WebServer(app.getOptions(), app.getEventBase()), app(app) {
  app.getOptions()["http-addresses"].setDefault("127.0.0.1:7396");

  setEventPriority(3);

  addMember(this, &Server::corsCB);
  addMember(HTTP_GET, "/", this, &Server::redirectWebControl);
  addMember(HTTP_GET, "/ping", this, &Server::redirectPing);
}


void Server::init() {
  auto &options = app.getOptions();

  // Allowed orgins
  auto origins = options["allowed-origins"].toStrings();
  for (unsigned i = 0; i < origins.size(); i++)
    allowedOrigins.insert(origins[i]);

  // Web root
  if (options["web-root"].hasValue()) {
    string root = options["web-root"];
    addHandler("/.*", root);
    addHandler("/.*", root + "/index.html");
  }

  // Init
  WebServer::init();
}


SmartPointer<Event::Request>
Server::createRequest(Event::RequestMethod method, const URI &uri,
                      const Version &version) {
  if (method == HTTP_GET &&
      String::startsWith(uri.getPath(), "/api/websocket")) {
    string name = uri.getPath().substr(14);

    auto client = SmartPtr(new WebsocketRemote(app, uri, version));
    app.add(client);
    return client;
  }

  return Event::WebServer::createRequest(method, uri, version);
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


bool Server::redirectWebControl(Event::Request &req) {
  req.redirect(Info::instance().get(app.getName(), "URL"));
  return true;
}


bool Server::redirectPing(Event::Request &req) {
  // v7 Web Control makes this jsonp request
  auto &uri = req.getURI();

  if (uri.has("callback")) {
    string callback = uri.get("callback");
    string redirect = Info::instance().get(app.getName(), "URL");
    string payload  = callback + "({\"redirect\":\"" + redirect + "\"})";
    req.setContentType("application/json");
    req.reply(payload);
    return true;
  }

  return false;
}
