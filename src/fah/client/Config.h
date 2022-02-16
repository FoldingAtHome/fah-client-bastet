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

#pragma once

#include <cbang/json/Observable.h>
#include <cbang/enum/ProcessPriority.h>


namespace FAH {
  namespace Client {
    class App;

    class Config : public cb::JSON::ObservableDict {
      App &app;

    public:
      Config(App &app);

      void init();

      bool getOnIdle() const;
      void setOnIdle(bool onIdle);

      void setPaused(bool paused);
      bool getPaused() const;

      void setFinish(bool finish);
      bool getFinish() const;

      const std::string &getUsername() {return getString("user");}
      const std::string &getPasskey() {return getString("passkey");}
      uint32_t getTeam() {return getU32("team");}

      uint32_t getCPUs() const;
      uint64_t getProjectKey() const;
      cb::ProcessPriority getCorePriority();
      cb::JSON::ValuePtr getGPU(const std::string &id);
    };
  }
}
