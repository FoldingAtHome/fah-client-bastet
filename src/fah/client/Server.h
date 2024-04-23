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

#include <cbang/http/Server.h>
#include <cbang/json/Value.h>
#include <cbang/openssl/SSLContext.h>

#include <list>
#include <set>


namespace FAH {
  namespace Client {
    class App;
    class Remote;

    class Server : public cb::HTTP::Server {
      App &app;

      std::set<std::string> allowedOrigins;

    public:
      Server(App &app);

      void init();

      // From cb::HTTP::Server
      using cb::HTTP::Server::init;
      cb::SmartPointer<cb::HTTP::Request> createRequest(
        const cb::SmartPointer<cb::HTTP::Conn> &connection,
        cb::HTTP::Method method, const cb::URI &uri,
        const cb::Version &version) override;

    protected:
      bool corsCB(cb::HTTP::Request &req);
      bool redirectWebControl(cb::HTTP::Request &req);
      bool redirectPing(cb::HTTP::Request &req);
    };
  }
}
