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

#pragma once

#include "CoreState.h"

#include <cbang/json/Value.h>
#include <cbang/openssl/Certificate.h>

#include <cbang/event/Request.h>
#include <cbang/event/Enum.h>
#include <cbang/event/Scheduler.h>

#include <functional>


namespace FAH {
  namespace Client {
    class App;

    class Core :
      public cb::Event::Scheduler<Core>, public CoreState::Enum,
      public cb::Event::Enum {
      App &app;
      cb::JSON::ValuePtr data;
      CoreState state = CORE_INIT;

      std::string cert;
      std::string sig;

      std::vector<std::function<void ()> > notifyCBs;

    public:
      Core(App &app, const cb::JSON::ValuePtr &data);

      std::string getURL() const;
      uint8_t getType() const;
      std::string getPath() const;
      std::string getFilename() const;

      void notify(std::function<void ()> cb);

      void next();

    protected:
      void ready();
      void load();
      void downloadResponse(const std::string &pkg);
      void download(const std::string &url);
      void response(cb::Event::Request &req);
    };
  }
}
