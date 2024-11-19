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

#include "CoreState.h"

#include <cbang/json/Value.h>
#include <cbang/openssl/Certificate.h>
#include <cbang/event/Event.h>
#include <cbang/http/Client.h>

#include <functional>


namespace FAH {
  namespace Client {
    class App;

    class Core :
      public CoreState::Enum,
      public cb::HTTP::Enum,
      public cb::RefCounted {
      App &app;
      cb::JSON::ValuePtr data;
      CoreState state = CORE_INIT;

      std::string cert;
      std::string sig;

      cb::Event::EventPtr nextEvent;
      cb::Event::EventPtr readyEvent;
      cb::HTTP::Client::RequestPtr pr;

    public:
      typedef std::function<void (unsigned, int)> progress_cb_t;

    private:
      std::vector<progress_cb_t> progressCBs;

    public:
      Core(App &app, const cb::JSON::ValuePtr &data);
      ~Core();

      CoreState getState()  const {return state;}
      bool      isReady()   const {return state == CORE_READY;}
      bool      isInvalid() const {return state == CORE_INVALID;}

      std::string getURL()  const;
      uint32_t    getType() const;
      std::string getPath() const;

      void addProgressCallback(progress_cb_t cb);

      void next();

    protected:
      void ready();
      void load();
      void downloadResponse(const std::string &pkg);
      void download(const std::string &url);
      void response(cb::HTTP::Request &req);
    };
  }
}
