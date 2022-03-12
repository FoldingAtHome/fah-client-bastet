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

#include "GPUResource.h"

#include <cbang/gpu/GPUIndex.h>
#include <cbang/event/Scheduler.h>
#include <cbang/event/Request.h>


namespace FAH {
  namespace Client {
    class App;
    class Unit;

    class GPUResources :
      public cb::JSON::ObservableDict,
      public cb::Event::Scheduler<GPUResources> {
      App &app;

      cb::GPUIndex gpuIndex;
      int64_t lastGPUsFail;

    public:
      GPUResources(App &app);

    protected:
      void load(const cb::JSON::Value &gpus);
      void response(cb::Event::Request &req);
      void update();
      void detect();
    };
  }
}
