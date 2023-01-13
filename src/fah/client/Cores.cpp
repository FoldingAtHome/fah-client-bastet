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

#include "Cores.h"

#include "App.h"

#include <cbang/log/Logger.h>
#include <cbang/event/Event.h>
#include <cbang/os/SystemUtilities.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


// TODO delete cores that havent been used in awhile
Cores::Cores(App &app) : app(app) {}


const SmartPointer<Core> &Cores::get(const JSON::ValuePtr &data) {
  string url = data->getString("url");

  auto it = cores.find(url);
  if (it == cores.end()) {
    SmartPointer<Core> core = new Core(app, data->copy());
    it = cores.insert(cores_t::value_type(url, core)).first;
  }

  return it->second;
}
