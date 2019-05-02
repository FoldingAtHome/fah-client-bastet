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

#include "UnitState.h"

#include <cbang/json/Serializable.h>
#include <cbang/json/Value.h>

#include <cbang/event/Enum.h>
#include <cbang/event/Scheduler.h>
#include <cbang/event/Request.h>

#include <cbang/os/Subprocess.h>
#include <cbang/os/Thread.h>


namespace FAH {
  namespace Client {
    class App;
    class Slot;
    class Core;

    class Unit :
      public cb::Event::Scheduler<Unit>, public cb::Event::Enum,
      public UnitState::Enum {
      App &app;
      Slot &slot;
      cb::SmartPointer<Core> core;

      cb::JSON::ValuePtr data;
      cb::JSON::ValuePtr wuData;
      std::string id;
      UnitState state;
      unsigned currentAS = 0;

      double progress = 0;

      cb::SmartPointer<cb::Subprocess> process;
      cb::SmartPointer<cb::Thread> logCopier;

    public:
      Unit(App &app, Slot &slot);
      Unit(App &app, Slot &slot, const cb::JSON::ValuePtr &data);
      ~Unit();

      const std::string &getID() const {return id;}
      std::string getLogPrefix() const;
      std::string getDirectory() const {return "work/" + id;}
      UnitState getState() const {return state;}
      std::string getWSBaseURL() const;
      uint64_t getDeadline() const;
      bool isExpired() const;

      void next();

    protected:
      void getCore();
      void run();
      void monitor();
      void dump();
      void clean();

      void save();
      void remove();

      void assignNextAS();
      void assignResponse(const cb::JSON::ValuePtr &data);
      void assign();
      void downloadResponse(const cb::JSON::ValuePtr &data);
      void download();
      void uploadResponse(const cb::JSON::ValuePtr &data);
      void upload();
      void response(cb::Event::Request &req);
    };
  }
}
