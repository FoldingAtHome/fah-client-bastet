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

#include "Config.h"
#include "Units.h"
#include "Remote.h"

#include <cbang/json/Observable.h>


namespace FAH {
  namespace Client {
    class App;

    class ResourceGroup : public cb::JSON::ObservableDict {
      App   &app;
      const std::string name;

      cb::SmartPointer<Config> config;
      cb::SmartPointer<Units>  units;
      std::list<cb::SmartPointer<Remote> > clients;

    public:
      ResourceGroup(App &app, const std::string &name,
                    const cb::JSON::ValuePtr &config,
                    const cb::JSON::ValuePtr &info);
      ~ResourceGroup();

      const std::string              &getName()   const {return name;}
      const cb::SmartPointer<Config> &getConfig() const {return config;}
      const cb::SmartPointer<Units>  &getUnits()  const {return units;}
      const cb::JSON::ValuePtr       &getInfo()   const {return get("info");}

      void add(const cb::SmartPointer<Remote> &client);
      void remove(Remote &client);

      void broadcast(const cb::JSON::ValuePtr &changes);

      // From cb::JSON::Value
      void notify(std::list<cb::JSON::ValuePtr> &change);
    };
  }
}
