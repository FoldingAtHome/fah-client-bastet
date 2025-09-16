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

#include "GPUResource.h"

#include <cbang/hw/GPUIndex.h>
#include <cbang/event/Event.h>
#include <cbang/http/Client.h>


namespace FAH {
  namespace Client {
    class App;
    class Unit;

    class GPUResources : public cb::JSON::ObservableDict {
      App &app;

      bool loaded   = false;
      bool detected = false;
      cb::GPUIndex gpuIndex;
      int64_t lastGPUsFail = 0;

      cb::Event::EventPtr event;
      cb::HTTP::Client::RequestPtr pr;

    public:
      GPUResources(App &app);
      ~GPUResources();

      bool isReady() const {return loaded && detected;}
      void signalGPUReady();

    protected:
      void load(const cb::JSON::Value &gpus);
      void response(cb::HTTP::Request &req);
      void update();
      void detect();
    };
  }
}
