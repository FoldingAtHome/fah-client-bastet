/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2024, foldingathome.org
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
#include "Groups.h"
#include "Core.h"
#include "CoreProcess.h"
#include "Cores.h"
#include "Config.h"
#include "ExitCode.h"

#include <cbang/Catch.h>

#include <cbang/log/Logger.h>
#include <cbang/log/TailFileToLog.h>

#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/hw/CPUInfo.h>
#include <cbang/hw/CPURegsX86.h>

#include <cbang/event/Event.h>
#include <cbang/http/ConnOut.h>

#include <cbang/time/Time.h>
#include <cbang/time/TimeInterval.h>

#include <cbang/util/Random.h>
#include <cbang/util/HumanSize.h>

#include <cbang/net/Base64.h>
#include <cbang/openssl/Digest.h>
#include <cbang/hw/GPUVendor.h>
#include <cbang/json/Reader.h>
#include <cbang/json/Builder.h>
#include <cbang/json/BufferWriter.h>

#include <cinttypes>
#include <cmath>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  static const uint64_t maxViewerBytes = 2.5e7;


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
#define CBANG_LOG_PREFIX getLogPrefix()


Unit::Unit(App &app) :
    app(app), event(app.getEventBase().newEvent(this, &Unit::next, 0)) {
  event->setPriority(6);  // Ensure event always runs after Group::update()
  triggerNext();
}


Unit::Unit(App &app, const string &group, uint64_t wu, uint32_t cpus,
           const std::set<string> &gpus) :
  Unit(app) {
  this->wu = wu;
  setGroup(&app.getGroups()->getGroup(group));
  insert("number", wu);
  setCPUs(cpus);
  setGPUs(gpus);
  setState(UNIT_ASSIGN);
}


Unit::Unit(App &app, const JSON::ValuePtr &data) : Unit(app) {
  this->data = data->get("data");
  merge(*data->get("state"));

  // Get group
  setGroup(&app.getGroups()->getGroup(getString("group", "")));

  // Fix old var names
  const char *vars[] = {"run-time", "pause-reason", 0};
  for (int i = 0; vars[i]; i++)
    if (has(vars[i])) {
      insert(String::replace(vars[i], "-", "_"), get(vars[i]));
      erase(vars[i]);
    }

  wu = getU64("number", -1);
  id = getString("id", "");
  LOG_INFO(3, "Loading work unit " << wu << " with ID " << id);
  if (id.empty()) setState(UNIT_DONE); // Invalid WU

  // Check that we still have the required core
  if (getState() == UNIT_RUN) setState(UNIT_CORE);
}


Unit::~Unit() {
  cancelRequest();
  endLogCopy();
}


void Unit::setGroup(const SmartPointer<Group> &group) {
  insert("group", group->getName());
  this->group = group;
}


const Config &Unit::getConfig() const {return group->getConfig();}


const string &Unit::getClientID() const {
  return data->selectString("request.data.id", "");
}


UnitState Unit::getState() const {return UnitState::parse(getString("state"));}


bool Unit::atRunState() const {
  auto state = getState();
  return (state == UNIT_CORE && getRunTime()) || state == UNIT_RUN;
}


bool Unit::hasRun()    const {return getRunTime() || UNIT_RUN <= getState();}
bool Unit::isWaiting() const {return wait && Time::now() < wait;}
bool Unit::isPaused()  const {return getPauseReason();}


void Unit::setPause(bool pause) {
  insertBoolean("paused", pause);
  if (!pause) triggerNext();
}


const char *Unit::getPauseReason() const {
  if (getConfig().getPaused())     return "Paused";
  if (group->waitForIdle())        return "Waiting for idle system";
  if (group->waitOnBattery())      return "Pausing on battery";
  if (group->waitOnGPU())          return "Waiting for GPU detection";
  if (getBoolean("paused", false)) return "Resources not available";
  if (app.shouldQuit())            return "Shutting down";
  return 0;
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
  for (auto gpu: *get("gpus"))
    gpus.insert(gpu->getString());
  return gpus;
}


bool Unit::hasGPU(const string &id) const {
  auto const &gpus = getGPUs();

  for (auto gpuID: gpus)
    if (gpuID == id) return true;

  return false;
}


uint64_t Unit::getRunTimeDelta() const {
  return processStartTime ? Time::now() - processStartTime - clockSkew : 0;
}


uint64_t Unit::getRunTime() const {
  // Stored ``run_time`` is run time up to the end of the last run
  int64_t runTime = getU64("run_time", 0);

  // If core process is currently running, add accumulated time
  runTime += getRunTimeDelta();

  return 0 < runTime ? runTime : 0;
}


uint64_t Unit::getRunTimeEstimate() const {
  // If valid, use estimate provided by the WS
  uint64_t estimate = data->selectU64("wu.data.estimate", 0);
  if (estimate) return estimate;

  // Make our own estimate
  if (getKnownProgress() && lastKnownProgressUpdateRunTime)
    return lastKnownProgressUpdateRunTime / getKnownProgress();

  // Make a wild guess based on timeout or 1 day
  return 0.2 * data->selectU64("assignment.data.timeout", Time::SEC_PER_DAY);
}


double Unit::getEstimatedProgress() const {
  if (isFinished()) return has("error") ? 0 : 1;

  // If the core process is not currently running, return saved "wu_progress"
  if (!processStartTime || !lastKnownProgressUpdate)
    return getNumber("wu_progress", 0);

  // Get estimated progress since last update from core
  double delta         = getRunTime() - lastKnownProgressUpdateRunTime;
  double runtime       = getRunTimeEstimate();
  double deltaProgress = (0 < delta && 0 < runtime) ? delta / runtime : 0;
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
    double bonus = sqrt(0.75 * (double)deadline / delta); // Bonus formula
    if (1 < bonus) credit *= bonus;
  }

  return credit;
}


uint64_t Unit::getETA() const {
  return getRunTimeEstimate() * (1 - getEstimatedProgress());
}


uint64_t Unit::getPPD() const {
  return (double)getCreditEstimate() / getRunTimeEstimate() * Time::SEC_PER_DAY;
}


string Unit::getLogPrefix() const {
  return String::printf("WU%" PRIu64 ":", wu);
}


string Unit::getDirectory() const {
  if (id.empty()) THROW("WU does not have an ID");
  return "work/" + id;
}


URI Unit::getWSURL(const string &path) const {
  string host = data->selectString("assignment.data.ws");
  return URI("https", host, 0, "/api" + path);
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
  case UNIT_DONE:
    return true;

  default: return false;
  }
}


bool Unit::isExpired() const {
  switch (getState()) {
  case UNIT_ASSIGN:
  case UNIT_DUMP:
  case UNIT_DONE:
    return false;

  default: return getDeadline() < Time::now();
  }
}


void Unit::triggerNext(double secs) {event->add(secs);}


void Unit::dumpWU() {
  LOG_INFO(3, "Dumping " << id);

  setWait(0); // Stop waiting
  retries = 0;
  success = true; // Don't delay group retry
  cancelRequest(); // Terminate any active connections

  switch (getState()) {
  case UNIT_ASSIGN: case UNIT_DOWNLOAD: clean("dumped"); break;
  case UNIT_CORE: case UNIT_RUN: case UNIT_UPLOAD: setState(UNIT_DUMP); break;
  case UNIT_DUMP: case UNIT_DONE: return; // Do nothing
  }

  save();
  triggerNext();
}


void Unit::save() {
  if (getState() < UNIT_CORE || getState() == UNIT_DONE) return;

  JSON::BufferWriter writer;

  writer.beginDict();
  writer.insert("state", *this);
  if (data.isSet()) writer.insert("data", *data);
  writer.endDict();
  writer.flush();

  app.getDB("units").set(id, writer.toString());
}


void Unit::cancelRequest() {pr.release();}


void Unit::setState(UnitState state) {
  if (hasString("state") && state == getState()) return;
  if (group.isSet()) group->triggerUpdate();
  insert("state", state.toString());
  clearProgress();
}


void Unit::next() {
  // Check if WU has expired
  if (isExpired()) {
    LOG_INFO(1, "Unit expired, deleting");
    return clean("expired");
  }

  // Update pause reason
  if (isPaused()) insert("pause_reason", getPauseReason());
  else if (hasString("pause_reason")) erase("pause_reason");

  // Monitor running core process
  if (process.isSet()) {
    if (process->isRunning()) {
      // Only interrupt after minimum run time to give the core time to
      // install it's interrupt handlers.
      if (isPaused() || getState() != UNIT_RUN || getCPUs() != runningCPUs) {
        const unsigned minRuntime = 5;
        auto delta = getRunTimeDelta();
        if (minRuntime <= delta) return stopRun();
        else triggerNext(minRuntime - delta);
      }

      return monitorRun();
    }

    finalizeRun();
    return save();
  }

  // Handle pause
  if (isPaused() && getState() < UNIT_DUMP) {
    if (!pr.isSet() && getState() == UNIT_ASSIGN) return clean("aborted");
    else {
      setWait(0); // Stop waiting
      retries = 0;
      return readViewerData();
    }
  }

  // Handle event backoff
  if (getState() <= UNIT_DUMP && isWaiting())
    return triggerNext(wait - Time::now());

  try {
    switch (getState()) {
    case UNIT_ASSIGN:   return assign();
    case UNIT_DOWNLOAD: return download();
    case UNIT_CORE:     return getCore();
    case UNIT_RUN:      return run();
    case UNIT_UPLOAD:   return upload();
    case UNIT_DUMP:     return dump();
    case UNIT_DONE:     return;
    }
  } CATCH_ERROR;

  retry();
}


void Unit::processStarted(const SmartPointer<CoreProcess> &process) {
  auto pid = process->getPID();
  LOG_INFO(3, "Started FahCore on PID " << pid);
  this->process = process;
  lastSkewTimer = processStartTime = Time::now();
  lastKnownDone = lastKnownTotal = lastKnownProgressUpdate = clockSkew = 0;
  insert("start_time", Time(processStartTime).toString());
  insert("pid", pid);
}


void Unit::processEnded() {
  insert("run_time", getRunTime());
  erase("start_time");
  erase("pid");
  processStartTime = 0;
}


void Unit::skewTimer() {
  uint64_t now = Time::now();

  // Detect and adjust for clock skew
  int64_t delta = (int64_t)now - lastSkewTimer;
  if (data < 0 || 300 < delta) {
    LOG_WARNING("Detected clock skew (" << TimeInterval(delta)
                << "), I/O delay, laptop hibernation, other slowdown or "
                "clock change noted, adjusting time estimates");
    clockSkew += delta;
  }

  lastSkewTimer = now;
}


double Unit::getKnownProgress() const {
  return lastKnownTotal ? (double)lastKnownDone / lastKnownTotal : 0;
}


void Unit::updateKnownProgress(uint64_t done, uint64_t total) {
  if (!total || total < done) return;

  if (lastKnownDone != done || lastKnownTotal != total) {
    lastKnownDone                  = done;
    lastKnownTotal                 = total;
    lastKnownProgressUpdate        = Time::now();
    lastKnownProgressUpdateRunTime = getRunTime();
  }
}


void Unit::setProgress(double done, double total, bool wu) {
  const char *key = wu ? "wu_progress" : "progress";
  double progress = round((total ? done / total : 0) * 1000) / 1000;
  double oldValue = getNumber(key, 0);

  if (oldValue != progress) {
    insert(key, progress);

    if (floor(oldValue * 100) < floor(progress * 100) && !wu && 1 < total)
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
        retry();

      } else if (core->isReady()) {
        // Update resource allocation only after WU is ready to run
        auto assign = data->get("assignment")->get("data");
        setCPUs(assign->getU32("cpus"));
        if (assign->hasList("gpus")) insert("gpus", assign->get("gpus"));
        else get("gpus")->clear();

        setState(UNIT_RUN);
        setPause(true); // Let Group start this WU when appropriate
        group->triggerUpdate();
      }
    };

  core->addProgressCallback(cb);
}


void Unit::run() {
  if (process.isSet()) return; // Already running

  // Make sure WU data exists
  if (!SystemUtilities::exists(getDirectory() + "/wudata_01.dat")) {
    LOG_ERROR("Missing WU data");
    return clean("missing");
  }

  // Remove old results if exists
  SystemUtilities::unlink(getDirectory() + "/wuresults_01.dat");

  // Rotate old log file
  string logFile = getDirectory() + "/logfile_01.txt";
  SystemUtilities::rotate(logFile, string(), 32);

  // Args
  vector<string> args;
  args.push_back("-dir");
  args.push_back(getID());
  args.push_back("-suffix");
  args.push_back("01");
  args.push_back("-version");
  args.push_back(app.getVersion().toString());
  args.push_back("-lifeline");
  args.push_back(String(SystemUtilities::getPID()));

  runningCPUs = getCPUs();

  auto &gpus = *get("gpus");
  if (gpus.size()) {
    string id = gpus.getString(0);
    auto &gpu = *app.getGPUs().get(id).cast<GPUResource>();

    // GPU UUID
    if (gpu.hasString("uuid")) {
      args.push_back("-gpu-uuid");
      args.push_back(gpu.getString("uuid"));
    }

    // GPU platform
    bool withCUDA = gpu.isComputeDeviceSupported("cuda", getConfig());
    bool withHIP  = gpu.isComputeDeviceSupported("hip",  getConfig());
    args.push_back("-gpu-platform");
    args.push_back(withCUDA ? "cuda" : "opencl");

    // Old GPU options
    args.push_back("-gpu-vendor");
    args.push_back(gpu.getString("type"));

    addGPUArgs(args, gpu, "opencl");
    if (withCUDA) addGPUArgs(args, gpu, "cuda");
    if (withHIP)  addGPUArgs(args, gpu, "hip");

    if (gpu.has("opencl")) {
      args.push_back("-gpu");
      args.push_back(String(gpu.get("opencl")->getU32("device")));
    }

  } else { // CPU
    args.push_back("-np");
    args.push_back(String(runningCPUs));
  }

  // Run
  auto process = SmartPtr(new CoreProcess(core->getPath()));
  process->exec(args);
  processStarted(process);
  startLogCopy(logFile); // Redirect core output to log
  triggerNext();

  insert("assignment", data->select("assignment.data"));
  insert("wu",         data->select("wu.data"));
  if (!has("wu_progress")) insert("wu_progress", 0);
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
  if (viewerFail < 0 || getState() < UNIT_CORE) return;

  try {
    if (topology.isNull()) readViewerTop();
    return readViewerFrame();
  } CATCH_WARNING;

  if (5 < ++viewerFail) {
    if (topology.isNull()) {
      LOG_WARNING("Giving up on reading visualization");
      viewerFail = -1;

    } else {
      LOG_WARNING("Giving up on reading visualization frame " << viewerFrame++);
      viewerFail = 0;
    }
  }
}


void Unit::readViewerTop() {
  string filename = getDirectory() + "/viewerTop.json";

  if (existsAndOlderThan(filename, 10)) {
    frames.clear();
    viewerBytes = SystemUtilities::getFileSize(filename);

    if (maxViewerBytes < viewerBytes) {
      LOG_WARNING("Visualization topology too large, disabling visualization");
      viewerFail = -1;
      return;
    }

    topology = JSON::Reader::parseFile(filename);
    viewerFail = 0;
  }
}


void Unit::readViewerFrame() {
  string filename =
    getDirectory() + String::printf("/viewerFrame%d.json", viewerFrame);

  if (existsAndOlderThan(filename, 10)) {
    uint64_t bytes = SystemUtilities::getFileSize(filename);

    if (maxViewerBytes < viewerBytes + bytes) {
      LOG_WARNING("Visualization size exceeded at frame " << viewerFrame
        << ", no more frames will be loaded");
      viewerFail = -1;
      return;
    }

    auto frame = JSON::Reader::parseFile(filename);

    if (!frames.empty() && *frames.back() == *frame)
      LOG_WARNING("Visualization frame " << viewerFrame
        << " unchanged, skipping");

    else if (!frame->empty()) {
      frames.push_back(frame);
      insert("frames", (uint32_t)frames.size());
      viewerBytes += bytes;
    }

    viewerFrame++;
    viewerFail = 0;
    triggerNext(); // Try to load another frame
  }
}


void Unit::setResults(const string &status, const string &dataHash) {
  auto request = data->get("request");
  auto assign  = data->get("assignment");
  auto wu      = data->get("wu", createDict());
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
  group->triggerUpdate(); // Notify parent
  triggerNext();
  endLogCopy();

  ExitCode code = (ExitCode::enum_t)process->wait();

#ifdef _WIN32
  if (0xc0000000 <= (unsigned)code) {
    LOG_ERROR("Core exited with Windows unhandled exception code "
                << String::printf("0x%08x", (unsigned)code)
                << ".  See https://bit.ly/2CXgWkZ for more information.");
    code = ExitCode::FAILED_1;
  }
#endif

  if (process->getWasKilled()) {
    LOG_ERROR("Core was killed");
    code = ExitCode::FAILED_1;
  }

  if (process->getDumpedCore()) {
    LOG_ERROR("Core crashed with exit code " << (unsigned)code);
    code = ExitCode::FAILED_1;
  }

  process.release();
  if (code == ExitCode::FINISHED_UNIT) setProgress(1, 1, true);
  processEnded();

  success = code == ExitCode::FINISHED_UNIT;
  bool ok = success || code == ExitCode::INTERRUPTED ||
    code == ExitCode::CORE_RESTART;
  LOG(CBANG_LOG_DOMAIN, ok ? LOG_INFO_LEVEL(1) : Logger::LEVEL_ERROR,
      "Core returned " << code << " (" << (unsigned)code << ')');

  // WU not complete if core was restarted or interrupted
  if (code == ExitCode::INTERRUPTED)  return;
  if (code == ExitCode::CORE_RESTART) return retry();

  if (!bytesCopiedToLog)
    LOG_ERROR("The folding core did not produce any log output.  This "
      "indicates that the core is not functional on your system.  Check "
      "for missing libraries or GPU drivers.  Make a post about your issue "
      "on https://foldingforum.org/ to get more help.");

  if (!code.isValid()) {
    LOG_ERROR("Core exited with an unknown error code " << (unsigned)code
      << " which probably indicates that it crashed. Dumping WU");
    return setState(UNIT_DUMP);
  }

  // Read result data
  string filename = getDirectory() + "/wuresults_01.dat";

  if (SystemUtilities::exists(filename)) {
    string resultData = SystemUtilities::read(filename);
    string hash64 = Digest::base64(resultData, "sha256");

    // TODO Send multi-part data with JSON followed by binary data
    setResults(ok ? "ok" : "failed", hash64);
    data->insert("data", Base64().encode(resultData));

    return setState(UNIT_UPLOAD);
  }

  LOG_ERROR("Run did not produce any results. Dumping WU");
  setState(UNIT_DUMP);
}


void Unit::stopRun() {
  process->stop();
  triggerNext(1);
}


void Unit::monitorRun() {
  try {
    // Check for clock skew
    skewTimer();

    // Read core shared info file
    readInfo();

    // Read visualization data
    readViewerData();

    // Update ETA, PPD and progress
    auto eta = TimeInterval(getETA(), true).toString();
    auto ppd = getPPD();
    if (eta != getString("eta", "")) insert("eta", eta);
    if (ppd != getU64("ppd", -1))    insert("ppd", ppd);
    setProgress(getEstimatedProgress(), 1, true);

    // Clear retries after running long enough
    if (retries && Time::SEC_PER_MIN * 2 < getRunTimeDelta()) retries = 0;
  } CATCH_ERROR;

  triggerNext(1);
}


void Unit::clean(const string &result) {
  LOG_DEBUG(3, "Cleaning WU");

  if (!id.empty()) {
    // Log WU
    insert("end_time", Time().toString());
    insert("result", result);
    TRY_CATCH_ERROR(app.logWU(*this));

    // Remove from disk
    TRY_CATCH_ERROR(SystemUtilities::rmdir(getDirectory(), true));

    // Remove from DB
    TRY_CATCH_ERROR(app.getDB("units").unset(id));
  }

  bool downloaded = UNIT_DOWNLOAD < getState();
  setState(UNIT_DONE);
  group->unitComplete(result, downloaded);
}


void Unit::setWait(double delay) {
  double now     = Time::now();
  double newWait = now + delay;

  if (newWait != wait && (isWaiting() || now < newWait)) {
    wait = newWait;
    insert("delay", delay);
    insert("wait",  Time(wait).toString());
  }
}


void Unit::retry() {
  triggerNext();

  // Clear any pending requests
  cancelRequest();

  try {
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
      double delay = std::pow(2, std::min(9U, retries));
      setWait(delay);
      LOG_INFO(1, "Retry #" << retries << " in " << TimeInterval(delay));

    } else {
      LOG_INFO(1, "Too many retries (" << (retries - 1) << "), failing WU");
      setWait(0);
      retries = 0;
      success = false;
      return clean("retries");
    }

    insert("retries", retries);
    return;

  } CATCH_ERROR;

  // Retry failed
  success = false;
  switch (getState()) {
  case UNIT_DONE: return;
  case UNIT_DUMP: return clean("failed");
  default:        return setState(UNIT_DUMP);
  }
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


void Unit::writeRequest(JSON::Sink &sink) const {
  auto &sysInfo = SystemInfo::instance();
  auto cpuInfo  = CPUInfo::create();
  CPURegsX86 cpuRegsX86;

  sink.beginDict();

  // Protect against replay
  sink.insert("time",    Time().toString());
  sink.insert("wu",      getU64("number"));

  // Client
  string id = Base64().encode(URLBase64().decode(app.getID()));
  sink.insert("version", app.selectString("info.version"));
  sink.insert("id",      id);

  // User
  sink.insert("user",    app.getConfig()->getUsername());
  sink.insert("team",    app.getConfig()->getTeam());
  sink.insert("passkey", app.getConfig()->getPasskey());
  string aid = app.selectString("info.account", "");
  if (!aid.empty()) sink.insert("account", aid);

  // OS
  sink.insertDict("os");
  sink.insert("version", sysInfo.getOSVersion().toString());
  sink.insert("type",    app.selectString("info.os"));
  sink.insert("memory",  sysInfo.getFreeMemory());
  sink.endDict();

  // Project
  auto gpus = getGPUs();
  sink.insertDict("project");
  if (app.getConfig()->hasString("cause"))
    sink.insert("cause", app.getConfig()->getString("cause"));
  sink.insertBoolean("beta", getConfig().getBeta(gpus));
  auto key = getConfig().getProjectKey(gpus);
  if (key) sink.insert("key", key);
  sink.endDict(); // project

  // Compute resources
  sink.insertDict("resources");

  // CPU
  sink.insertDict("cpu");
  sink.insert("cpu",            app.selectString("info.cpu"));
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
  for (auto &id: gpus) {
    auto &gpu = *app.getGPUs().get(id).cast<GPUResource>();
     sink.beginInsert(id);
     gpu.writeRequest(sink, getConfig());
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

  LOG_INFO(1, "Requesting WU assignment for user "
    << app.getConfig()->getUsername() << " team "
    << app.getConfig()->getTeam());
  LOG_DEBUG(3, *data);

  // TODO validate peer certificate
  URI uri("https", app.getNextAS(), 0, "/api/assign");

  pr = app.getClient()
    .call(uri, HTTP::Method::HTTP_POST, this, &Unit::response);

  pr->getRequest()->send([&] (JSON::Sink &sink) {
    sink.beginDict();
    sink.insert("data", *data);
    sink.insert("signature", Base64().encode(signature));
    sink.insert("pub-key", app.getKey().publicToPEMString());
    sink.endDict();
  });

  pr->send();
}


void Unit::downloadResponse(const JSON::ValuePtr &data) {
  // Check certificate, F@H usage & signature
  auto request = data->get("request");
  auto assign  = data->get("assignment");
  auto wu      = data->get("wu");
  string cert  = wu->getString("certificate");
  string inter = wu->getString("intermediate");
  string sig64 = wu->getString("signature");
  wu = wu->get("data");

  LOG_INFO(1, "Received WU " << data->format(
    "P{assignment.data.project} R{wu.data.run} C{wu.data.clone} G{wu.data.gen}",
    "???"));

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
    f->write(wuData.data(), wuData.length());
  }

  setState(UNIT_CORE);
  this->data = data;
  save();
}


void Unit::download() {
  if (pr.isSet()) return; // Already downloading

  LOG_INFO(1, "Downloading WU");

  // Monitor download progress
  auto progressCB =
    [this] (const Progress &p) {setProgress(p.getTotal(), p.getSize());};

  pr = app.getClient()
    .call(getWSURL("/assign"), HTTP::Method::HTTP_POST, this,
          &Unit::response);

  pr->getRequest()->send([&] (JSON::Sink &sink) {data->write(sink);});
  clearProgress();
  pr->getConnection()->getReadProgress().setCallback(progressCB, 1);
  pr->send();
}


void Unit::uploadResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Credited");
  logCredit(data);
  clean(success ? "credited" : "failed");
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
    string host = csList.getString(cs);
    uri = URI("https", host, 0, "/api/results");
  }

  pr = app.getClient()
    .call(uri, HTTP::Method::HTTP_POST, this, &Unit::response);

  pr->getRequest()->send([&] (JSON::Sink &sink) {data->write(sink);});
  clearProgress();
  pr->getConnection()->getWriteProgress().setCallback(progressCB, 1);
  pr->send();
}


void Unit::dumpResponse(const JSON::ValuePtr &data) {
  LOG_INFO(1, "Dumped");
  logCredit(data);
  clean("dumped");
}


void Unit::dump() {
  if (pr.isSet()) return; // Already dumping

  LOG_INFO(1, "Sending dump report");
  setResults("dumped", "");
  LOG_DEBUG(5, *data);

  pr = app.getClient()
    .call(getWSURL("/results"), HTTP::Method::HTTP_POST, this,
          &Unit::response);

  pr->getRequest()->send([&] (JSON::Sink &sink) {data->write(sink);});
  pr->send();
}


void Unit::response(HTTP::Request &req) {
  pr.release(); // Deref request object

  try {
    if (req.logResponseErrors()) {
      if (req.getConnectionError()) return retry();

      try {
        auto msg = req.getJSONMessage();

        if (msg.isSet()) {
          if (msg->hasDict("error"))
            insert("error", msg->selectString("error.message"));
          else if (msg->hasString("error"))
            insert("error", msg->getString("error"));
        }
      } CATCH_ERROR;

      // Handle HTTP response codes
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
      default: return clean("rejected");
      }

    } else {
      if (has("error")) erase("error");

      switch (getState()) {
      case UNIT_ASSIGN:     assignResponse(req.getInputJSON()); break;
      case UNIT_DOWNLOAD: downloadResponse(req.getInputJSON()); break;
      case UNIT_UPLOAD:     uploadResponse(req.getInputJSON()); break;
      case UNIT_DUMP:         dumpResponse(req.getInputJSON()); break;
      default: THROW("Unexpected unit state " << getState());
      }
    }

    next();
    return;
  } CATCH_ERROR;

  retry();
}


void Unit::logCredit(const JSON::ValuePtr &data) {
  try {
    string dir = Time().toString("credits/%Y/%m/");
    SystemUtilities::ensureDirectory(dir);
    data->write(*SystemUtilities::oopen(dir + getID() + ".json"));
  } CATCH_ERROR;
}


void Unit::startLogCopy(const string &filename) {
  bytesCopiedToLog = 0;
  endLogCopy();
  logCopier = new TailFileToLog(app.getEventBase(), filename, getLogPrefix());
}


void Unit::endLogCopy() {
  if (logCopier.isNull()) return;
  bytesCopiedToLog = logCopier->getBytesCopied();
  logCopier.release();
}
