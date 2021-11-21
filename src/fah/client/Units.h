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

#include "Unit.h"

#include <set>
#include <string>


namespace FAH {
  namespace Client {
    class Units : public cb::JSON::ObservableList,
                  public cb::Event::Scheduler<Units> {
      typedef struct {
        std::set<unsigned> wus;     // Selected WU indices
        unsigned cpus;              // Unused CPUs
        std::set<std::string> gpus; // Unused GPUs
      } state_t;

      App &app;

      bool isConfigLoaded = false;
      uint64_t wus = 0;
      uint64_t lastWU = 0;
      uint32_t failures = 0;
      uint64_t waitUntil = 0;

    public:
      Units(App &app);

      void add(const cb::SmartPointer<Unit> &unit);
      bool isPaused() const;
      void setPause(bool pause);
      void unitComplete(bool success);
      void update();
      void triggerUpdate();
      void load();
      void save();

    private:
      uint64_t getProjectKey() const;
      static bool isBetter(const state_t &a, const state_t &b);
      state_t findBestFit(const state_t &current, unsigned i) const;
    };
  }
}
