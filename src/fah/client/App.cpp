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

#include "App.h"
#include "Server.h"
#include "Account.h"
#include "GPUResources.h"
#include "Groups.h"
#include "Units.h"
#include "Cores.h"
#include "Config.h"
#include "OS.h"
#include "PasskeyConstraint.h"
#include "Remote.h"

#include <cbang/Catch.h>
#include <cbang/Info.h>
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
#include <cbang/os/CPUInfo.h>

#include <cbang/openssl/SSL.h>
#include <cbang/openssl/SSLContext.h>
#include <cbang/openssl/KeyGenPacifier.h>
#include <cbang/openssl/Digest.h>
#include <cbang/openssl/CertificateStore.h>
#include <cbang/openssl/CertificateStoreContext.h>

#include <cbang/config/MinMaxConstraint.h>
#include <cbang/config/MinConstraint.h>

#include <set>
#include <csignal>


using namespace FAH::Client;
using namespace cb;
using namespace std;

namespace FAH {
  namespace Client {
    namespace BuildInfo {void addBuildInfo(const char *category);}
    extern const DirectoryResource resource0;
  }
}


namespace {
  unsigned getDefaultCPUs() {
    unsigned count  = SystemInfo::instance().getCPUCount();
    unsigned pcount = SystemInfo::instance().getPerformanceCPUCount();

    if (1 < count) count--; // Reserve one CPU by default

    return (pcount && pcount < count) ? pcount : count;
  }
}


App::App() :
  Application("Folding@home Client", App::_hasFeature), base(true, 10),
  dns(base), client(base, dns, new SSLContext), server(new Server(*this)),
  account(new Account(*this)), gpus(new GPUResources(*this)),
  cores(new Cores(*this)),
  saveConfigEvent(base.newEvent(this, &App::saveGlobalConfig, 0)) {

  // Info
  Client::BuildInfo::addBuildInfo(getName().c_str());
  string url = Info::instance().get(getName(), "URL");

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
    ->setDefault(url + " http://localhost:7396");
  options.add("web-root", "Path to files to be served by the client's Web "
              "server")->setDefault("fah-web-control/dist");
  options.add("on-idle", "Folding only when idle.")->setDefault(false);
  options.popCategory();

  // Note these options are available but hidden in non-debug builds
  options.pushCategory("Debugging");
  options.add("debug-libevent", "Enable verbose libevent debugging"
              )->setDefault(false);
  options.add("assignment-servers")
    ->setDefault("assign1.foldingathome.org assign2.foldingathome.org "
                 "assign3.foldingathome.org assign4.foldingathome.org "
                 "assign5.foldingathome.org assign6.foldingathome.org");
  options.add("api-server")->setDefault("https://api.foldingathome.org");
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
  options.add("cause", "The cause you prefer to support.")->setDefault("any");
  options.add("beta", "Accept beta projects.")->setDefault(false);
  options.popCategory();

  options.pushCategory("Resource Settings");
  options.add("cpus", "Number of cpus FAH client will use.",
              new MaxConstraint<int32_t>(SystemInfo::instance().getCPUCount())
    )->setDefault(getDefaultCPUs());

  options.popCategory();

  options["allow"].setDefault("127.0.0.1");
  options["deny"].setDefault("0/0");

  // Configure log
  options["verbosity"].setDefault(3);
  options["log"].setDefault("log.txt");
  options["log-no-info-header"].setDefault(true);
  options["log-thread-prefix"].setDefault(true);
  options["log-short-level"].setDefault(true);
  options["log-rotate-max"].setDefault(90);
  options["log-rotate-period"].setDefault(Time::SEC_PER_DAY);

  // Handle exit signal
  base.newSignal(SIGINT,  this, &App::signalEvent)->add();
  base.newSignal(SIGTERM, this, &App::signalEvent)->add();

  // Network timeout
  client.setReadTimeout(60);
  client.setWriteTimeout(60);

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


SmartPointer<Groups> App::getGroups() const {
  return get("groups").cast<Groups>();
}


SmartPointer<Config> App::getConfig() const {
  return get("config").cast<Config>();
}


SmartPointer<Units> App::getUnits() const {
  return get("units").cast<Units>();
}


void App::configure(const JSON::Value &msg) {
  if (!validateChange(msg)) return;

  if (msg.hasDict("config")) {
    auto config = msg.get("config");

    if (config->hasDict("groups"))
      getGroups()->configure(*config->get("groups"));

    getConfig()->configure(*config);
  }

  triggerUpdate();
}


string App::getURL() const {return Info::instance().get(getName(), "URL");}
string App::getPubKey() const {return Base64().encode(key.publicToSPKI());}


void App::add(const SmartPointer<Remote> &remote) {
  LOG_DEBUG(3, "Adding remote " << remote->getName());
  remotes.push_back(remote);
}


void App::remove(Remote &remote) {
  LOG_DEBUG(3, "Removing remote " << remote.getName());
  remotes.remove(SmartPointer<Remote>::Phony(&remote));
}


void App::triggerUpdate() {getGroups()->triggerUpdate();}
bool App::isActive()   const {return getUnits()->isActive();}
bool App::hasFailure() const {return getUnits()->hasFailure();}


void App::setState(const JSON::Value &msg) {
  if (!validateChange(msg)) return;
  getGroups()->setState(msg);
}


void App::setState(const string &state) {
  auto msg = createDict();
  msg->insert("cmd", "state");
  msg->insert("state", state);
  setState(*msg);
}


bool App::getPaused() const {return getGroups()->getPaused();}
bool App::keepAwake() const {return getGroups()->keepAwake();}


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
  cert.getPublicKey().verify(signature, hash);
}


void App::checkBase64SHA256(
  const string &certificate, const string &intermediate,
  const string &sig64, const string &data, const string &usage) {
  check(certificate, intermediate, Base64().decode(sig64),
        Digest::hash(data, "sha256"), usage);
}


string App::getNextAS() {
  auto servers = options["assignment-servers"].toStrings();
  if (servers.empty()) THROW("No assignment servers");
  if (servers.size() <= nextAS) nextAS = 0;
  return servers[nextAS++];
}


uint64_t App::getNextWUID() {
  auto &db = getDB("config");
  uint64_t id = db.getInteger("wus", 0);
  db.set("wus", id + 1);
  return id + 1;
}


bool App::validateChange(const JSON::Value &msg) {
  string cmd  = msg.getString("cmd");
  string time = msg.getString("time", Time().toString());
  string key  = "change-time-" + cmd;
  auto &db    = getDB("config");

  if (db.has(key) && Time::parse(time) <= Time::parse(db.getString(key)))
    return false; // outdated

  // Save change time
  db.set(key, time);

  return true;
}


void App::upgradeDB() {
  auto &db     = getDB("config");
  auto version = db.getInteger("version", 0);

  if (db.has("config") && (version == 1 || !getDB("groups").has("")))
    getDB("groups").set("", db.getString("config"));

  if (version < 2) db.set("version", 2);
}


void App::loadConfig() {
  // Info
  auto &sysInfo = SystemInfo::instance();

  insertDict("info");
  auto d = get("info");
  d->insert("version",     getVersion().toString());
  d->insert("url",         getURL());
  d->insert("os",          os->getName());
  d->insert("os_version",  sysInfo.getOSVersion().toString());
  d->insert("cpu",         os->getCPU());
  d->insert("cpu_brand",   CPUInfo::create()->getBrand());
  d->insert("cpus",        sysInfo.getCPUCount());
  d->insert("gpus",        gpus);
  d->insert("mach_name",   account->getMachName());
  try {
    d->insert("hostname",  sysInfo.getHostname());
  } CATCH_WARNING;

  Info &info = Info::instance();
  d->insert("mode",               info.get(getName(), "Mode"));
  d->insert("revision",           info.get(getName(), "Revision"));
  d->insertBoolean("has_battery", info.get("System",  "Has Battery") == "true");
  d->insertBoolean("on_battery",  info.get("System",  "On Battery")  == "true");

  auto &db = getDB("config");

  // Generate key
  if (!db.has("key")) {
    key.generateRSA(4096, 65537, new KeyGenPacifier("Generating RSA key"));
    db.set("key", key.privateToPEMString());
  }

  key.readPrivatePEM(db.getString("key"));

  // Generate ID from key
  string id = Digest::urlBase64(key.getRSA_N().toBinString(), "sha256");
  d->insert("id", id);
  LOG_INFO(1, "F@H ID = " << id);

  // Global config
  auto &r       = FAH::Client::resource0.get("global.json");
  auto defaults = JSON::Reader::parseString(r.toString());
  SmartPointer<Config> config = new Config(*this, defaults);

  config->load(getOptions());
  if (db.has("config")) config->load(*db.getJSON("config"));

  insert("config", config);
}


int App::init(int argc, char *argv[]) {
  int ret = Application::init(argc, argv);
  os = OS::create(*this);
  return ret;
}


void App::run() {
  // Libevent debugging
  if (options["debug-libevent"].toBoolean()) Event::Event::enableDebugLogging();

  // Load root certs
  client.getSSLContext()->loadSystemRootCerts();

  // Open DB
  LOG_INFO(1, "Opening Database");
  db.open("client.db");

  // Check that we have AS
  if (options["assignment-servers"].toStrings().empty())
    THROW("No assignment servers");

  // Initialize
  upgradeDB();
  loadConfig();
  insert("groups", new Groups(*this));
  server->init();
  account->init();
  insert("units", new Units(*this));

  // Open Web interface
  if (options["open-web-control"].toBoolean())
    SystemUtilities::openURI(Info::instance().get(getName(), "URL"));

  // Event loop
  os->dispatch();

  // Dealocate
  clear();

  LOG_INFO(1, "Clean exit");
}


void App::requestExit() {
  auto &groups = *getGroups();
  SmartPointer<unsigned> count = new unsigned(groups.size());

  auto cb = [this, count]() {if (!--*count) base.loopExit();};

  for (auto &name: groups.keys())
    groups.getGroup(name).shutdown(cb);

  Application::requestExit();
}


void App::notify(const list<JSON::ValuePtr> &change) {
  if (remotes.empty() || shouldQuit()) return; // Avoid many calls during init

  // Automatically save changes to config
  bool isConfig = 2 < change.size() && change.front()->getString() == "config";
  if (isConfig) saveConfigEvent->activate();

  auto changes = SmartPtr(new JSON::List(change.begin(), change.end()));
  LOG_DEBUG(5, __func__ << ' ' << *changes);

  for (auto &remote: remotes)
    remote->sendChanges(changes);
}


void App::signalEvent(Event::Event &, int, unsigned) {requestExit();}


void App::saveGlobalConfig() {
  getDB("config").set("config", getConfig()->toString());
}
