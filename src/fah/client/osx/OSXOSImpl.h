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

#include <fah/client/OS.h>

#include <cbang/thread/Thread.h>
#include <cbang/os/osx/MacOSString.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/IOKitLib.h>
#include <atomic>


namespace FAH {
  namespace Client {
    class App;

    class OSXOSImpl : public OS, public cb::Thread {
      static OSXOSImpl *singleton;

      std::atomic<bool> systemIsIdle = false;
      bool screensaverIsActive       = false;
      bool screenIsLocked            = false;
      bool loginwindowIsActive       = false;

      io_service_t displayWrangler = 0;
      io_object_t  displayNotifier = 0;
      IONotificationPortRef displayNotePort   = 0;
      CFRunLoopSourceRef    displayNoteSource = 0;

      cb::MacOSRef<SCDynamicStoreRef>  consoleUserDS;
      cb::MacOSRef<CFRunLoopSourceRef> consoleUserRLS;
      std::string consoleUser;

      int displayPower           = 0;

    public:
      OSXOSImpl(App &app);
      ~OSXOSImpl();

      static OSXOSImpl &instance();

      // From OS
      const char *getName() const {return "macosx";}
      bool isSystemIdle() const {return systemIsIdle;}
      void dispatch();

      // From cb::Thread
      void run();

      // Callbacks
      void consoleUserChanged
      (SCDynamicStoreRef store, CFArrayRef changedKeys, void *info);
      void displayPowerChanged
      (void *context, io_service_t service, natural_t mtype, void *marg);
      void finishInit();

    protected:
      void initialize();
      void addHeartbeatTimerToRunLoop(CFRunLoopRef loop);
      void deregisterForConsoleUserNotifications();
      bool registerForConsoleUserNotifications();
      bool registerForDarwinNotifications();
      void deregisterForDisplayPowerNotifications();
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
