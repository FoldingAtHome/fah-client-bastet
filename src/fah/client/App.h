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

#pragma once

#include <cbang/Application.h>

#include <cbang/event/Base.h>
#include <cbang/event/DNSBase.h>
#include <cbang/event/Client.h>

#include <cbang/db/Database.h>
#include <cbang/db/NameValueTable.h>

#include <cbang/openssl/KeyPair.h>
#include <cbang/openssl/Certificate.h>
#include <cbang/openssl/CertificateChain.h>

#include <cbang/net/IPAddress.h>
#include <cbang/enum/ProcessPriority.h>


namespace cb {
  namespace Event {class Event;}
  namespace JSON {class Sink;}
}


namespace FAH {
  namespace Client {
    class Server;
    class GPUResources;
    class Units;
    class Cores;
    class Config;

    class App : public cb::Application {
      cb::Event::Base base;
      cb::Event::DNSBase dns;
      cb::Event::Client client;

      cb::Certificate caCert;

      cb::DB::Database db;
      typedef std::map<std::string, cb::SmartPointer<cb::DB::NameValueTable> >
      tables_t;
      tables_t tables;

      cb::SmartPointer<Server> server;
      cb::SmartPointer<GPUResources> gpus;
      cb::SmartPointer<Units> units;
      cb::SmartPointer<Cores> cores;
      cb::SmartPointer<Config> config;
      cb::SmartPointer<cb::JSON::Value> info;

      cb::KeyPair key;
      std::vector<cb::IPAddress> servers;
      unsigned nextAS = 0;

    public:
      App();

      static bool _hasFeature(int feature);

      cb::Event::Base &getEventBase() {return base;}
      cb::Event::DNSBase &getEventDNS() {return dns;}
      cb::Event::Client &getClient() {return client;}

      cb::DB::NameValueTable &getDB(const std::string name);

      Server &getServer() {return *server;}
      GPUResources &getGPUs() {return *gpus;}
      Units &getUnits() {return *units;}
      Cores &getCores() {return *cores;}
      Config &getConfig() {return *config;}
      cb::JSON::Value &getInfo() const {return *info;}

      const cb::KeyPair &getKey() const {return key;}
      void validate(const cb::Certificate &cert,
                    const cb::Certificate &intermediate) const;
      void validate(const cb::Certificate &cert) const;
      bool hasFAHKeyUsage(const cb::Certificate &cert,
                          const std::string &usage) const;
      void check(const std::string &certificate,
                 const std::string &intermediate,
                 const std::string &signature, const std::string &hash,
                 const std::string &usage);
      void checkBase64SHA256(const std::string &certificate,
                             const std::string &intermediate,
                             const std::string &sig64, const std::string &data,
                             const std::string &usage);

      const cb::IPAddress &getNextAS();
      const char *getOS() const;
      const char *getCPU() const;

      void loadConfig();
      void loadServers();

      // From cb::Application
      void run();

      void signalEvent(cb::Event::Event &e, int signal, unsigned flags);
    };
  }
}
