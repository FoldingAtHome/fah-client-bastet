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

#include "ComputeResources.h"

#include "App.h"
#include "ComputeResource.h"

#include <cbang/String.h>
#include <cbang/Catch.h>
#include <cbang/event/Event.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/json/Reader.h>
#include <cbang/time/Time.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  const unsigned gpusUpdateFreq = Time::SEC_PER_DAY * 5;


  template <typename LIB, typename Resources>
  void match(Resources &resources, const std::string &name) {
    set<string> matched;
    auto &lib = LIB::instance();

    for (auto it = lib.begin(); it != lib.end(); it++)
      for (auto it2 = resources.begin(); it2 != resources.end(); it2++) {
        auto &resource = *it2->second;
        if (!resource.isGPU()) continue;

        if (resource.getBusID() != it->pciBus ||
            resource.getSlotID() != it->pciSlot ||
            matched.find(resource.getID()) != matched.end()) continue;

        matched.insert(resource.getID());
        resource.set(name, *it);
      }
  }
}


ComputeResources::ComputeResources(App &app) :
  Event::Scheduler<ComputeResources>(app.getEventBase()), app(app),
  lastGPUsFail(0) {
  schedule(&ComputeResources::gpusGet);
}


void ComputeResources::add(const SmartPointer<ComputeResource> &resource) {
  resources[resource->getID()] = resource;
}


bool ComputeResources::has(const string &id) const {
  return resources.find(id) != resources.end();
}


ComputeResource &ComputeResources::get(const string &id) {
  auto it = resources.find(id);
  if (it == resources.end()) THROWS("Compute resource " << id << " not found");
  return *it->second;
}


void ComputeResources::load() {
  auto &db = app.getDB("resources");

  db.foreach(
    [this] (const string &id, const string &data) {
      LOG_INFO(3, "Loading compute resource " << id);
      add(new ComputeResource(app, id, JSON::Reader::parseString(data)));
    });
}


void ComputeResources::save() {
  for (auto it = resources.begin(); it != resources.end(); it++)
    it->second->save();
}


void ComputeResources::start() {
  for (auto it = resources.begin(); it != resources.end(); it++)
    it->second->start();
}


void ComputeResources::write(JSON::Sink &sink) const {
  sink.beginDict();

  for (auto it = resources.begin(); it != resources.end(); it++) {
    sink.beginInsert(it->first);
    it->second->write(sink);
  }

  sink.endDict();
}


void ComputeResources::gpusLoad(const JSON::Value &gpus) {
  gpuIndex.read(gpus);
  load();
  detect();
  start();
}


void ComputeResources::gpusResponse(cb::Event::Request *req, int err) {
  if (req && !err)
    try {
      auto gpus = req->getInputJSON();
      gpusLoad(*gpus);
      *SystemUtilities::oopen("gpus.json") << *gpus;
      lastGPUsFail = 0;
      return;
    } CATCH_ERROR;

  LOG_WARNING("Failed to update GPUs");

  // Exponential backoff, try at least once per day
  unsigned secs = lastGPUsFail ? 2 * (Time::now() - lastGPUsFail) : 5;
  lastGPUsFail = Time::now();
  schedule(&ComputeResources::gpusGet, min(Time::SEC_PER_DAY, secs));
}


void ComputeResources::gpusGet() {
  // Try to load cached gpus.json
  const string filename = "gpus.json";

  if (SystemUtilities::exists(filename) &&
      Time::now() < SystemUtilities::getModificationTime(filename) +
      gpusUpdateFreq)
    try {
      gpusLoad(*JSON::Reader::parse(filename));
      return;
    } CATCH_ERROR;

  // Download GPUs JSON
  URI uri = "https://api.foldingathome.org/gpus";
  app.getClient().call(uri, Event::RequestMethod::HTTP_GET,
                       this, &ComputeResources::gpusResponse)->send();
}


void ComputeResources::detect() {
  // Enumerate PCI bus
  set<string> found;
  auto &info = PCIInfo::instance();
  for (auto it = info.begin(); it != info.end(); it++) {
    const GPU &gpu = gpuIndex.find(it->getVendorID(), it->getDeviceID());
    if (!gpu.getType()) continue;

    SmartPointer<ComputeResource> resource = new ComputeResource(app, gpu, *it);
    found.insert(resource->getID());
    if (has(resource->getID())) continue;
    LOG_INFO(3, "Adding new compute resource " << resource->getID());
    add(resource);
  }

  // Delete GPUs that are gone
  for (auto it = resources.begin(); it != resources.end(); it++) {
    auto &resource = *it->second;
    if (resource.isGPU() && found.find(resource.getID()) == found.end()) {
      LOG_INFO(3, "Deleting missing compute resource " << resource.getID());
      resource.setState(ComputeState::COMPUTE_DELETE);
    }
  }

  // Match with detected compute devices
  match<CUDALibrary>(resources, "cuda");
  match<OpenCLLibrary>(resources, "opencl");

  // Get CPUs available
  auto &config = app.getDB("config");
  int cpus = SystemInfo::instance().getCPUCount();
  int maxCPUs = config.getInteger("max-cpus", 0);
  if (maxCPUs && maxCPUs < cpus) cpus = maxCPUs;

  // Allocate CPU resources
  bool haveCPU = false;
  for (auto it = resources.begin(); !haveCPU && it != resources.end(); it++) {
    auto &resource = *it->second;
    cpus -= resource.getCPUs();
    if (!resource.isGPU()) haveCPU = true;
  }

  if (!haveCPU) {
    unsigned index = 0;
    const int maxCPUsPerSlot = 32;

    while (0 < cpus) {
      SmartPointer<ComputeResource> resource =
        new ComputeResource(app, index++, min(maxCPUsPerSlot, cpus));
      resource->setState(ComputeState::COMPUTE_IDLE);
      cpus -= maxCPUsPerSlot;
      LOG_INFO(3, "Adding new compute resource " << resource->getID());
      add(resource);
    }
  }

  LOG_INFO(3, "resources = " << *this);

  // Schedule next update
  schedule(&ComputeResources::gpusGet, gpusUpdateFreq);
}
