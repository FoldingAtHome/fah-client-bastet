/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2026, foldingathome.org
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
#include "OS.h"

#include <cbang/String.h>
#include <cbang/Catch.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/json/Reader.h>
#include <cbang/time/Time.h>
#include <cbang/log/Logger.h>
#include <cbang/hw/OpenCLLibrary.h>
#include <cbang/hw/CUDALibrary.h>
#include <cbang/hw/HIPLibrary.h>
#include <cbang/util/WeakCallback.h>

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
      LOG_DEBUG(3, LIB::getName() << " not supported: " << e.getMessage());
    }

    return devices;
  }
}


GPUResources::GPUResources(App &app) :
  app(app),
  updateEvent(app.getEventBase().newEvent([this] {update();}, 0)),
  detectEvent(app.getEventBase().newEvent([this] {detect();}, 0)) {
  updateEvent->activate();
}


GPUResources::~GPUResources() {}


bool GPUResources::waitOnGPU(const string &id) const {
  return !detected ||
    (unconfiguredGPUs.count(id) && app.getUptime() < maxWaitTime);
}


void GPUResources::load(const JSON::Value &gpus) {
  gpuIndex.read(gpus);
  loaded = true;
  updateEvent->add(updateFreq);
  detect();
}


void GPUResources::response(HTTP::Request &req) {
  pr.release();

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
  updateEvent->add(secs);
}


void GPUResources::update() {
  // Try to load cached gpus.json
  auto filename = "gpus.json"s;

  if (SystemUtilities::exists(filename)) {
    bool fail = true;

    try {
      LOG_INFO(2, "Loading " << filename);
      load(*JSON::Reader::parseFile(filename));
      fail = false;
    } CATCH_ERROR;

    auto modTime = SystemUtilities::getModificationTime(filename);
    if (!fail && Time::now() < modTime + updateFreq) return;

  } else LOG_INFO(2, "No previous " << filename);

  // Download GPUs JSON
  URI uri = "https://api.foldingathome.org/gpus";
  auto cb = [this] (HTTP::Request &req) {response(req);};
  pr = app.getClient().call(uri, HTTP::Method::HTTP_GET, WeakCall(this, cb));
  pr->send();
}


void GPUResources::detect() {
  map<string, SmartPointer<GPUResource>> resources;

  // Enumerate OpenCL
  auto openclGPUs = get_gpus<OpenCLLibrary>();
  for (auto &cd: openclGPUs) {
    if (!cd.isPCIValid()) continue;
    string id = "gpu:" + cd.getPCIID();

    auto res = resources[id] = new GPUResource(id);
    res->set("opencl", cd);
  }

#ifndef __APPLE__
  // Enumerate CUDA and match with OpenCL
  auto cudaGPUs = get_gpus<CUDALibrary>();
  for (auto &cd: cudaGPUs) {
    if (!cd.isPCIValid()) continue;
    string id = "gpu:" + cd.getPCIID();

    SmartPointer<GPUResource> res;
    auto it = resources.find(id);
    if (it != resources.end()) res = it->second;
    else resources[id] = res = new GPUResource(id);

    res->set("cuda", cd);
  }
#endif // __APPLE__

  // Enumerate HIP and match with OpenCL/CUDA
  auto hipGPUs = get_gpus<HIPLibrary>();
  for (auto &cd: hipGPUs) {
    if (!cd.isPCIValid()) continue;
    string id = "gpu:" + cd.getPCIID();

    SmartPointer<GPUResource> res;
    auto it = resources.find(id);
    if (it != resources.end()) res = it->second;
    else resources[id] = res = new GPUResource(id);

    res->set("hip", cd);
  }

  // Enumerate PCI bus
  std::set<string> valid;
  PCIInfo info; // Don't use singleton

  for (auto &dev: info) {
    const auto &gpu = gpuIndex.find(dev.getVendorID(), dev.getDeviceID());
    string id = "gpu:" + dev.getID();

    SmartPointer<GPUResource> res;
    auto it = resources.find(id);
    if (it != resources.end()) res = it->second;
    else if (!gpu.getType()) continue; // Ignore non-GPUs
    else resources[id] = res = new GPUResource(id);

    if (gpu.getSpecies()) valid.insert(id);

    // NOTE, It's possible the device was found by OpenCL/CUDA/HIP but the
    // driver did not report PCI info.

    res->setPCI(dev);

    // A GPU device is supported if GPUs.txt lists a non-zero species and
    // at least one ComputeDevice is found for OpenCL/CUDA/HIP.
    res->insertBoolean("supported", gpu.getSpecies() && it != resources.end());

    if (!res->hasString("description") && !gpu.getDescription().empty())
      res->insert("description", gpu.getDescription());
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

  // Track unconfigured GPUs
  unconfiguredGPUs.clear();
  for (auto &id: valid)
    if (!has(id)) unconfiguredGPUs.insert(id);

  if (unconfiguredGPUs.size() && app.getUptime() < maxWaitTime)
    detectEvent->add(5); // Try again later

  detected = true;
  if (changed) {
    LOG_INFO(3, "gpus = " << *this);
    app.triggerUpdate();
  }
}
