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

#include <cbang/json/JSON.h>
#include <cbang/event/Event.h>


namespace FAH {
  namespace Client {
    class App;

    class Remote {
      App &app;

      std::string vizUnitID;
      unsigned vizFrame = 0;

      bool followLog = false;
      std::streamoff logOffset = 0;
      cb::SmartPointer<std::iostream> log;
      cb::SmartPointer<cb::Event::Event> logEvent;

    public:
      Remote(App &app);
      virtual ~Remote();

      App &getApp() const {return app;}

      virtual std::string getName() const = 0;
      virtual void send(const cb::JSON::ValuePtr &msg) = 0;
      virtual void close() = 0;

      void sendViz();
      void readLogToNextLine();
      void sendLog();
      void sendChanges(const cb::JSON::ValuePtr &changes);

      void onMessage(const cb::JSON::ValuePtr &msg);
      void onOpen();
      void onComplete();
    };
  }
}
