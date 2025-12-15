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
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Server::Server(App &app) :
  HTTP::Server(app.getEventBase()), app(app) {
  addOptions(app.getOptions());
  app.getOptions()["http-addresses"].setDefault("127.0.0.1:7396");
  setPortPriority(3);
}


void Server::init() {
  auto &options = app.getOptions();

  // Allowed origins
  for (auto origin: options["allowed-origins"].toStrings())
    allowedOrigins.push_back(Regex::escape(origin));
  for (auto origin: options["allowed-origin-exprs"].toStrings())
    allowedOrigins.push_back(origin);

  addMember(this, &Server::corsCB);

  addMember(HTTP_GET, "/ping", this, &Server::redirectPing);
  addMember(HTTP_GET, "/api/websocket", this, &Server::handleWebsocket);

  // Web root
  if (options["web-root"].hasValue()) {
    string root = options["web-root"];
    if (SystemUtilities::exists(root)) {
      addHandler("/.*", root);
      addHandler("/.*", root + "/index.html");
    }
  }

  // Init
  HTTP::Server::init(options);

  addMember(HTTP_GET, "/.*", this, &Server::redirectWebControl);
}


bool Server::allowed(const string &origin) const {
  for (auto re: allowedOrigins)
    if (re.match(origin)) return true;

  return false;
}


bool Server::corsCB(HTTP::Request &req) {
  if (req.inHas("Origin")) {
    string origin = req.inGet("Origin");

    if (!allowed(origin)) THROWX("Access denied by Origin: " << origin, HTTP_UNAUTHORIZED);

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


bool Server::redirectWebControl(HTTP::Request &req) {
  req.redirect(app.getURL());
  return true;
}


bool Server::redirectPing(HTTP::Request &req) {
  // v7 Web Control makes this jsonp request
  auto &uri = req.getURI();

  if (uri.has("callback")) {
    string callback = uri.get("callback");
    string payload  = callback + "({\"redirect\":\"" + app.getURL() + "\"})";
    req.setContentType("application/json");
    req.reply(payload);
    return true;
  }

  return false;
}


bool Server::handleWebsocket(HTTP::Request &req) {
  auto ws = SmartPtr(new WebsocketRemote(app));
  ws->upgrade(req);
  app.add(ws);
  return true;
}
