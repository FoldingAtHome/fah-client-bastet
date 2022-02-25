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

#include "OS.h"

#include <fah/client/App.h>
#include <fah/client/Config.h>
#include <fah/client/Units.h>

#if defined(_WIN32)
#include "win/WinOSImpl.h"
#elif defined(__APPLE__)
#include "osx/OSXOSImpl.h"
#else
#include "lin/LinOSImpl.h"
#endif

#include <fah/client/App.h>

#include <cbang/Info.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


OS::OS(App &app) : app(app) {
  app.getEventBase().setTimeout([this] () {updateIdle();}, 0);
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


void OS::requestExit() {app.requestExit();}


const char *OS::getCPU() const {
#if defined(__aarch64__)
  return "arm64";
#endif

  return sizeof(void *) == 4 ? "x86" : "amd64";
}


void OS::dispatch() {app.getEventBase().dispatch();}


void OS::setPaused(bool paused) {
  app.getConfig().setPaused(paused);
  app.getUnits().triggerUpdate(true);
}


bool OS::getPaused()  const {return app.getConfig().getPaused();}
bool OS::isIdle()     const {return app.getUnits().isIdle();}
bool OS::hasFailure() const {return app.getUnits().hasFailure();}


void OS::updateIdle() {
  app.getEventBase().setTimeout([this] () {updateIdle();}, 2);

  bool idle = isSystemIdle() && app.getConfig().getOnIdle();
  if (idle == this->idle) return;

  this->idle = idle;
  app.getUnits().triggerUpdate(true);
}
