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

#include "App.h"
#include "Server.h"
#include "GPUResources.h"
#include "Units.h"
#include "Cores.h"
#include "Config.h"
#include "OS.h"
#include "ResourceGroup.h"
#include "PasskeyConstraint.h"
#include "CausePref.h"

#include <cbang/Catch.h>
#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>
#include <cbang/time/Time.h>
#include <cbang/json/Sink.h>
#include <cbang/util/Resource.h>

#include <cbang/net/URI.h>
#include <cbang/net/Base64.h>

#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/os/SignalManager.h> // For SIGHUP on Windows

#include <cbang/openssl/SSL.h>
#include <cbang/openssl/SSLContext.h>
#include <cbang/openssl/KeyGenPacifier.h>
#include <cbang/openssl/Digest.h>
#include <cbang/openssl/CertificateStore.h>
#include <cbang/openssl/CertificateStoreContext.h>

#include <cbang/config/MinMaxConstraint.h>
#include <cbang/config/MinConstraint.h>
#include <cbang/config/EnumConstraint.h>

#include <set>

#include <signal.h>


using namespace FAH::Client;
using namespace cb;
using namespace std;

namespace FAH {
  namespace Client {
    namespace BuildInfo {void addBuildInfo(const char *category);}
    extern const DirectoryResource resource0;
  }
}


App::App() :
  Application("Folding@home Client", App::_hasFeature), base(true, 10),
  dns(base), client(base, dns, new SSLContext), server(new Server(*this)),
  gpus(new GPUResources(*this)), cores(new Cores(*this)), os(OS::create(*this)),
  info(new JSON::Dict) {

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
  options.add("allowed-origins", "Web origins (URLs) allowed to access this "
              "client.  Only trusted origins should be added.  Web pages at "
              "added origins will be able to control the client.")
    ->setDefault("https://app.foldingathome.org");
  options.add("web-root", "Path to files to be served by the client's Web "
              "server");
  options.popCategory();

  // Note these options are available but hidden in non-debug builds
  options.pushCategory("Debugging");
  options.add("debug-libevent", "Enable verbose libevent debugging"
              )->setDefault(false);
  options.add("assignment-servers")
    ->setDefault("assign1.foldingathome.org assign2.foldingathome.org "
                 "assign3.foldingathome.org assign4.foldingathome.org "
                 "assign5.foldingathome.org assign6.foldingathome.org");
  options.popCategory();

  SmartPointer<Option> opt;
  options.pushCategory("User Information");
  options.add("user", "Your user name.")->setDefault("Anonymous");
  options.add("team", "Your team number.",
              new MinMaxConstraint<int32_t>(0, 2147483647))->setDefault(0);
  opt = options.add("passkey", "Your passkey.", new PasskeyConstraint);
  opt->setDefault("");
  opt->setObscured();
  options.popCategory();

  options.pushCategory("Project Settings");
  options.add("project-key", "Key for access to restricted testing projects."
              )->setDefault(0);
  options.alias("project-key", "key");
  options.add("cause", "The cause you prefer to support.",
              new EnumConstraint<CausePref>)->setDefault("any");
  options.popCategory();

  options.pushCategory("Resource Settings");
  options.add("cpus", "Number of cpus FAH client will use.",
              new MaxConstraint<int32_t>(SystemInfo::instance().getCPUCount()));
  options.popCategory();

  // Configure log
  options["verbosity"].setDefault(3);
  options["log"].setDefault("log.txt");
  options["log-no-info-header"].setDefault(true);
  options["log-thread-prefix"].setDefault(true);
  options["log-short-level"].setDefault(true);
  options["log-rotate-max"].setDefault(16);
  options["log-date-periodically"].setDefault(Time::SEC_PER_DAY);

  // Handle exit signal
  base.newSignal(SIGINT,  this, &App::signalEvent)->add();
  base.newSignal(SIGTERM, this, &App::signalEvent)->add();

  // Network timeout
  client.setReadTimeout(45);
  client.setWriteTimeout(45);

  // Ignore SIGPIPE & SIGHUP
#ifndef _WIN32
  ::signal(SIGPIPE, SIG_IGN);
#endif
  ::signal(SIGHUP, SIG_IGN);

  // Add custom certificate extension
  SSL::createObject("1.2.3.4.70.64.72", "fahKeyUsage",
                    "Folding@home Key Usage");
  Certificate::addExtensionAlias("fahKeyUsage", "nsComment");

  // Load Certificate Authority
  const Resource *caRes = FAH::Client::resource0.find("ca.pem");
  if (!caRes) THROW("Could not find root certificate resource");
  caCert = caRes->toString();

  // TODO get CRL from F@H periodically
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


const SmartPointer<ResourceGroup> &App::newGroup(const string &name) {
  if (shouldQuit()) THROW("Shutting down");

  auto &db = getDB("groups");
  JSON::ValuePtr config;

  if (db.has(name)) config = db.getJSON(name);
  else if (db.has("")) {
    config = db.getJSON("");
    config->insert("cpus", 0);
    config->erase("gpus");
    config->erase("peers");

  } else config = new JSON::Dict;

  return groups[name] =
    new ResourceGroup(*this, name, config, info->copy(true));
}


const SmartPointer<ResourceGroup> &App::getGroup(const string &name) const {
  auto it = groups.find(name);
  if (it == groups.end()) THROW("Group '" << name << "' not found");
  return it->second;
}


void App::saveGroup(const ResourceGroup &group) {
  getDB("groups").set(group.getName(), *group.getConfig());
}


void App::updateGroups() {
  std::set<string> peers;
  auto &root   = *getGroup("");
  auto &config = *root.getConfig();

  // Add new groups
  if (config.hasList("peers")) {
    auto &list = *config.get("peers");

    for (unsigned i = 0; i < list.size(); i++) {
      string name = list.getString(i);

      if (!name.empty() && name[0] == '/') {
        if (groups.find(name) == groups.end()) newGroup(name);
        peers.insert(name);
      }
    }
  }

  auto db = getDB("groups");

  // Remove deleted groups
  for (auto it = groups.begin(); it != groups.end();) {
    const string &name = it->first;

    if (!name.empty() && name[0] == '/' && peers.find(name) == peers.end()) {
      // Dump any WUs and move to root group
      auto &units = *it->second->getUnits();
      while (units.size()) {
        auto unit = units.removeUnit(0);
        root.getUnits()->add(unit);
        unit->dumpWU();
      }

      it = groups.erase(it);
      db.unset(name);

    } else it++;
  }
}


void App::updateResources() {
  // Determine which GPUs and CPUs are already in use
  map<string, string> gpuUsedBy;
  uint32_t availableCPUs = info->getU32("cpus");
  int32_t  remainingCPUs = availableCPUs;

  for (auto group: groups) {
    auto &config = *group.second->getConfig();
    remainingCPUs -= config.getCPUs();

    for (unsigned i = 0; i < gpus->size(); i++) {
      string gpuID = gpus->keyAt(i);

      if (config.isGPUEnabled(gpuID) &&
          gpuUsedBy.find(gpuID) == gpuUsedBy.end())
        gpuUsedBy[gpuID] = group.second->getName();
    }
  }

  if (remainingCPUs < 0) remainingCPUs = 0; // Check for over-allocation

  // Set group GPUs and CPUs
  for (auto group: groups) {
    auto  &config = *group.second->getConfig();
    auto     info = group.second->get("info");
    uint32_t cpus = min(config.getCPUs(), availableCPUs);

    info->insert("cpus", cpus + remainingCPUs);
    availableCPUs -= cpus;
    if (cpus < config.getCPUs()) config.insert("cpus", cpus);

    JSON::ValuePtr groupGPUs = new JSON::Dict;

    for (unsigned i = 0; i < gpus->size(); i++) {
      string gpuID = gpus->keyAt(i);

      auto it = gpuUsedBy.find(gpuID);
      if (it == gpuUsedBy.end() || it->second == group.second->getName())
        groupGPUs->insert(gpuID, gpus->get(i)->copy(true));
    }

    if (*info->get("gpus") != *groupGPUs) info->insert("gpus", groupGPUs);
  }
}


void App::triggerUpdate() {
  for (auto group: groups)
    group.second->getUnits()->triggerUpdate(true);
}


bool App::isActive() const {
  for (auto group: groups)
    if (group.second->getUnits()->isActive())
      return true;

  return false;
}


bool App::hasFailure() const {
  for (auto group: groups)
    if (group.second->getUnits()->hasFailure())
      return true;

  return false;
}


void App::setPaused(bool paused) {
  for (auto group: groups)
    group.second->getConfig()->setPaused(paused);

  triggerUpdate();
}


bool App::getPaused() const {
  for (auto group: groups)
    if (!group.second->getConfig()->getPaused())
      return false;

  return true;
}


void App::validate(const Certificate &cert,
                   const Certificate &intermediate) const {
  CertificateChain chain;
  chain.add(intermediate);
  CertificateStore store;
  store.add(caCert);
  CertificateStoreContext(store, cert, chain).verify();
}


void App::validate(const Certificate &cert) const {validate(cert, cert);}


bool App::hasFAHKeyUsage(const Certificate &cert, const string &usage) const {
  vector<string> tokens;
  String::tokenize(cert.getExtension("fahKeyUsage", ""), tokens);

  for (unsigned i = 0; i < tokens.size(); i++)
    if (tokens[i] == usage) return true;

  return false;
}


void App::check(const string &certificate, const string &intermediate,
                const string &signature, const string &hash,
                const string &usage) {
  // Check certificate
  Certificate cert(certificate);
  if (intermediate.empty()) validate(cert);
  else validate(cert, intermediate);

  // Check FAH key usage
  vector<string> tokens;
  String::tokenize(usage, tokens, "|");
  bool valid = false;
  for (unsigned i = 0; i < tokens.size() && !valid; i++)
    if (hasFAHKeyUsage(cert, tokens[i])) valid = true;
  if (!valid) THROW("Certificate not valid for F@H key usage " << usage);

  // Check signature
  cert.getPublicKey()->verify(signature, hash);
}


void App::checkBase64SHA256(const string &certificate,
                            const string &intermediate,
                            const string &sig64, const string &data,
                            const string &usage) {
  check(certificate, intermediate, Base64().decode(sig64),
        Digest::hash(data, "sha256"), usage);
}


const IPAddress &App::getNextAS() {
  if (servers.empty()) THROW("No AS");
  if (servers.size() <= nextAS) nextAS = 0;
  return servers[nextAS++];
}


uint64_t App::getNextWUID() {
  auto &db = getDB("config");
  uint64_t id = db.getInteger("wus", 0);
  db.set("wus", id + 1);
  return id + 1;
}


void App::upgradeDB() {
  auto &configDB = getDB("config");
  auto &groupsDB = getDB("groups");

  if (!groupsDB.has("") && configDB.has("config"))
    groupsDB.set("", configDB.getString("config"));
}


void App::loadConfig() {
  // Info
  info->insert("version", getVersion().toString());
  info->insert("os",      os->getName());
  info->insert("cpu",     os->getCPU());
  info->insert("cpus",    SystemInfo::instance().getCPUCount());
  info->insert("gpus",    gpus);

  auto &configDB = getDB("config");

  // Generate key
  if (!configDB.has("key")) {
    key.generateRSA(4096, 65537, new KeyGenPacifier("Generating RSA key"));
    configDB.set("key", key.privateToString());
  }

  key.readPrivate(configDB.getString("key"));

  // Generate ID from key
  string id = Digest::base64(key.getPublic().toBinString(), "sha256");
  info->insert("id", id);
  LOG_INFO(3, "id = " << id);
}


void App::loadServers() {
  auto addresses = options["assignment-servers"].toStrings();
  if (addresses.empty()) THROW("No assignment servers");

  for (auto address: addresses)
    try {IPAddress::ipsFromString(address, servers);} CATCH_ERROR;

  if (servers.empty())
    THROW("Failed to find any assignment server IP addresses");
}


void App::loadGroups() {
  newGroup("");
  updateGroups();
}


void App::loadUnits() {
  unsigned count = 0;

  getDB("units").foreach(
    [this, &count] (const string &id, const string &dataStr) {
      try {
        auto data  = JSON::Reader::parseString(dataStr);
        auto group = data->selectString("state.group", "");
        auto wu    = data->selectU64("state.number");
        bool dump  = groups.find(group) == groups.end();

        if (dump) group = "";

        LOG_INFO(3, "Loading work unit " << wu << " to group '" << group
                 << "' with ID " << id);

        SmartPointer<Unit> unit = new Unit(*this, data);
        getGroup(group)->getUnits()->add(unit);
        if (dump) unit->dumpWU();
        count++;
      } CATCH_ERROR;
    });

  LOG_INFO(3, "Loaded " << count << " wus.");
}


void App::run() {
  // Libevent debugging
  if (options["debug-libevent"].toBoolean()) Event::Event::enableDebugLogging();

  // Load root certs
  client.getSSLContext()->loadSystemRootCerts();

  // Open DB
  LOG_INFO(1, "Opening Database");
  db.open("client.db");

  // Initialize
  upgradeDB();
  server->init();
  loadConfig();
  loadServers();
  loadGroups();
  loadUnits();

  // Open Web interface
  if (options["open-web-control"].toBoolean())
    SystemUtilities::openURI("https://app.foldingathome.org/");

  // Event loop
  os->dispatch();

  LOG_INFO(1, "Clean exit");
}


void App::requestExit() {
  SmartPointer<unsigned> count = new unsigned(groups.size());
  auto cb = [count, this]() {if (!--*count) base.loopExit();};

  for (auto group: groups)
    group.second->getUnits()->shutdown(cb);

  Application::requestExit();
}


void App::signalEvent(Event::Event &, int, unsigned) {requestExit();}
