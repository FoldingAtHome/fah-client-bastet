/******************************************************************************\

                 This file is part of the Folding@home Client.

          The fahclient runs Folding@home protein folding simulations.
                   Copyright (c) 2001-2019, foldingathome.org
                              All rights reserved.

      This program is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
       the Free Software Foundation; either version 2 of the License, or
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

#include "GUI.h"

#include "win32/Win32SysTray.h"
#include "osx/OSXNotifications.h"

#include <fah/client/App.h>

#include <cbang/config/Options.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


GUI::GUI(App &app) :
#ifdef _WIN32
  sysTray(new Win32SysTray(app)),
#elif defined(__APPLE__)
  notary(new OSXNotifications(app)),
#endif
  enabled(true) {
  Options &options = app.getOptions();

  options.pushCategory("GUI");
  options.addTarget("gui-enabled", enabled, "Set to false to disable the GUI.  "
                    "A GUI is not currently supported on all operating "
                    "systems.");
  options.popCategory();
}


void GUI::init() {
  if (!enabled) return;

#ifdef _WIN32
  sysTray->init();
#elif defined(__APPLE__)
  notary->init();
#endif
}


void GUI::update() {
  if (!enabled) return;

#ifdef _WIN32
  sysTray->update();
#elif defined(__APPLE__)
  notary->update();
#endif
}


void GUI::shutdown() {
  if (!enabled) return;

#ifdef _WIN32
  sysTray->shutdown();
#elif defined(__APPLE__)
  notary->shutdown();
#endif
}
