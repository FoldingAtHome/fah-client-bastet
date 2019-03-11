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

#include "App.h"
#include "Server.h"
#include "ComputeResources.h"

#include <cbang/Catch.h>
#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>
#include <cbang/time/Time.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/net/URI.h>
#include <cbang/json/Sink.h>
#include <cbang/Info.h>

#include <cbang/openssl/SSLContext.h>
#include <cbang/openssl/KeyGenPacifier.h>
#include <cbang/openssl/Digest.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;

namespace FAH {
  namespace Client {
    namespace BuildInfo {void addBuildInfo(const char *category);}
  }
}


App::App() :
  Application("Folding@home Client", App::_hasFeature), dns(base),
  client(base, dns, new SSLContext), server(new Server(*this)),
  resources(new ComputeResources(*this)) {

  // Info
  Client::BuildInfo::addBuildInfo(getName().c_str());

  // Configure commandline
  cmdLine.setAllowConfigAsFirstArg(true);
  cmdLine.setAllowPositionalArgs(false);

  // Options
  options.pushCategory("Client");
  options.add("open-web-control", "Make an operating system specific call to "
              "open the Web Control in a browser once client is fully loaded"
              )->setDefault(false);
  options.popCategory();

  // Note these options are available but hidden in non-debug builds
  options.pushCategory("Debugging");
  options.add("debug-libevent", "Enable verbose libevent debugging"
              )->setDefault(false);
  options.add("assignment-servers")
    ->setDefault("assign1.foldingathome.org assign2.foldingathome.org");
  options.popCategory();

  // Configure log
  options["verbosity"].setDefault(3);
  options["log"].setDefault("log.txt");
  options["log-no-info-header"].setDefault(true);
  options["log-thread-prefix"].setDefault(true);
  options["log-rotate-max"].setDefault(16);
  options["log-date-periodically"].setDefault(Time::SEC_PER_HOUR * 6);

  // Handle exit signal
  base.newSignal(SIGINT, this, &App::signalEvent)->add();
  base.newSignal(SIGTERM, this, &App::signalEvent)->add();
}


bool App::_hasFeature(int feature) {
  switch (feature) {
  case FEATURE_INFO: return true;
  case FEATURE_SIGNAL_HANDLER: return false;
  default: return Application::_hasFeature(feature);
  }
}


DB::NameValueTable &App::getDB(const string name) {
  auto it = tables.find(name);

  if (it == tables.end()) {
    SmartPointer<DB::NameValueTable> table = new DB::NameValueTable(db, name);
    table->create();
    table->init();
    it = tables.insert(tables_t::value_type(name, table)).first;
  }

  return *it->second;
}


const char *App::getCPU() const {
  // These are currently the only CPUs we support
  return sizeof(void *) == 4 ? "x86" : "amd64";
}


const char *App::getOS() const {
#ifdef _WIN32
  return "win32";

#elif __APPLE__
  return "macosx";

#else
  // OS Type
  string platform =
    String::toLower(Info::instance().get(getName(), "Platform"));

  // 'platform' is the string returned in Python by:
  //   os.platform.lower() + ' ' + platform.release()
  if (platform.find("linux")   != string::npos) return "linux";
  if (platform.find("freebsd") != string::npos) return "freebsd";
  if (platform.find("openbsd") != string::npos) return "openbsd";
  if (platform.find("netbsd")  != string::npos) return "netbsd";
  if (platform.find("solaris") != string::npos) return "solaris";
#endif

  return "unknown";
}


void App::writeSystemInfo(JSON::Sink &sink) {
  auto &info = SystemInfo::instance();

  sink.insert("version",     getVersion().toString());
  sink.insert("id",          getID());
  sink.insert("memory",      info.getFreeMemory());
  sink.insertBoolean("idle", false); // TODO get system idle state

  sink.insertDict("cpu");
  sink.insert("type",     getCPU());
  sink.insert("vendor",   info.getCPUVendor());
  sink.insert("cores",    info.getCPUCount());
  sink.insert("family",   info.getCPUFamily());
  sink.insert("model",    info.getCPUModel());
  sink.insert("stepping", info.getCPUStepping());
  sink.insert("extended", info.getCPUExtendedFeatures());
  sink.insert("features", info.getCPUFeatures());
  sink.insert("80000001", info.getCPUFeatures80000001());
  sink.endDict();

  sink.insertDict("os");
  sink.insert("type",      getOS());
  sink.insert("version",   info.getOSVersion().toString());
  sink.endDict();
}


void App::loadID() {
  auto &config = getDB("config");

  // Generate key
  if (!config.has("key")) {
    key.generateRSA(4096, 65537, new KeyGenPacifier("Generating RSA key"));
    config.set("key", key.privateToString());
  }

  key.readPrivate(config.getString("key"));

  // Generate ID from key
  Digest sha256("sha256");
  sha256.update(key.getPublic().toBinString());
  id = sha256.toBase64();

  LOG_INFO(3, "id = " << id);
}


void App::loadServers() {
  auto addresses = options["assignment-servers"].toStrings();

  if (addresses.empty()) THROW("No assignment servers");

  for (auto it = addresses.begin(); it != addresses.end(); it++)
    try {IPAddress::ipsFromString(*it, servers, 80);} CATCH_ERROR;

  if (servers.empty())
    THROW("Failed to find any assignment servers IP addresses");
}


void App::run() {
  // Libevent debugging
  if (options["debug-libevent"].toBoolean()) Event::Event::enableDebugLogging();

  // Open DB
  LOG_INFO(1, "Opening Database");
  db.open("client.db");

  // Initialize
  server->init();
  loadID();
  loadServers();

  // Open Web interface
  if (options["open-web-control"].toBoolean())
    SystemUtilities::openURI("https://console.foldingathome.org/");

  // Event loop
  base.dispatch();

  // Save
  resources->save();

  LOG_INFO(1, "Clean exit");
}


void App::signalEvent(Event::Event &e, int signal, unsigned flags) {
  base.loopExit();
}
