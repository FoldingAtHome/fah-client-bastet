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

#include "Unit.h"

#include "App.h"
#include "OS.h"
#include "Server.h"
#include "GPUResources.h"
#include "Units.h"
#include "Core.h"
#include "Cores.h"
#include "Config.h"
#include "ExitCode.h"

#include <cbang/Catch.h>

#include <cbang/log/Logger.h>
#include <cbang/log/TailFileToLog.h>

#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/os/CPUInfo.h>
#include <cbang/os/CPURegsX86.h>

#include <cbang/event/Event.h>
#include <cbang/event/HTTPConnOut.h>
#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/gpu/GPUVendor.h>
#include <cbang/time/Time.h>
#include <cbang/time/TimeInterval.h>
#include <cbang/util/Random.h>
#include <cbang/json/Reader.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  string idFromSig(const string &sig) {
    if (sig.empty()) return "";
    return Digest::urlBase64(sig, "sha256");
  }


  string idFromSig64(const string &sig64) {
    if (sig64.empty()) return "";
    return idFromSig(Base64().decode(sig64));
  }


  void addGPUArgs(vector<string> &args, const ComputeDevice &cd,
                  const string &name) {
    if (!cd.isValid()) return;

    args.push_back("-" + name + "-platform");
    args.push_back(String(cd.platformIndex));
    args.push_back("-" + name + "-device");
    args.push_back(String(cd.deviceIndex));
  }


  bool existsAndOlderThan(const string &path, unsigned secs) {
    return SystemUtilities::exists(path) &&
      secs < Time::now() - SystemUtilities::getModificationTime(path);
  }
}


#undef CBANG_LOG_PREFIX
#define CBANG_LOG_PREFIX << getLogPrefix()


Unit::Unit(App &app) :
  app(app), event(app.getEventBase().newEvent(this, &Unit::next, 0)) {}


Unit::Unit(App &app, uint64_t wu, uint32_t cpus,
           const std::set<string> &gpus) : Unit(app) {
  this->wu = wu;
  insert("number", wu);
  insert("cpus", cpus);

  auto l = createList();
  for (auto it = gpus.begin(); it != gpus.end(); it++) l->append(*it);
  insert("gpus", l);

  setState(UNIT_ASSIGN);
}


Unit::Unit(App &app, const JSON::ValuePtr &data) : Unit(app) {
  this->data = data->get("data");
  merge(*data->get("state"));

  wu = getU64("number", -1);
  id = getString("id", "");
  if (id.empty()) setState(UNIT_DONE); // Invalid WU

  // Check that we still have the required core
  if (getState() == UNIT_RUN) setState(UNIT_CORE);

  // Start out paused, wait for Units to decide the resource allocation
  insertBoolean("paused", true);
}


Unit::~Unit() {
  if (pr.isSet()) {
    pr->setCallback(0);
    pr->getConnection().close();
  }

  if (logCopier.isSet()) logCopier->join();

  event->del();
}


void Unit::setState(UnitState state) {
  if (hasString("state") && state == getState()) return;
  app.getUnits().triggerUpdate();
  insert("state", state.toString());
}


UnitState Unit::getState() const {return UnitState::parse(getString("state"));}
uint64_t Unit::getProjectKey() const {return app.getConfig().getProjectKey();}
bool Unit::isWaiting() const {return wait && Time::now() < wait;}


bool Unit::isPaused() const {
  return app.getConfig().getPaused() || app.getOS().shouldIdle() ||
    getBoolean("paused", true) || app.shouldQuit();
}


void Unit::setPause(bool pause) {
  insertBoolean("paused", pause);
  if (!pause) triggerNext();
}


const char *Unit::getPauseReason() const {
  if (app.getConfig().getPaused()) return "Paused by user";
  if (app.getOS().shouldIdle())    return "Waiting for idle system";
  if (getBoolean("paused", true))  return "Resources not available";
  if (app.shouldQuit())            return "Shutting down";
  if (isWaiting())                 return "Waiting to retry";
  return "Not paused";
}


bool Unit::isRunning() const {return process.isSet();}


uint64_t Unit::getRunTime() const {
  // Stored ``run-time`` is run time up to the end of the last run
  int64_t runTime = getU64("run-time", 0);

  // If core process is currently running, add accumulated time
  if (processStartTime) runTime += Time::now() - processStartTime - clockSkew;

  return 0 < runTime ? runTime : 0;
}


uint64_t Unit::getRunTimeEstimate() const {
  // If valid, use estimate provided by the WS
  uint64_t estimate = data->selectU64("wu.data.estimate", 0);
  if (estimate) return estimate;

  // Make our own estimate
  if (getKnownProgress() && lastKnownProgressUpdateRunTime)
    return lastKnownProgressUpdateRunTime / getKnownProgress();

  // Make a wild guess based on timeout
  return 0.2 * data->selectU64("assignment.data.timeout", 1);
}


double Unit::getEstimatedProgress() const {
  if (isFinished()) return has("error") ? 0 : 1;

  // If the core process is not currently running, return saved "progress"
  if (!processStartTime || !lastKnownProgressUpdate)
    return getNumber("progress", 0);

  // Get estimated progress since last update from core
  double delta = getRunTime() - lastKnownProgressUpdateRunTime;
  double deltaProgress = delta / getRunTimeEstimate();
  if (0.01 < deltaProgress) deltaProgress = 0.01; // No more than 1%

  double progress = getKnownProgress() + deltaProgress;
  return progress < 1 ? progress : 1; // No more than 100%
}


uint64_t Unit::getCreditEstimate() const {
  uint64_t credit = data->selectU64("assignment.data.credit", 0);

  // Compute bonus estimate
  // Use request time to account for potential client/AS clock offset
  // Note, if the client's clock has changed ``requested`` may be < now
  uint64_t requested = Time::parse(data->selectString("request.data.time"));
  uint64_t timeout   = data->selectU64("assignment.data.timeout",  0);
  uint64_t deadline  = data->selectU64("assignment.data.deadline", 0);
  int64_t  delta     = (int64_t)Time::now() - requested + getETA();

  // No bonus after timeout
  if (0 < delta && delta < (int64_t)timeout) {
    double bonus = sqrt(0.75 * (double)deadline / delta); // Bonus forumula
    if (1 < bonus) credit *= bonus;
  }

  return credit;
}


uint64_t Unit::getETA() const {
  return getRunTimeEstimate() * (1 - getEstimatedProgress());
}


uint64_t Unit::getPPD() const {
  return getCreditEstimate() / getRunTimeEstimate() * Time::SEC_PER_DAY;
}


string Unit::getLogPrefix() const {return String::printf("WU%" PRIu64 ":", wu);}

string Unit::getDirectory() const {
  if (id.empty()) THROW("WU does not have an ID");
  return "work/" + id;
}

string Unit::getWSBaseURL() const {
  string addr    = data->selectString("assignment.data.ws");
  uint32_t port  = data->selectU32("assignment.data.port", 443);
  string portStr = port == 443 ? "" : (":" + String(port));
  string scheme  = (port == 443 || port == 8084) ? "https" : "http";
  return scheme + "://" + addr + portStr + "/api";
}


uint64_t Unit::getDeadline() const {
  // Use request time to account for potential client/AS clock offset
  return Time::parse(data->selectString("request.data.time")) +
    data->selectU64("assignment.data.deadline");
}


bool Unit::isFinished() const {
  switch (getState()) {
  case UNIT_UPLOAD:
  case UNIT_DUMP:
  case UNIT_CLEAN:
  case UNIT_DONE:
    return true;

  default: return false;
  }
}


bool Unit::isExpired() const {
  switch (getState()) {
  case UNIT_ASSIGN:
  case UNIT_DUMP:
  case UNIT_CLEAN:
  case UNIT_DONE:
    return false;

  default: return getDeadline() < Time::now();
  }
}


void Unit::triggerNext(double secs) {
  if (event->isPending()) return;
  event->add(secs);
}


void Unit::dumpWU() {
  switch (getState()) {
  case UNIT_ASSIGN: case UNIT_DOWNLOAD:            setState(UNIT_CLEAN); break;
  case UNIT_CORE: case UNIT_RUN: case UNIT_UPLOAD: setState(UNIT_DUMP);  break;
  case UNIT_DUMP: case UNIT_CLEAN: case UNIT_DONE: return; // Do nothing
  }

  // Stop waiting
  setWait(0);
  retries = 0;
  event->del();

  pr.release(); // Terminate any active connections
  triggerNext();
}


void Unit::save() {
  if (getState() == UNIT_ASSIGN || getState() == UNIT_DONE) return;

  JSON::BufferWriter writer;

  writer.beginDict();
  writer.insert("state", *this);
  if (data.isSet()) writer.insert("data", *data);
  writer.endDict();
  writer.flush();

  app.getDB("units").set(id, writer.toString());
}


void Unit::next() {
  // Check if WU has expired
  if (isExpired()) {
    LOG_INFO(1, "Unit expired, deleting");
    setState(UNIT_CLEAN);
  }

  // Update pause reason
  if (isPaused()) insert("pause-reason", getPauseReason());
  else if (hasString("pause-reason")) erase("pause-reason");

  // Monitor running core process
  if (process.isSet()) {
    if (process->isRunning()) {
      if (isPaused() || getState() != UNIT_RUN) return stopRun();
      return monitorRun();
    }

    return finalizeRun();
  }

  // Handle pause
  if (isPaused() && getState() != UNIT_CLEAN && getState() != UNIT_DUMP)
    return readViewerData();

  // Handle event backoff
  if (getState() != UNIT_DONE && isWaiting()) {
    uint64_t now = Time::now();
    return triggerNext(now < wait ? wait - now : 0);
  }

  try {
    switch (getState()) {
    case UNIT_ASSIGN:   assign();   break;
    case UNIT_DOWNLOAD: download(); break;
    case UNIT_CORE:     getCore();  break;
    case UNIT_RUN:      run();      break;
    case UNIT_UPLOAD:   upload();   break;
    case UNIT_DUMP:     dump();     break;
    case UNIT_CLEAN:    clean();    break;
    case UNIT_DONE:                 break;
    }

    return;
  } CATCH_ERROR;

  retry();
}


void Unit::processStarted() {
  lastProcessTimer = processStartTime = Time::now();
  processInterruptTime = lastKnownDone = lastKnownTotal =
    lastKnownProgressUpdate = clockSkew = 0;
}


void Unit::processEnded() {
  insert("run-time", getRunTime());
  processStartTime = 0;
}


void Unit::processTimer() {
  uint64_t now = Time::now();

  // Detect and adjust for clock skew
  int64_t delta = (int64_t)now - lastProcessTimer;
  if (data < 0 || 15 < delta) {
    LOG_WARNING("Detected clock skew (" << TimeInterval(delta)
                << "), I/O delay, laptop hibernation, other slowdown or "
                "clock change noted, adjusting time estimates");
    clockSkew += delta;
  }

  lastProcessTimer = now;
}


double Unit::getKnownProgress() const {
  return lastKnownTotal ? (double)lastKnownDone / lastKnownTotal : 0;
}


void Unit::updateKnownProgress(uint64_t done, uint64_t total) {
  if (!total || total < done) return;

  if (lastKnownDone != done || lastKnownTotal != total) {
    lastKnownDone = done;
    lastKnownTotal = total;
    lastKnownProgressUpdate = Time::now();
    lastKnownProgressUpdateRunTime = getRunTime();
  }
}


void Unit::setProgress(double done, double total) {
  double progress = done / total;
  double oldValue = getNumber("progress", 0);

  progress = round(progress * 1000) / 1000;

  if (oldValue != progress) {
    insert("progress", progress);

    if (floor(oldValue * 100) < floor(progress * 100) && getState() != UNIT_RUN)
      LOG_INFO(1, getState() << String::printf(" %0.0f%% ", progress * 100));
  }
}


void Unit::getCore() {
  if (core.isSet()) return; // Already have core

  core = app.getCores().get(data->select("assignment.data.core"));

  auto cb =
    [this] (unsigned complete, int total) {
      setProgress(complete, total);

      if (core->isInvalid()) {
        LOG_INFO(1, "Failed to download core");
        setState(UNIT_DUMP);
        triggerNext();
      }

      if (core->isReady()) {
        setState(UNIT_RUN);
        triggerNext();
      }
    };

  core->addProgressCallback(cb);
}


void Unit::run() {
  if (process.isSet()) return; // Already running

  // Make sure WU data exists
  if (!SystemUtilities::exists(getDirectory() + "/wudata_01.dat")) {
    LOG_ERROR("Missing WU data");
    return clean();
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
  args.push_back(SystemUtilities::absolute(core->getPath()));
  args.push_back("-dir");
  args.push_back(getID());
  args.push_back("-suffix");
  args.push_back("01");
  args.push_back("-version");
  args.push_back(app.getVersion().toString());
  args.push_back("-lifeline");
  args.push_back(String(SystemUtilities::getPID()));
  args.push_back("-checkpoint");
  args.push_back(app.getConfig().getAsString("checkpoint"));

  auto &gpus = *get("gpus");
  if (gpus.size()) {
    auto &gpu = *app.getGPUs().get(gpus.getString(0)).cast<GPUResource>();

    args.push_back("-gpu-vendor");
    args.push_back(gpu.getGPU().getType().toString());
    addGPUArgs(args, gpu.getOpenCL(), "opencl");
    addGPUArgs(args, gpu.getCUDA(), "cuda");
    args.push_back("-gpu");
    args.push_back(String(gpu.getOpenCL().deviceIndex));

  } else { // CPU
    uint32_t cpus = getU32("cpus");
    args.push_back("-np");
    args.push_back(String(cpus));
  }

  // Rotate old log file
  string logFile = getDirectory() + "/logfile_01.txt";
  SystemUtilities::rotate(logFile, string(), 32);

  // Run
  LOG_INFO(3, "Running FahCore: " << Subprocess::assemble(args));
  process->setWorkingDirectory("work");
  process->exec(args, Subprocess::NULL_STDOUT | Subprocess::NULL_STDERR |
                Subprocess::CREATE_PROCESS_GROUP | Subprocess::W32_HIDE_WINDOW,
                app.getConfig().getCorePriority());
  LOG_INFO(3, "Started FahCore on PID " << process->getPID());

  // Redirect core output to log
  if (logCopier.isSet()) logCopier->join();
  logCopier = new TailFileToLog(logFile, getLogPrefix());
  logCopier->start();

  processStarted();
  triggerNext();

  insert("assignment", data->select("assignment.data"));
  insert("wu", data->select("wu.data"));
}


void Unit::readInfo() {
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
      updateKnownProgress(info.done, info.total);
    }
  }
}


void Unit::readViewerData() {
  if (topology.isNull()) readViewerTop();
  if (readViewerFrame()) triggerNext();
}


void Unit::readViewerTop() {
  string filename = getDirectory() + "/viewerTop.json";

  if (existsAndOlderThan(filename, 10)) {
    topology = JSON::Reader(filename).parse();
    frames.clear();
  }
}


bool Unit::readViewerFrame() {
  string filename =
    getDirectory() + String::printf("/viewerFrame%d.json", viewerFrame);

  if (existsAndOlderThan(filename, 10)) {
    auto frame = JSON::Reader(filename).parse();

    if (!frames.empty() && *frames.back() == *frame)
      LOG_WARNING("Visualization frame " << viewerFrame
                  << " unchanged, skipping");

    else {
      frames.push_back(frame);
      insert("frames", (uint32_t)frames.size());
    }

    viewerFrame++;
    return true;
  }

  return false;
}


void Unit::setResults(const string &status, const string &dataHash) {
  auto request = data->get("request");
  auto assign = data->get("assignment");
  auto wu = data->get("wu");
  string sigData =
    request->toString() + assign->toString() + wu->toString() + status +
    dataHash;
  string sig64 = app.getKey().signBase64SHA256(sigData);

  JSON::Builder builder;
  builder.beginDict();
  if (!status.empty()) builder.insert("status", status);
  builder.insert("sha256", dataHash);
  builder.insert("signature", sig64);
  builder.endDict();

  data->insert("results", builder.getRoot());
}


void Unit::finalizeRun() {
  if (logCopier.isSet()) logCopier->join();
  logCopier.release();

  ExitCode code = (ExitCode::enum_t)process->wait();

  if (process->getWasKilled())  LOG_WARNING("Core was killed");
  if (process->getDumpedCore()) LOG_WARNING("Core crashed");
  if (code == ExitCode::FINISHED_UNIT) setProgress(1, 1);

  process.release();
  processEnded();

  bool ok = code == ExitCode::FINISHED_UNIT || code == ExitCode::INTERRUPTED;
  LOG(CBANG_LOG_DOMAIN, ok ? LOG_INFO_LEVEL(1) : Logger::LEVEL_WARNING,
      "Core returned " << code << " (" << (unsigned)code << ')');

#ifdef _WIN32
  if (0xc000000 <= code)
    LOG_WARNING("Core exited with Windows unhandled exception code "
                << String::printf("0x%08x", (unsigned)code)
                << ".  See https://bit.ly/2CXgWkZ for more information.");
#endif

  // Notify parent
  app.getUnits().triggerUpdate();

  // WU not complete if core was interrupted
  if (code == ExitCode::INTERRUPTED) return triggerNext();

  // Read result data
  string filename = getDirectory() + "/wuresults_01.dat";

  if (SystemUtilities::exists(filename)) {
    string resultData = SystemUtilities::read(filename);
    string hash64 = Digest::base64(resultData, "sha256");

    // TODO Set status "ok" once WS are upgraded
    // TODO Send multi-part data with JSON followed by binary data
    setResults("", hash64);
    data->insert("data", Base64().encode(resultData));
    setState(UNIT_UPLOAD);

  } else setState(UNIT_DUMP);

  triggerNext();
}


void Unit::stopRun() {
  if (!processInterruptTime) {
    processInterruptTime = Time::now();
    process->interrupt();

  } else if (processInterruptTime + 60 < Time::now()) {
    LOG_WARNING("Core did not shutdown gracefully, killing process");
    process->kill();
    processInterruptTime = 1; // Prevent further interrupt or kill
  }

  triggerNext(1);
}


void Unit::monitorRun() {
  try {
    // Time run
    processTimer();

    // Read core shared info file
    readInfo();

    // Read visualization data
    readViewerData();

    // Update ETA, PPD and progress
    string eta = TimeInterval(getETA()).toString();
    uint64_t ppd = getPPD();
    if (eta != getString("eta", "")) insert("eta", eta);
    if (ppd != getU64("ppd", 0)) insert("ppd", ppd);
    setProgress(getEstimatedProgress(), 1);
  } CATCH_ERROR;

  triggerNext(1);
}


void Unit::clean() {
  LOG_DEBUG(3, "Cleaning WU");

  // Remove from disk
  TRY_CATCH_ERROR(SystemUtilities::rmdir(getDirectory(), true));

  // Remove from DB
  TRY_CATCH_ERROR(app.getDB("units").unset(id));

  setState(UNIT_DONE);
  app.getUnits().unitComplete(success);
}


void Unit::setWait(double delay) {
  wait = Time::now() + delay;
  insert("delay", delay);
  insert("wait", Time(wait).toString());
}


void Unit::retry() {
  if (++retries < 10) {
    double delay = pow(2, retries);
    setWait(delay);
    LOG_INFO(1, "Retrying in " << TimeInterval(delay));

  } else {
    LOG_INFO(1, "Too many retries, failing WU");
    setWait(0);
    retries = 0;
    setState(UNIT_CLEAN);
  }

  insert("retries", retries);

  next();
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

  // Update CPUs
  unsigned cpus = assign->getU32("cpus");
  LOG_DEBUG(3, "Assignment for " << cpus << " cpus");
  insert("cpus", cpus);

  // Update GPUs
  if (assign->hasList("gpus")) insert("gpus", assign->get("gpus"));
  else get("gpus")->clear();

  // Try to allocate more units now that our resources have been updated
  app.getUnits().triggerUpdate();

  this->data = data;
  setState(UNIT_DOWNLOAD);
}


void Unit::writeRequest(JSON::Sink &sink) {
  auto &info = SystemInfo::instance();
  auto cpuInfo = CPUInfo::create();
  CPURegsX86 cpuRegsX86;

  sink.beginDict();

  // Protect against replay
  sink.insert("time",           Time().toString());
  sink.insert("wu",             getU64("number"));

  // Client
  sink.insert("version",        app.getVersion().toString());
  sink.insert("id",             app.getInfo().getString("id"));

  // User
  sink.insert("user",           app.getConfig().getUsername());
  sink.insert("team",           app.getConfig().getTeam());
  sink.insert("passkey",        app.getConfig().getPasskey());

  // OS
  sink.insertDict("os");
  sink.insert("version",        info.getOSVersion().toString());
  sink.insert("type",           app.getOS().getName());
  sink.insert("memory",         info.getFreeMemory());
  sink.endDict();

  // Project
  auto &config = app.getConfig();
  sink.insertDict("project");
  if (config.hasString("release"))
    sink.insert("release", config.getString("release"));
  if (config.hasString("cause"))
    sink.insert("cause", config.getString("cause"));
  if (getProjectKey()) sink.insert("key", getProjectKey());
  sink.endDict(); // project

  // Compute resources
  sink.insertDict("resources");

  // CPU
  sink.insertDict("cpu");
  sink.insert("cpu",            app.getOS().getCPU());
  sink.insert("cpus",           getU32("cpus"));
  sink.insert("vendor",         cpuInfo->getVendor());
  sink.insert("signature",      cpuInfo->getSignature());
  sink.insert("family",         cpuInfo->getFamily());
  sink.insert("model",          cpuInfo->getModel());
  sink.insert("stepping",       cpuInfo->getStepping());
  sink.insert("features",       cpuRegsX86.getCPUFeatures());
  sink.insert("extended",       cpuRegsX86.getCPUExtendedFeatures());
  sink.insert("80000001",       cpuRegsX86.getCPUFeatures80000001());
  sink.endDict(); // cpu

  // GPU
  auto &gpus = *get("gpus");
  for (unsigned i = 0; i < gpus.size(); i++) {
    auto &gpu = *app.getGPUs().get(gpus.getString(i)).cast<GPUResource>();

    sink.beginInsert(gpu.getID());
    gpu.writeRequest(sink);
  }

  sink.endDict(); // resources
  sink.endDict();
}


void Unit::assign() {
  if (pr.isSet()) return; // Already assigning

  data = JSON::build([this] (JSON::Sink &sink) {writeRequest(sink);});
  string signature = app.getKey().signSHA256(data->toString());

  id = idFromSig(signature);
  insert("id", id);

  LOG_INFO(1, "Requesting WU assignment");
  LOG_DEBUG(3, *data);

  // TODO validate peer certificate
  URI uri = "https://" + app.getNextAS().toString() + "/api/assign";

  pr = app.getClient()
    .call(uri, Event::RequestMethod::HTTP_POST, this, &Unit::response);

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

  // Decode data
  string wuData = Base64().decode(data->getString("data"));
  data->erase("data"); // Don't save WU data to DB

  // Log it
  LOG_DEBUG(3, *data);

  // Check data hash
  if (wu->getString("sha256") != Digest::base64(wuData, "sha256"))
    THROW("WU data hash does not match");

  // Create and populate WU directory
  if (!SystemUtilities::exists(getDirectory())) {
    SystemUtilities::ensureDirectory(getDirectory());

    auto f = SystemUtilities::oopen(getDirectory() + "/wudata_01.dat");
    f->write(wuData.c_str(), wuData.length());
  }

  setState(UNIT_CORE);
  this->data = data;
  save(); // Not strictly necessary
}


void Unit::download() {
  if (pr.isSet()) return; // Already downloading

  LOG_INFO(1, "Downloading WU");

  // Monitor download progress
  auto progressCB =
    [this] (const Progress &p) {setProgress(p.getTotal(), p.getSize());};

  URI uri = getWSBaseURL() + "/assign";
  pr = app.getClient()
    .call(uri, Event::RequestMethod::HTTP_POST, this, &Unit::response);

  data->write(*pr->getJSONWriter());
  setProgress(0, 0);
  pr->getConnection().getReadProgress().setCallback(progressCB, 1);
  pr->send();
}


void Unit::uploadResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Credited");
  setState(UNIT_CLEAN);
  success = true;

  // Log credit record
  try {
    SystemUtilities::ensureDirectory("credits");
    data->write(*SystemUtilities::oopen("credits/" + getID() + ".json"));
  } CATCH_ERROR;
}


void Unit::upload() {
  if (pr.isSet()) return; // Already uploading

  LOG_INFO(1, "Uploading WU results");

  // Monitor upload progress
  auto progressCB =
    [this] (const Progress &p) {setProgress(p.getTotal(), p.getSize());};

  URI uri = getWSBaseURL() + "/results";
  pr = app.getClient()
    .call(uri, Event::RequestMethod::HTTP_POST, this, &Unit::response);

  auto writer = pr->getJSONWriter();
  data->write(*writer);
  writer->close();

  setProgress(0, 0);
  pr->getConnection().getWriteProgress().setCallback(progressCB, 1);
  pr->send();
}


void Unit::dumpResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Dumped");
  setState(UNIT_CLEAN);
  success = true;

  // Log dump record
  try {
    SystemUtilities::ensureDirectory("credits");
    data->write(*SystemUtilities::oopen("credits/" + getID() + ".json"));
  } CATCH_ERROR;
}


void Unit::dump() {
  if (pr.isSet()) return; // Already dumping

  LOG_INFO(1, "Sending dump report");

  setResults("dumped", "");

  URI uri = getWSBaseURL() + "/results";
  pr = app.getClient()
    .call(uri, Event::RequestMethod::HTTP_POST, this, &Unit::response);

  auto writer = pr->getJSONWriter();
  data->write(*writer);
  writer->close();

  pr->send();
}


void Unit::response(Event::Request &req) {
  pr.release(); // Deref request object

  try {
    if (req.getConnectionError()) {
      LOG_ERROR("Failed response: " << req.getConnectionError());
      return retry();
    }

    if (!req.isOk()) {
      LOG_ERROR(req.getResponseCode() << ": " << req.getInput());

      try {
        auto msg = req.getJSONMessage();

        if (msg->hasDict("error"))
          insert("error", msg->selectString("error.message"));
        else if (msg->hasString("error"))
          insert("error", msg->getString("error"));
      } CATCH_ERROR;

      // Handle HTTP reponse codes
      switch (req.getResponseCode()) {
      case HTTP_SERVICE_UNAVAILABLE: retry(); break;

      case HTTP_BAD_REQUEST: case HTTP_NOT_ACCEPTABLE: case HTTP_GONE:
      default:
        setState(UNIT_CLEAN);
        break;
      }

    } else {
      if (has("error")) erase("error");

      switch (getState()) {
      case UNIT_ASSIGN:   assignResponse(req.getInputJSON());   break;
      case UNIT_DOWNLOAD: downloadResponse(req.getInputJSON()); break;
      case UNIT_UPLOAD:   uploadResponse(req.getInputJSON());   break;
      case UNIT_DUMP:     dumpResponse(req.getInputJSON());     break;
      default: THROW("Unexpected unit state " << getState());
      }
    }

    next();
    return;
  } CATCH_ERROR;

  retry();
}
