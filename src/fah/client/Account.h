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

#include <cbang/event/Event.h>
#include <cbang/util/LifetimeManager.h>
#include <cbang/ws/JSONWebsocket.h>
#include <cbang/util/Backoff.h>
#include <cbang/json/Value.h>
#include <cbang/openssl/Cipher.h>
#include <cbang/openssl/KeyPair.h>


namespace FAH {
  namespace Client {
    class App;
    class NodeRemote;

    class Account : public cb::WS::JSONWebsocket {
      App &app;

      typedef enum {
        STATE_IDLE,
        STATE_LINK,
        STATE_INFO,
        STATE_CONNECT,
        STATE_CONNECTED,
      } state_t;

      state_t state = STATE_IDLE;

      cb::JSON::ValuePtr data;

      cb::KeyPair accountKey;
      std::string sessionKey;
      cb::SmartPointer<cb::Cipher> cipher;

      cb::SmartPointer<cb::Event::Event> updateEvent;
      cb::Backoff updateBackoff = cb::Backoff(15, 4 * 60);

      std::set<std::string> ivs;

      typedef std::map<std::string, cb::SmartPointer<NodeRemote>> nodes_t;
      nodes_t nodes;

    public:
      Account(App &app);

      std::string getMachName() const;

      void init();
      void link(const std::string &token, const std::string &machName);
      void restart();

      void sendEncrypted(const cb::JSON::Value &msg, const std::string &sid);

    protected:
      void setState(state_t state);
      void setData(const cb::JSON::ValuePtr &data);
      void retry();
      void reset();

      void info();
      void connect();
      void link();
      void update();

      void onEncrypted   (const cb::JSON::ValuePtr &msg);
      void onBroadcast   (const cb::JSON::ValuePtr &msg);
      void onSessionClose(const cb::JSON::ValuePtr &msg);

      // From cb::WS::Websocket
      void onOpen() override;
      void onMessage(const cb::JSON::ValuePtr &msg) override;
      void onClose(cb::WS::Status status, const std::string &msg) override;

      // From cb::WS::JSONWebsocket
      void send(const cb::JSON::Value &msg) override;
    };
  }
}
