/******************************************************************************\

                  This file is part of the Folding@home Client.

          The fah-client runs Folding@home protein folding simulations.
                    Copyright (c) 2001-2022, foldingathome.org
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

#if defined(_WIN32)
#include "win/WinOSImpl.h"
#elif defined(__APPLE__)
#include "osx/OSXOSImpl.h"
#else
#include "lin/LinOSImpl.h"
#endif

#include <cbang/Info.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


OS::OS(App &app) :
  app(app), event(app.getEventBase().newEvent(this, &OS::update)) {
  event->add(2);
}


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


void OS::requestExit() const {
  app.getEventBase().newEvent(&app, &App::requestExit, 0)->add(0);
}


void OS::togglePause() const {
  app.getEventBase().newEvent([this] () {
    app.setPaused(!app.getPaused());
  }, 0)->add();
}


void OS::update() {
  if (isSystemIdle() != idle) {
    idle = !idle;
    app.triggerUpdate();
  }

  paused  = app.getPaused();
  active  = app.isActive();
  failure = app.hasFailure();
}
