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
#include <cbang/util/Version.h>
#include <atomic>

#include "resource.h"

#define _WINSOCKAPI_ // Stop windows from including winsock.h
#include <windows.h>


namespace FAH {
  namespace Client {
    class App;

    class WinOSImpl : public OS, public cb::Thread {
      static WinOSImpl *singleton;

      std::string appName;
      cb::Version version;
      std::string copyright;
      std::string url;

      bool systrayEnabled = true;

      HINSTANCE hInstance;
      HWND hWnd = 0;
      NOTIFYICONDATA notifyIconData;

      int iconCurrent = 0;

      std::atomic<bool> inAwayMode = false;
      std::atomic<bool> displayOff = false;

    public:
      WinOSImpl(App &app);
      ~WinOSImpl();

      static WinOSImpl &instance();

      void init();

      // From OS
      const char *getName() const override {return "win32";}
      const char *getCPU() const override;
      bool isSystemIdle() const override {return inAwayMode || displayOff;}
      void dispatch() override;

      // From cb::Thread
      void run() override;

      LRESULT windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    protected:
      void openWebControl();
      void showAbout(HWND hWnd);
      void updateIcon();
      void setSysTray(int icon, LPCTSTR tip);
      void popup(HWND hWnd);
    };
  }
}
