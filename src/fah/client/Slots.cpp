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

#include "Slots.h"

#include "App.h"
#include "Slot.h"

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


  template <typename LIB>
  void match(Slots &slots, const std::string &name) {
    set<string> matched;
    auto &lib = LIB::instance();

    for (auto it = lib.begin(); it != lib.end(); it++)
      for (unsigned i = 0; i < slots.size(); i++) {
        auto &slot = *slots.get(i).cast<Slot>();
        if (!slot.isGPU()) continue;

        if (slot.getPCIBus() != it->pciBus ||
            slot.getPCISlot() != it->pciSlot ||
            matched.find(slot.getID()) != matched.end()) continue;

        matched.insert(slot.getID());
        slot.set(name, *it);
      }
  }
}


Slots::Slots(App &app) :
  Event::Scheduler<Slots>(app.getEventBase()), app(app),
  lastGPUsFail(0) {
  schedule(&Slots::gpusGet);
}


void Slots::add(const SmartPointer<Slot> &slot) {
  slot->load();
  insert(slot->getID(), slot);
}


void Slots::load() {
  app.getDB("slots").foreach(
    [this] (const string &id, const string &data) {
      LOG_INFO(3, "Loading compute slot " << id);
       add(new Slot(app, id, JSON::Reader::parseString(data)));
    });
}


void Slots::save() {
  for (unsigned i = 0; i < size(); i++)
    get(i).cast<Slot>()->save();
}


void Slots::update() {
  for (unsigned i = 0; i < size(); i++)
    get(i).cast<Slot>()->next();
}


void Slots::gpusLoad(const JSON::Value &gpus) {
  gpuIndex.read(gpus);
  load();
  detect();
  update();
}


void Slots::gpusResponse(Event::Request &req) {
  if (req.isOk())
    try {
      auto gpus = req.getInputJSON();
      gpusLoad(*gpus);
      *SystemUtilities::oopen("gpus.json") << *gpus;
      lastGPUsFail = 0;
      return;
    } CATCH_ERROR;

  LOG_WARNING("Failed to update GPUs");

  // Exponential backoff, try at least once per day
  unsigned secs = lastGPUsFail ? 2 * (Time::now() - lastGPUsFail) : 5;
  lastGPUsFail = Time::now();
  schedule(&Slots::gpusGet, min(Time::SEC_PER_DAY, secs));
}


void Slots::gpusGet() {
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
                       this, &Slots::gpusResponse)->send();
}


void Slots::detect() {
  // Enumerate PCI bus
  std::set<string> found;
  auto &info = PCIInfo::instance();
  for (auto it = info.begin(); it != info.end(); it++) {
    const GPU &gpu = gpuIndex.find(it->getVendorID(), it->getDeviceID());
    if (!gpu.getType()) continue;

    SmartPointer<Slot> slot = new Slot(app, gpu, *it);
    found.insert(slot->getID());
    if (has(slot->getID())) continue;
    LOG_INFO(3, "Adding new compute slot " << slot->getID());
    add(slot);
  }

  // Delete GPUs that are gone
  for (unsigned i = 0; i < size(); i++) {
    auto &slot = *get(i).cast<Slot>();
    if (slot.isGPU() && found.find(slot.getID()) == found.end()) {
      LOG_INFO(3, "Deleting missing compute slot " << slot.getID());
      slot.setState(SlotState::SLOT_DELETE);
    }
  }

  // Match with detected compute devices
  match<CUDALibrary>(*this, "cuda");
  match<OpenCLLibrary>(*this, "opencl");

  // Get CPUs available
  auto &config = app.getDB("config");
  int cpus = SystemInfo::instance().getCPUCount();
  int maxCPUs = config.getInteger("max-cpus", 0);
  if (maxCPUs && maxCPUs < cpus) cpus = maxCPUs;

  // Allocate CPU slots
  bool haveCPU = false;
  for (unsigned i = 0; !haveCPU && i < size(); i++) {
    auto &slot = *get(i).cast<Slot>();
    cpus -= slot.getCPUs();
    if (!slot.isGPU()) haveCPU = true;
  }

  if (!haveCPU) {
    unsigned index = 0;
    const int maxCPUsPerSlot = 32;

    while (0 < cpus) {
      SmartPointer<Slot> slot =
        new Slot(app, index++, min(maxCPUsPerSlot, cpus));
      slot->setState(SlotState::SLOT_IDLE);
      cpus -= maxCPUsPerSlot;
      LOG_INFO(3, "Adding new compute slot " << slot->getID());
      add(slot);
    }
  }

  LOG_INFO(3, "slots = " << *this);

  // Schedule next update
  schedule(&Slots::gpusGet, gpusUpdateFreq);
}
