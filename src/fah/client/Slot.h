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

#include "SlotState.h"

#include <cbang/json/Observable.h>

#include <cbang/gpu/CUDALibrary.h>
#include <cbang/gpu/OpenCLLibrary.h>
#include <cbang/gpu/GPU.h>

#include <cbang/event/Request.h>
#include <cbang/event/Scheduler.h>

#include <cbang/pci/PCIInfo.h>
#include <cbang/enum/ProcessPriority.h>


namespace FAH {
  namespace Client {
    class App;
    class Unit;
    class Core;

    class Slot :
      public cb::JSON::ObservableDict,
      public cb::Event::Scheduler<Slot>,
      public SlotState::Enum {
      App &app;
      std::string id;
      SlotState state;

      uint16_t nonce;
      std::vector<cb::SmartPointer<Unit> > units;

      Slot(App &app, const std::string &id, const cb::JSON::ValuePtr &data,
           SlotState state);

    public:
      Slot(App &app, unsigned index, unsigned cpus);
      Slot(App &app, const cb::GPU &gpu, const cb::PCIDevice &pci);
      Slot(App &app, const std::string &id, const cb::JSON::ValuePtr &data);

      bool isEnabled() const {return state != SLOT_DISABLE;}
      SlotState getState() const {return state;}
      void setState(SlotState state);
      double getCheckpoint() const;
      unsigned getCPUUsage() const;
      cb::ProcessPriority getCorePriority() const;

      std::string getID() const {return id;}
      bool isGPU() const {return id[0] == 'g';}

      uint32_t getCPUs() const {return getU32("cpus", 1);}

      uint16_t getPCIVendor() const {return get("pci")->getU16("vendor");}
      uint16_t getPCIDevice() const {return get("pci")->getU16("device");}
      uint16_t getPCIBus() const {return get("pci")->getU16("bus");}
      uint16_t getPCISlot() const {return get("pci")->getU16("slot");}

      const std::string &getGPUType() const
        {return get("gpu")->getString("type");}

      const std::string &getDriver(const std::string &name) const
        {return get(name)->getString("driver");}
      const std::string &getCompute(const std::string &name) const
        {return get(name)->getString("compute");}
      uint32_t getPlatformIndex(const std::string &name) const
        {return get(name)->getU32("platform");}
      uint32_t getDeviceIndex(const std::string &name) const
        {return get(name)->getU32("device");}

      void set(const cb::PCIDevice &pci);
      void set(const cb::GPU &gpu);
      void set(const std::string &name, const cb::ComputeDevice &cd);

      void add(const cb::SmartPointer<Unit> &unit);
      void writeRequest(cb::JSON::Sink &sink);

      void next();
      void load();
      void save();
    };
  }
}
