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

#include "OSXOSImpl.h"

#include <fah/client/App.h>

#include <cbang/Exception.h>
#include <cbang/Catch.h>
#include <cbang/log/Logger.h>
#include <cbang/event/Base.h>
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


namespace {

#pragma mark c callbacks

  void consoleUserCB(SCDynamicStoreRef s, CFArrayRef keys, void *info) {
    LOG_DEBUG(5, "consoleUserCB on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
    OSXOSImpl::instance().consoleUserChanged(s, keys, info);
  }

  void displayPowerCB(void *ctx, io_service_t service,
                             natural_t mtype, void *marg) {
    LOG_DEBUG(5, "displayPowerCB on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
    OSXOSImpl::instance().displayPowerChanged(ctx, service, mtype, marg);
  }

  void finishInitTimerCB(CFRunLoopTimerRef timer, void *info) {
    LOG_DEBUG(5, "finishInitTimerCB on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
    CFRunLoopTimerInvalidate(timer); // in case it would repeat
    OSXOSImpl::instance().finishInit();
  }

  void updateTimerCB(CFRunLoopTimerRef timer, void *info) {
    LOG_DEBUG(5, "updateTimerCB on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
    OSXOSImpl::instance().updateTimerFired(timer, info);
  }

  void heartbeatTimerCB(CFRunLoopTimerRef timer, void *info) {
    LOG_DEBUG(5, "heartbeat on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
  }

  void noteQuitCB(CFNotificationCenterRef center, void *observer,
    CFNotificationName name, const void *object, CFDictionaryRef info) {
    LOG_DEBUG(5, "noteQuitCB on thread "  << pthread_self() <<
      (pthread_main_np() ? " main" : ""));
    std::string n = CFStringGetCStringPtr(name, kCFStringEncodingUTF8);
    LOG_INFO(3, "Received notification " << n);
    OSXOSImpl::instance().requestExit();
  }

}

#pragma mark c functions

static unsigned getIdleSeconds() {
  // copied from PowerManagement, without throttling
  unsigned idleSeconds = 0;
  io_iterator_t iter = 0;

  if (IOServiceGetMatchingServices
      (kIOMasterPortDefault, IOServiceMatching("IOHIDSystem"), &iter) ==
      KERN_SUCCESS) {
    io_registry_entry_t entry = IOIteratorNext(iter);

    if (entry)  {
      CFMutableDictionaryRef dict = 0;
      if (IORegistryEntryCreateCFProperties
          (entry, &dict, kCFAllocatorDefault, 0) == KERN_SUCCESS) {
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


#pragma mark class OSXOSImpl

OSXOSImpl *OSXOSImpl::singleton = 0;


OSXOSImpl::OSXOSImpl(App &app) : OS(app), consoleUser(CFSTR("unknown")) {
  if (singleton) THROW("There can be only one OSXOSImpl");
  singleton = this;
  initialize();

  app.getEventBase().newEvent(this, &OSXOSImpl::start, 0)->activate();
}


OSXOSImpl::~OSXOSImpl() {
  // stop subthread
  stop();

  // Stop any update timer
  if (updateTimer) {
    CFRunLoopTimerInvalidate(updateTimer);
    CFRelease(updateTimer);
  }

  // Deregister for display power changes
  if (displayWrangler) IOObjectRelease(displayWrangler);

  if (displayNoteSource)
    CFRunLoopRemoveSource(CFRunLoopGetMain(), displayNoteSource,
                          kCFRunLoopDefaultMode);

  if (displayNotePort) IONotificationPortDestroy(displayNotePort);

  if (displayNotifier) IOObjectRelease(displayNotifier);

  // Deregister for console user changes
  if (consoleUserRLS) {
    CFRunLoopSourceInvalidate(consoleUserRLS);
    CFRelease(consoleUserRLS);
  }

  if (consoleUserDS) {
    SCDynamicStoreSetNotificationKeys(consoleUserDS, 0, 0);
    CFRelease(consoleUserDS);
  }

  if (consoleUser) CFRelease(consoleUser);

  CFNotificationCenterRef nc = CFNotificationCenterGetDarwinNotifyCenter();
  CFNotificationCenterRemoveEveryObserver(nc, this);

  Thread::join();
  singleton = 0;
}


OSXOSImpl &OSXOSImpl::instance() {
  if (!singleton) THROW("No OSXOSImpl instance");
  return *singleton;
}


void OSXOSImpl::init() {
  // init subthread in run
  threadRunLoop = CFRunLoopGetCurrent();
  // add heartbeat timer to keep runloop from exiting early
  addHeartbeatTimerToRunLoop(threadRunLoop);
}


void OSXOSImpl::initialize() {
  if (!registerForDisplayPowerNotifications())
    LOG_ERROR("Failed to register for display power changes");

  if (!registerForConsoleUserNotifications())
    LOG_ERROR("Failed to register for console user changes");

  registerForLaunchEvents(); // should never fail

  if (!registerForDarwinNotifications())
    LOG_ERROR("Failed to register for darwin notifications");

  // finish init in a timer callback, we so can't have stale values
  // a timer sometimes never fires if the start date is missed
  CFRunLoopTimerRef timer =
    CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + 10, 5,
      0, 0, finishInitTimerCB, 0);

  if (timer) {
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
  } else
    finishInit();

  addHeartbeatTimerToRunLoop(CFRunLoopGetMain());
}


void OSXOSImpl::addHeartbeatTimerToRunLoop(CFRunLoopRef loop) {
  // note this may fail silently
  if (loop == NULL)
    return;
  CFRunLoopTimerRef timer =
    CFRunLoopTimerCreate(0, 0, 600, 0, 0, heartbeatTimerCB, 0);
  if (timer) {
    CFRunLoopAddTimer(loop, timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
  }
}


void OSXOSImpl::dispatch() {
  // integrate cb::Event (libevent) and CFRunLoop
  // this is meant for main thread, but should work for any
  // toss libevent loop to another thread so CFRunLoop can use this thread
  // catch any exception and re-throw in calling thread
  // uncaught exceptions in a dispatched block will terminate the program
  CFRunLoopRef rloop = CFRunLoopGetCurrent();
  // create a serial queue
  dispatch_queue_t queue = dispatch_queue_create(NULL, 0);
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block bool did_raise = false;
  __block cb::Exception the_exception;
  if (queue == NULL) {
    // out of memory! use a global concurrent queue
    queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_retain(queue); // probably not needed
  }
  dispatch_async(queue, ^{
    //sleep(10); // for testing CFRunLoop alone for a while
    try {
      //throw Exception("test cb::Exception");
      getApp().getEventBase().dispatch();
    } catch (Exception &e) {
      // TODO more catch exception types
      did_raise = true;
      the_exception = e;
      LOG_DEBUG(5, "Exception in dispatched block: " << e);
    }
    // stop the caller thread run loop
    CFRunLoopStop(rloop);
    // signal this block is done
    dispatch_semaphore_signal(semaphore);
  });
  CFRunLoopRun();
  // wait for event_base_dispatch() to finish
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (semaphore) dispatch_release(semaphore);
  if (queue) dispatch_release(queue);
  if (did_raise)
    throw the_exception;
}


void OSXOSImpl::run() {
  init();
  CFRunLoopRun();
}


void OSXOSImpl::stop() {
  Thread::stop();
  if (threadRunLoop)
    CFRunLoopStop(threadRunLoop);
}


void OSXOSImpl::finishInit() {
  // Init display power state if registration succeeded
  if (displayNoteSource && displayPower == kDisplayPowerOn) {
    int state = getCurrentDisplayPower();

    if (state == kDisplayPowerDimmed) displayDidDim();
    else if (kDisplayPowerOff <= state && state < kDisplayPowerDimmed)
      displayDidSleep();
  }

  // Init console user if registration succeeded
  if (consoleUserRLS && !consoleUser) consoleUserChanged(0, 0, 0);
}


void OSXOSImpl::updateSystemIdle() {
  // Once idle on loginwindow, stay idle
  bool shouldBeIdle = displayPower == kDisplayPowerOff || screensaverIsActive ||
    screenIsLocked || (systemIsIdle && loginwindowIsActive);

  if (!shouldBeIdle && loginwindowIsActive) {
    // Go idle on logout after idleOnLoginwindowDelay seconds of no activity.
    // Note: idleSeconds can miss mouse moves, so this might trigger early
    // VNC activity does not affect idleSeconds
    unsigned idleSeconds = getIdleSeconds();
    if (idleSeconds < (unsigned)idleOnLoginwindowDelay)
      delayedUpdateSystemIdle(idleOnLoginwindowDelay - idleSeconds);
    else shouldBeIdle = true;
  }

  systemIsIdle = shouldBeIdle;
}


void OSXOSImpl::updateTimerFired(CFRunLoopTimerRef timer, void *info) {
  currentDelay = 0;
  updateSystemIdle();
}


void OSXOSImpl::delayedUpdateSystemIdle(int delay) {
  if (delay < 0) delay = idleDelay;
  if (delay < 1) delay = 1;

  CFAbsoluteTime candidateTime = CFAbsoluteTimeGetCurrent() + delay;

  if (!updateTimer) {
    updateTimer = CFRunLoopTimerCreate
      (kCFAllocatorDefault, candidateTime, kCFTimeIntervalMax, 0, 0,
       updateTimerCB, 0);

    if (!updateTimer) {
      LOG_ERROR("Unable to create update timer");
      currentDelay = 0;
      // Create failed. Do immediate update if we won't infinite loop
      if (!loginwindowIsActive) updateSystemIdle();
      return;
    }

    CFRunLoopAddTimer(CFRunLoopGetMain(), updateTimer, kCFRunLoopDefaultMode);
    currentDelay = delay;

  } else if (!currentDelay) {
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


void OSXOSImpl::displayDidDim() {displayPower = kDisplayPowerDimmed;}
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
    if (displayPower == kDisplayPowerOn) displayDidDim();
    else if (displayPower != kDisplayPowerOff) displayDidSleep();
    break;

  case kIOMessageDeviceHasPoweredOn:
    if (displayPower == kDisplayPowerDimmed) displayDidUndim();
    else displayDidWake();
    break;
  }
}


int OSXOSImpl::getCurrentDisplayPower() {
  // will only return values kDisplayPowerUnknown thru kDisplayPowerOn
  int state = kDisplayPowerUnknown;
  int maxstate = 4;

  if (!displayWrangler)
    displayWrangler = IOServiceGetMatchingService
      (kIOMasterPortDefault, IOServiceNameMatching("IODisplayWrangler"));

  if (displayWrangler) {
    CFMutableDictionaryRef props = 0;
    CFMutableDictionaryRef dict = 0;
    CFNumberRef power = 0;
    kern_return_t kr;
    int n;

    kr = IORegistryEntryCreateCFProperties(displayWrangler, &props, 0, 0);
    if (kr == KERN_SUCCESS) {
      dict = (CFMutableDictionaryRef)CFDictionaryGetValue
        (props, CFSTR("IOPowerManagement"));

      if (dict) {
        // MaxPowerState is available on 10.7+
        // Assume 4 if 0 which is seems true on 10.5+
        power = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("MaxPowerState"));
        if (!power || !CFNumberGetValue(power, kCFNumberIntType, &maxstate))
          maxstate = 4;

        power = (CFNumberRef)CFDictionaryGetValue
          (dict, CFSTR("CurrentPowerState"));
        if (!power) power = (CFNumberRef)CFDictionaryGetValue
                      (dict, CFSTR("DevicePowerState"));

        if (power && CFNumberGetValue(power, kCFNumberIntType, &n))
          state = n;
      }
    }

    if (props) CFRelease(props);
    IOObjectRelease(displayWrangler);
    displayWrangler = 0;
  }

  if (maxstate != 4) {
    if (maxstate < 1) state = kDisplayPowerUnknown;

    else if (maxstate == 1) {
      if (state == maxstate) state = kDisplayPowerOn;
      else if (0 <= state) state = kDisplayPowerOff;

    } else {
      if (state == maxstate) state = kDisplayPowerOn;
      else if (state == maxstate - 1) state = kDisplayPowerDimmed;
      else if (0 <= state) state = kDisplayPowerOff;
    }
  }

  if (state < 0 || maxstate < state) state = kDisplayPowerUnknown;

  return state;
}


bool OSXOSImpl::registerForDisplayPowerNotifications() {
  bool ok = true;
  kern_return_t kr;

  displayPower = kDisplayPowerOn; // usually true when client starts
  displayWrangler = IOServiceGetMatchingService
    (kIOMasterPortDefault, IOServiceNameMatching("IODisplayWrangler"));

  if (!displayWrangler) return false;

  if (ok) {
    displayNotePort = IONotificationPortCreate(kIOMasterPortDefault);
    ok = displayNotePort;
  }

  if (ok) {
    kr = IOServiceAddInterestNotification
      (displayNotePort, displayWrangler, kIOGeneralInterest, displayPowerCB, 0,
       &displayNotifier);

    ok = kr == KERN_SUCCESS;
  }

  if (ok) {
    displayNoteSource = IONotificationPortGetRunLoopSource(displayNotePort);
    ok = displayNoteSource;
  }

  if (ok) CFRunLoopAddSource
            (CFRunLoopGetMain(), displayNoteSource, kCFRunLoopDefaultMode);

  if (!ok) {
    if (displayNotePort) {
      IONotificationPortDestroy(displayNotePort);
      displayNotePort = 0;
    }

    if (displayNotifier) {
      IOObjectRelease(displayNotifier);
      displayNotifier = 0;
    }
  }

  // keep displayWrangler for getCurrentDisplayPower in finishInit
  return ok;
}


void OSXOSImpl::consoleUserChanged(SCDynamicStoreRef store,
                                   CFArrayRef changedKeys, void *info) {
  if (consoleUser) CFRelease(consoleUser);
  consoleUser = SCDynamicStoreCopyConsoleUser(consoleUserDS, 0, 0);

  bool wasActive = loginwindowIsActive;
  loginwindowIsActive = !consoleUser || !CFStringGetLength(consoleUser) ||
                         CFEqual(consoleUser, CFSTR("loginwindow"));

  bool changed = wasActive != loginwindowIsActive;

  if (changed || !store) {
    if (loginwindowIsActive) delayedUpdateSystemIdle();
    else updateSystemIdle(); // no delay when switching away from loginwindow
  }
}


bool OSXOSImpl::registerForConsoleUserNotifications() {
  bool ok = false;
  CFArrayRef keys = 0;
  CFArrayRef patterns = 0;
  SCDynamicStoreContext context = {0, 0, 0, 0, 0};
  CFStringRef consoleUserKey = SCDynamicStoreKeyCreateConsoleUser(0);

  consoleUserDS = SCDynamicStoreCreate
    (0, CFSTR("FAHClient Console User Watcher"), consoleUserCB, &context);

  CFStringRef values[] = {consoleUserKey};
  keys = CFArrayCreate(0, (const void **)values, 1, &kCFTypeArrayCallBacks);

  ok = consoleUserKey && keys && consoleUserDS;

  if (ok) ok = SCDynamicStoreSetNotificationKeys(consoleUserDS, keys, patterns);

  if (ok) {
    consoleUserRLS = SCDynamicStoreCreateRunLoopSource(0, consoleUserDS, 0);
    ok = consoleUserRLS;
  }

  if (ok) CFRunLoopAddSource
            (CFRunLoopGetMain(), consoleUserRLS, kCFRunLoopDefaultMode);

  if (!ok) {
    if (consoleUserDS) {
      SCDynamicStoreSetNotificationKeys(consoleUserDS, 0, 0);
      CFRelease(consoleUserDS);
      consoleUserDS = 0;
    }

    if (consoleUser) CFRelease(consoleUser);
    consoleUser = CFSTR("unknown");
  }

  if (consoleUserKey) CFRelease(consoleUserKey);
  if (keys) CFRelease(keys);

  // Initial consoleUser value will be set in finishInit
  return ok;
}


bool OSXOSImpl::registerForDarwinNotifications() {
  bool isok = false;
  CFNotificationCenterRef nc = CFNotificationCenterGetDarwinNotifyCenter();
  void *observer = this;
  CFStringRef name = NULL;
  std::string base;
  std::string user = "nobody";
  std::string n;

  if (nc == NULL) return false;

  struct passwd *pwent = getpwuid(getuid());
  if (pwent && pwent->pw_name)
    user = pwent->pw_name;

  base = "org.foldingathome.fahclient." + user + ".";

  n = base + "stop";
  name = CFStringCreateWithCString(NULL, n.c_str(), kCFStringEncodingUTF8);
  if (name) {
    CFNotificationCenterAddObserver(nc, observer,
        &noteQuitCB, name, NULL,
        CFNotificationSuspensionBehaviorCoalesce);
    CFRelease(name);
    isok = true;
    LOG_DEBUG(1, "listening for notification " << n);
  }

  return isok;
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
