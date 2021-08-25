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

#include <fah/client/OS.h>

#include <cbang/os/Thread.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/IOKitLib.h>


namespace FAH {
  namespace Client {
    class App;

    class OSXOSImpl : public OS, public cb::Thread {
      static OSXOSImpl *singleton;

      bool systemIsIdle = false;
      bool screensaverIsActive = false;
      bool screenIsLocked = false;
      bool loginwindowIsActive = false;

      io_service_t displayWrangler = 0;
      IONotificationPortRef displayNotePort = 0;
      CFRunLoopSourceRef displayNoteSource = 0;
      io_object_t displayNotifier = 0;

      SCDynamicStoreRef consoleUserDS = 0;
      CFRunLoopSourceRef consoleUserRLS = 0;
      CFStringRef consoleUser = 0;

      CFRunLoopRef threadRunLoop = 0;
      CFRunLoopTimerRef updateTimer = 0;

      int idleDelay = 5;
      int currentDelay = 0;
      int idleOnLoginwindowDelay = 30;
      int displayPower = 0;

    public:
      OSXOSImpl(App &app);
      ~OSXOSImpl();

      static OSXOSImpl &instance();

      // From OS
      bool isSystemIdle() const {return systemIsIdle;}
      void dispatch();

      // From cb::Thread
      void run();
      void stop();

      // Callbacks
      void consoleUserChanged
      (SCDynamicStoreRef store, CFArrayRef changedKeys, void *info);
      void displayPowerChanged
      (void *context, io_service_t service, natural_t mtype, void *marg);
      void finishInit();
      void updateTimerFired(CFRunLoopTimerRef timer, void *info);

    protected:
      void init();
      void initialize();
      void addHeartbeatTimerToRunLoop(CFRunLoopRef loop);
      bool registerForConsoleUserNotifications();
      bool registerForDarwinNotifications();
      bool registerForDisplayPowerNotifications();
      bool registerForLaunchEvents();
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
