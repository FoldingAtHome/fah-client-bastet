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


namespace cb {
  namespace Event {class Event;}
  namespace JSON {class Sink;}
}


namespace FAH {
  namespace Client {
    class Server;
    class Slots;
    class Cores;

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
      cb::SmartPointer<Slots> slots;
      cb::SmartPointer<Cores> cores;

      cb::KeyPair key;
      std::string id;
      std::vector<cb::IPAddress> servers;

    public:
      App();

      static bool _hasFeature(int feature);

      cb::Event::Base &getEventBase() {return base;}
      cb::Event::DNSBase &getEventDNS() {return dns;}
      cb::Event::Client &getClient() {return client;}

      cb::DB::NameValueTable &getDB(const std::string name);

      Server &getServer() {return *server;}
      Slots &getSlots() {return *slots;}
      Cores &getCores() {return *cores;}

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

      const std::string &getID() const {return id;}
      const std::vector<cb::IPAddress> &getServers() const {return servers;}

      const char *getCPU() const;
      const char *getOS() const;

      void writeSystemInfo(cb::JSON::Sink &sink);

      void loadID();
      void loadServers();

      // From cb::Application
      void run();

      void signalEvent(cb::Event::Event &e, int signal, unsigned flags);
    };
  }
}
