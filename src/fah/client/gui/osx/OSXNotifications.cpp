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

#ifdef __APPLE__

#include "OSXNotifications.h"

#include <fah/client/App.h>

#include <cbang/Exception.h>
#include <cbang/Catch.h>
#include <cbang/util/SmartLock.h>
#include <cbang/util/SmartUnlock.h>
#include <cbang/log/Logger.h>

#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <limits>

using namespace FAH::Client;
using namespace cb;
using namespace std;


static const CFTimeInterval kCFTimeIntervalMax =
  std::numeric_limits<CFTimeInterval>::max();

enum {
  kDisplayPowerUnknown = -1,
  kDisplayPowerOff = 0,
  kDisplayPowerDimmed = 3,
  kDisplayPowerOn = 4
};


extern "C" {
  static void consoleUserCB(SCDynamicStoreRef s, CFArrayRef keys, void *info) {
    OSXNotifications::instance().consoleUserChanged(s, keys, info);
  }

  static void displayPowerCB(void *ctx, io_service_t service,
                             natural_t mtype, void *marg) {
    OSXNotifications::instance().displayPowerChanged(ctx, service, mtype, marg);
  }

  static void finishInitTimerCB(CFRunLoopTimerRef timer, void *info) {
    OSXNotifications::instance().finishInit();
  }

  static void updateTimerCB(CFRunLoopTimerRef timer, void *info) {
    OSXNotifications::instance().updateTimerFired(timer, info);
  }
}


static unsigned getIdleSeconds() {
  // copied from PowerManagement, without throttling
  unsigned idleSeconds = 0;
  io_iterator_t iter = 0;

  if (IOServiceGetMatchingServices(kIOMasterPortDefault,
                                   IOServiceMatching("IOHIDSystem"), &iter) ==
      KERN_SUCCESS) {
    io_registry_entry_t entry = IOIteratorNext(iter);

    if (entry)  {
      CFMutableDictionaryRef dict = NULL;
      if (IORegistryEntryCreateCFProperties(entry, &dict, kCFAllocatorDefault,
                                            0) == KERN_SUCCESS) {
        CFNumberRef obj =
          (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("HIDIdleTime"));

        if (obj) {
          int64_t nanoseconds = 0;
          if (CFNumberGetValue(obj, kCFNumberSInt64Type, &nanoseconds))
            idleSeconds = (unsigned)(nanoseconds / 1000000000);
        }
        CFRelease(dict);
      }
      IOObjectRelease(entry);
    }
    IOObjectRelease(iter);
  }
  return idleSeconds;
}


OSXNotifications *OSXNotifications::singleton = 0;


OSXNotifications::OSXNotifications(App &app) :
  app(app), systemIsIdle(false), screensaverIsActive(false),
  screenIsLocked(false), loginwindowIsActive(false), displayWrangler(0),
  displayNotePort(NULL), displayNoteSource(NULL), displayNotifier(0),
  consoleUserDS(NULL), consoleUserRLS(NULL), consoleUser(NULL),
  updateTimer(NULL),
  idleDelay(5), currentDelay(0), idleOnLoginwindowDelay(30),
  displayPower(0) {

  if (singleton) THROW("OSXNotifications already exists");
  singleton = this;

  consoleUser = CFSTR("unknown");
}


void OSXNotifications::init() {
  if (!registerForDisplayPowerNotifications())
    LOG_ERROR("Failed to register for display power changes");

  if (!registerForConsoleUserNotifications())
    LOG_ERROR("Failed to register for console user changes");

  // finish init in a timer callback, we so can't have stale values
  CFRunLoopTimerRef timer;
  timer = CFRunLoopTimerCreate(NULL, 0, 0, 0, 0, finishInitTimerCB, NULL);
  if (timer) {
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
  }
}


void OSXNotifications::update() {
  SmartUnlock unlock(&app);
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5.0, false);
}


void OSXNotifications::finishInit() {
  // init display power state if registration succeeded
  if (displayNoteSource && displayPower == kDisplayPowerOn) {
    int state = getCurrentDisplayPower();
    if (state == kDisplayPowerDimmed)
      displayDidDim();
    else if (kDisplayPowerOff <= state && state < kDisplayPowerDimmed)
      displayDidSleep();
  }

  // init console user if registration succeeded
  if (consoleUserRLS && (consoleUser == NULL))
    consoleUserChanged(NULL, NULL, NULL);
}


void OSXNotifications::shutdown() {
  // stop any update timer
  if (updateTimer) {
    CFRunLoopTimerInvalidate(updateTimer);
    CFRelease(updateTimer);
    updateTimer = NULL;
    currentDelay = 0;
  }

  // deregister for display power changes
  if (displayWrangler) {
    IOObjectRelease(displayWrangler);
    displayWrangler = 0;
  }
  if (displayNoteSource) {
    CFRunLoopRemoveSource(CFRunLoopGetMain(), displayNoteSource,
                          kCFRunLoopDefaultMode);
    displayNoteSource = NULL;
  }
  if (displayNotePort) {
    IONotificationPortDestroy(displayNotePort);
    displayNotePort = NULL;
  }
  if (displayNotifier) {
    IOObjectRelease(displayNotifier);
    displayNotifier = 0;
  }

  // deregister for console user changes
  if (consoleUserRLS)
  {
    CFRunLoopSourceInvalidate(consoleUserRLS);
    CFRelease(consoleUserRLS);
    consoleUserRLS = NULL;
  }
  if (consoleUserDS) {
    SCDynamicStoreSetNotificationKeys(consoleUserDS, NULL, NULL);
    CFRelease(consoleUserDS);
    consoleUserDS = NULL;
  }
  if (consoleUser) {
    CFRelease(consoleUser);
    consoleUser = CFSTR("unknown");
  }

  // clear flags
  displayPower = kDisplayPowerOn;
  screensaverIsActive = screenIsLocked = loginwindowIsActive = false;
  systemIsIdle = false;

  try {
    SmartLock lock(&app);
    app.setIdle(false);
  } CATCH_ERROR;
}


void OSXNotifications::updateSystemIdle() {
  bool shouldBeIdle;

  // TODO maybe have atomic bool (int) options such as idle-on-loginwindow
  // if not atomic, we could periodically lock App and sync

  // once idle on loginwindow, stay idle
  shouldBeIdle =
    displayPower == kDisplayPowerOff ||
    screensaverIsActive ||
    screenIsLocked ||
    (systemIsIdle && loginwindowIsActive);

  if (!shouldBeIdle && loginwindowIsActive) {
    // Go idle on logout after idleOnLoginwindowDelay seconds of no activity.
    // Note: idleSeconds can miss mouse moves, so this might trigger early
    // VNC activity does not affect idleSeconds
    unsigned idleSeconds = getIdleSeconds();
    if (idleSeconds < (unsigned)idleOnLoginwindowDelay)
      delayedUpdateSystemIdle(idleOnLoginwindowDelay - idleSeconds);
    else {
      shouldBeIdle = true;
    }
  }

  if (systemIsIdle != shouldBeIdle) {
    systemIsIdle = shouldBeIdle;
    try {
      SmartLock lock(&app);
      app.setIdle(systemIsIdle);
    } CATCH_ERROR;
  }
}


void OSXNotifications::updateTimerFired(CFRunLoopTimerRef timer, void *info) {
  currentDelay = 0;
  updateSystemIdle();
}


void OSXNotifications::delayedUpdateSystemIdle(int delay) {
  if (delay < 0)
    delay = idleDelay;
  if (delay < 1)
    delay = 1;

  CFAbsoluteTime candidateTime = CFAbsoluteTimeGetCurrent() + delay;

  if (updateTimer == NULL) {
    updateTimer = CFRunLoopTimerCreate(kCFAllocatorDefault,
                                       candidateTime, kCFTimeIntervalMax,
                                       0, 0, updateTimerCB, NULL);
    if (updateTimer == NULL) {
      LOG_ERROR("Unable to create update timer");
      currentDelay = 0;
      // Create failed. Do immediate update if we won't infinite loop
      if (!loginwindowIsActive) updateSystemIdle();
      return;
    }
    CFRunLoopAddTimer(CFRunLoopGetMain(), updateTimer, kCFRunLoopDefaultMode);
    currentDelay = delay;

  } else if (currentDelay == 0) {
    CFRunLoopTimerSetNextFireDate(updateTimer, candidateTime);
    currentDelay = delay;

  } else if (delay < currentDelay) {
    // Use sooner of current fire date and candidateTime
    CFAbsoluteTime currentTime = CFRunLoopTimerGetNextFireDate(updateTimer);
    if (candidateTime < currentTime) {
      CFRunLoopTimerSetNextFireDate(updateTimer, candidateTime);
      currentDelay = delay;
    }
  }
}


void OSXNotifications::displayDidDim() {
  displayPower = kDisplayPowerDimmed;
}


void OSXNotifications::displayDidUndim() {
  displayPower = kDisplayPowerOn;
}


void OSXNotifications::displayDidSleep() {
  displayPower = kDisplayPowerOff;
  delayedUpdateSystemIdle();
}


void OSXNotifications::displayDidWake() {
  displayPower = kDisplayPowerOn;

  // Idle logout can wake screen before switch to loginwindow
  // Might only happen pre-OSX 10.7
  // idleSeconds can lie during idle logout, so can't check that
  // One second delay gives time to get console user change
  delayedUpdateSystemIdle(1);
}


void OSXNotifications::displayPowerChanged(void *context,
                          io_service_t service,
                          natural_t mtype,
                          void *marg) {
	switch (mtype) {
    case kIOMessageDeviceWillPowerOff:
      // may be called several times
      // first is dim, second and more are off
      if (displayPower == kDisplayPowerOn)
        displayDidDim();
      else if (displayPower != kDisplayPowerOff)
        displayDidSleep();
      break;
    case kIOMessageDeviceHasPoweredOn:
      if (displayPower == kDisplayPowerDimmed)
        displayDidUndim();
      else
        displayDidWake();
      break;
	}
}


int OSXNotifications::getCurrentDisplayPower() {
  // will only return values kDisplayPowerUnknown thru kDisplayPowerOn
  int state = kDisplayPowerUnknown;
  int maxstate = 4;

  if (displayWrangler == 0)
    displayWrangler = IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceNameMatching("IODisplayWrangler"));

  if (displayWrangler) {
    CFMutableDictionaryRef props = NULL;
    CFMutableDictionaryRef dict = NULL;
    CFNumberRef power = NULL;
    kern_return_t		kr;
    int n;

    kr = IORegistryEntryCreateCFProperties(displayWrangler, &props, NULL, 0);
    if (kr == KERN_SUCCESS) {
      dict = (CFMutableDictionaryRef)CFDictionaryGetValue(props,
                                                  CFSTR("IOPowerManagement"));
      if (dict) {
        // MaxPowerState is available on 10.7+
        // Assume 4 if NULL which is seems true on 10.5+
        power = (CFNumberRef)CFDictionaryGetValue(dict,
                                                  CFSTR("MaxPowerState"));
        if (!(power && CFNumberGetValue(power, kCFNumberIntType, &maxstate)))
          maxstate = 4;

        power = (CFNumberRef)CFDictionaryGetValue(dict,
                                                  CFSTR("CurrentPowerState"));
        if (power == NULL)
          power = (CFNumberRef)CFDictionaryGetValue(dict,
                                                  CFSTR("DevicePowerState"));

        if (power && CFNumberGetValue(power, kCFNumberIntType, &n))
            state = n;
      }
    }

    if (props) CFRelease(props);
    IOObjectRelease(displayWrangler);
    displayWrangler = 0;
  }

  if (maxstate != 4) {
    if (maxstate < 1) {
      state = kDisplayPowerUnknown;
    } else if (maxstate == 1) {
      if (state == maxstate) state = kDisplayPowerOn;
      else if (0 <= state) state = kDisplayPowerOff;
    } else {
      if (state == maxstate) state = kDisplayPowerOn;
      else if (state == (maxstate - 1)) state = kDisplayPowerDimmed;
      else if (0 <= state) state = kDisplayPowerOff;
    }
  }

  if (state < 0 || maxstate < state) state = kDisplayPowerUnknown;

  return state;
}


bool OSXNotifications::registerForDisplayPowerNotifications() {
  bool ok = true;
  kern_return_t kr;

  displayPower = kDisplayPowerOn; // usually true when client starts

	displayWrangler = IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceNameMatching("IODisplayWrangler"));
	if (displayWrangler == 0) return false;

  if (ok) {
    displayNotePort = IONotificationPortCreate(kIOMasterPortDefault);
    ok = (displayNotePort != NULL);
  }

  if (ok) {
    kr = IOServiceAddInterestNotification(displayNotePort, displayWrangler,
                                          kIOGeneralInterest, displayPowerCB,
                                          NULL, &displayNotifier);
    ok = (kr == KERN_SUCCESS);
  }

  if (ok) {
    displayNoteSource = IONotificationPortGetRunLoopSource(displayNotePort);
    ok = (displayNoteSource != NULL);
  }

  if (ok)
    CFRunLoopAddSource(CFRunLoopGetMain(), displayNoteSource,
                       kCFRunLoopDefaultMode);

  if (!ok) {
    if (displayNotePort) {
      IONotificationPortDestroy(displayNotePort);
      displayNotePort = NULL;
    }
    if (displayNotifier) {
      IOObjectRelease(displayNotifier);
      displayNotifier = 0;
    }
  }

  // keep displayWrangler for getCurrentDisplayPower in finishInit
  return ok;
}


void OSXNotifications::consoleUserChanged(SCDynamicStoreRef store,
                                          CFArrayRef changedKeys,
                                          void *info) {
  if (consoleUser) CFRelease(consoleUser);
  consoleUser = SCDynamicStoreCopyConsoleUser(consoleUserDS, NULL, NULL);

  bool wasActive = loginwindowIsActive;

  loginwindowIsActive = (consoleUser == NULL ||
                         CFStringGetLength(consoleUser) == 0 ||
                         CFEqual(consoleUser, CFSTR("loginwindow")));

  bool changed = (wasActive != loginwindowIsActive);

  if (changed || store == NULL) {
    if (loginwindowIsActive)
      delayedUpdateSystemIdle();
    else
      updateSystemIdle(); // no delay when switching away from loginwindow
  }
}


bool OSXNotifications::registerForConsoleUserNotifications() {
  bool ok = false;
  CFArrayRef keys = NULL;
  CFArrayRef patterns = NULL;
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  CFStringRef consoleUserKey = SCDynamicStoreKeyCreateConsoleUser(NULL);

  consoleUserDS = SCDynamicStoreCreate(NULL,
                                       CFSTR("FAHClient Console User Watcher"),
                                       consoleUserCB, &context);

  CFStringRef values[] = {consoleUserKey};
  keys = CFArrayCreate(NULL, (const void **)values, 1, &kCFTypeArrayCallBacks);

  ok = (consoleUserKey && keys && consoleUserDS);

  if (ok) {
    ok = SCDynamicStoreSetNotificationKeys(consoleUserDS, keys, patterns);
  }

  if (ok) {
    consoleUserRLS = SCDynamicStoreCreateRunLoopSource(NULL, consoleUserDS, 0);
    ok = (consoleUserRLS != NULL);
  }

  if (ok)
    CFRunLoopAddSource(CFRunLoopGetMain(), consoleUserRLS,
                       kCFRunLoopDefaultMode);

  if (!ok) {
    if (consoleUserDS) {
      SCDynamicStoreSetNotificationKeys(consoleUserDS, NULL, NULL);
      CFRelease(consoleUserDS);
      consoleUserDS = NULL;
    }
    if (consoleUser) CFRelease(consoleUser);
    consoleUser = CFSTR("unknown");
  }

  if (consoleUserKey) CFRelease(consoleUserKey);
  if (keys) CFRelease(keys);
  // initial consoleUser value will be set in finishInit
  return ok;
}

#endif // __APPLE__
