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

#include "Unit.h"

#include "App.h"
#include "Server.h"
#include "Slot.h"
#include "Core.h"
#include "Cores.h"
#include "ExitCode.h"

#include <cbang/Catch.h>

#include <cbang/log/Logger.h>
#include <cbang/log/TailFileToLog.h>

#include <cbang/event/Event.h>
#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/gpu/GPUVendor.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  string idFromSig(const string &sig) {
    return URLBase64().encode(Digest::hash(sig, "sha256"));
  }


  string idFromSig64(const string &sig64) {
    return idFromSig(Base64().decode(sig64));
  }


  void addGPUArgs(vector<string> &args, Slot &slot, const string &name) {
    if (!slot.has(name)) return;

    args.push_back("-" + name + "-platform");
    args.push_back(String(slot.getPlatformIndex(name)));
    args.push_back("-" + name + "-device");
    args.push_back(String(slot.getDeviceIndex(name)));
  }
}


#undef CBANG_LOG_PREFIX
#define CBANG_LOG_PREFIX << getLogPrefix()


Unit::Unit(App &app, Slot &slot) :
  Event::Scheduler<Unit>(app.getEventBase()), app(app), slot(slot),
  state(UNIT_ASSIGN) {}


Unit::Unit(App &app, Slot &slot, const JSON::ValuePtr &data) : Unit(app, slot) {
  this->data = data;
  id = idFromSig64(data->selectString("request.signature"));
  state = UNIT_CORE;
}


Unit::~Unit() {
  if (logCopier.isSet()) logCopier->join();
}


string Unit::getLogPrefix() const {
  return slot.getID() + ":" + id.substr(0, 12) + ":";
}


string Unit::getWSBaseURL() const {
  string addr = data->selectString("assignment.data.ws");
  int port = app.getOptions()["ws-port"].toInteger();
  string portStr = port == 80 ? "" : (":" + String(port));
  return "http://" + addr + portStr + "/api";
}


uint64_t Unit::getDeadline() const {
  return Time(data->selectString("assignment.data.deadline"));
}


bool Unit::isExpired() const {return getDeadline() < Time::now();}


void Unit::next() {
  try {
    switch (state) {
    case UNIT_ASSIGN:   assign();   break;
    case UNIT_DOWNLOAD: download(); break;
    case UNIT_CORE:     getCore();  break;
    case UNIT_RUN:      run();      break;
    case UNIT_UPLOAD:   upload();   break;
    case UNIT_CLEAN:    clean();    break;
    case UNIT_DONE:                 break;
    }
  } CATCH_ERROR;

  // TODO Handle retry
}


void Unit::getCore() {
  core = app.getCores().get(data->select("assignment.data.core"));

  auto cb =
    [this] () {
      // TODO check if core is valid
      state =
        (data->has("results") || data->has("dump")) ? UNIT_UPLOAD : UNIT_RUN;
      schedule(&Unit::next);
    };

  core->notify(cb);
}


void Unit::run() {
  // Create and populate WU directory
  if (!SystemUtilities::exists(getDirectory())) {
    SystemUtilities::ensureDirectory(getDirectory());

    string data = Base64().decode(this->data->getString("data"));
    auto f = SystemUtilities::oopen(getDirectory() + "/wudata_01.dat");
    f->write(data.c_str(), data.length());
  }

  // Remove old results if exists
  SystemUtilities::unlink(getDirectory() + "/wuresults_01.dat");

  process = new Subprocess;

  // Set environment library paths
  vector<string> paths;
  string execPath = SystemUtilities::getExecutablePath();
  paths.push_back(SystemUtilities::dirname(execPath));
  string corePath = SystemUtilities::absolute(core->getPath());
  paths.push_back(SystemUtilities::dirname(corePath));
  const string &ldPath = SystemUtilities::library_path;
  if (SystemUtilities::getenv(ldPath))
    SystemUtilities::splitPaths(SystemUtilities::getenv(ldPath), paths);
  process->set(ldPath, SystemUtilities::joinPaths(paths));

  // Args
  vector<string> args;
  args.push_back("../" + core->getPath());
  args.push_back("-dir");
  args.push_back(getID());
  args.push_back("-suffix");
  args.push_back("01");
  args.push_back("-version");
  args.push_back(app.getVersion().toString());
  args.push_back("-lifeline");
  args.push_back(String(SystemUtilities::getPID()));
  args.push_back("-checkpoint");
  args.push_back(String(slot.getCheckpoint()));

  if (slot.isGPU()) {
    args.push_back("-gpu-vendor");
    args.push_back(slot.getGPUType());
    addGPUArgs(args, slot, "opencl");
    addGPUArgs(args, slot, "cuda");
    args.push_back("-gpu");
    args.push_back(String(slot.getDeviceIndex("opencl")));

  } else { // CPU
    args.push_back("-cpu");
    args.push_back(String(slot.getCPUUsage()));

    unsigned cpus = slot.getCPUs();
    unsigned maxCPUs = data->selectU32("assignment.data.cpus", 0);
    if (maxCPUs && maxCPUs < cpus) {
      LOG_WARNING("AS lowered CPUs from " << cpus << " to " << maxCPUs);
      cpus = maxCPUs;
    }

    if (cpus) {
      args.push_back("-np");
      args.push_back(String(cpus));
    }
  }

  // Rotate old log file
  string logFile = getDirectory() + "/logfile_01.txt";
  SystemUtilities::rotate(logFile, string(), 32);

  // Run
  LOG_INFO(3, "Running FahCore: " << Subprocess::assemble(args));
  process->setWorkingDirectory("work");
  process->exec(args, Subprocess::NULL_STDOUT | Subprocess::NULL_STDERR |
                Subprocess::CREATE_PROCESS_GROUP | Subprocess::W32_HIDE_WINDOW,
                slot.getCorePriority());
  LOG_INFO(3, "Started FahCore on PID " << process->getPID());

  // Redirect core output to log
  logCopier = new TailFileToLog(logFile, getLogPrefix());
  logCopier->start();

  schedule(&Unit::monitor);

  slot.insert("assignment", data->select("assignment.data"));
  slot.insert("wu", data->select("wu.data"));
}


void Unit::monitor() {
  if (process->isRunning()) {
    // Read shared info
    try {
      string filename = getDirectory() + "/wuinfo_01.dat";
      if (SystemUtilities::exists(filename)) {
        struct WUInfo {
          uint32_t type;
          uint8_t  reserved[80];
          uint32_t total;
          uint32_t done;
        };

        WUInfo info;
        auto f = SystemUtilities::iopen(filename);
        f->read((char *)&info, sizeof(WUInfo));

        if (f->gcount() == sizeof(WUInfo)) {
          if (info.type != core->getType()) THROW("Invalid WU info");

          double progress = info.total ? (double)info.done / info.total : 0;
          if (this->progress != progress) {
            this->progress = progress;
            LOG_INFO(1, String::printf("progress = %0.2f%% ", progress * 100));

            slot.get("wu")->insert("progress", progress);
          }
        }
      }
    } CATCH_ERROR;

    schedule(&Unit::monitor, 1);

  } else {
    progress = 1;
    logCopier->join();
    logCopier.release();

    ExitCode code = (ExitCode::enum_t)process->wait();

    if (process->getWasKilled()) LOG_WARNING("Core was killed");
    if (process->getDumpedCore()) LOG_WARNING("Core crashed");
    if (code == ExitCode::INTERRUPTED) return; // TODO check if exiting

    process.release();

    bool ok = code == ExitCode::FINISHED_UNIT || code == ExitCode::INTERRUPTED;
    LOG(CBANG_LOG_DOMAIN, ok ? LOG_INFO_LEVEL(1) : Logger::LEVEL_WARNING,
        "Core returned " << code << " (" << (unsigned)code << ')');

#ifdef _WIN32
    if (0xc000000 <= code)
      LOG_WARNING("Core exited with Windows unhandled exception code "
                  String::printf("0x%08x", (unsigned)code)
                  << ".  See https://bit.ly/2CXgWkZ for more information.");
#endif

    // Read and sign results
    string filename = getDirectory() + "/wuresults_01.dat";

    if (SystemUtilities::exists(filename)) {
      string results = SystemUtilities::read(filename);
      string hash64 = Digest::base64(results, "sha256");
      auto request = data->get("request");
      auto assign = data->get("assignment");
      auto wu = data->get("wu");
      string sigData =
        request->toString() + assign->toString() + wu->toString() + hash64;
      string sig64 = app.getKey().signBase64SHA256(sigData);

      JSON::Builder builder;
      builder.beginDict();
      builder.insert("sha256", hash64);
      builder.insert("signature", sig64);
      builder.endDict();

      data->insert("results", builder.getRoot());
      data->insert("data", Base64().encode(results));

      state = UNIT_UPLOAD;

    } else dump();

    save();
    schedule(&Unit::next);
  }
}


void Unit::dump() {
  string wuSig64 = data->selectString("wu.signature");
  data->insert("dump", app.getKey().signBase64SHA256(wuSig64));
  data->insertUndefined("data");
  state = UNIT_UPLOAD;
}


void Unit::clean() {
  LOG_DEBUG(3, "Cleaning WU");

  try {
    SystemUtilities::rmdir(getDirectory(), true);
    remove();
  } CATCH_ERROR;

  state = UNIT_DONE;
  slot.next();
}


void Unit::save() {app.getDB(slot.getID()).set(id, *data);}
void Unit::remove() {app.getDB(slot.getID()).unset(id);}


void Unit::assignNextAS() {
  if (app.getServers().size() <= ++currentAS) {
    currentAS = 0;
    schedule(&Unit::assign, 60); // TODO backoff

  } else assign();
}


void Unit::assignResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Received WU assignment " << id);
  LOG_DEBUG(3, data->toString());

  // Check certificate, key usage and signature
  auto request = data->get("request");
  auto assign = data->get("assignment");
  string cert = assign->getString("certificate");
  string sig64 = assign->getString("signature");
  assign = assign->get("data");
  app.checkBase64SHA256(cert, "", sig64,
                        request->toString() + assign->toString(), "AS");

  // Check that this is the same request we sent
  if (idFromSig64(request->getString("signature")) != id)
    THROW("WS response does not match request");

  this->data = data;
  state = UNIT_DOWNLOAD;
}


void Unit::assign() {
  data = JSON::build([this] (JSON::Sink &sink) {slot.writeRequest(sink);});
  string signature = app.getKey().signSHA256(data->toString());

  id = idFromSig(signature);

  LOG_INFO(1, "Requesting WU assignment");

  // TODO use https
  URI uri = "http://" + app.getServers()[currentAS].toString() + "/api/assign";
  auto pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                                 this, &Unit::response);
  auto writer = pr->getJSONWriter();
  writer->beginDict();
  writer->insert("data", *data);
  writer->insert("signature", Base64().encode(signature));
  writer->insert("pub-key", app.getKey().publicToString());
  writer->endDict();
  writer->close();

  pr->send();
}


void Unit::downloadResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Received WU");

  // Check certificate, F@H usage & signature
  auto request = data->get("request");
  auto assign = data->get("assignment");
  auto wu = data->get("wu");
  string cert = wu->getString("certificate");
  string inter = wu->getString("intermediate");
  string sig64 = wu->getString("signature");
  wu = wu->get("data");
  string sigData = request->toString() + assign->toString() + wu->toString();
  app.checkBase64SHA256(cert, inter, sig64, sigData, "WS");

  // Check data hash
  if (wu->getString("sha256") !=
      Digest::base64(Base64().decode(data->getString("data")), "sha256"))
    THROW("WU data hash does not match");

  state = UNIT_CORE;
  this->data = data;
  save();
}


void Unit::download() {
  LOG_INFO(1, "Downloading WU");

  URI uri = getWSBaseURL() + "/assign";
  auto pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                                 this, &Unit::response);
  data->write(*pr->getJSONWriter());
  pr->send();
}


void Unit::uploadResponse(const JSON::ValuePtr &data) {
  // TODO
  LOG_INFO(1, "Credit " << *data);
  state = UNIT_CLEAN;

  // TODO Check signature
}


void Unit::upload() {
  LOG_INFO(1, "Uploading WU results");

  URI uri = getWSBaseURL() + "/results";
  auto pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                                 this, &Unit::response);

  auto writer = pr->getJSONWriter();
  data->write(*writer);
  writer->close();

  pr->send();
}


void Unit::response(Event::Request &req) {
  try {
    if (req.getConnectionError()) THROW("No response");

    if (!req.isOk()) {
      LOG_ERROR(req.getResponseCode() << ": " << req.getInput());

      // Handle HTTP reponse codes
      switch (req.getResponseCode()) {
      case HTTP_SERVICE_UNAVAILABLE: // TODO retry
      case HTTP_BAD_REQUEST: // TODO stop this slot?
      default: state = UNIT_CLEAN; break;
      }

    } else
      switch (state) {
      case UNIT_ASSIGN: assignResponse(req.getInputJSON()); break;
      case UNIT_DOWNLOAD: downloadResponse(req.getInputJSON()); break;
      case UNIT_UPLOAD: uploadResponse(req.getInputJSON()); break;
      default: THROW("Unexpected unit state " << state);
      }

    next();
    return;
  } CATCH_ERROR;

  // TODO retry on error
}
