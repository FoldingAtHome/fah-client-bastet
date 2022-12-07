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

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  const unsigned updateFreq = Time::SEC_PER_DAY * 5;


  template <typename LIB>
  void match(GPUResources &gpus, const string &name) try {
    set<string> matched;
    auto &lib = LIB::instance();

    for (auto &dev: lib)
      for (unsigned i = 0; i < gpus.size(); i++) {
        auto &gpu = *gpus.get(i).cast<GPUResource>();

        if (gpu.getPCI().getBusID()   != dev.pciBus  ||
            gpu.getPCI().getSlotID()  != dev.pciSlot ||
            matched.find(gpu.getID()) != matched.end()) continue;

        matched.insert(gpu.getID());
        gpu.set(name, dev);
      }
  } CATCH_ERROR;
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

  if (SystemUtilities::exists(filename) &&
      Time::now() < SystemUtilities::getModificationTime(filename) + updateFreq)
    try {
      load(*JSON::Reader::parse(filename));
      return;
    } CATCH_ERROR;

  // Download GPUs JSON
  URI uri = "https://api.foldingathome.org/gpus";
  app.getClient().call(uri, Event::RequestMethod::HTTP_GET,
                       this, &GPUResources::response)->send();
}


void GPUResources::detect() {
  bool changed = false;

  // Enumerate PCI bus
  std::set<string> found;
  auto &info = PCIInfo::instance();

  for (auto &dev: info) {
    const GPU &gpu = gpuIndex.find(dev.getVendorID(), dev.getDeviceID());
    if (!gpu.getType()) continue;

    SmartPointer<GPUResource> res = new GPUResource(gpu, dev);
    string id = res->getID();
    found.insert(id);
    if (has(id)) continue;

    LOG_INFO(3, "Adding GPU " << id);
    insert(id, res);
    changed = true;
  }

  // Delete GPUs that are gone
  for (unsigned i = 0; i < size();) {
    string id = keyAt(i);

    if (found.find(id) == found.end()) {
      LOG_INFO(3, "Deleting GPU " << id);
      erase(id);
      changed = true;

    } else i++;
  }

  // Match with detected devices
#ifndef __APPLE__
  match<CUDALibrary>(*this, "cuda");
#endif
  match<OpenCLLibrary>(*this, "opencl");

  if (changed) {
    LOG_INFO(3, "gpus = " << *this);
    app.updateResources();
  }
}
