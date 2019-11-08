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

#pragma once
#ifdef _WIN32

#include "resource.h"

#define _WINSOCKAPI_ // Stop windows from including winsock.h
#include <windows.h>

namespace FAH {
  namespace Client {
    class App;

    class Win32SysTray {
      static Win32SysTray *singleton;

      App &app;

      HINSTANCE hInstance;
      HWND hWnd;
      NOTIFYICONDATA notifyIconData;

      int iconCurrent;

      bool inAwayMode;
      bool displayOff;

    public:
      Win32SysTray(App &app);

      static Win32SysTray &instance() {return *singleton;}

      void init();
      LRESULT windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
      void update();
      void shutdown();

    protected:
      void updateIcon();
      void processMessages();
      void setSysTray(int icon, LPCTSTR tip);
      void popup(HWND hWnd);
    };
  }
}

#endif // _WIN32

