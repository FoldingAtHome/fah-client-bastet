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

#include "WebsocketRemote.h"
#include "App.h"

#include <cbang/event/HTTPConn.h>

using namespace std;
using namespace cb;
using namespace FAH::Client;


WebsocketRemote::WebsocketRemote(
  App &app, const URI &uri, const Version &version) :
  Remote(app), Event::JSONWebsocket(uri, version) {}


void WebsocketRemote::send(const cb::JSON::ValuePtr &msg) {
  pingEvent->add(15);
  if (isActive()) Event::JSONWebsocket::send(*msg);
}


void WebsocketRemote::close() {getConnection()->close();}


void WebsocketRemote::onOpen() {
  pingEvent = getApp().getEventBase()
    .newEvent(this, &WebsocketRemote::sendPing, 0);
  Remote::onOpen();
}


void WebsocketRemote::onClose(Event::WebsockStatus status, const string &msg) {
  cb::Event::JSONWebsocket::onClose(status, msg);
  if (hasConnection()) getConnection()->close();
}


void WebsocketRemote::onComplete() {
  if (pingEvent.isSet()) pingEvent->del();
  Remote::onComplete();
}


void WebsocketRemote::sendPing() {
  // This "ping" is sent because the browser front-end is unable to detect
  // Websocket protocol level PING/PONG events.  With out this application level
  // ping, the front-end would be unable to quickly detect client disconnects.
  send(new JSON::String("ping"));
}
