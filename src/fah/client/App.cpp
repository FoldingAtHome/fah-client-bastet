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

#include "App.h"
#include "Server.h"
#include "GPUResources.h"
#include "Units.h"
#include "Cores.h"
#include "Config.h"
#include "OS.h"

#include <cbang/Catch.h>
#include <cbang/event/Event.h>
#include <cbang/log/Logger.h>
#include <cbang/time/Time.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/os/SystemInfo.h>
#include <cbang/net/URI.h>
#include <cbang/json/Sink.h>
#include <cbang/Info.h>
#include <cbang/util/Resource.h>
#include <cbang/net/Base64.h>

#include <cbang/openssl/SSL.h>
#include <cbang/openssl/SSLContext.h>
#include <cbang/openssl/KeyGenPacifier.h>
#include <cbang/openssl/Digest.h>
#include <cbang/openssl/CertificateStore.h>
#include <cbang/openssl/CertificateStoreContext.h>

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
  Application("Folding@home Client", App::_hasFeature), base(true), dns(base),
  client(base, dns, new SSLContext), server(new Server(*this)),
  gpus(new GPUResources(*this)), units(new Units(*this)),
  cores(new Cores(*this)), config(new Config(*this)), os(OS::create(*this)),
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
              "client.  Only trused origins should be added.  Web pages at "
              "added origins will be able to control the client.")
    ->setDefault("https://console.foldingathome.org");
  options.add("web-root", "Path to files to be served by the client's Web "
              "server");
  // TODO this will not work on macOS or Windows
  options.add("ssl-ca-certificates", "Path to trusted SSL CA certificates file"
    )->setDefault("/etc/ssl/certs/ca-certificates.crt");
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

  // Configure log
  options["verbosity"].setDefault(3);
  options["log"].setDefault("log.txt");
  options["log-no-info-header"].setDefault(true);
  options["log-thread-prefix"].setDefault(true);
  options["log-short-level"].setDefault(true);
  options["log-rotate-max"].setDefault(16);
  options["log-date-periodically"].setDefault(Time::SEC_PER_DAY);

  // Handle exit signal
  base.newSignal(SIGINT, this, &App::signalEvent)->add();
  base.newSignal(SIGTERM, this, &App::signalEvent)->add();

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


const char *App::getCPU() const {
  // These are currently the only CPUs we support
  return sizeof(void *) == 4 ? "x86" : "amd64";
}


void App::loadConfig() {
  // Info
  info->insert("version", getVersion().toString());
  info->insert("os", getOS());
  info->insert("cpu", getCPU());
  info->insert("cpus", SystemInfo::instance().getCPUCount());
  info->insert("gpus", gpus);

  auto &config = getDB("config");

  // Generate key
  if (!config.has("key")) {
    key.generateRSA(4096, 65537, new KeyGenPacifier("Generating RSA key"));
    config.set("key", key.privateToString());
  }

  key.readPrivate(config.getString("key"));

  // Generate ID from key
  string id = Digest::base64(key.getPublic().toBinString(), "sha256");
  info->insert("id", id);
  LOG_INFO(3, "id = " << id);
}


void App::loadServers() {
  auto addresses = options["assignment-servers"].toStrings();

  if (addresses.empty()) THROW("No assignment servers");

  for (auto it = addresses.begin(); it != addresses.end(); it++)
    try {IPAddress::ipsFromString(*it, servers);} CATCH_ERROR;

  if (servers.empty())
    THROW("Failed to find any assignment servers IP addresses");
}


void App::run() {
  // Libevent debugging
  if (options["debug-libevent"].toBoolean()) Event::Event::enableDebugLogging();

  // Load root certs
  string caCertsFile = options["ssl-ca-certificates"];
  if (!caCertsFile.empty())
    client.getSSLContext()->loadVerifyLocationsFile(caCertsFile);

  // Open DB
  LOG_INFO(1, "Opening Database");
  db.open("client.db");

  // Initialize
  server->init();
  config->init();
  loadConfig();
  loadServers();

  // Connect JSON::Observables
  server->insert("units",  units);
  server->insert("config", config);
  server->insert("info",   info);
  // TODO Add access to log

  // Open Web interface
  if (options["open-web-control"].toBoolean())
    SystemUtilities::openURI("https://console.foldingathome.org/");

  // Event loop
  base.dispatch();

  LOG_INFO(1, "Clean exit");
}


void App::signalEvent(Event::Event &e, int signal, unsigned flags) {
  base.loopExit();
}
