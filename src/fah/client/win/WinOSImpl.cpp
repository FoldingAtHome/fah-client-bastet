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

#include "WinOSImpl.h"

#include <fah/client/App.h>

#include <cbang/Exception.h>
#include <cbang/SStream.h>
#include <cbang/Catch.h>
#include <cbang/Info.h>
#include <cbang/os/SysError.h>
#include <cbang/os/SystemUtilities.h>
#include <cbang/log/Logger.h>
#include <cbang/openssl/Digest.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma comment(lib, "user32.lib")

using namespace FAH::Client;
using namespace cb;
using namespace std;


namespace {
  const GUID guid_system_awaymode =
    {0x98a7f580, 0x01f7, 0x48aa,
     {0x9c, 0x0f, 0x44, 0x35, 0x2c, 0x29, 0xe5, 0xC0}};

  const GUID guid_monitor_power_on =
    {0x02731015, 0x4510, 0x4526,
     {0x99, 0xe6, 0xe5, 0xa1, 0x7e, 0xbd, 0x1a, 0xea}};


  extern "C" LRESULT CALLBACK windowProcCB
  (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    try {
      return WinOSImpl::instance().windowProc(hWnd, message, wParam, lParam);
    } CATCH_ERROR;

    return 0;
  }


  extern "C" INT_PTR CALLBACK aboutProcCB
  (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
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


  bool isWin7OrLater() {
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
    return VerifyVersionInfo
      (&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask);
  }
}


WinOSImpl *WinOSImpl::singleton = 0;


WinOSImpl::WinOSImpl(App &app) :
  OS(app), appName(app.getName()), version(app.getVersion()),
  hInstance((HINSTANCE)GetModuleHandle(0)) {
  if (singleton) THROW("There can be only one WinOSImpl");
  singleton = this;

  Options &options = app.getOptions();

  options.pushCategory("Windows");
  options.addTarget("systray", systrayEnabled, "Set to false to disable the "
                    "Windows systray icon.");
  options.popCategory();
}


WinOSImpl::~WinOSImpl() {singleton = 0;}


WinOSImpl &WinOSImpl::instance() {
  if (!singleton) THROW("No WinOSImpl instance");
  return *singleton;
}


void WinOSImpl::init() {
  HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NORMAL));
  if (!hIcon) THROW("Failed to load icon");

  // Create window class
  const char *className = appName.c_str();
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

  if (!RegisterClassEx(&wcex))
    THROW("Failed to register window class: " << SysError());

  // Create the systray window and hide it
  hWnd =
    CreateWindow(className, className, 0, 0, 0, 400, 600, 0, 0, hInstance, 0);
  if (!hWnd) THROW("Failed to create systray window: " << SysError());
  ShowWindow(hWnd, SW_HIDE);

  // Create Systray Icon
  memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));
  notifyIconData.cbSize           = NOTIFYICONDATA_V1_SIZE;
  notifyIconData.uID              = ID_USER_SYSTRAY;
  notifyIconData.uFlags           = NIF_ICON | NIF_MESSAGE;
  notifyIconData.uCallbackMessage = WM_SYSTRAYMSG;
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

  // Remove possible old icon
  Shell_NotifyIcon(NIM_DELETE, &notifyIconData);

  // Add it
  notifyIconData.hWnd = hWnd;
  if (!Shell_NotifyIcon(NIM_ADD, &notifyIconData))
    THROW("Failed to register systray icon: " << SysError());

  // Register for PM away mode events
  RegisterPowerSettingNotification(
    hWnd, &guid_system_awaymode, DEVICE_NOTIFY_WINDOW_HANDLE);
  RegisterPowerSettingNotification(
    hWnd, &guid_monitor_power_on, DEVICE_NOTIFY_WINDOW_HANDLE);

  // Create icon update timer
  SetTimer(hWnd, ID_UPDATE_TIMER, 1000, 0);

  LOG_INFO(1, "Started Windows systray control");
}


const char *WinOSImpl::getCPU() const {
  SYSTEM_INFO info;
  GetNativeSystemInfo(&info);

  switch (info.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64: return "amd64";
  case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
#ifdef PROCESSOR_ARCHITECTURE_ARM64
  case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
#endif
  case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
  default: return OS::getCPU();
  }
}


void WinOSImpl::dispatch() {
  if (systrayEnabled) Thread::start();

  OS::dispatch();

  if (hWnd) PostMessage(hWnd, WM_CLOSE, 0, 0);
  Thread::join();
}


void WinOSImpl::run() {
  init();

  MSG msg;

  while (hWnd && 0 < GetMessage(&msg, hWnd, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (hWnd) DestroyWindow(hWnd);
}


LRESULT WinOSImpl::windowProc(HWND hWnd, UINT message, WPARAM wParam,
                              LPARAM lParam) {
  switch (message) {
  case WM_DESTROY: case WM_QUERYENDSESSION: case WM_ENDSESSION:
    Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
    if (hWnd) KillTimer(hWnd, ID_UPDATE_TIMER);
    OS::requestExit();
    PostQuitMessage(0);
    hWnd = 0;
    return 0;

  case WM_POWERBROADCAST:
    if (wParam == PBT_POWERSETTINGCHANGE) {
      POWERBROADCAST_SETTING &pbs = *((POWERBROADCAST_SETTING *)lParam);

      if (pbs.PowerSetting == guid_system_awaymode)
        inAwayMode = *((DWORD *)pbs.Data);

      else if (pbs.PowerSetting == guid_monitor_power_on)
        displayOff = !*((DWORD *)pbs.Data);

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
    case ID_USER_WEBCONTROL: openWebControl();          return 0;
    case ID_USER_FOLD:       OS::setState(STATE_FOLD);  return 0;
    case ID_USER_PAUSE:      OS::setState(STATE_PAUSE); return 0;
    case ID_USER_ABOUT:      showAbout(hWnd);           return 0;
    case ID_USER_EXIT:       DestroyWindow(hWnd);       return 0;
    }
    break;
  }

  case WM_TIMER:
    if (wParam == ID_UPDATE_TIMER) updateIcon();
    break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}


void WinOSImpl::openWebControl() {
  string url = Info::instance().get(appName, "URL");
  if (ShellExecute(hWnd, "open", url.c_str(), 0, 0,
                   SW_SHOWDEFAULT) <= (HINSTANCE)32)
    LOG_ERROR("Failed to open Web control: " << SysError());
}


void WinOSImpl::showAbout(HWND hWnd) {
  string text = SSTR
    ("Folding@home Client\n\r"
     "Version " << version << "\n\r"
     "\n\r"
     "Copyright (c) 2001-2024 foldingathome.org\n\r"
     "\n\r"
     "Folding@home uses your excess computing power to help scientists at "
     "Universities around the world to better understand and find cures "
     "for diseases such as Alzheimer's, Huntington's, and many forms of "
     "cancer.\n\r"
     "\n\r"
     "For more information visit: https://foldingathome.org/");

  MSGBOXPARAMS msg;
  msg.cbSize      = sizeof(MSGBOXPARAMS);
  msg.hwndOwner   = hWnd;
  msg.hInstance   = hInstance;
  msg.dwStyle     = MB_USERICON | MB_OK | MB_SYSTEMMODAL;
  msg.lpszIcon    = MAKEINTRESOURCE(IDI_NORMAL);
  msg.lpszCaption = appName.c_str();
  msg.lpszText    = text.c_str();

  MessageBoxIndirect(&msg);
}


void WinOSImpl::updateIcon() {
  if (OS::hasFailure())
    setSysTray(IDI_FAILURE, "One or more folding processes have failed");
  else if (!OS::isActive()) setSysTray(IDI_INACTIVE, "Not folding");
  else setSysTray(IDI_NORMAL, "Folding active");
}


void WinOSImpl::setSysTray(int icon, LPCTSTR tip) {
  if (iconCurrent == icon) return;
  iconCurrent = icon;

  notifyIconData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(icon));

  if (tip) {
    strncpy(notifyIconData.szTip, tip, sizeof(notifyIconData.szTip));
    notifyIconData.uFlags |= NIF_TIP;

  } else notifyIconData.uFlags &= ~NIF_TIP;

  Shell_NotifyIcon(NIM_MODIFY, &notifyIconData);
}


void WinOSImpl::popup(HWND hWnd) {
  HMENU hMenu = CreatePopupMenu();

  AppendMenu(hMenu, 0, ID_USER_WEBCONTROL, "&Web Control");
  AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
  AppendMenu(hMenu, 0, ID_USER_FOLD,       "&Fold");
  AppendMenu(hMenu, 0, ID_USER_PAUSE,      "&Pause");
  AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
  AppendMenu(hMenu, 0, ID_USER_ABOUT,      "&About");
  AppendMenu(hMenu, 0, ID_USER_EXIT,       "&Quit");

  // Get mouse position
  POINT point;
  GetCursorPos(&point);

  // Make foreground, otherwise menu won't go away
  SetForegroundWindow(hWnd);

  // Track the popup menu
  TrackPopupMenu
    (hMenu, TPM_RIGHTALIGN | TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, 0);

  // So the 2nd time the menu is displayed it won't disappear
  PostMessage(hWnd, WM_NULL, 0, 0);

  // Free menu
  DestroyMenu(hMenu);
}
