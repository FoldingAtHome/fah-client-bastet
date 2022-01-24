/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2020, foldingathome.org
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
    return Digest::urlBase64(sig, "sha256");
  }


  string idFromSig64(const string &sig64) {
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


Unit::Unit(App &app, uint64_t wu, uint32_t cpus, const std::set<string> &gpus,
           uint64_t projectKey) : Unit(app) {
  this->wu = wu;
  insert("wu", wu);
  insert("cpus", cpus);
  if (projectKey) insert("key", projectKey);

  auto l = createList();
  for (auto it = gpus.begin(); it != gpus.end(); it++) l->append(*it);
  insert("gpus", l);

  setState(UNIT_ASSIGN);
}


Unit::Unit(App &app, const JSON::ValuePtr &data) : Unit(app) {
  this->data = data->get("data");
  merge(*data->get("state"));

  wu = getU64("wu", -1);
  id = idFromSig64(this->data->selectString("request.signature"));
  insert("id", id);

  runTime = getU64("run-time", 0);

  setState(UNIT_CORE);
  insertBoolean("paused", true);
  insert("pause-reason", "Initializing.");
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
uint64_t Unit::getProjectKey() const {return getU64("key", 0);}
bool Unit::isWaiting() const {return wait && Time::now() < wait;}


bool Unit::isPaused() const {
  return app.getConfig().getPaused() || isIdling() ||
          getBoolean("paused", true);
}


void Unit::setPause(bool pause) {insertBoolean("paused", pause);}


const char *Unit::getPauseReason() const {
  if (app.getConfig().getPaused()) return "Paused by user.";
  if (getBoolean("paused", true)) return "Resources not available.";
  if (isWaiting()) return "Waiting to retry.";
  if (isIdling()) return "Waiting for idle system.";
  return "Not paused";
}


uint64_t Unit::getRunTimeEstimate() const {
  double progress = getKnownProgress();
  if (progress) return (runTime / progress);
  uint64_t estimate = data->selectU64("assignment.data.estimate", 0);
  if (estimate <= 0)
    estimate = 0.2 * data->selectU64("assignment.data.timeout", 0);
  return estimate;
}


uint64_t Unit::getCurrentFrameTime() const {
  uint64_t now = Time::now();
  if (isFrameTimerRunning) return frameTime + now - lastTimeUpdate;
  return frameTime;
}


double Unit::getCurrentFrameProgress() const {
  double estimate = getRunTimeEstimate();
  double frameProgress = estimate ?  getCurrentFrameTime() / estimate: 0;
  if (0.01 < frameProgress) return 0.01;
  return frameProgress;
}


double Unit::getEstimatedProgress() const {
  if (isFinished()) return has("error") ? 0 : 1;
  if (getState() == UNIT_RUN)
    return getKnownProgress() + getCurrentFrameProgress();
  return 0;
}


double Unit::getCreditEstimate() const {
  uint64_t credit = data->selectU64("assignment.data.credit", 0);
  if (credit <= 0) return 0;

  double bonus = 1;
  double kFactor = 0.75;

  if (kFactor) {
    uint64_t timeAssigned = data->selectU64("assignment.data.assigned", 0);
    uint64_t timeout = data->selectU64("assignment.data.timeout", 0);
    uint64_t deadline = data->selectU64("assignment.data.deadline", 0);
    uint64_t delta = Time::now() - timeAssigned + getETA();

    if (delta && delta < timeout) {
      bonus = sqrt(kFactor * (double)deadline / delta);
      if (bonus < 1) bonus = 1;
    }
  }

  return bonus * credit;
}


uint64_t Unit::getETA() const {
  double progress = getEstimatedProgress();
  if (1.0 <= progress) return 0;

  return (uint64_t)(getRunTimeEstimate()*(1.0 - progress));
}


double Unit::getPPD() const {
  double estimate = getRunTimeEstimate();
  return estimate <= 0 ? 0 : getCreditEstimate() / estimate * Time::SEC_PER_DAY;
}


string Unit::getLogPrefix() const {return String::printf("WU%" PRIu64 ":", wu);}


string Unit::getWSBaseURL() const {
  string addr    = data->selectString("assignment.data.ws");
  uint32_t port  = data->selectU32("assignment.data.port", 443);
  string portStr = port == 443 ? "" : (":" + String(port));
  string scheme  = (port == 443 || port == 8084) ? "https" : "http";
  return scheme + "://" + addr + portStr + "/api";
}


uint64_t Unit::getDeadline() const {
  return Time(data->selectString("assignment.data.time")) +
    data->selectU64("assignment.data.deadline");
}


bool Unit::isFinished() const {
  switch (getState()) {
  case UNIT_UPLOAD:
  case UNIT_CLEAN:
  case UNIT_DONE:
    return true;

  default: return false;
  }
}


bool Unit::isExpired() const {
  switch (getState()) {
  case UNIT_ASSIGN:
  case UNIT_CLEAN:
  case UNIT_DONE:
    return false;

  default: return getDeadline() < Time::now();
  }
}


bool Unit::isIdling() const {
  return app.getConfig().getOnIdle() && !app.getOS().isSystemIdle();
}


void Unit::triggerNext(double secs) {if (!event->isPending()) event->add(secs);}


void Unit::triggerExit() {
  insert("run-time", runTime);
  insert("frameTime", getCurrentFrameTime());
  save();
}

void Unit::next() {
  // Check if WU has expired
  if (isExpired()) {
    LOG_INFO(1, "Unit expired, deleting");
    setState(UNIT_CLEAN);
  }

  // Pause/unpause WU
  if (isPaused()) {
    insert("pause-reason", getPauseReason());

    // Stop the process
    if (process.isSet() && process->isRunning())
      process->interrupt();

    // Stop the frameTimer
    if (isFrameTimerRunning) stopFrameTimer();

    if (getState() != UNIT_CLEAN) return;

  } else {
    if (!isFrameTimerRunning) startFrameTimer();
    if (hasString("pause-reason")) erase("pause-reason");
  }

  // Handle event backoff
  if (getState() != UNIT_DONE && isWaiting()) {
    uint64_t now = Time::now();
    return triggerNext(now < wait ? (wait - now) : 0);
  }

  try {
    switch (getState()) {
    case UNIT_ASSIGN:   assign();   break;
    case UNIT_DOWNLOAD: download(); break;
    case UNIT_CORE:     getCore();  break;
    case UNIT_RUN:      run();      break;
    case UNIT_UPLOAD:   upload();   break;
    case UNIT_CLEAN:    clean();    break;
    case UNIT_DONE:                 break;
    }

    return;
  } CATCH_ERROR;

  retry();
}


void Unit::startFrameTimer() {
  if (!isFrameTimerRunning) {
    isFrameTimerRunning = true;
    if (!startTime) frameTime = getU64("frameTime", 0);
    startTime = Time::now();
    lastUpdate = lastTimeUpdate = 0;
  }
}


void Unit::stopFrameTimer() {
  if (isFrameTimerRunning) {
    isFrameTimerRunning = false;
    if (lastTimeUpdate) {
      uint64_t now = Time::now();
      frameTime += now - lastTimeUpdate;
      lastTimeUpdate = now;
    }
  }
}


void Unit::updateFrameTimer(uint64_t frame, uint64_t total) {
  if (!isFrameTimerRunning) {
    LOG_ERROR("Frame timer not running, starting it");
    startFrameTimer();
    return;
  }
  if (!total || total < frame) return;
  uint64_t now = Time::now();

  // Check for start
  if (!lastUpdate) lastUpdate = lastTimeUpdate = now;
  else {
    // Detect and adjust for clock skew
    int64_t delta = (int64_t)now - lastUpdate;
    if (60 < delta) {
      LOG_WARNING("Detected clock skew (" << TimeInterval(delta)
                  << "), I/O delay, laptop hibernation or other slowdown "
                  "noted, adjusting time estimates");
      frameTime += lastUpdate - lastTimeUpdate;
      lastTimeUpdate = now;
    }
    lastUpdate = now;
  }

  if (currentFrame + 1 == frame) {
    uint64_t currentFrameTime = getCurrentFrameTime();
    if (currentFrameTime) runTime += currentFrameTime;
    currentFrame = frame;
    lastTimeUpdate = now;
    startTime = now;
    frameTime = 0;
  }
}


void Unit::setProgress(double complete, int total) {
  if (complete && total <= 0) return;

  double progress = complete ? (double)complete / total : 0;

  if (getNumber("progress", 0) != progress) {
    LOG_INFO(1, getState() << String::printf(" %0.2f%% ", progress * 100));

    insert("progress", progress);
  }
}


void Unit::getCore() {
  core = app.getCores().get(data->select("assignment.data.core"));

  auto cb =
    [this] (unsigned complete, int total) {
      setProgress(complete, total);

      if (core->isInvalid()) {
        LOG_INFO(1, "Failed to download core");
        dump();
        triggerNext();
      }

      if (core->isReady()) {
        // TODO handle dump correctly
        setState(
          (data->has("results") || data->has("dump")) ? UNIT_UPLOAD : UNIT_RUN);
        triggerNext();
      }
    };

  core->addProgressCallback(cb);
}


void Unit::run() {
  if (process.isSet()) return monitor();

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

  // Start the frameTimer
  startFrameTimer();

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
      knownProgress = info.done * 1.0 / info.total;
      updateFrameTimer(info.done, info.total);
      setProgress(getEstimatedProgress(), 1);
    }
  }
}


void Unit::readViewerTop() {
  string filename = getDirectory() + "/viewerTop.json";

  if (existsAndOlderThan(filename, 10)) {
    topology = JSON::Reader(filename).parse();
    frames.clear();
  }
}


void Unit::readViewerFrame() {
  string filename =
    getDirectory() + String::printf("/viewerFrame%d.json", viewerFrame);

  if (existsAndOlderThan(filename, 10)) {
    frames.push_back(JSON::Reader(filename).parse());
    insert("frames", (uint64_t)frames.size());
    viewerFrame++;
  }
}


void Unit::readResults() {
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

    setState(UNIT_UPLOAD);

  } else dump();
}


bool Unit::finalizeRun() {
  if (logCopier.isSet()) logCopier->join();
  logCopier.release();

  ExitCode code = (ExitCode::enum_t)process->wait();

  if (process->getWasKilled()) LOG_WARNING("Core was killed");
  if (process->getDumpedCore()) LOG_WARNING("Core crashed");
  if (code == ExitCode::FINISHED_UNIT) setProgress(1, 1);

  process.release();

  bool ok = code == ExitCode::FINISHED_UNIT || code == ExitCode::INTERRUPTED;
  LOG(CBANG_LOG_DOMAIN, ok ? LOG_INFO_LEVEL(1) : Logger::LEVEL_WARNING,
      "Core returned " << code << " (" << (unsigned)code << ')');

#ifdef _WIN32
  if (0xc000000 <= code)
    LOG_WARNING("Core exited with Windows unhandled exception code "
                << String::printf("0x%08x", (unsigned)code)
                << ".  See https://bit.ly/2CXgWkZ for more information.");
#endif

  return code != ExitCode::INTERRUPTED;
}


void Unit::monitor() {
  if (process->isRunning()) {
    TRY_CATCH_ERROR(readInfo());
    if (topology.isNull()) TRY_CATCH_ERROR(readViewerTop());
    else TRY_CATCH_ERROR(readViewerFrame());

    // Update ETA and PPD
    insert("eta", TimeInterval(getETA()).toString());
    insert("ppd", String::printf("%.0f", getPPD()));

    triggerNext(1);

  } else {
    if (finalizeRun()) {
      stopFrameTimer();
      readResults();
    }
    save();
    triggerNext();
  }
}


void Unit::dump() {
  LOG_DEBUG(3, "Dumping WU");

  // TODO Implement dumping with current WS
  setState(UNIT_CLEAN);
}


void Unit::clean() {
  LOG_DEBUG(3, "Cleaning WU");

  try {
    SystemUtilities::rmdir(getDirectory(), true);
    remove();
  } CATCH_ERROR;

  setState(UNIT_DONE);
  app.getUnits().unitComplete(success);
}


void Unit::retry() {
  if (++retries < 10) {
    double delay = pow(2, retries);
    wait = Time::now() + delay;
    LOG_INFO(1, "Retrying in " << TimeInterval(delay));

  } else {
    LOG_INFO(1, "Too many retries, failing WU");
    retries = 0;
    wait = 0;
    setState(UNIT_CLEAN);
  }

  insert("retries", retries);

  next();
}


void Unit::save() {
  JSON::BufferWriter writer;

  writer.beginDict();
  writer.insert("state", *this);
  if (data.isSet()) writer.insert("data", *data);
  writer.endDict();
  writer.flush();

  app.getDB("units").set(id, writer.toString());
}


void Unit::remove() {app.getDB("units").unset(id);}


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

  sink.beginDict();

  // Protect against replay
  // TODO apply measured AS time offset
  sink.insert("time",           Time().toString());
  sink.insert("wu",             getU64("wu"));

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

  // Compute resources
  sink.insertDict("resources");

  // CPU
  sink.insertDict("cpu");
  sink.insert("cpu",            app.getOS().getCPU());
  sink.insert("cpus",           getU32("cpus"));
  sink.insert("extended",       info.getCPUExtendedFeatures());
  sink.insert("vendor",         info.getCPUVendor());
  sink.insert("features",       info.getCPUFeatures());
  sink.insert("signature",      info.getCPUSignature());
  sink.insert("80000001",       info.getCPUFeatures80000001());

  // Project
  sink.insertDict("project");

  auto &config = app.getConfig();
  if (config.hasString("release"))
    sink.insert("release", config.getString("release"));
  if (config.hasString("cause"))
    sink.insert("cause", config.getString("cause"));
  if (getProjectKey()) sink.insert("key", getProjectKey());

  sink.endDict(); // project
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
  data = JSON::build([this] (JSON::Sink &sink) {writeRequest(sink);});
  string signature = app.getKey().signSHA256(data->toString());

  id = idFromSig(signature);
  insert("id", id);

  LOG_INFO(1, "Requesting WU assignment");
  LOG_DEBUG(3, *data);

  // TODO validate peer certificate
  URI uri = "https://" + app.getNextAS().toString() + "/api/assign";

  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
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
  save();
}


void Unit::download() {
  LOG_INFO(1, "Downloading WU");

  // Monitor download progress
  auto progressCB =
    [this] (const Progress &p) {setProgress(p.getTotal(), p.getSize());};

  URI uri = getWSBaseURL() + "/assign";
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                            this, &Unit::response);
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
  LOG_INFO(1, "Uploading WU results");

  // Monitor upload progress
  auto progressCB =
    [this] (const Progress &p) {setProgress(p.getTotal(), p.getSize());};

  URI uri = getWSBaseURL() + "/results";
  pr = app.getClient().call(uri, Event::RequestMethod::HTTP_POST,
                            this, &Unit::response);

  auto writer = pr->getJSONWriter();
  data->write(*writer);
  writer->close();

  setProgress(0, 0);
  pr->getConnection().getWriteProgress().setCallback(progressCB, 1);
  pr->send();
}


void Unit::response(Event::Request &req) {
  try {
    if (req.getConnectionError()) {
      LOG_ERROR("Failed response: " << req.getConnectionError());
      return retry();
    }

    if (!req.isOk()) {
      LOG_ERROR(req.getResponseCode() << ": " << req.getInput());

      try {
        auto msg = req.getJSONMessage();
        insert("error", msg->selectString("error.message"));
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
      default: THROW("Unexpected unit state " << getState());
      }
    }

    next();
    return;
  } CATCH_ERROR;

  retry();
}
