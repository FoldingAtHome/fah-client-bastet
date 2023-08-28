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

#pragma once

#include <cbang/event/Event.h>
#include <cbang/event/JSONWebsocket.h>
#include <cbang/util/Backoff.h>
#include <cbang/json/Value.h>
#include <cbang/openssl/Cipher.h>


namespace FAH {
  namespace Client {
    class App;
    class NodeRemote;

    class Account : public cb::Event::JSONWebsocket {
      App &app;

      std::string token;
      std::string machName = "machine-#";
      cb::JSON::ValuePtr data;

      std::string key;
      cb::SmartPointer<cb::Cipher> cipher;

      cb::SmartPointer<cb::Event::Event> updateEvent;
      cb::Backoff updateBackoff = cb::Backoff(15, 4 * 60);

      std::set<std::string> ivs;

      typedef std::map<std::string, cb::SmartPointer<NodeRemote>> nodes_t;
      nodes_t nodes;

    public:
      Account(App &app);

      void setData(const cb::JSON::ValuePtr &data);
      void setToken(const std::string &token);

      const std::string &getMachName() const {return machName;}
      void setMachName(const std::string &name) {machName = name;}

      void init();
      void reset();

      void sendEncrypted(const cb::JSON::Value &msg, const std::string &sid);

    protected:
      void requestInfo();
      void connect();
      void link();
      void update();

      // From cb::Event::Websocket
      void onOpen();
      void onMessage(const cb::JSON::ValuePtr &msg);
      void onClose(cb::Event::WebsockStatus status, const std::string &msg);

      // From cb::Event::JSONWebsocket
      void send(const cb::JSON::Value &msg);
    };
  }
}
