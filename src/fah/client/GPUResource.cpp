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

#include "GPUResource.h"

#include "App.h"
#include "Config.h"

#include <cbang/String.h>
#include <cbang/json/JSON.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  string makeID(const cb::PCIDevice &pci) {
    return String::printf("gpu:%02d:%02d:%02d", pci.getBusID(), pci.getSlotID(),
                          pci.getFunctionID());
  }
}


GPUResource::GPUResource(App &app, const cb::GPU &gpu,
                         const cb::PCIDevice &pci) :
  id(makeID(pci)), gpu(gpu), pci(pci) {

  insert("type", String::toLower(gpu.getType().toString()));
  insert("description", gpu.getDescription());

  config = app.getConfig().getGPU(getID());
}


bool GPUResource::isEnabled() const {
  return config->getBoolean("enabled", false);
}


void GPUResource::set(const string &name, const ComputeDevice &cd) {
  if (name == "cuda") cuda = cd;
  if (name == "opencl") opencl = cd;
}


void GPUResource::writeRequest(JSON::Sink &sink) const {
  sink.beginDict();

  sink.insert("gpu",         getString("type"));
  sink.insert("vendor",      pci.getVendorID());
  sink.insert("device",      pci.getDeviceID());

  if (cuda.isValid()) {
    sink.insertDict("cuda");
    sink.insert("compute",   cuda.computeVersion.toString());
    sink.insert("driver",    cuda.driverVersion.toString());
    sink.endDict();
  }

  if (opencl.isValid()) {
    sink.insertDict("opencl");
    sink.insert("compute",   opencl.computeVersion.toString());
    sink.insert("driver",    opencl.driverVersion.toString());
    sink.endDict();
  }

  sink.endDict();
}
