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

#pragma once

#include "Remote.h"

#include <cbang/ws/JSONWebsocket.h>


namespace FAH {
  namespace Client {
    class WebsocketRemote : public Remote, public cb::WS::JSONWebsocket {
      cb::SmartPointer<cb::Event::Event> pingEvent;

    public:
      WebsocketRemote(
        App &app, const cb::SmartPointer<cb::HTTP::Conn> &connection,
        const cb::URI &uri, const cb::Version &version);

      // From Remote
      std::string getName() const {return getClientAddr().toString(false);}
      void send(const cb::JSON::ValuePtr &msg);
      void close();

      // From cb::WS::JSONWebsocket
      using cb::WS::JSONWebsocket::send;
      void onMessage(const cb::JSON::ValuePtr &msg) {Remote::onMessage(msg);}

      // From cb::WS::Websocket
      void onOpen();
      void onClose(cb::WS::Status status, const std::string &msg);

      // From cb::HTTP::Request
      void onComplete();

    protected:
      void sendPing();
    };
  }
}
