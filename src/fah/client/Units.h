/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2022, foldingathome.org
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

#include "Unit.h"

#include <string>
#include <functional>


namespace FAH {
  namespace Client {
    class ResourceGroup;
    class Config;

    class Units : public cb::JSON::ObservableList,
                  public UnitState::Enum {
      App &app;
      ResourceGroup &group;
      cb::SmartPointer<Config> config;

      cb::SmartPointer<cb::Event::Event> event;
      uint32_t failures = 0;
      uint64_t waitUntil = 0;

      std::function<void ()> shutdownCB;

    public:
      Units(App &app, ResourceGroup &group,
            const cb::SmartPointer<Config> &config);

      const ResourceGroup &getGroup()  const {return group;}
      const Config        &getConfig() const {return *config;}

      bool isActive() const;
      bool hasFailure() const;
      bool hasUnrunWUs() const;

      void add(const cb::SmartPointer<Unit> &unit);
      unsigned getUnitIndex(const std::string &id) const;
      Unit &getUnit(unsigned index) const;
      Unit &getUnit(const std::string &id) const;
      const cb::SmartPointer<Unit> removeUnit(unsigned index);
      void dump(const std::string &unitID);
      void unitComplete(bool success);
      void update();
      void triggerUpdate(bool updateUnits = false);
      void shutdown(std::function<void ()> cb);

    protected:
      void setWait(double delay);
    };
  }
}
