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


SmartPointer<OS> OS::create(App &app) {
#if defined(_WIN32)
  return new WinOSImpl(app);

#elif defined(__APPLE__)
  return new OSXOSImpl(app);

#else
  return new LinOSImpl(app);
#endif
}


const char *OS::getName() const {
#ifdef _WIN32
  return "win32";

#elif __APPLE__
  return "macosx";

#else
  // OS Type
  string platform =
    String::toLower(Info::instance().get(app.getName(), "Platform"));

  // 'platform' is the string returned in Python by:
  //   os.platform.lower() + ' ' + platform.release()
  if (platform.find("linux")   != string::npos) return "linux";
  if (platform.find("freebsd") != string::npos) return "freebsd";
  if (platform.find("openbsd") != string::npos) return "openbsd";
  if (platform.find("netbsd")  != string::npos) return "netbsd";
  if (platform.find("solaris") != string::npos) return "solaris";
#endif

  return "unknown";
}


void OS::requestExit() {app.requestExit();}

void OS::dispatch() {app.getEventBase().dispatch();}

void OS::setOnIdle(bool onIdle) {}
bool OS::getOnIdle() const {return false;}

void OS::setPaused(bool paused) {}
bool OS::getPaused() const {return false;}

void OS::setPower(Power power) {}
Power OS::getPower() const {return Power::POWER_FULL;}

bool OS::isIdle() const {return false;}
bool OS::hasFailure() const {return false;}
