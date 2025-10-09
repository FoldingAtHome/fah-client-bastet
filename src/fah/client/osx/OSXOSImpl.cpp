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

#include "OSXOSImpl.h"

#include <fah/client/App.h>

#include <cbang/Exception.h>
#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/event/Event.h>

#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <limits>
#include <xpc/xpc.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>

#include <fah/screen-agent/defines.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


enum {
  kDisplayPowerUnknown = -1,
  kDisplayPowerOff     = 0,
  kDisplayPowerDimmed  = 3,
  kDisplayPowerOn      = 4
};


namespace {
  CFStringRef kScreenIdle    = CFSTR(SCREEN_IDLE_NOTIFICATION);
  CFStringRef kScreenNotIdle = CFSTR(SCREEN_NOT_IDLE_NOTIFICATION);

#pragma mark c callbacks
  void consoleUserCB(SCDynamicStoreRef s, CFArrayRef keys, void *info) {
    OSXOSImpl::instance().consoleUserChanged(s, keys, info);
  }


  void displayPowerCB(
    void *ctx, io_service_t service, natural_t mtype, void *marg) {
    OSXOSImpl::instance().displayPowerChanged(ctx, service, mtype, marg);
  }


  void heartbeatTimerCB(CFRunLoopTimerRef timer, void *info) {
    LOG_DEBUG(5, "heartbeat on thread "  << pthread_self() <<
              (pthread_main_np() ? " main" : ""));
  }


  void noteQuitCB(CFNotificationCenterRef center, void *observer,
                  CFNotificationName name, const void *object,
                  CFDictionaryRef info) {
    LOG_INFO(3, "Received notification "
             << CFStringGetCStringPtr(name, kCFStringEncodingUTF8));
    OSXOSImpl::instance().requestExit();
  }


  void screenIdleCB(CFNotificationCenterRef center, void *observer,
                  CFNotificationName name, const void *object,
                  CFDictionaryRef info) {
    OSXOSImpl::instance().noteScreenIdle();
  }


  void screenNotIdleCB(CFNotificationCenterRef center, void *observer,
                  CFNotificationName name, const void *object,
                  CFDictionaryRef info) {
    OSXOSImpl::instance().noteScreenNotIdle();
  }
}


OSXOSImpl *OSXOSImpl::singleton = 0;


OSXOSImpl::OSXOSImpl(App &app) : OS(app), consoleUser("unknown") {
  if (singleton) THROW("There can be only one OSXOSImpl");
  singleton = this;
  initialize();
}


OSXOSImpl::~OSXOSImpl() {
  if (displayWrangler) IOObjectRelease(displayWrangler);

  if (displayNoteSource)
    CFRunLoopRemoveSource(
      CFRunLoopGetMain(), displayNoteSource, kCFRunLoopDefaultMode);

  deregisterForDisplayPowerNotifications();
  deregisterForConsoleUserNotifications();

  CFNotificationCenterRef nc = CFNotificationCenterGetDarwinNotifyCenter();
  CFNotificationCenterRemoveEveryObserver(nc, this);

  singleton = 0;
}


OSXOSImpl &OSXOSImpl::instance() {
  if (!singleton) THROW("No OSXOSImpl instance");
  return *singleton;
}


void OSXOSImpl::initialize() {
  if (!registerForDisplayPowerNotifications())
    LOG_ERROR("Failed to register for display power changes");

  if (!registerForConsoleUserNotifications())
    LOG_ERROR("Failed to register for console user changes");

  registerForLaunchEvents(); // should never fail

  if (!registerForDarwinNotifications())
    LOG_ERROR("Failed to register for darwin notifications");

  // finish init in main runloop, so we can't have stale values
  dispatch_async(dispatch_get_main_queue(), ^{finishInit();});

  addHeartbeatTimerToRunLoop(CFRunLoopGetMain());
}


void OSXOSImpl::addHeartbeatTimerToRunLoop(CFRunLoopRef loop) {
  // note this may fail silently
  if (!loop) return;

  MacOSRef<CFRunLoopTimerRef> timer(
    CFRunLoopTimerCreate(0, 0, 3600, 0, 0, heartbeatTimerCB, 0));
  if (timer) CFRunLoopAddTimer(loop, timer, kCFRunLoopDefaultMode);
}


void OSXOSImpl::dispatch() {
  if (!pthread_main_np())
    THROW("OSXOSImpl::dispatch() must be called on main thread");
  LOG_DEBUG(5, "OSXOSImpl::dispatch()");

  Thread::start();
  CFRunLoopRun();
  OS::requestExit();
  Thread::join();
}


void OSXOSImpl::run() {
  TRY_CATCH_ERROR(OS::dispatch());
  CFRunLoopStop(CFRunLoopGetMain());
}


void OSXOSImpl::finishInit() {
    LOG_DEBUG(5, "OSXOSImpl::finishInit() on thread "  << pthread_self() <<
              (pthread_main_np() ? " main" : ""));

  // Init display power state if registration succeeded
  if (displayNoteSource && displayPower == kDisplayPowerOn) {
    int state = getCurrentDisplayPower();

    if (state == kDisplayPowerDimmed) displayDidDim();
    else if (kDisplayPowerOff <= state && state < kDisplayPowerDimmed)
      displayDidSleep();
  }

  // Init console user if registration succeeded
  if (consoleUserRLS) consoleUserChanged(0, 0, 0);
}


void OSXOSImpl::updateSystemIdle() {
  bool shouldBeIdle;
  if (gotScreenNotIdleRecently()) shouldBeIdle = false;
  else shouldBeIdle = displayPower == kDisplayPowerOff || loginwindowIsActive ||
    gotScreenIdleRecently();
  if (shouldBeIdle == systemIsIdle) return;
  systemIsIdle = shouldBeIdle;

  event->activate();
}


void OSXOSImpl::delayedUpdateSystemIdle(int delay) {
  // default delay 5 sec, min 2 sec
  int64_t delta;
  if (delay < 0)
    delta = 5 * NSEC_PER_SEC;
  else if (delay < 3)
    delta = 2 * NSEC_PER_SEC;
  else
    delta = delay * NSEC_PER_SEC;
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, delta);
  dispatch_after(when, dispatch_get_main_queue(), ^{updateSystemIdle();});
}


void OSXOSImpl::displayDidDim()   {displayPower = kDisplayPowerDimmed;}
void OSXOSImpl::displayDidUndim() {displayPower = kDisplayPowerOn;}


void OSXOSImpl::displayDidSleep() {
  displayPower = kDisplayPowerOff;
  delayedUpdateSystemIdle();
}


void OSXOSImpl::displayDidWake() {
  displayPower = kDisplayPowerOn;

  // Idle logout can wake screen before switch to loginwindow
  // Might only happen pre-OSX 10.7
  // idleSeconds can lie during idle logout, so can't check that
  // One second delay gives time to get console user change
  delayedUpdateSystemIdle(1);
}


void OSXOSImpl::displayPowerChanged(void *context, io_service_t service,
                                    natural_t mtype, void *marg) {
  switch (mtype) {
  case kIOMessageDeviceWillPowerOff:
    // may be called several times
    // first is dim, second and more are off
    // on macOS 12, probably earlier, we sometimes only get one message
    if (displayPower != kDisplayPowerOff) displayDidSleep();
    break;

  case kIOMessageDeviceHasPoweredOn:
    if (displayPower == kDisplayPowerDimmed) displayDidUndim();
    else displayDidWake();
    break;

  case kIOMessageCanDevicePowerOff: break;
  default: LOG_DEBUG(3, __func__ << " unknown mtype: " << mtype);
  }
}


int OSXOSImpl::getCurrentDisplayPower() {
  // will only return values kDisplayPowerUnknown thru kDisplayPowerOn
  int state    = kDisplayPowerUnknown;
  int maxstate = 4;

  if (!displayWrangler)
    displayWrangler = IOServiceGetMatchingService
      (kIOMasterPortDefault, IOServiceNameMatching("IODisplayWrangler"));

  if (displayWrangler) {
    MacOSRef<CFMutableDictionaryRef> props;
    auto kr = IORegistryEntryCreateCFProperties(
      displayWrangler, &props.get(), 0, 0);

    if (kr == KERN_SUCCESS) {
      auto dict = (CFMutableDictionaryRef)CFDictionaryGetValue
        (props, CFSTR("IOPowerManagement"));

      if (dict) {
        // MaxPowerState is available on 10.7+
        // Assume 4 if 0 which is seems true on 10.5+
        auto power =
          (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("MaxPowerState"));

        if (!power || !CFNumberGetValue(power, kCFNumberIntType, &maxstate))
          maxstate = 4;

        power = (CFNumberRef)CFDictionaryGetValue
          (dict, CFSTR("CurrentPowerState"));
        if (!power) power = (CFNumberRef)CFDictionaryGetValue
                      (dict, CFSTR("DevicePowerState"));

        int n;
        if (power && CFNumberGetValue(power, kCFNumberIntType, &n))
          state = n;
      }
    }

    IOObjectRelease(displayWrangler);
    displayWrangler = 0;
  }

  if (maxstate != 4) {
    if (maxstate < 1)                 state = kDisplayPowerUnknown;
    else if (maxstate == 1) {
      if (state == maxstate)          state = kDisplayPowerOn;
      else if (0 <= state)            state = kDisplayPowerOff;

    } else {
      if (state == maxstate)          state = kDisplayPowerOn;
      else if (state == maxstate - 1) state = kDisplayPowerDimmed;
      else if (0 <= state)            state = kDisplayPowerOff;
    }
  }

  if (state < 0 || maxstate < state)  state = kDisplayPowerUnknown;

  return state;
}


void OSXOSImpl::deregisterForDisplayPowerNotifications() {
  if (displayNotePort) {
    IONotificationPortDestroy(displayNotePort);
    displayNotePort = 0;
  }

  if (displayNotifier) {
    IOObjectRelease(displayNotifier);
    displayNotifier = 0;
  }
}


bool OSXOSImpl::registerForDisplayPowerNotifications() {
  displayPower    = kDisplayPowerOn; // usually true when client starts
  displayWrangler = IOServiceGetMatchingService
    (kIOMasterPortDefault, IOServiceNameMatching("IODisplayWrangler"));

  if (!displayWrangler) return false;

  bool ok = displayNotePort = IONotificationPortCreate(kIOMasterPortDefault);

  if (ok) {
    kern_return_t kr = IOServiceAddInterestNotification
      (displayNotePort, displayWrangler, kIOGeneralInterest, displayPowerCB, 0,
       &displayNotifier);

    ok = kr == KERN_SUCCESS;
  }

  if (ok) ok = displayNoteSource =
            IONotificationPortGetRunLoopSource(displayNotePort);
  if (ok) CFRunLoopAddSource
            (CFRunLoopGetMain(), displayNoteSource, kCFRunLoopDefaultMode);

  if (!ok) deregisterForDisplayPowerNotifications();

  // keep displayWrangler for getCurrentDisplayPower in finishInit
  return ok;
}


void OSXOSImpl::consoleUserChanged(
  SCDynamicStoreRef store, CFArrayRef changedKeys, void *info) {
  consoleUser = MacOSString(SCDynamicStoreCopyConsoleUser(consoleUserDS, 0, 0));

  LOG_DEBUG(4, __func__ << "() " << (string)consoleUser);

  bool wasActive = loginwindowIsActive;
  loginwindowIsActive = consoleUser.empty() || consoleUser == "loginwindow";
  bool changed = wasActive != loginwindowIsActive;

  if (changed || !store) {
    if (loginwindowIsActive) delayedUpdateSystemIdle(20);
    else updateSystemIdle(); // no delay when switching away from loginwindow
  }
}


void OSXOSImpl::deregisterForConsoleUserNotifications() {
  if (consoleUserDS) {
    SCDynamicStoreSetNotificationKeys(consoleUserDS, 0, 0);
    consoleUserDS = 0;
  }

  if (consoleUserRLS) {
    CFRunLoopSourceInvalidate(consoleUserRLS);
    consoleUserRLS = 0;
  }

  consoleUser = "unknown";
}


bool OSXOSImpl::registerForConsoleUserNotifications() {
  SCDynamicStoreContext context = {0, 0, 0, 0, 0};

  consoleUserDS = SCDynamicStoreCreate
    (0, CFSTR("FAHClient Console User Watcher"), consoleUserCB, &context);

  MacOSString consoleUserKey(SCDynamicStoreKeyCreateConsoleUser(0));
  CFStringRef values[] = {(CFStringRef)consoleUserKey};
  MacOSRef<CFArrayRef> keys(
    CFArrayCreate(0, (const void **)values, 1, &kCFTypeArrayCallBacks));

  bool ok = consoleUserKey && keys && consoleUserDS;
  CFArrayRef patterns = 0;

  if (ok) ok = SCDynamicStoreSetNotificationKeys(consoleUserDS, keys, patterns);
  if (ok) ok = consoleUserRLS =
            SCDynamicStoreCreateRunLoopSource(0, consoleUserDS, 0);
  if (ok) CFRunLoopAddSource
            (CFRunLoopGetMain(), consoleUserRLS, kCFRunLoopDefaultMode);

  if (!ok) deregisterForConsoleUserNotifications();

  // Initial consoleUser value will be set in finishInit
  return ok;
}


void OSXOSImpl::noteScreenIdle() {
  screenIdleExpiry =
   dispatch_time(DISPATCH_TIME_NOW, SCREEN_NOTIFICATION_EXPIRES * NSEC_PER_SEC);
  bool wasIdle = screenIdle;
  screenIdle = true;
  dispatch_after(screenIdleExpiry + NSEC_PER_SEC, dispatch_get_main_queue(), ^{
    updateSystemIdle();
  });
  if (!wasIdle) delayedUpdateSystemIdle(5);
}


void OSXOSImpl::noteScreenNotIdle() {
  screenNotIdleExpiry =
   dispatch_time(DISPATCH_TIME_NOW, SCREEN_NOTIFICATION_EXPIRES * NSEC_PER_SEC);
  screenNotIdle = true;
  dispatch_after(screenNotIdleExpiry + NSEC_PER_SEC,dispatch_get_main_queue(),^{
    updateSystemIdle();
  });
  updateSystemIdle();
}


bool OSXOSImpl::gotScreenIdleRecently() {
  if (!screenIdle) return false;
  if (dispatch_time(DISPATCH_TIME_NOW, 0) < screenIdleExpiry) return true;
  screenIdle = false;
  return false;
}


bool OSXOSImpl::gotScreenNotIdleRecently() {
  if (!screenNotIdle) return false;
  if (dispatch_time(DISPATCH_TIME_NOW, 0) < screenNotIdleExpiry) return true;
  screenNotIdle = false;
  return false;
}


bool OSXOSImpl::registerForDarwinNotifications() {
  CFNotificationCenterRef nc = CFNotificationCenterGetDarwinNotifyCenter();
  if (!nc) return false;

  CFNotificationCenterAddObserver(
    nc, (void *)this, &screenIdleCB, kScreenIdle, 0,
    CFNotificationSuspensionBehaviorCoalesce);

  CFNotificationCenterAddObserver(
    nc, (void *)this, &screenNotIdleCB, kScreenNotIdle, 0,
    CFNotificationSuspensionBehaviorCoalesce);

  string user = "nobody";
  struct passwd *pwent = getpwuid(getuid());
  if (pwent && pwent->pw_name) user = pwent->pw_name;

  MacOSString name("org.foldingathome.fahclient." + user + ".stop");
  CFNotificationCenterAddObserver(
    nc, (void *)this, &noteQuitCB, name, 0,
    CFNotificationSuspensionBehaviorCoalesce);

  return true;
}


bool OSXOSImpl::registerForLaunchEvents() {
  // register for xpc launchd.plist LaunchEvents
  // this MUST be done if launchd might have LaunchEvents for us
  // because we never un-register, it's important to not use OSXOSImpl
  dispatch_queue_t q = dispatch_get_main_queue();
  xpc_set_event_stream_handler(
    "com.apple.iokit.matching", q,
    ^(xpc_object_t event) {
      const char *ename = xpc_dictionary_get_string(event, XPC_EVENT_KEY_NAME);
      // DO NOTHING but consume event
      LOG_INFO(5, "LaunchEvents IO: " << ename);
    });

  xpc_set_event_stream_handler(
    "com.apple.notifyd.matching", q,
    ^(xpc_object_t event) {
      const char *nname = xpc_dictionary_get_string(event, "Notification");
      // DO NOTHING but consume event
      LOG_INFO(5, "LaunchEvents notification: " << nname);
    });

  return true;
}
