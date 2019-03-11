/******************************************************************************\

                  This file is part of the Folding@home Client.

           The fahclient runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2019, foldingathome.org
                               All rights reserved.

       This program is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
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

#include "ComputeState.h"

#include <cbang/json/Serializable.h>
#include <cbang/json/Value.h>

#include <cbang/gpu/CUDALibrary.h>
#include <cbang/gpu/OpenCLLibrary.h>
#include <cbang/gpu/GPU.h>

#include <cbang/event/Request.h>
#include <cbang/event/Scheduler.h>

#include <cbang/pci/PCIInfo.h>


namespace FAH {
  namespace Client {
    class App;
    class Unit;

    class ComputeResource :
      public cb::JSON::Serializable,
      public cb::Event::Scheduler<ComputeResource>,
      public ComputeState::Enum {
      App &app;
      std::string id;
      cb::JSON::ValuePtr data;
      ComputeState state;

      unsigned currentAS = 0;
      std::vector<cb::SmartPointer<Unit> > units;

      ComputeResource(App &app, const std::string &id,
                      const cb::JSON::ValuePtr &data,
                      ComputeState state);

    public:
      ComputeResource(App &app, unsigned index, unsigned cpus);
      ComputeResource(App &app, const cb::GPU &gpu, const cb::PCIDevice &pci);
      ComputeResource(App &app, const std::string &id,
                      const cb::JSON::ValuePtr &data);

      bool isEnabled() const {return state != COMPUTE_DISABLE;}
      ComputeState getState() const {return state;}
      void setState(ComputeState state);

      std::string getID() const {return id;}
      bool isGPU() const {return id[0] == 'g';}

      const cb::JSON::Value &get(const char *name) const
        {return *data->get(name);}

      uint32_t getCPUs() const {return data->getU32("cpus", 1);}

      uint16_t getVendorID() const {return get("pci").getU16("vendor");}
      uint16_t getDeviceID() const {return get("pci").getU16("device");}
      uint16_t getBusID() const {return get("pci").getU16("bus");}
      uint16_t getSlotID() const {return get("pci").getU16("slot");}

      const std::string &getGPUType() const
        {return get("gpu").getString("type");}
      uint16_t getGPUSpecies() const {return get("gpu").getU16("species");}

      const std::string &getDriver(const char *name) const
        {return get(name).getString("driver");}
      const std::string &getCompute(const char *name) const
        {return get(name).getString("compute");}

      void set(const cb::PCIDevice &pci);
      void set(const cb::GPU &gpu);
      void set(const std::string &name, const cb::ComputeDevice &cd);

      void writeAssign(cb::JSON::Sink &sink);

      void start();
      void save();

      // cb::JSON::Serializable
      void write(cb::JSON::Sink &sink) const {data->write(sink);}

    protected:
      void assignNextAS();
      void assignResponse(cb::Event::Request *req, int err);
      void assign();
    };
  }
}
