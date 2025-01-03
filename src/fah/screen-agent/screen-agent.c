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

// Observe screensaver start/stop, screen lock/unlock notifications.
// Periodically post darwin notifications for screen idle/notidle.
// These notifications are observed by fah-client, and expire
// Agent will exit non-zero on SIGHUP, possibly to be restarted by launchd.

#ifndef __APPLE__
#error "This can only be compiled for macOS"
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <xpc/xpc.h>
#include <notify.h>
#include <sysexits.h>
#include <unistd.h>
#include <os/log.h>

#include "defines.h"

#define AGENT_LABEL "org.foldingathome.fah-screen-agent"

#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x
#ifndef VERSION
#define VERSION 0.0.0
#endif
static const char *version = STRINGIFY(VERSION);

struct _DTimer {
  dispatch_source_t ds;
  dispatch_time_t   start;
  uint64_t          interval;
  uint64_t          leeway;
};
typedef struct _DTimer DTimer;

static DTimer timer;

static dispatch_source_t sig_ds[NSIG] = {0};

static int quit_sig = 0;
static bool restart = false;
static CFAbsoluteTime start_time = 0;
static os_log_t logger = OS_LOG_DEFAULT;

static bool screenIsLocked = false;
static bool screensaverIsActive = false;
static bool screenIdle = false;

static CFNotificationCenterRef nc;
static const char *observer = AGENT_LABEL;

const char *kScreenIdle    = SCREEN_IDLE_NOTIFICATION;
const char *kScreenNotIdle = SCREEN_NOT_IDLE_NOTIFICATION;


#if DEBUG
static void print_time() {
  time_t now = time(0);
  struct tm sTm;
  char buff[24];
  gmtime_r(&now, &sTm);
  strftime(buff, sizeof(buff), "%F %TZ", &sTm);
  printf("%s ", buff);
}

#define LOGDEBUG(format, ...) do { \
    if (isatty(STDOUT_FILENO)) { \
      print_time(); \
      printf(format, ## __VA_ARGS__); \
      printf("\n"); \
    } else os_log_debug(logger, format, ## __VA_ARGS__); \
  } while(0)

#else
#define LOGDEBUG(format, ...)

#endif


static void notify() {
  LOGDEBUG("%s %s", __FUNCTION__, screenIdle ? kScreenIdle : kScreenNotIdle);
  notify_post(screenIdle ? kScreenIdle : kScreenNotIdle);
}


static void create_timer() {
  // periodically repeat notifications, which expire in client
  dispatch_queue_t q = dispatch_get_main_queue();
  timer.ds       = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, q);
  timer.interval = SCREEN_NOTIFICATION_INTERVAL * NSEC_PER_SEC;
  timer.leeway   = SCREEN_NOTIFICATION_LEEWAY   * NSEC_PER_SEC;
  timer.start    = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 4);
  assert(timer.ds);
  dispatch_source_set_timer(timer.ds,timer.start,timer.interval,timer.leeway);
  dispatch_source_set_event_handler(timer.ds, ^{notify();});
  dispatch_resume(timer.ds);
}


static void reset_timer() {
  timer.start = dispatch_time(DISPATCH_TIME_NOW, timer.interval);
  dispatch_source_set_timer(timer.ds,timer.start,timer.interval,timer.leeway);
}


static void update() {
  LOGDEBUG("%s", __FUNCTION__);
  bool shouldBeIdle = screenIsLocked || screensaverIsActive;
  if (screenIdle == shouldBeIdle) return;
  screenIdle = shouldBeIdle;
  reset_timer();
  notify();
}


void screenDidLock(CFNotificationCenterRef center, void *observer,
          CFStringRef name, const void *object, CFDictionaryRef userInfo) {
  LOGDEBUG("%s", __FUNCTION__);
  screenIsLocked = true;
  update();
}


void screenDidUnlock(CFNotificationCenterRef center, void *observer,
          CFStringRef name, const void *object, CFDictionaryRef userInfo) {
  LOGDEBUG("%s", __FUNCTION__);
  screenIsLocked = false;
  update();
}


void screensaverDidStart(CFNotificationCenterRef center, void *observer,
          CFStringRef name, const void *object, CFDictionaryRef userInfo) {
  LOGDEBUG("%s", __FUNCTION__);
  screensaverIsActive = true;
  update();
}


void screensaverDidStop(CFNotificationCenterRef center, void *observer,
          CFStringRef name, const void *object, CFDictionaryRef userInfo) {
  LOGDEBUG("%s", __FUNCTION__);
  screensaverIsActive = false;
  update();
}


struct note_item {
  CFStringRef in;
  CFNotificationCallback cb;
};


static const struct note_item notes[] = {
  {CFSTR("com.apple.screensaver.didstart"), screensaverDidStart},
  {CFSTR("com.apple.screensaver.didstop"),  screensaverDidStop},
  {CFSTR("com.apple.screenIsLocked"),       screenDidLock},
  {CFSTR("com.apple.screenIsUnlocked"),     screenDidUnlock},
  {NULL, NULL}
};


static void register_for_launch_events() {
  // register for xpc launchd.plist LaunchEvents
  // this MUST be done if launchd might have LaunchEvents for us
  // we never un-register
  dispatch_queue_t q = dispatch_get_main_queue();
  xpc_set_event_stream_handler("com.apple.iokit.matching", q,
    ^(xpc_object_t event) {
      // DO NOTHING but consume event
#if DEBUG
      const char *ename = xpc_dictionary_get_string(event, XPC_EVENT_KEY_NAME);
      LOGDEBUG("LaunchEvents IO: %s", ename);
#endif
    });

  xpc_set_event_stream_handler("com.apple.notifyd.matching", q,
    ^(xpc_object_t event) {
      // DO NOTHING but consume event
#if DEBUG
      const char *nname = xpc_dictionary_get_string(event, "Notification");
      LOGDEBUG("LaunchEvents notification: %s", nname);
#endif
    });
}


static void got_signal(int sig) {
  // dispatch callback, NOT a signal handler
  LOGDEBUG("%s %s", __FUNCTION__, sys_signame[sig]);
  switch (sig) {
    case SIGTERM:
      quit_sig = sig;
      // fallthrough
    case SIGINT:
      CFRunLoopStop(CFRunLoopGetMain());
      break;
    case SIGHUP:
      restart = true;
      CFRunLoopStop(CFRunLoopGetMain());
      break;
    default: break;
  }
}


static void dummy_signal_handler(int sig) {}


static void install_signal_handlers() {
  sigset_t sigs;
  sigemptyset(&sigs);

  sigaddset(&sigs, SIGTERM);
  sigaddset(&sigs, SIGINT);
  sigaddset(&sigs, SIGHUP);

  struct sigaction action = {0};
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);

  dispatch_queue_t q = dispatch_get_main_queue();
  // dummy handler instead of SIG_IGN so syscalls can still be interrupted
  action.sa_handler = dummy_signal_handler;
  for(int s = 1; s < NSIG; s++) {
    if (sigismember(&sigs, s)) {
      sigaction(s, &action, NULL);
      sig_ds[s] = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, s, 0, q);
      assert(sig_ds[s]);
      dispatch_source_set_event_handler(sig_ds[s], ^{got_signal(s);});
      dispatch_resume(sig_ds[s]);
    }
  }
}


static void release_signal_handlers() {
  for(int s = 1; s < NSIG; s++)
    if (sig_ds[s]) dispatch_release(sig_ds[s]);
}


static void usage(int argc, const char * argv[]) {
  printf("usage: %s [{start|stop}]\n", argv[0]);
  printf(" start  convenience for /bin/launchctl start %s\n", AGENT_LABEL);
  printf(" stop   convenience for /bin/launchctl stop  %s\n", AGENT_LABEL);
  printf(" -h,--help\n");
  printf(" --version\n");
}


static void parse_args(int argc, const char * argv[]) {
  if (getppid() != 1 && 1 < argc) {
    const char *prog = "/bin/launchctl";
    if (strcmp("start", argv[1]) == 0 || strcmp("stop", argv[1]) == 0) {
      printf("%s %s %s\n", prog, argv[1], AGENT_LABEL);
      execl(prog, prog, argv[1], AGENT_LABEL);
    }
    if (strcmp("--help", argv[1]) == 0 || strcmp("-h", argv[1]) == 0) {
      usage(argc, argv);
    } else if (strcmp("--version", argv[1]) == 0) {
      printf("%s\n", version);
    } else {
      printf("error: unknown argument: %s\n", argv[1]);
      usage(argc, argv);
      exit(EX_USAGE);
    }
    exit(0);
  }
}


static void init(int argc, const char * argv[]) {
  start_time = CFAbsoluteTimeGetCurrent();
#if DEBUG
  logger = os_log_create(AGENT_LABEL, "main");
#endif
  nc = CFNotificationCenterGetDistributedCenter();

  if (geteuid() == 0) {
    fprintf(stderr, "error: %s cannot be run as root\n", argv[0]);
    exit(1);
  }
  // FIXME: refuse to run if not GUI session

  parse_args(argc, argv);

  install_signal_handlers();
  register_for_launch_events();

  // TODO: maybe get initial values from CGSessionCopyCurrentDictionary
  // https://stackoverflow.com/a/11511419
  // this should always be correct for a fresh gui login
  screenIsLocked = false;
  screensaverIsActive = false;
  screenIdle = false;

  // set up notifications
  for (int i = 0; notes[i].in; i++)
    CFNotificationCenterAddObserver(nc, observer, notes[i].cb,
      notes[i].in, NULL, CFNotificationSuspensionBehaviorCoalesce);

  create_timer();
}


int main(int argc, const char * argv[]) {
  init(argc, argv);
  CFRunLoopRun();

  // any critical cleanup would go here

  // exit immediately if think launchd signaled us
  if (quit_sig) exit(0);

  // exit non-zero if want to restart (assume appropriate KeepAlive)
  if (restart) exit(EX_TEMPFAIL);

#if DEBUG
  // non-critical cleanup
  CFNotificationCenterRemoveEveryObserver(nc, observer);
  dispatch_source_cancel(timer.ds);
  dispatch_release(timer.ds);
  release_signal_handlers();
#endif

  // run 10+ sec or launchd may think we crashed and relaunch us
  if (getppid() == 1) {
    CFTimeInterval run_time = CFAbsoluteTimeGetCurrent() - start_time;
    if (run_time < 10.0) sleep(10 - (int)run_time);
  }

  return 0;
}
