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

#include "OS.h"

#include <fah/client/App.h>
#include <fah/client/GPUResources.h>

#if defined(_WIN32)
#include "win/WinOSImpl.h"
#elif defined(__APPLE__)
#include "osx/OSXOSImpl.h"
#else
#include "lin/LinOSImpl.h"
#endif

#include <cbang/Info.h>
#include <cbang/os/PowerManagement.h>
#include <cbang/log/Logger.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


OS::OS(App &app) : app(app), paused(false), active(false), failure(false),
  onBattery(false), state(STATE_NULL),
  event(app.getEventBase().newEvent(this, &OS::update)) {event->add(2);}


OS::~OS() {}


SmartPointer<OS> OS::create(App &app) {
#if defined(_WIN32)
  return new WinOSImpl(app);

#elif defined(__APPLE__)
  return new OSXOSImpl(app);

#else
  return new LinOSImpl(app);
#endif
}


const char *OS::getCPU() const {
#if defined(__aarch64__)
  return "arm64";
#endif

  return sizeof(void *) == 4 ? "x86" : "amd64";
}


void OS::dispatch() {app.getEventBase().dispatch();}
void OS::requestExit() {app.requestExit();}
void OS::setState(state_t state) {this->state = state; event->activate();}


void OS::update() {
  switch (state.exchange(STATE_NULL)) {
  case STATE_NULL:                           break;
  case STATE_FOLD:   app.setState("fold");   break;
  case STATE_PAUSE:  app.setState("pause");  break;
  case STATE_FINISH: app.setState("finish"); break;
  }

  if (isSystemIdle() != idle) {
    idle = !idle;
    app.triggerUpdate();
  }

  paused  = app.getPaused();
  active  = app.isActive();
  failure = app.hasFailure();

  auto &pm = PowerManagement::instance();
  bool onBattery = pm.onBattery();
  if (this->onBattery != onBattery) {
    this->onBattery = onBattery;
    app.triggerUpdate();
  }

  // Keep system awake if not on battery
  if (!onBattery && app.keepAwake()) lastKeepAwake = Time::now();
  pm.allowSystemSleep(30 < Time::now() - lastKeepAwake);

  // Signal when GPU is ready
  if (!gpuReady && isGPUReady()) {
    LOG_DEBUG(3, "GPU ready");
    gpuReady = true;
    app.getGPUs().signalGPUReady();
  }

  // Update application info
  app.get("info")->insertBoolean("on_battery", onBattery);
}