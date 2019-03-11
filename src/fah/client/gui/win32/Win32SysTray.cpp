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

#ifdef _WIN32

#include "Win32SysTray.h"

#include <fah/client/App.h>
#include <fah/client/slot/SlotManager.h>

#include <cbang/Exception.h>
#include <cbang/SStream.h>
#include <cbang/Catch.h>
#include <cbang/util/SmartLock.h>
#include <cbang/util/SmartUnlock.h>
#include <cbang/os/SysError.h>
#include <cbang/os/Subprocess.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/log/Logger.h>
#include <cbang/openssl/Digest.h>

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  void run_cmd(const string &exe) {
    Subprocess proc;
    proc["PATH"] =
      SystemUtilities::dirname(SystemUtilities::getExecutablePath());
    proc.exec(exe);
  }
}


extern "C" LRESULT CALLBACK windowProcCB(HWND hWnd, UINT message, WPARAM wParam,
                                         LPARAM lParam) {
  try {
    return Win32SysTray::instance().windowProc(hWnd, message, wParam, lParam);
  } CATCH_ERROR;

  return 0;
}


extern "C" INT_PTR CALLBACK aboutProcCB(HWND hDlg, UINT message, WPARAM wParam,
                                        LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: return (INT_PTR)1;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)1;
    }
    break;
  }

  return (INT_PTR)0;
}


namespace {
  const GUID guid_system_awaymode =
    {0x98a7f580, 0x01f7, 0x48aa,
     {0x9c, 0x0f, 0x44, 0x35, 0x2c, 0x29, 0xe5, 0xC0}};
  const GUID guid_monitor_power_on =
    {0x02731015, 0x4510, 0x4526,
     {0x99, 0xe6, 0xe5, 0xa1, 0x7e, 0xbd, 0x1a, 0xea}};
}


Win32SysTray *Win32SysTray::singleton = 0;


Win32SysTray::Win32SysTray(App &app) :
  app(app), hInstance((HINSTANCE)GetModuleHandle(0)), hWnd(0), iconCurrent(0),
  inAwayMode(false), displayOff(false) {
  if (singleton) THROW("Win32SysTray already exists");
  singleton = this;
}


static bool isWin7OrLater() {
  // Initialize OSVERSIONINFOEX
  OSVERSIONINFOEX osvi;
  memset(&osvi, 0, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  osvi.dwMajorVersion = 6;
  osvi.dwMinorVersion = 1;

  // Initialize the condition mask
  DWORDLONG dwlConditionMask = 0;
  VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

  // Perform the test
  return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION,
                           dwlConditionMask);
}


void Win32SysTray::init() {
  HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NORMAL));

  // Create window class
  const char *className = app.getName().c_str();
  WNDCLASSEX wcex;
  memset(&wcex, 0, sizeof(wcex));
  wcex.cbSize        = sizeof(WNDCLASSEX);
  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc   = (WNDPROC)windowProcCB;
  wcex.hInstance     = hInstance;
  wcex.hIcon         = hIcon;
  wcex.hCursor       = LoadCursor(0, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszClassName = className;

  if (RegisterClassEx(&wcex) == 0)
    THROWS("Failed to register window class: " << SysError());

  // Create the systray window and hide it
  hWnd = CreateWindow(className, className, 0, 0, 0, 400, 600, 0, 0,
                      hInstance, 0);
  if (!hWnd) THROWS("Failed to create systray window: " << SysError());
  ShowWindow(hWnd, SW_HIDE);

  // Create Systray Icon
  memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));
  notifyIconData.cbSize           = sizeof(NOTIFYICONDATA);
  notifyIconData.uID              = ID_USER_SYSTRAY;
  notifyIconData.uFlags           = NIF_ICON | NIF_MESSAGE;
  notifyIconData.uCallbackMessage = WM_SYSTRAYMSG;
  notifyIconData.hWnd             = hWnd;
  notifyIconData.hIcon            = hIcon;

  // ID
  if (isWin7OrLater()) {
    // Use a different GUID for each executable path.  See
    // http://stackoverflow.com/questions/7432319/notifyicondata-guid-problem
    Digest digest("sha256");
    digest.update(SystemUtilities::getExecutablePath());
    const vector<uint8_t> &hash = digest.getDigest();

    const GUID guid = {0};
    for (unsigned i = 0; i < sizeof(guid) && i < hash.size(); i++)
      ((uint8_t *)&guid)[i] = hash[i];

    notifyIconData.guidItem = guid;
    notifyIconData.uFlags |= NIF_GUID;
  }

  // Add it
  if (!Shell_NotifyIcon(NIM_ADD, &notifyIconData))
    THROWS("Failed to register systray icon: " << SysError());

  // Register for PM away mode events
  typedef HPOWERNOTIFY (WINAPI *func_t)(HANDLE, LPCGUID, DWORD);
  func_t func = (func_t)GetProcAddress(GetModuleHandle(TEXT("user32.dll")),
                                       "RegisterPowerSettingNotification");
  if (func) {
    func(hWnd, &guid_system_awaymode, DEVICE_NOTIFY_WINDOW_HANDLE);
    func(hWnd, &guid_monitor_power_on, DEVICE_NOTIFY_WINDOW_HANDLE);
  }
}


LRESULT Win32SysTray::windowProc(HWND hWnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  SmartLock lock(&app);

  SlotManager &slotMan = app.getSlotManager();

  switch (message) {
  case WM_DESTROY:
    Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
    app.requestExit();
    PostQuitMessage(0);
    return 0;

  case WM_POWERBROADCAST:
    if (wParam == PBT_POWERSETTINGCHANGE) {
      POWERBROADCAST_SETTING &pbs = *((POWERBROADCAST_SETTING *)lParam);

      if (pbs.PowerSetting == guid_system_awaymode)
        inAwayMode = *((DWORD *)pbs.Data);
      else if (pbs.PowerSetting == guid_monitor_power_on)
        displayOff = !*((DWORD *)pbs.Data);

      app.setIdle(inAwayMode || displayOff);
      return 0;
    }
    break;

  case WM_SYSTRAYMSG:
    if (wParam == ID_USER_SYSTRAY &&
        (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP))
      popup(hWnd); // Left or right mouse button up bring up menu
    return 0;

  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case ID_USER_WEBCONTROL:
      if (ShellExecute(hWnd, "open", "https://client.foldingathome.org/", 0,
                       0, SW_SHOWDEFAULT) <= (HINSTANCE)32)
        LOG_ERROR("Failed to open Web control: " << SysError());
      return 0;

    case ID_USER_VIEWER: run_cmd("FAHViewer"); return 0;
    case ID_USER_CONTROL: run_cmd("FAHControl"); return 0;

    case ID_USER_PAUSE:
      slotMan.setPaused(!slotMan.isPaused());
      app.setConfigured();
      return 0;

    case ID_USER_IDLE:
      slotMan.setIdle(!slotMan.getIdle());
      app.setConfigured();
      return 0;

    case ID_USER_LIGHT:
    case ID_USER_MEDIUM:
    case ID_USER_FULL: {
      unsigned level =
        PowerLevel::PLEVEL_LIGHT + LOWORD(wParam) - ID_USER_LIGHT;
      slotMan.setPowerLevel((PowerLevel::enum_t)level);
      app.setConfigured();
      return 0;
    }

    case ID_USER_ABOUT: {
      string text = SSTR
        ("Folding@home Client\n\r"
         "Version " << app.getVersion() << "\n\r"
         "\n\r"
         "Copyright (c) 2001-2019 foldingathome.org\n\r"
         "\n\r"
         "Folding@home uses your excess computing power to help scientists at "
         "Universities around the world to better understand and find cures "
         "for diseases such as Alzheimer's, Huntington's, and many forms of "
         "cancer.\n\r"
         "\n\r"
         "For more information visit: https://foldingathome.org/");

      MSGBOXPARAMS msg;
      msg.cbSize = sizeof(MSGBOXPARAMS);
      msg.hwndOwner = hWnd;
      msg.hInstance = hInstance;
      msg.dwStyle = MB_USERICON | MB_OK | MB_SYSTEMMODAL;
      msg.lpszIcon = MAKEINTRESOURCE(IDI_NORMAL);
      msg.lpszCaption = app.getName().c_str();
      msg.lpszText = text.c_str();

      // Unlock for blocking operation
      SmartUnlock unlock(&app);

      MessageBoxIndirect(&msg);

      return 0;
    }

    case ID_USER_EXIT:
      DestroyWindow(hWnd);
      return 0;
    }
    break;
  }
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}


void Win32SysTray::update() {
  updateIcon();
  SmartUnlock unlock(&app); // Relocked in windowProc()
  processMessages();
}


void Win32SysTray::shutdown() {
  if (hWnd) {
    DestroyWindow(hWnd);
    hWnd = 0;
  }

  processMessages();
}


void Win32SysTray::processMessages() {
  MSG msg;
  while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}


void Win32SysTray::updateIcon() {
  if (app.getSlotManager().hasFailure())
    setSysTray(IDI_FAILURE, "One or more folding slot failed");
  else if (app.getSlotManager().isIdle())
    setSysTray(IDI_INACTIVE, "No active folding slots");
  else setSysTray(IDI_NORMAL, "Folding active");
}


void Win32SysTray::setSysTray(int icon, LPCTSTR tip) {
  if (iconCurrent == icon) return;
  iconCurrent = icon;

  notifyIconData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(icon));

  if (tip) {
    strncpy(notifyIconData.szTip, tip, sizeof(notifyIconData.szTip));
    notifyIconData.uFlags |= NIF_TIP;

  } else notifyIconData.uFlags &= ~NIF_TIP;

  Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
}


void Win32SysTray::popup(HWND hWnd) {
  // Create popup menu
  HMENU hMenu = CreatePopupMenu();

  AppendMenu(hMenu, 0, ID_USER_WEBCONTROL, "&Web Control");
  AppendMenu(hMenu, 0, ID_USER_CONTROL, "Advanced &Control");
  AppendMenu(hMenu, 0, ID_USER_VIEWER, "Protein &Viewer");

  AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
  PowerLevel power = app.getSlotManager().getPowerLevel();

  AppendMenu(hMenu, power == PowerLevel::PLEVEL_FULL ? MF_CHECKED : 0,
             ID_USER_FULL, "Full");
  AppendMenu(hMenu, power == PowerLevel::PLEVEL_MEDIUM ? MF_CHECKED : 0,
             ID_USER_MEDIUM, "Medium");
  AppendMenu(hMenu, power == PowerLevel::PLEVEL_LIGHT ? MF_CHECKED : 0,
             ID_USER_LIGHT, "Light");

  AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
  bool idle = app.getSlotManager().getIdle();
  AppendMenu(hMenu, idle ? MF_CHECKED : 0, ID_USER_IDLE, "On Idle");

  bool paused = app.getSlotManager().isPaused();
  AppendMenu(hMenu, paused ? MF_CHECKED : 0, ID_USER_PAUSE, "Pause");

  AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
  AppendMenu(hMenu, 0, ID_USER_ABOUT, "&About");
  AppendMenu(hMenu, 0, ID_USER_EXIT, "&Quit");

  // Get mouse position
  POINT point;
  GetCursorPos(&point);

  // Make foreground, otherwise menu won't go away
  SetForegroundWindow(hWnd);

  // Unlock for blocking operation
  SmartUnlock unlock(&app);

  // Track the popup menu
  TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
                 point.x, point.y, 0, hWnd, 0);

  // So the 2nd time the menu is displayed it wont disappear
  PostMessage(hWnd, WM_NULL, 0, 0);

  // Free menu
  DestroyMenu(hMenu);
}

#endif // _WIN32
