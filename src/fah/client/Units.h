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

#include "Unit.h"


namespace FAH {
  namespace Client {
    class Units : public cb::JSON::ObservableList {
      App &app;

   public:
      Units(App &app);

      bool isActive()    const;
      bool hasFailure()  const;

      void add(const cb::SmartPointer<Unit> &unit);
      int getUnitIndex(const std::string &id) const;
      cb::SmartPointer<Unit> findUnit(const std::string &id);
      cb::SmartPointer<Unit> getUnit(unsigned index) const;
      cb::SmartPointer<Unit> getUnit(const std::string &id) const;
      void removeUnit(const std::string &id);
    };
  }
}
