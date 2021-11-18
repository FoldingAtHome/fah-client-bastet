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


namespace FAH {
  namespace Client {

    typedef std::set<unsigned> wu_set_t;
    typedef struct {
      bool feasible = true;
      uint32_t gpus = 0;
      int32_t cpus = 0;
      wu_set_t unitSet;
      unsigned largestCpuWu = 0;
    } state_t;

    class Units : public cb::JSON::ObservableList,
                  public cb::Event::Scheduler<Units> {
      App &app;

      bool isConfigLoaded = false;
      uint64_t wus = 0;
      uint64_t lastWU = 0;
      uint32_t failures = 0;
      uint64_t waitUntil = 0;

    public:
      Units(App &app);

      void add(const cb::SmartPointer<Unit> &unit);

      void setPause(bool pause, const std::string unitID = std::string());

      void unitComplete(bool success);
      void update();
      void load();
      void save();

    private:
      uint64_t getProjectKey() const;
      bool compare(state_t a, state_t b);
      state_t getState(const state_t& current, unsigned index, std::set<std::string> gpus);
      state_t findBestFit(const state_t& current, unsigned i, std::set<std::string> gpus);
    };
  }
}
