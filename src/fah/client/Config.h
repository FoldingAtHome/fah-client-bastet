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

#pragma once

#include <cbang/json/Observable.h>
#include <cbang/config/Options.h>


namespace FAH {
  namespace Client {
    class App;

    class Config : public cb::JSON::ObservableDict {
      App &app;
      cb::JSON::ValuePtr defaults;

      typedef cb::JSON::ObservableDict Super_T;

    public:
      Config(App &app, const cb::JSON::ValuePtr &defaults);

      void load(const cb::JSON::Value &config);
      void load(const cb::Options &opts);

      void configure(const cb::JSON::Value &config);
      void setState(const cb::JSON::Value &msg);

      bool getOnIdle() const;
      bool getDifferentIdleResources() const;
      bool getOnBattery() const;
      bool getKeepAwake() const;
      void setPaused(bool paused);
      bool getPaused() const;
      bool getFinish() const;
      std::string getUsername() const;
      std::string getPasskey() const;
      uint32_t getTeam() const;
      uint64_t getProjectKey(const std::set<std::string> &gpus) const;
      bool getBeta(const std::set<std::string> &gpus) const;

      uint32_t getCPUs() const;
      uint32_t getCPUs(bool isIdle) const;
      std::set<std::string> getGPUs() const;
      std::set<std::string> getGPUs(bool isIdle) const;
      bool isGPUEnabled(const std::string &id) const;
      bool isGPUEnabled(const std::string &id, bool isIdle) const;
      bool isComputeDeviceEnabled(const std::string &type) const;
      void disableGPU(const std::string &id);

      // From JSON::Value
      cb::JSON::Value::iterator insert(const std::string &key,
        const cb::JSON::ValuePtr &value) override;
      using Super_T::insert;
    };
  }
}
