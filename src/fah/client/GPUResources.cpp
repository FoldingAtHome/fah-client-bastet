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

#include "GPUResources.h"

#include "App.h"
#include "Units.h"
#include "GPUResource.h"

#include <cbang/String.h>
#include <cbang/Catch.h>
#include <cbang/event/Event.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/json/Reader.h>
#include <cbang/time/Time.h>
#include <cbang/log/Logger.h>
#include <cbang/gpu/OpenCLLibrary.h>
#include <cbang/gpu/CUDALibrary.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  const unsigned updateFreq = Time::SEC_PER_DAY * 5;


  template <typename LIB>
  vector<ComputeDevice> get_gpus() {
    vector<ComputeDevice> devices;

    try {
      auto &lib = LIB::instance();

      for (auto &dev: lib) {
        LOG_DEBUG(3, dev);

        if (dev.isValid() && dev.gpu)
          devices.push_back(dev);
      }
    } catch (const Exception &e) {
      LOG_ERROR(LIB::getName() << " not supported: " << e.getMessage());
    }

    return devices;
  }
}


GPUResources::GPUResources(App &app) :
  Event::Scheduler<GPUResources>(app.getEventBase()), app(app),
  lastGPUsFail(0) {
  schedule(&GPUResources::update);
}


void GPUResources::load(const JSON::Value &gpus) {
  gpuIndex.read(gpus);
  TRY_CATCH_ERROR(detect());
  schedule(&GPUResources::update, updateFreq);
}


void GPUResources::response(Event::Request &req) {
  if (req.isOk())
    try {
      auto gpus = req.getInputJSON();
      load(*gpus);
      *SystemUtilities::oopen("gpus.json") << *gpus;
      lastGPUsFail = 0;
      return;
    } CATCH_ERROR;

  LOG_WARNING("Failed to update GPUs");

  // Exponential backoff, try at least once per day
  unsigned secs = lastGPUsFail ? 2 * (Time::now() - lastGPUsFail) : 5;
  lastGPUsFail = Time::now();
  if (Time::SEC_PER_DAY < secs) secs = Time::SEC_PER_DAY;
  schedule(&GPUResources::update, secs);
}


void GPUResources::update() {
  // Try to load cached gpus.json
  const string filename = "gpus.json";

  if (SystemUtilities::exists(filename)) {
    bool fail = true;

    try {
      load(*JSON::Reader::parse(filename));
      fail = false;
    } CATCH_ERROR;

    auto modTime = SystemUtilities::getModificationTime(filename);
    if (!fail && Time::now() < modTime + updateFreq) return;
  }

  // Download GPUs JSON
  URI uri = "https://api.foldingathome.org/gpus";
  app.getClient().call(uri, Event::RequestMethod::HTTP_GET,
                       this, &GPUResources::response)->send();
}


void GPUResources::detect() {
  map<string, SmartPointer<GPUResource>> resources;

  // Enumerate OpenCL
  auto openclGPUs = get_gpus<OpenCLLibrary>();
  for (auto &cd: openclGPUs) {
    if (!cd.isPCIValid()) continue;
    string id = "gpu:" + cd.getPCIID();
    SmartPointer<GPUResource> res = new GPUResource(id);
    res->set("opencl", cd);
    resources[id] = res;
  }

#ifndef __APPLE__
  // Enumerate CUDA and match with OpenCL
  auto cudaGPUs = get_gpus<CUDALibrary>();
  for (auto &cd: cudaGPUs) {
    if (!cd.isPCIValid()) continue;
    string id = "gpu:" + cd.getPCIID();
    auto it   = resources.find(id);

    if (it != resources.end()) {
      auto &res = it->second;
      res->set("cuda", cd);
      continue;
    }

    SmartPointer<GPUResource> res = new GPUResource(id);
    res->set("cuda", cd);
    resources[id] = res;
  }
#endif // __APPLE__

  // Enumerate PCI bus
  std::set<string> found;
  auto &info = PCIInfo::instance();
  for (auto &dev: info) {
    const auto &gpu = gpuIndex.find(dev.getVendorID(), dev.getDeviceID());
    string id = "gpu:" + dev.getID();
    auto it   = resources.find(id);

    if (it != resources.end()) {
      auto &res = it->second;
      res->setPCI(dev);
      res->insertBoolean("supported", gpu.getSpecies());
      continue;
    }

    if (!gpu.getType()) continue; // Ignore non-GPUs

    SmartPointer<GPUResource> res = new GPUResource(id);
    res->setPCI(dev);
    res->insertBoolean("supported", false);
    resources[id] = res;
  }

  // Match with existing GPUResources
  bool changed = false;
  for (auto &p: resources) {
    auto &id = p.first;

    if (!has(id) || get(id)->toString() != p.second->toString()) {
      insert(id, p.second);
      changed = true;
    }
  }

  loaded = true;
  if (changed) {
    LOG_INFO(3, "gpus = " << *this);
    app.updateResources();
  }
}
