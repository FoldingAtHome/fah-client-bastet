/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2026, foldingathome.org
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
#include <cbang/event/Port.h>
#include <cbang/log/Logger.h>
#include <cbang/net/SockAddr.h>
#include <cbang/net/URI.h>
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Server::Server(App &app) :
  HTTP::Server(app.getEventBase()), app(app) {
  addOptions(app.getOptions());
  app.getOptions()["http-addresses"].setDefault("127.0.0.1:7396");
  app.getOptions()["http-max-body-size"   ].setDefault(maxInputSize);
  app.getOptions()["http-max-headers-size"].setDefault(maxInputSize);
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

  return allowedLoopback(origin);
}


// Allow loopback origins, but only on addresses this server actually listens
// on.  Otherwise any local app serving a page on any other loopback address or
// port could control the client.
bool Server::allowedLoopback(const string &origin) const {
  try {
    URI uri(origin);

    bool secure = uri.getScheme() == "https";
    if (!secure && uri.getScheme() != "http") return false;

    string host = uri.getHost();
    bool   name = host == "localhost";

    // Which loopback address ``localhost`` resolves to is up to the browser
    SockAddr addr;
    if (!name) {
      // Strip IPv6 brackets
      if (2 < host.size() && host.front() == '[' && host.back() == ']')
        host = host.substr(1, host.size() - 2);

      addr = SockAddr::parse(host);
      if (!addr.isLoopback()) return false;
    }

    for (auto &port: getPorts()) {
      auto &a = port->getAddr();

      if (port->isSecure() != secure || a.getPort() != uri.getPort()) continue;

      // A wildcard bind listens on the loopback addresses too
      if (a.isZero()) return true;
      if (name ? a.isLoopback() : a.toString(false) == addr.toString(false))
        return true;
    }

  } catch (const Exception &) {} // Malformed Origin

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

    // Sanitize callback to valid JS identifiers to prevent XSS injection
    if (!Regex("[a-zA-Z_][a-zA-Z0-9_.]*").match(callback))
      THROWX("Invalid JSONP callback", HTTP_BAD_REQUEST);

    string payload  = callback + "({\"redirect\":\"" + app.getURL() + "\"})";
    req.setContentType("application/javascript");
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
