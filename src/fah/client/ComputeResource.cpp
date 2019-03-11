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

#include "ComputeResource.h"

#include "App.h"
#include "Unit.h"

#include <cbang/String.h>
#include <cbang/json/JSON.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>

#include <cbang/net/Base64.h>
#include <cbang/openssl/Certificate.h>
#include <cbang/openssl/KeyPair.h>
#include <cbang/openssl/KeyContext.h>
#include <cbang/openssl/Digest.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


ComputeResource::ComputeResource(App &app, const string &id,
                                 const JSON::ValuePtr &data,
                                 ComputeState state) :
  Event::Scheduler<ComputeResource>(app.getEventBase()), app(app), id(id),
  data(data), state(state) {}


ComputeResource::ComputeResource(App &app, unsigned index, unsigned cpus) :
  ComputeResource(app, String::printf("cpu:%d", index), new JSON::Dict,
                  COMPUTE_DISABLE) {
  data->insert("cpus", cpus);
}


ComputeResource::ComputeResource(App &app, const cb::GPU &gpu,
                                 const cb::PCIDevice &pci) :
  ComputeResource(app, String::printf("gpu:%02d:%02d:%02d", pci.getBusID(),
                                      pci.getSlotID(), pci.getFunctionID()),
                  new JSON::Dict, COMPUTE_DISABLE) {
  set(gpu);
  set(pci);
}


ComputeResource::ComputeResource(App &app, const std::string &id,
                                 const cb::JSON::ValuePtr &data) :
  ComputeResource(app, id, data,
                  ComputeState::parse(data->getString("state", "disable"))) {}


void ComputeResource::setState(ComputeState state) {
  data->insert("state", String::toLower(state.toString()));
  this->state = state;
}


void ComputeResource::set(const PCIDevice &pci) {
  JSON::Builder builder;
  pci.write(builder);
  data->insert("pci", builder.getRoot());
}


void ComputeResource::set(const GPU &gpu) {
  JSON::ValuePtr d = new JSON::Dict;
  d->insert("type",    String::toLower(gpu.getType().toString()));
  d->insert("species", gpu.getSpecies());
  data->insert("gpu", d);
}


void ComputeResource::set(const string &name, const ComputeDevice &cd) {
  JSON::ValuePtr d = new JSON::Dict;
  d->insert("driver",   cd.driverVersion.toString());
  d->insert("compute",  cd.computeVersion.toString());
  d->insert("platform", cd.platformIndex);
  d->insert("device",   cd.deviceIndex);
  data->insert(name, d);
}


void ComputeResource::writeAssign(JSON::Sink &sink) {
  auto &info = SystemInfo::instance();
  auto &config = app.getDB("config");

  sink.beginDict();

  // Client
  sink.insert("version",        app.getVersion().toString());
  sink.insert("client_id",      app.getID());
  if (data->hasString("client-type"))
    sink.insert("type",         data->getString("client-type"));

  // User
  if (config.has("user"))
    sink.insert("user",         config.getString("user"));
  if (config.has("team"))
    sink.insert("team",         config.getInteger("team"));
  if (config.has("passkey"))
    sink.insert("passkey",      config.getString("passkey"));
  if (data->hasString("cause"))
    sink.insert("cause",        data->getString("cause"));
  if (data->hasString("project-key"))
    sink.insert("project_key",  data->getString("project-key"));

  // OS
  sink.insert("os_version",     info.getOSVersion().toString());
  sink.insert("os",             app.getOS());
  sink.insert("memory",         info.getFreeMemory());

  // CPU
  sink.insert("cpu",            app.getCPU());
  sink.insert("cpus",           data->getU32("cpus", 1));
  sink.insert("cpu_extended",   info.getCPUExtendedFeatures());
  sink.insert("cpu_vendor",     info.getCPUVendor());
  sink.insert("cpu_features",   info.getCPUFeatures());
  sink.insert("cpu_signature",  info.getCPUSignature());
  sink.insert("cpu_80000001",   info.getCPUFeatures80000001());

  // GPU
  if (isGPU()) {
    sink.insert("gpu",            getGPUType());
    sink.insert("gpu_species",    getGPUSpecies());
    sink.insert("gpu_vendor_id",  getVendorID());
    sink.insert("gpu_device_id",  getDeviceID());
    sink.insert("cuda_compute",   getCompute("cuda"));
    sink.insert("cuda_driver",    getDriver("cuda"));
    sink.insert("opencl_compute", getCompute("opencl"));
    sink.insert("opencl_driver",  getDriver("opencl"));
  }

  sink.endDict();
}


void ComputeResource::start() {
  switch (state) {
  case COMPUTE_DISABLE: return;
  case COMPUTE_DELETE:  return; // TODO

  case COMPUTE_IDLE:
  case COMPUTE_ASSIGN:
    assign();
    break;

  case COMPUTE_RUN:      break;
  case COMPUTE_FINISH:   break;
  case COMPUTE_PAUSE:    break;
  }
}


void ComputeResource::save() {app.getDB("resources").set(getID(), toString());}


void ComputeResource::assignNextAS() {
  if (app.getServers().size() <= ++currentAS) {
    currentAS = 0;
    schedule(&ComputeResource::assign, 60); // TODO backoff

  } else assign();
}


void ComputeResource::assignResponse(Event::Request *req, int err) {
  if (!req || req->getResponseCode() != Event::HTTPStatus::HTTP_OK) {
    LOG_ERROR("Error response from AS");
    assignNextAS();
    return;
  }

  auto assignment = req->getInputJSON();

  if (assignment->getString(0) != "assign") {
    LOG_ERROR("Invalid response from AS " << *assignment);
    assignNextAS();
  }

  // Check AS signature
  Certificate asCert(assignment->getString(3));
  KeyPair asPub;
  asCert.getPublicKey(asPub);
  KeyContext keyCtx(asPub);
  keyCtx.verifyInit();
  string signature = Base64().decode(assignment->getString(2));
  string data = assignment->getDict(1).toString();
  keyCtx.verify(signature, Digest::hash(data, "sha256"));

  units.push_back(new Unit(app, assignment));
  units.back()->download();
}


void ComputeResource::assign() {
  setState(COMPUTE_ASSIGN);

  // TODO use https
  URI uri = "http://" + app.getServers()[currentAS].toString() + "/api/assign";
  auto pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                                 this, &ComputeResource::assignResponse);
  writeAssign(*pr->getJSONWriter());
  pr->send();
}
