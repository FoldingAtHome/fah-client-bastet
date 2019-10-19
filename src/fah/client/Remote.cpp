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


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "msg: " << *msg);

  string cmd = msg->getString("cmd", "");
  string unit = msg->getString("unit", "");

  if (cmd == "pause") app.getUnits().setPause(true, unit);
  if (cmd == "unpause") app.getUnits().setPause(false, unit);
  if (cmd == "config") app.getConfig().merge(*msg->get("config"));
}


void Remote::onOpen() {send(app.getServer());}
void Remote::onComplete() {app.getServer().remove(*this);}
