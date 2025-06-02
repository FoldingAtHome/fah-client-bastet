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
#include <cbang/util/Regex.h>

#include <vector>


namespace FAH {
  namespace Client {
    class App;
    class Remote;

    class Server : public cb::HTTP::Server {
      App &app;
      std::vector<cb::Regex> allowedOrigins;

    public:
      Server(App &app);

      void init();

      bool allowed(const std::string &origin) const;

      // From cb::HTTP::Server
      using cb::HTTP::Server::init;

    protected:
      bool corsCB(cb::HTTP::Request &req);
      bool redirectWebControl(cb::HTTP::Request &req);
      bool redirectPing(cb::HTTP::Request &req);
      bool handleWebsocket(cb::HTTP::Request &req);
    };
  }
}
