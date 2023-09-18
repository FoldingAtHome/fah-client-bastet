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
#include <cbang/json/Observable.h>

#include <map>


namespace cb {
  namespace Event {class Event;}
  namespace JSON  {class Sink;}
}


namespace FAH {
  namespace Client {
    class Server;
    class Account;
    class GPUResources;
    class Units;
    class Cores;
    class Config;
    class OS;
    class Remote;

    class App :
      public cb::Application,
      public cb::Event::Enum,
      public cb::JSON::ObservableDict {

      cb::Event::Base    base;
      cb::Event::DNSBase dns;
      cb::Event::Client  client;
      cb::KeyPair        key;

      cb::Certificate caCert;

      cb::DB::Database db;
      typedef std::map<std::string, cb::SmartPointer<cb::DB::NameValueTable> >
      tables_t;
      tables_t tables;

      cb::SmartPointer<Server>       server;
      cb::SmartPointer<Account>      account;
      cb::SmartPointer<GPUResources> gpus;
      cb::SmartPointer<Cores>        cores;
      cb::SmartPointer<OS>           os;
      cb::SmartPointer<Config>       config;
      cb::SmartPointer<Units>        units;

      std::list<cb::SmartPointer<Remote>> remotes;

      unsigned nextAS = 0;

    public:
      App();

      static bool _hasFeature(int feature);

      cb::Event::Base    &getEventBase() {return base;}
      cb::Event::DNSBase &getEventDNS()  {return dns;}
      cb::Event::Client  &getClient()    {return client;}
      cb::KeyPair        &getKey()       {return key;}

      cb::DB::NameValueTable &getDB(const std::string name);

      Server             &getServer()    {return *server;}
      Account            &getAccount()   {return *account;}
      GPUResources       &getGPUs()      {return *gpus;}
      Cores              &getCores()     {return *cores;}
      OS                 &getOS()        {return *os;}
      Config             &getConfig()    {return *config;}
      Units              &getUnits()     {return *units;}

      std::string getID() const {return selectString("info.id");}
      std::string getPubKey() const;

      void add(const cb::SmartPointer<Remote> &remote);
      void remove(Remote &remote);

      void triggerUpdate();
      bool isActive() const;
      bool hasFailure() const;
      void setPaused(bool paused);
      bool getPaused() const;

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

      std::string getNextAS();
      uint64_t getNextWUID();

      bool validateChange(const cb::JSON::Value &msg);

      void upgradeDB();
      void loadConfig();
      void loadUnits();

      // From cb::Application
      int init(int argc, char *argv[]);
      void run();
      void requestExit();

      // From cb::JSON::Value
      void notify(std::list<cb::JSON::ValuePtr> &change);

      void signalEvent(cb::Event::Event &e, int signal, unsigned flags);
    };
  }
}
