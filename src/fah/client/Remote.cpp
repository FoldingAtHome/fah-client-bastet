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


void Remote::onMessage(const JSON::ValuePtr &msg) {
  LOG_DEBUG(3, "msg: " << *msg);

  string cmd = msg->getString("cmd", "");

  if (cmd == "pause")   app.getConfig().setPaused(true);
  if (cmd == "unpause") app.getConfig().setPaused(false);
  if (cmd == "config")  app.getConfig().merge(*msg->get("config"));

  app.getUnits().triggerUpdate(true);
}


void Remote::onOpen() {send(app.getServer());}
void Remote::onComplete() {app.getServer().remove(*this);}
