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

#include "Units.h"

#include <functional>


namespace FAH {
  namespace Client {
    class App;
    class Config;

    class Group : public cb::JSON::ObservableDict, public UnitState::Enum {
      App &app;
      const std::string name;
      cb::SmartPointer<Config> config;

      cb::Event::EventPtr event;
      uint32_t failures   = 0;
      uint64_t waitUntil  = 0;

      std::function<void ()> shutdownCB;


      struct UnitIterator : public cb::JSON::Iterator {
        const std::string &group;

        UnitIterator(const Value &value, const std::string &group, int i = -1) :
          cb::JSON::Iterator(value, i), group(group) {
          if (!match()) ++(*this); // Find first
        }


        bool match() const {
          return valid() && (**this)->getGroup().getName() == group;
        }


        UnitIterator &operator++() {
          while (valid()) {
            i++;
            if (match()) return *this;
          }

          i = -1;
          return *this;
        }


        cb::SmartPointer<Unit> operator*() const {
          if (!valid()) CBANG_THROW("Invalid WU iterator");
          return value.get(i).cast<Unit>();
        }
      };


      struct Units {
        const Value &value;
        const std::string &group;
        Units(const Value &value, const std::string &group) :
          value(value), group(group) {}
        UnitIterator begin() const {return UnitIterator(value, group, 0);}
        UnitIterator end() const {return UnitIterator(value, group);}
      };

   public:
      Group(App &app, const std::string &name);

      const std::string &getName() const {return name;}

      Config &getConfig() const {return *config;}

      Units units() const;

      void setState(const cb::JSON::Value &msg);

      bool waitForIdle() const;
      bool waitOnBattery() const;
      bool keepAwake() const;
      bool isActive() const;
      bool hasUnrunWUs() const;
      void triggerUpdate();
      void shutdown(std::function<void ()> cb);
      void unitComplete(bool success);

      void save();
      void remove();

      // From cb::JSON::Value
      void notify(const std::list<cb::JSON::ValuePtr> &change);

    protected:
      void update();
      void setWait(double delay);
    };
  }
}
