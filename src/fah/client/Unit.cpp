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
#include "ResourceGroup.h"

#include <cbang/Catch.h>

#include <cbang/log/Logger.h>
#include <cbang/log/TailFileToLog.h>

#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/os/CPUInfo.h>
#include <cbang/os/CPURegsX86.h>

#include <cbang/event/Event.h>
#include <cbang/event/HTTPConnOut.h>

#include <cbang/time/Time.h>
#include <cbang/time/TimeInterval.h>

#include <cbang/util/Random.h>
#include <cbang/util/HumanSize.h>

#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/gpu/GPUVendor.h>
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


  void addGPUArgs(vector<string> &args, const GPUResource &gpu,
                  const string &name) {
    if (!gpu.has(name)) return;
    const auto &drv = *gpu.get(name);

    args.push_back("-" + name + "-platform");
    args.push_back(String(drv.getU32("platform")));
    args.push_back("-" + name + "-device");
    args.push_back(String(drv.getU32("device")));
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


Unit::Unit(App &app, uint64_t wu, const string &group, uint32_t cpus,
           const std::set<string> &gpus) :
  Unit(app) {
  this->wu = wu;
  insert("number", wu);
  insert("group", group);
  setCPUs(cpus);
  setGPUs(gpus);
  setState(UNIT_ASSIGN);
}


Unit::Unit(App &app, const JSON::ValuePtr &data) :
  Unit(app) {
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
  cancelRequest();
  if (logCopier.isSet()) logCopier->join();
  event->del();
}


void Unit::setUnits(const cb::SmartPointer<Units> &units) {
  this->units = units;

  if (units.isSet()) {
    string group = units->getGroup().getName();
    if (group != getGroup()) insert("group", group);
  }
}


const Config &Unit::getConfig() const {return units->getConfig();}
string    Unit::getGroup() const {return getString("group", "");}
UnitState Unit::getState() const {return UnitState::parse(getString("state"));}


bool Unit::atRunState() const {
  auto state = getState();
  return (state == UNIT_CORE && getRunTime()) || state == UNIT_RUN;
}


bool Unit::hasRun() const {return getRunTime() || UNIT_RUN <= getState();}
uint64_t Unit::getProjectKey() const {return getConfig().getProjectKey();}
bool Unit::isWaiting() const {return wait && Time::now() < wait;}


bool Unit::isPaused() const {
  return getConfig().getPaused() || units->waitForIdle() ||
    getBoolean("paused", true) || app.shouldQuit();
}


void Unit::setPause(bool pause) {
  insertBoolean("paused", pause);
  if (!pause) triggerNext();
}


const char *Unit::getPauseReason() const {
  if (getConfig().getPaused())     return "Paused by user";
  if (units->waitForIdle())        return "Waiting for idle system";
  if (getBoolean("paused", true))  return "Resources not available";
  if (app.shouldQuit())            return "Shutting down";
  if (isWaiting())                 return "Waiting to retry";
  return "Not paused";
}


bool Unit::isRunning() const {return process.isSet();}


void Unit::setCPUs(uint32_t cpus) {
  if (!hasU32("cpus") || cpus != getCPUs()) insert("cpus", cpus);
}


uint32_t Unit::getMinCPUs() const {
  uint32_t cpus = getCPUs();
  return data.isSet() ?
    data->selectU32("assignment.data.min_cpus", cpus) : cpus;
}


uint32_t Unit::getMaxCPUs() const {
  uint32_t cpus = getCPUs();
  return data.isSet() ?
    data->selectU32("assignment.data.max_cpus", cpus) : cpus;
}


void Unit::setGPUs(const std::set<string> &gpus) {
  auto l = createList();
  for (auto id: gpus) l->append(id);
  insert("gpus", l);
}


std::set<string> Unit::getGPUs() const {
  std::set<string> gpus;

  auto &l = *get("gpus");
  for (unsigned i = 0; i < l.size(); i++)
    gpus.insert(l.getString(i));

  return gpus;
}


bool Unit::hasGPU(const string &id) const {
  auto const &gpus = getGPUs();

  for (auto gpuID: gpus)
    if (gpuID == id) return true;

  return false;
}


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
  double delta         = getRunTime() - lastKnownProgressUpdateRunTime;
  double runtime       = getRunTimeEstimate();
  double deltaProgress = runtime ? delta / runtime : 0;
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


string Unit::getLogPrefix() const {
  return String::printf("%s:WU%" PRIu64 ":", getGroup().c_str(), wu);
}


string Unit::getDirectory() const {
  if (id.empty()) THROW("WU does not have an ID");
  return "work/" + id;
}


URI Unit::getWSURL(const string &path) const {
  string host    = data->selectString("assignment.data.ws");
  uint32_t port  = data->selectU32("assignment.data.port", 443);
  string scheme  = (port == 443 || port == 8084) ? "https" : "http";
  IPAddress addr(host, port);

  return URI(scheme, addr, "/api" + path);
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


void Unit::triggerNext(double secs) {event->add(secs);}


void Unit::dumpWU() {
  LOG_INFO(3, "Dumping " << id);

  switch (getState()) {
  case UNIT_ASSIGN: case UNIT_DOWNLOAD:            setState(UNIT_CLEAN); break;
  case UNIT_CORE: case UNIT_RUN: case UNIT_UPLOAD: setState(UNIT_DUMP);  break;
  case UNIT_DUMP: case UNIT_CLEAN: case UNIT_DONE: return; // Do nothing
  }

  // Stop waiting
  setWait(0);
  retries = 0;
  event->del();

  cancelRequest(); // Terminate any active connections
  save();
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


void Unit::cancelRequest() {
  if (pr.isSet()) {
    pr->setCallback(0);
    pr->getConnection().close();
    pr.release();
  }
}


void Unit::setState(UnitState state) {
  if (hasString("state") && state == getState()) return;
  if (units.isSet()) units->triggerUpdate();
  insert("state", state.toString());
  setProgress(0, 0);
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
      if (isPaused() || getState() != UNIT_RUN || getCPUs() != runningCPUs)
        return stopRun();
      return monitorRun();
    }

    return finalizeRun();
  }

  // Handle pause
  if (isPaused() && getState() < UNIT_DUMP) {
    if (!pr.isSet() && getState() == UNIT_ASSIGN) setState(UNIT_CLEAN);
    else {
      setWait(0); // Stop waiting
      retries = 0;
      return readViewerData();
    }
  }

  // Handle event backoff
  if (getState() < UNIT_CLEAN && isWaiting())
    return triggerNext(wait - Time::now());

  try {
    switch (getState()) {
    case UNIT_ASSIGN:   return assign();
    case UNIT_DOWNLOAD: return download();
    case UNIT_CORE:     return getCore();
    case UNIT_RUN:      return run();
    case UNIT_UPLOAD:   return upload();
    case UNIT_DUMP:     return dump();
    case UNIT_CLEAN:    return clean();
    case UNIT_DONE:     return;
    }
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
  double progress = total ? done / total : 0;
  double oldValue = getNumber("progress", 0);

  progress = round(progress * 1000) / 1000;

  if (oldValue != progress) {
    insert("progress", progress);

    if (floor(oldValue * 100) < floor(progress * 100) && getState() != UNIT_RUN)
      LOG_INFO(1, getState() << String::printf(" %0.0f%% ", progress * 100)
               << HumanSize(done) << "B of " << HumanSize(total) << 'B');
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
        data.release();
        setState(UNIT_ASSIGN);
        retry();

      } else if (core->isReady()) {
        // Update resource allocation only after WU is ready to run
        auto assign = data->get("assignment")->get("data");
        setCPUs(assign->getU32("cpus"));
        if (assign->hasList("gpus")) insert("gpus", assign->get("gpus"));
        else get("gpus")->clear();

        setState(UNIT_RUN);
        triggerNext();
      }
    };

  core->addProgressCallback(cb);
}


void Unit::run() {
  if (process.isSet()) return; // Already running

  // Reset retry count
  retries = 0;

  // Make sure WU data exists
  if (!SystemUtilities::exists(getDirectory() + "/wudata_01.dat")) {
    LOG_ERROR("Missing WU data");
    return setState(UNIT_CLEAN);
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

  runningCPUs = getU32("cpus");

  auto &gpus = *get("gpus");
  if (gpus.size()) {
    auto &gpu = *app.getGPUs().get(gpus.getString(0)).cast<GPUResource>();

    args.push_back("-gpu-vendor");
    args.push_back(gpu.getString("type"));
    addGPUArgs(args, gpu, "opencl");
    addGPUArgs(args, gpu, "cuda");

    if (gpu.has("opencl")) {
      args.push_back("-gpu");
      args.push_back(String(gpu.get("opencl")->getU32("device")));
    }

  } else { // CPU
    args.push_back("-np");
    args.push_back(String(runningCPUs));
  }

  // Rotate old log file
  string logFile = getDirectory() + "/logfile_01.txt";
  SystemUtilities::rotate(logFile, string(), 32);

  // Run
  LOG_INFO(3, "Running FahCore: " << Subprocess::assemble(args));
  process->setWorkingDirectory("work");
  process->exec(args, Subprocess::NULL_STDOUT | Subprocess::NULL_STDERR |
                Subprocess::CREATE_PROCESS_GROUP | Subprocess::W32_HIDE_WINDOW,
                getConfig().getCorePriority());
  LOG_INFO(3, "Started FahCore on PID " << process->getPID());

  // Redirect core output to log
  if (logCopier.isSet()) logCopier->join();
  logCopier = new TailFileToLog(logFile, getLogPrefix());
  logCopier->start();

  processStarted();
  triggerNext();

  insert("assignment", data->select("assignment.data"));
  insert("wu",         data->select("wu.data"));
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
  if (getState() < UNIT_CORE) return;
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
  auto assign  = data->get("assignment");
  auto wu      = data->get("wu");
  string sigData =
    request->toString() + assign->toString() + wu->toString() + status +
    dataHash;
  string sig64 = app.getKey().signBase64SHA256(sigData);

  JSON::Builder builder;
  builder.beginDict();
  if (!status.empty())   builder.insert("status", status);
  if (!dataHash.empty()) builder.insert("sha256", dataHash);
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
  units->triggerUpdate();

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
    string   eta = TimeInterval(getETA()).toString();
    uint64_t ppd = getPPD();
    if (eta != getString("eta", "")) insert("eta", eta);
    if (ppd != getU64   ("ppd",  0)) insert("ppd", ppd);
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
  units->unitComplete(success);
}


void Unit::setWait(double delay) {
  wait = Time::now() + delay;
  insert("delay", delay);
  insert("wait",  Time(wait).toString());
}


void Unit::retry() {
  // Clear any pending requests
  cancelRequest();

  // Retry results with CS
  if (getState() == UNIT_UPLOAD && data->select("wu.data")->hasList("cs")) {
    auto const &csList = data->selectList("wu.data.cs");

    if (csList.size()) {
      if (cs <  (int)csList.size()) cs++;
      if (cs == (int)csList.size()) cs = -1;
      return next();
    }
  }

  if (++retries < 10 || getState() == UNIT_ASSIGN ||
      (retries <= 50 && UNIT_UPLOAD <= getState())) {
    double delay = pow(2, std::min(9U, retries));
    setWait(delay);
    LOG_INFO(1, "Retry #" << retries << " in " << TimeInterval(delay));

  } else {
    LOG_INFO(1, "Too many retries (" << (retries - 1) << "), failing WU");
    setWait(0);
    retries = 0;
    setState(UNIT_CLEAN);
  }

  insert("retries", retries);

  triggerNext();
}


void Unit::assignResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Received WU assignment " << id);
  LOG_DEBUG(3, data->toString());

  // Check certificate, key usage and signature
  auto request = data->get("request");
  auto assign  = data->get("assignment");
  string cert  = assign->getString("certificate");
  string sig64 = assign->getString("signature");
  assign = assign->get("data");
  app.checkBase64SHA256(cert, "", sig64,
                        request->toString() + assign->toString(), "AS");

  // Check that this is the same request we sent
  if (idFromSig64(request->getString("signature")) != id)
    THROW("WS response does not match request");

  LOG_DEBUG(3, "Received assignment for " << assign->getU32("cpus")
            << " cpus and " << get("gpus")->size() << " gpus");

  this->data = data;
  setState(UNIT_DOWNLOAD);
}


void Unit::writeRequest(JSON::Sink &sink) {
  auto &sysInfo = SystemInfo::instance();
  auto cpuInfo = CPUInfo::create();
  CPURegsX86 cpuRegsX86;

  sink.beginDict();

  // Protect against replay
  sink.insert("time",    Time().toString());
  sink.insert("wu",      getU64("number"));

  // Client
  auto &info = *units->getGroup().getInfo();
  sink.insert("version", info.getString("version"));
  sink.insert("id",      info.getString("id"));

  // User
  sink.insert("user",    getConfig().getUsername());
  sink.insert("team",    getConfig().getTeam());
  sink.insert("passkey", getConfig().getPasskey());

  // OS
  sink.insertDict("os");
  sink.insert("version", sysInfo.getOSVersion().toString());
  sink.insert("type",    info.getString("os"));
  sink.insert("memory",  sysInfo.getFreeMemory());
  sink.endDict();

  // Project
  sink.insertDict("project");
  if (getConfig().hasString("release"))
    sink.insert("release", getConfig().getString("release"));
  if (getConfig().hasString("cause"))
    sink.insert("cause", getConfig().getString("cause"));
  if (getProjectKey()) sink.insert("key", getProjectKey());
  sink.endDict(); // project

  // Compute resources
  sink.insertDict("resources");

  // CPU
  sink.insertDict("cpu");
  sink.insert("cpu",            info.getString("cpu"));
  sink.insert("cpus",           getCPUs());
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

  LOG_INFO(1, "Requesting WU assignment for user " << getConfig().getUsername()
           << " team " << getConfig().getTeam());
  LOG_DEBUG(3, *data);

  // TODO validate peer certificate
  URI uri("https", app.getNextAS(), "/api/assign");

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
  auto assign  = data->get("assignment");
  auto wu      = data->get("wu");
  string cert  = wu->getString("certificate");
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

  pr = app.getClient()
    .call(getWSURL("/assign"), Event::RequestMethod::HTTP_POST, this,
          &Unit::response);

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

  // Get WS or CS URI
  URI uri;
  if (cs == -1) uri = getWSURL("/results");
  else {
    auto const &csList = data->selectList("wu.data.cs");
    IPAddress addr = csList.getString(cs);
    uri = URI(addr.hasHost() ? "https" : "http", addr, "/api/results");
  }

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
  LOG_DEBUG(3, *data);

  pr = app.getClient()
    .call(getWSURL("/results"), Event::RequestMethod::HTTP_POST, this,
          &Unit::response);

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
      case HTTP_SERVICE_UNAVAILABLE:
        // We always need a new assignment token
        if (getState() == UNIT_DOWNLOAD) {
          data.release();
          setState(UNIT_ASSIGN);
        }
        retry();
        break;

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
