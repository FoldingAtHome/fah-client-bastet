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
#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/IOKitLib.h>


namespace FAH {
  namespace Client {
    class App;

    class OSXNotifications {
      static OSXNotifications *singleton;

      App &app;

      bool systemIsIdle;
      bool screensaverIsActive;
      bool screenIsLocked;
      bool loginwindowIsActive;

      io_service_t displayWrangler;
      IONotificationPortRef displayNotePort;
      CFRunLoopSourceRef displayNoteSource;
      io_object_t displayNotifier;

      SCDynamicStoreRef consoleUserDS;
      CFRunLoopSourceRef consoleUserRLS;
      CFStringRef consoleUser;

      CFRunLoopTimerRef updateTimer;

      int idleDelay;
      int currentDelay;
      int idleOnLoginwindowDelay;
      int displayPower;

    public:
      OSXNotifications(App &app);

      static OSXNotifications &instance() {return *singleton;}

      bool isSystemIdle() const {return systemIsIdle;}

      // init, update, shutdown must only be called from main thread
      void init();
      void update(); // normally does not return until app is quitting
      void shutdown();

      // private callbacks
      void consoleUserChanged(SCDynamicStoreRef store,
                              CFArrayRef changedKeys,
                              void *info);
      void displayPowerChanged(void *context, io_service_t service,
                               natural_t mtype, void *marg);
      void finishInit();
      void updateTimerFired(CFRunLoopTimerRef timer, void *info);

    protected:
      bool registerForConsoleUserNotifications();
      bool registerForDisplayPowerNotifications();
      void updateSystemIdle();
      void delayedUpdateSystemIdle(int delay = -1);
      void displayDidDim();
      void displayDidUndim();
      void displayDidSleep();
      void displayDidWake();
      int getCurrentDisplayPower();
    };
  }
}

#endif // __APPLE__
