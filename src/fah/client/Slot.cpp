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

#include "Slot.h"

#include "App.h"
#include "Unit.h"

#include <cbang/String.h>
#include <cbang/json/JSON.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>
#include <cbang/net/Base64.h>
#include <cbang/util/Random.h>

#include <cbang/openssl/Certificate.h>
#include <cbang/openssl/KeyPair.h>
#include <cbang/openssl/KeyContext.h>
#include <cbang/openssl/Digest.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


Slot::Slot(App &app, const string &id, const JSON::ValuePtr &data,
           SlotState state) :
  Event::Scheduler<Slot>(app.getEventBase()), app(app), id(id),
  state(state), nonce(Random::instance().rand<uint16_t>()) {merge(*data);}


Slot::Slot(App &app, unsigned index, unsigned cpus) :
  Slot(app, String::printf("cpu:%d", index), new JSON::Dict, SLOT_DISABLE) {
  insert("cpus", cpus);
}


Slot::Slot(App &app, const cb::GPU &gpu, const cb::PCIDevice &pci) :
  Slot(app, String::printf("gpu:%02d:%02d:%02d", pci.getBusID(),
                           pci.getSlotID(), pci.getFunctionID()),
       new JSON::Dict, SLOT_DISABLE) {
  set(gpu);
  set(pci);
}


Slot::Slot(App &app, const std::string &id, const cb::JSON::ValuePtr &data) :
  Slot(app, id, data,
       SlotState::parse(data->getString("state", "disable"))) {}


void Slot::setState(SlotState state) {
  insert("state", String::toLower(state.toString()));
  this->state = state;
}


double Slot::getCheckpoint() const {return getNumber("checkpoint", 15);}


unsigned Slot::getCPUUsage() const {
  return (unsigned)getNumber("cpu-usage", 100);
}


ProcessPriority Slot::getCorePriority() const {
  return ProcessPriority::parse(getString("priority", "idle"));
}


void Slot::set(const PCIDevice &pci) {
  insert("pci", JSON::build([pci] (JSON::Sink &sink) {pci.write(sink);}));
}


void Slot::set(const GPU &gpu) {
  JSON::ValuePtr d = new JSON::Dict;
  d->insert("type",    String::toLower(gpu.getType().toString()));
  d->insert("species", gpu.getSpecies());
  insert("gpu", d);
}


void Slot::set(const string &name, const ComputeDevice &cd) {
  JSON::ValuePtr d = new JSON::Dict;
  d->insert("driver",   cd.driverVersion.toString());
  d->insert("compute",  cd.computeVersion.toString());
  d->insert("platform", cd.platformIndex);
  d->insert("device",   cd.deviceIndex);
  insert(name, d);
}


void Slot::add(const SmartPointer<Unit> &unit) {
  units.push_back(unit);
  unit->schedule(&Unit::next);
}


void Slot::writeRequest(JSON::Sink &sink) {
  auto &info = SystemInfo::instance();
  auto &config = app.getDB("config");

  sink.beginDict();

  // Protect against replay
  sink.insert("time", Time().toString()); // TODO use measured AS time offset
  sink.insert("nonce", nonce++);

  // Client
  sink.insert("version",        app.getVersion().toString());
  if (hasString("client-type"))
    sink.insert("type",         getString("client-type"));

  // User
  if (config.has("id"))
    sink.insert("id",           config.getString("id"));
  if (config.has("user"))
    sink.insert("user",         config.getString("user"));
  if (config.has("team"))
    sink.insert("team",         config.getInteger("team"));
  if (hasString("cause"))
    sink.insert("cause",        getString("cause"));
  if (has("project-key"))
    sink.insert("project-key",  *get("project-key"));

  // OS
  sink.insertDict("os");
  sink.insert("version",        info.getOSVersion().toString());
  sink.insert("type",           app.getOS());
  sink.insert("memory",         info.getFreeMemory());
  sink.endDict();

  // CPU
  sink.insertDict("cpu");
  sink.insert("type",           app.getCPU());
  sink.insert("cpus",           getU32("cpus", 1));
  sink.insert("extended",       info.getCPUExtendedFeatures());
  sink.insert("vendor",         info.getCPUVendor());
  sink.insert("features",       info.getCPUFeatures());
  sink.insert("signature",      info.getCPUSignature());
  sink.insert("80000001",       info.getCPUFeatures80000001());
  sink.endDict();

  // GPU
  if (isGPU()) {
    sink.insertDict("gpu");
    sink.insert("type",         getGPUType());
    sink.insert("vendor",       getPCIVendor());
    sink.insert("device",       getPCIDevice());
    sink.insertDict("cuda");
    sink.insert("compute",      getCompute("cuda"));
    sink.insert("driver",       getDriver("cuda"));
    sink.endDict();
    sink.insertDict("opencl");
    sink.insert("compute",      getCompute("opencl"));
    sink.insert("driver",       getDriver("opencl"));
    sink.endDict();
    sink.endDict();
  }

  sink.endDict();
}


void Slot::next() {
  // Remove completed units
  for (auto it = units.begin(); it != units.end();)
    if ((*it)->getState() == UnitState::UNIT_DONE) it = units.erase(it);
    else it++;

  switch (state) {
  case SLOT_DISABLE: return;
  case SLOT_DELETE:  return; // TODO

  case SLOT_IDLE:
  case SLOT_RUN:
    // Add new unit
    if (units.empty()) add(new Unit(app, *this));
    break;

  case SLOT_FINISH:   break;
  case SLOT_PAUSE:    break;
  }
}


void Slot::load() {
  app.getDB(id).foreach(
    [this] (const string &id, const string &data) {
      LOG_INFO(3, "Loading work unit " << id);
      add(new Unit(app, *this, JSON::Reader::parseString(data)));
    });
}


void Slot::save() {app.getDB("slots").set(getID(), toString());}
