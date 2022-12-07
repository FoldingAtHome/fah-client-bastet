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

#include <cbang/json/Observable.h>

#include <cbang/gpu/CUDALibrary.h>
#include <cbang/gpu/OpenCLLibrary.h>
#include <cbang/gpu/GPU.h>

#include <cbang/pci/PCIInfo.h>


namespace FAH {
  namespace Client {
    class GPUResource : public cb::JSON::ObservableDict {
      std::string id;

      cb::GPU gpu;
      cb::PCIDevice pci;
      cb::ComputeDevice cuda;
      cb::ComputeDevice opencl;

    public:
      GPUResource(const cb::GPU &gpu, const cb::PCIDevice &pci);

      const std::string &getID() const {return id;}

      const cb::GPU &getGPU() const {return gpu;}
      const cb::PCIDevice &getPCI() const {return pci;}
      const cb::ComputeDevice &getCUDA() const {return cuda;}
      const cb::ComputeDevice &getOpenCL() const {return opencl;}

      using cb::JSON::ObservableDict::set;
      void set(const std::string &name, const cb::ComputeDevice &cd);

      void writeRequest(cb::JSON::Sink &sink) const;
    };
  }
}
