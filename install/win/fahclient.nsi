; Defines
!define PRODUCT_NAME            "fahclient"
!define PROJECT_NAME            "Folding@home"
!define DISPLAY_NAME            "Folding@home Client"

!define WEB_CLIENT_URL          "https://console.foldingathome.org/"

!define CLIENT_NAME             "fahclient"
!define CLIENT_EXE              "fahclient.exe"
!define CLIENT_ICON             "fahlogo.ico"

!define UNINSTALLER             "Uninstall.exe"

!define PRODUCT_CONFIG          "config.xml"
!define PRODUCT_LICENSE         "LICENSE"
!define PRODUCT_VENDOR          "foldingathome.org"
!define PRODUCT_TARGET          "%(package)s"
!define PRODUCT_VERSION         "%(version)s"
!define PRODUCT_WEBSITE         "https://foldingathome.org/"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_UNINST_KEY \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY \
  "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}"

!define UNINSTALL_URL "https://apps.foldingathome.org/uninstall.php"

!define MUI_ABORTWARNING
!define MUI_ICON "images\${CLIENT_ICON}"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_BITMAP_NOSTRETCH


; Character types
!define UPPER       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
!define LOWER       "abcdefghijklmnopqrstuvwxyz"
!define NUMBER      "0123456789"
!define QUOTE       "$\""
!define PUNCTUATION ".,-_[]{}()!@#$$^&*=+?/\~`'<>:;|%%${QUOTE}"
!define SPACE       " $\t$\n$\r"
!define ALPHA       "${UPPER}${LOWER}"
!define ALPHANUM    "${ALPHA}${NUMBER}"
!define HEXADECIMAL "ABCDEFabcdef${NUMBER}"
!define DONOR_CHARS "${ALPHANUM}${PUNCTUATION}"


; Startup modes
!define STARTUP_AT_LOGIN     1
!define STARTUP_AS_SERVICE   2
!define STARTUP_MANUALLY     3

; Variables
Var StartupMode
Var UNINSTDIR

; Includes
!include MUI2.nsh
!include nsDialogs.nsh
!include LogicLib.nsh
!include EnvVarUpdate.nsh
!include WinVer.nsh

; Config
Name "${DISPLAY_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_TARGET}"
InstallDir "$PROGRAMFILES%(PACKAGE_ARCH)s\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show


; Pages
Var DataDir
!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE "${PRODUCT_LICENSE}"
Page custom InstallLevel

!define MUI_PAGE_CUSTOMFUNCTION_PRE DirectoryPre1
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave1
!insertmacro MUI_PAGE_DIRECTORY

!define MUI_PAGE_CUSTOMFUNCTION_PRE DirectoryPre2
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave2
!define MUI_DIRECTORYPAGE_VARIABLE $DataDir
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Data directory"
!define MUI_DIRECTORYPAGE_TEXT_TOP \
  "Please select a folder for ${DISPLAY_NAME} configuration and data."
!insertmacro MUI_PAGE_DIRECTORY

Page custom InstallDialog
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start Folding@home"
!define MUI_FINISHPAGE_RUN_FUNCTION StartFAH
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!insertmacro MUI_PAGE_FINISH

!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_TITLE "Instructions"
!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_INFO \
  "You may choose to save your configuration and work unit data for later \
   use or completely uninstall."
!insertmacro MUI_UNPAGE_COMPONENTS
UninstPage custom un.UninstallQuestion
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

RequestExecutionLevel admin


; Sections
Section -Install
  ; 32/64 bit registry
  SetRegView %(PACKAGE_ARCH)s

  ReadRegStr $UNINSTDIR ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" \
      "Path"

  ; Shutdown running client
  DetailPrint "Shutting down any local clients.  (Please wait)"
  nsExec::Exec '"$UNINSTDIR\${CLIENT_EXE}" --send-command=shutdown'

  ; Terminate
  FindProcDLL::KillProc "$UNINSTDIR\${CLIENT_EXE}"
  FindProcDLL::WaitProcEnd "$UNINSTDIR\${CLIENT_EXE}"

  ; Remove Autostart
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"

  ; Uninstall old software
  ; Avoid simply removing the whole directory as that causes subsequent file
  ; writes to fail for several seconds afterwards.
  DetailPrint "Uninstalling any conflicting Folding@home software"
  Push $INSTDIR
  Call EmptyDir

  ; Install files
  install_files:
  ClearErrors
  SetOverwrite try
  SetOutPath "$INSTDIR"
  File "${CLIENT_EXE}"
  File "images\${CLIENT_ICON}"
  File /oname=README.txt "README.md"
  File /oname=License.txt "LICENSE"
  File /oname=ChangeLog.txt "CHANGELOG.md"
  %(NSIS_INSTALL_FILES)s

  IfErrors 0 +2
    MessageBox MB_RETRYCANCEL "Failed to install files.  Most likely some \
        software, possibly Folding@home is currently using one or more files \
        that the installer is trying to upgrade.  Please stop all running \
        Folding@home software and quit any applications that are using files \
        from the Folding@home installation.  Note, complete shutdown can take \
        a little while after the application has closed." \
        IDRETRY install_files IDCANCEL abort

  ; Add to PATH
  SetShellVarContext current
  ${EnvVarUpdate} $0 "PATH" "A" "HKCU" $INSTDIR

  ; DataDir
  CreateDirectory $DataDir
  AccessControl::GrantOnFile "$DataDir" "(S-1-5-32-545)" "FullAccess"

  SetOutPath $DataDir ; Set working directory for shortcuts, etc.

  ; Desktop
  CreateShortCut "$DESKTOP\Folding@home.lnk" \
      '"$INSTDIR\${CLIENT_EXE}" --open-web-control' "$INSTDIR\${CLIENT_ICON}"
  Delete "$DESKTOP\Start Folding@home.lnk"

  ; Write uninstaller
write_uninstaller:
  ClearErrors
  WriteUninstaller "$INSTDIR\${UNINSTALLER}"
  IfErrors 0 +2
    MessageBox MB_ABORTRETRYIGNORE "Failed to create uninstaller" \
      IDABORT abort IDRETRY write_uninstaller

  ; Save uninstall information
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "UninstallString" "$INSTDIR\${UNINSTALLER}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayIcon" "$INSTDIR\${PRODUCT_NAME}.ico"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "URLInfoAbout" "${PRODUCT_WEBSITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "Publisher" "${PRODUCT_VENDOR}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" \
    "Path" "$INSTDIR"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DataDirectory" $DataDir

  ; Start Menu
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\folding@home.lnk" \
    '"$INSTDIR\${CLIENT_EXE}" --open-web-control'  "$INSTDIR\${CLIENT_ICON}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\fahclient.lnk" \
    "$INSTDIR\fahclient.url" "" "$INSTDIR\${CLIENT_ICON}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\data directory.lnk" "$DataDir"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\about ${PROJECT_NAME}.lnk" \
    "$INSTDIR\About ${PROJECT_NAME}.url"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\uninstall.lnk" \
    "$INSTDIR\${UNINSTALLER}"

  ; Internet shortcuts
  WriteIniStr "$INSTDIR\fahclient.url" "InternetShortcut" "URL" \
    "${CLIENT_URL}"
  WriteIniStr "$INSTDIR\About ${PROJECT_NAME}.url" "InternetShortcut" "URL" \
    "${PRODUCT_WEBSITE}"

  ;  Autostart
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk" # Clean up old link
  ${If} $StartupMode == ${STARTUP_AT_LOGIN}
    CreateShortCut "$SMSTARTUP\Folding@home.lnk" \
        "$INSTDIR\${CLIENT_EXE}" "$INSTDIR\${CLIENT_ICON}"
  ${Else}
    Delete "$SMSTARTUP\Folding@home.lnk"
  ${EndIf}

  Return

abort:
  Abort
SectionEnd


Section -un.Program
  ; Shutdown running client
  DetailPrint "Shutting down any local clients"
  nsExec::Exec '"$INSTDIR\${CLIENT_EXE}" --send-command=shutdown'

  ; Menu
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

  ; Autostart
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"

  ; Desktop
  Delete "$DESKTOP\Folding@home.lnk"

  ; Remove from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" $INSTDIR

  ; Registry
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

  ; Program directory
  remove_dir:
  ClearErrors
  RMDir /r "$INSTDIR"
  IfErrors 0 +3
    IfSilent +2
    MessageBox MB_RETRYCANCEL "Failed to remove $INSTDIR.  Please stop all \
      running Folding@home software." IDRETRY remove_dir
SectionEnd


Section /o un.Data
  RMDir /r $DataDir
SectionEnd


; Functions
Function .onInit
  ${IfNot} ${AtLeastWinXP}
    MessageBox MB_OK "XP and above required"
    Quit
  ${EndIf}
FunctionEnd


Function TerminateApp
  Pop $0

  FindWindow $1 '' $0
  IntCmp $1 0 terminate_app_done

  System::Call 'user32.dll::GetWindowThreadProcessId(i r0, *i .r1) i .r2'
  System::Call 'kernel32.dll::OpenProcess(i 0x00100001, i 0, i r1) i .r2'
  SendMessage $1 16 0 0 /TIMEOUT=5000
  System::Call 'kernel32.dll::WaitForSingleObject(i r2, i 5000) i .r1'
  IntCmp $1 0 terminate_app_close
  System::Call 'kernel32.dll::TerminateProcess(i r2, i 0) i .r1'
  terminate_app_close:
  System::Call 'kernel32.dll::CloseHandle(i r2) i .r1'
  terminate_app_done:
FunctionEnd


Function EmptyDir
  Pop $0

  FindFirst $1 $2 $0\*.*
  empty_dir_loop:
    StrCmp $2 "" empty_dir_done

    ; Skip . and ..
    StrCmp $2 "." empty_dir_next
    StrCmp $2 ".." empty_dir_next

    ; Remove subdirectories
    IfFileExists $0\$2\*.* 0 +3
    RmDir /r $0\$2
    Goto empty_dir_next

    ; Remove file
    Delete $INSTDIR\$2

    ; Next
    empty_dir_next:
    FindNext $1 $2
    Goto empty_dir_loop

  ; Done
  empty_dir_done:
  FindClose $1
FunctionEnd


Function ValidPath
  Pop $0

  ; Get Length
  StrLen $1 $0

loop:
  ; Check length
  StrCmp $1 0 pass

  ; Get next char
  StrCpy $2 $0 1
  StrCpy $0 $0 "" 1

  ; Check for invalid characters
  StrCmp $2 "@" fail
  StrCmp $2 "?" fail
  StrCmp $2 "*" fail
  StrCmp $2 "|" fail
  StrCmp $2 "<" fail
  StrCmp $2 ">" fail
  StrCmp $2 "'" fail
  StrCmp $2 '"' fail

  ; Decrement length
  IntOp $1 $1 - 1
  Goto loop

pass:
  Return

fail:
  MessageBox MB_OK `The following characters are not allowed: @?*|<>'"`
  Abort
FunctionEnd

Var InstallModeInitialized
Var ExpressInstall
Var ExpressInstallWidget
Var CustomInstall
Var CustomInstallWidget
Function InstallLevel
  ${IfNot} $InstallModeInitialized == "True"
    StrCpy $ExpressInstall ${BST_CHECKED}
    StrCpy $CustomInstall ${BST_UNCHECKED}
    StrCpy $InstallModeInitialized "True"
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "${PRODUCT_NAME}" "Installer Mode"

  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}

  ${NSD_CreateRadioButton} 10u 13u 200u 12u "Express install (Recommended)"
  Pop $ExpressInstallWidget
  ${NSD_SetState} $ExpressInstallWidget $ExpressInstall
  ${NSD_OnClick} $ExpressInstallWidget OnExpressInstallChange

  ${NSD_CreateRadioButton} 10u 26u 200u 12u "Custom install (Advanced)"
  Pop $CustomInstallWidget
  ${NSD_SetState} $CustomInstallWidget $CustomInstall
  ${NSD_OnClick} $CustomInstallWidget OnCustomInstallChange

  nsDialogs::Show
FunctionEnd


Function OnExpressInstallChange
  Pop $0
  ${NSD_GetState} $ExpressInstallWidget $ExpressInstall
  ${If} $ExpressInstall == ${BST_CHECKED}
    StrCpy $CustomInstall ${BST_UNCHECKED}
  ${EndIf}
FunctionEnd


Function OnCustomInstallChange
  Pop $0
  ${NSD_GetState} $CustomInstallWidget $CustomInstall
  ${If} $CustomInstall == ${BST_CHECKED}
    StrCpy $ExpressInstall ${BST_UNCHECKED}
  ${EndIf}
FunctionEnd


Function DirectoryPre1
  ${If} $ExpressInstall == ${BST_CHECKED}
    Abort
  ${EndIf}
FunctionEnd


Function DirectoryLeave1
  Push $INSTDIR
  Call ValidPath
FunctionEnd


Function DirectoryLeave2
  Push $DataDir
  Call ValidPath

  StrLen $0 $INSTDIR
  StrCpy $1 $DataDir $0
  StrCmp $INSTDIR $1 0 exit

  MessageBox MB_OKCANCEL \
    "WARNING: If the data directory is a sub-directory of the install \
     directory it will always be removed at uninstall time." IDOK exit
  Abort

exit:
FunctionEnd


Function DirectoryPre2
  ${If} $DataDir == ""
    ReadRegStr $DataDir ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
      "DataDirectory"
  ${EndIf}

  ${If} $DataDir == ""
    StrCpy $DataDir "$APPDATA\${PRODUCT_NAME}"
  ${EndIf}

  ${If} $ExpressInstall == ${BST_CHECKED}
    Abort
  ${EndIf}
FunctionEnd


Var InstallInitialized
Function InstallDialog
  ${IfNot} $InstallInitialized == "True"
    StrCpy $StartupMode ${STARTUP_AT_LOGIN}
  ${EndIf}

  ${If} $ExpressInstall == ${BST_CHECKED}
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "${PRODUCT_NAME}" "Installation"

  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}

  ; Client Startup
  ${NSD_CreateLabel} 0 0 200u 12u \
    "How would you like to start the Folding@home software?"
  Pop $0

  ${NSD_CreateRadioButton} 10u 13u 300u 12u \
    "Automatically start at login time. (Recommended)"
  Pop $0
  ${If} $StartupMode == ${STARTUP_AT_LOGIN}
    ${NSD_SetState} $0 ${BST_CHECKED}
  ${Else}
    ${NSD_SetState} $0 ${BST_UNCHECKED}
  ${EndIf}
  ${NSD_OnClick} $0 OnStartupAtLoginChange

  ${NSD_CreateRadioButton} 10u 39u 300u 12u "Start manually. (Expert)"
  Pop $0
  ${If} $StartupMode == ${STARTUP_MANUALLY}
    ${NSD_SetState} $0 ${BST_CHECKED}
  ${Else}
    ${NSD_SetState} $0 ${BST_UNCHECKED}
  ${EndIf}
  ${NSD_OnClick} $0 OnStartupManuallyChange

  StrCpy $InstallInitialized "True"
  nsDialogs::Show
FunctionEnd


Function OnStartupAtLoginChange
  Pop $0
  ${NSD_GetState} $0 $1
  ${If} $1 == ${BST_CHECKED}
    StrCpy $StartupMode ${STARTUP_AT_LOGIN}
  ${EndIf}
FunctionEnd


Function OnStartupManuallyChange
  Pop $0
  ${NSD_GetState} $0 $1
  ${If} $1 == ${BST_CHECKED}
    StrCpy $StartupMode ${STARTUP_MANUALLY}
  ${EndIf}
FunctionEnd


Function StartFAH
  ${If} $StartupMode == ${STARTUP_AT_LOGIN}
    ExecShell "open" "$SMPROGRAMS\${PRODUCT_NAME}\fahclient.lnk"
  ${EndIf}
FunctionEnd


Function un.onInit
  ; Get Data Directory
  ReadRegStr $DataDir ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" \
    "DataDirectory"
FunctionEnd
