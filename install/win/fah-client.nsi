; Defines
!define PRODUCT_NAME            "FAHClient"
!define PROJECT_NAME            "Folding@home"
!define DISPLAY_NAME            "Folding@home Client"

!define WEB_CLIENT_URL          "%(PACKAGE_URL)s"

!define CLIENT_NAME             "FAHClient"
!define CLIENT_EXE              "FAHClient.exe"
!define CLIENT_ICON             "FAHClient.ico"
!define CLIENT_HOME             "..\.."

!define MENU_PATH               "$SMPROGRAMS\${PROJECT_NAME}"

!define UNINSTALLER             "Uninstall.exe"

!define PRODUCT_CONFIG          "config.xml"
!define PRODUCT_LICENSE         "${CLIENT_HOME}\LICENSE"
!define PRODUCT_VENDOR          "foldingathome.org"
!define PRODUCT_TARGET          "${CLIENT_HOME}\%(package)s"
!define PRODUCT_VERSION         "%(PACKAGE_VERSION)s"
!define PRODUCT_WEBSITE         "https://foldingathome.org/"
!define PRODUCT_UNINST_KEY \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_DIR_REGKEY \
  "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}"

; Language page settings
!define MUI_LANGDLL_ALWAYSSHOW
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_LANGDLL_REGISTRY_ROOT "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_DIR_REGKEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
; Fix warning message for N/A page LangString for ${LANG_ASTURIAN} Components
!define MUI_COMPONENTSPAGE

; Installer settings
!define MUI_ABORTWARNING
!define MUI_ICON "${CLIENT_HOME}\images\fahlogo.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "Resources\Header-150x57.bmp"
!define MUI_HEADERIMAGE_BITMAP_NOSTRETCH
!define MUI_WELCOMEFINISHPAGE_BITMAP "Resources\Side-164x314.bmp"
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_TITLE_3LINES
; GPL doesn't require being accepted to only install the software
; https://www.gnu.org/licenses/gpl-faq.en.html#ClickThrough
!define MUI_LICENSEPAGE_TEXT_BOTTOM " "
; Change Accept to a Next button
!define MUI_LICENSEPAGE_BUTTON $(^NextBtn)

; Uninstaller settings
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_UNHEADERIMAGE
!define MUI_UNHEADERIMAGE_BITMAP "Resources\Header-150x57.bmp"
!define MUI_UNHEADERIMAGE_BITMAP_NOSTRETCH

; Additional Plugins and Include support
!addincludedir "Include"
!addplugindir "plugins\x86-unicode"
Unicode true ; For all languages to display correctly


; Variables
Var AutoStart
Var UninstDir
Var UnDataDir
Var DataDir
Var DataDirText


; Includes
!include MUI2.nsh
!include nsDialogs.nsh
!include LogicLib.nsh
!include EnvVarUpdate.nsh
!include WinVer.nsh
!include FileFunc.nsh ; File Functions Header used by RefreshShellIcons
!insertmacro RefreshShellIcons
!insertmacro un.RefreshShellIcons


; Config
Name "${DISPLAY_NAME} v${PRODUCT_VERSION}"
BrandingText "${DISPLAY_NAME} v${PRODUCT_VERSION}"
OutFile "${PRODUCT_TARGET}"
InstallDir "$PROGRAMFILES%(PACKAGE_ARCH)s\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
SetCompressor /SOLID lzma


; Pages
!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE "$(MUILicense)"
!insertmacro MUI_PAGE_DIRECTORY
Page custom OnInstallPageEnter OnInstallPageLeave
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\HideConsole.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS \
  "$\"$INSTDIR\${CLIENT_EXE}$\" --open-web-control"
!define MUI_FINISHPAGE_RUN_TEXT "Start Folding@home"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
;!define MUI_FINISHPAGE_NOAUTOCLOSE ; uncomment for install details
!insertmacro MUI_PAGE_FINISH

!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_TITLE "Instructions"
!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_INFO \
  "You may choose to save your configuration and work unit data for later \
   use or completely uninstall."
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!include MultiLang.nsh

RequestExecutionLevel admin


; Sections
Section -Install
  ReadRegStr $UninstDir HKLM "${PRODUCT_DIR_REGKEY}" "Path"

  ; Remove from PATH and Registry (FAH v7.x uninstaller was not run)
  IfFileExists "$UninstDir\FAHControl.exe" 0 skip_remove
    ; FAH v7.x is in 32 bit registry
    SetRegView 32
    ReadRegStr $UninstDir HKLM "${PRODUCT_DIR_REGKEY}" "Path"
    ReadRegStr $UnDataDir HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory"
    ${EnvVarUpdate} $0 "PATH" "R" "HKCU" $UninstDir
    DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    ; 32/64 bit registry
    SetRegView %(PACKAGE_ARCH)s
    ; Set a flag indicating overwriting FAH v7.x
    StrCpy $3 "v7"

  ; Remove service (FAH Client v7.x only commands)
  IfFileExists "$UninstDir\${CLIENT_EXE}" 0 skip_remove
    DetailPrint "$(^UninstallingSubText)$\r$\n$UninstDir\${CLIENT_EXE}$\r$\n \
      Removing service (This can take awhile)"
    nsExec::Exec '"$UninstDir\${CLIENT_EXE}" --stop-service'
    nsExec::Exec '"$UninstDir\${CLIENT_EXE}" --uninstall-service'
skip_remove:

  ; Terminate
  Call CloseApps

  ; Remove Autostart
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"
  Delete "$SMSTARTUP\FAHControl.lnk"

  ; Data directory
  CreateDirectory "$DataDir"
  ; Set working directory for files, etc. Do before AccessControl::GrantOnFile
  SetOutPath "$DataDir"
  AccessControl::GrantOnFile "$DataDir" "(S-1-5-32-545)" "FullAccess"

  ; Uninstall old software
  ; Avoid simply removing the whole directory as that causes subsequent file
  ; writes to fail for several seconds.
  StrCmp $3 "v7" 0 skip_v7cleanup
  ; Same or different data folder as FAH v7
  RMDir /r "$UnDataDir\cores"
  RMDir /r "$UnDataDir\themes"
  Delete "$UnDataDir\GPUs.txt"
  ; Different data folder, copy FAH v7 settings to new v8 data location
  ${If} $UnDataDir != $DataDir
    CopyFiles "$UnDataDir\config.xml" "$DataDir\config.xml"
    CopyFiles "$UnDataDir\FAHControl.db" "$DataDir\FAHControl.db"
    CopyFiles "$UnDataDir\log.txt" "$DataDir\log.txt"
    CopyFiles "$UnDataDir\configs\*.xml" "$DataDir\configs"
    CopyFiles "$UnDataDir\logs\*.txt" "$DataDir\logs"
    CopyFiles "$UnDataDir\work" "$DataDir\work"
    DetailPrint "Folding@home $(^UninstallingSubText)$\r$\n$UnDataDir"
    ; Remove sub-folders recursively
    RMDir /r "$UnDataDir\configs"
    RMDir /r "$UnDataDir\logs"
    RMDir /r "$UnDataDir\work"
    Delete "$UnDataDir\*.txt"
    Delete "$UnDataDir\*.xml"
    Delete "$UnDataDir\*.db"
    ; Only remove DataDir when empty. Avoid recursive remove to DataDir
    RMDir "$UnDataDir"
  ${EndIf}

  DetailPrint "Folding@home $(^UninstallingSubText)$\r$\n$UninstDir"
  ; Remove lib folder (FAH v7.x uninstaller was not run)
  RMDir /r "$UninstDir\lib"
skip_v7cleanup:
  ; Delete program files. Leave the folder, if it's the new location
  Delete "$UninstDir\*"
  StrCmp $INSTDIR $UninstDir +2
    RMDir "$UninstDir"

  ; Remove v8 settings
  ${EnvVarUpdate} $0 "PATH" "R" "HKCU" $UninstDir
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

  ; Install files
install_files:
  ClearErrors
  SetOverwrite try
  SetOutPath "$INSTDIR"
  File /oname=${CLIENT_EXE}  "${CLIENT_HOME}\fah-client.exe"
  File "${CLIENT_HOME}\HideConsole.exe"
  File /oname=${CLIENT_ICON} "${CLIENT_HOME}\images\fahlogo.ico"
  File /oname=README.txt     "${CLIENT_HOME}\README.md"
  File /oname=License.txt    "${CLIENT_HOME}\LICENSE"
  File /oname=ChangeLog.txt  "${CLIENT_HOME}\CHANGELOG.md"
  %(NSIS_INSTALL_FILES)s

  IfErrors 0 +2
    MessageBox MB_RETRYCANCEL "$(^CopyFailed)$(^DirSubCaption).  Most likely \
      some software, possibly Folding@home is currently using one or more \
      files that the installer is trying to upgrade.  Please stop all running \
      Folding@home software and quit any applications that are using files \
      from the Folding@home installation.  Note, complete shutdown can take \
      a little while after the application has closed." \
      IDRETRY install_files IDCANCEL abort

  ; Add to PATH
  ${EnvVarUpdate} $0 "PATH" "A" "HKCU" $INSTDIR

  ; Set working directory for shortcuts, etc.
  SetOutPath "$DataDir"

  ; Delete old desktop links for Current and All Users
  SetShellVarContext current
  Delete "$DESKTOP\FAHControl.lnk"
  Delete "$DESKTOP\Folding@home.lnk"
  Delete "$DESKTOP\Folding @Home.lnk"
  SetShellVarContext all
  Delete "$DESKTOP\FAHControl.lnk"
  Delete "$DESKTOP\Folding@home.lnk"
  Delete "$DESKTOP\Folding @Home.lnk"

  ; Desktop link
  CreateShortCut "$DESKTOP\Folding @Home.lnk" "$INSTDIR\HideConsole.exe" \
    '"$INSTDIR\${CLIENT_EXE}" --open-web-control' "$INSTDIR\${CLIENT_ICON}"

  ; Write uninstaller
write_uninstaller:
  ClearErrors
  WriteUninstaller "$INSTDIR\${UNINSTALLER}"
  IfErrors 0 +2
    MessageBox MB_ABORTRETRYIGNORE "$(^ErrorCreating) $(^UninstallCaption)" \
      IDABORT abort IDRETRY write_uninstaller

  ; Save uninstall information
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" \
    "UninstallString" "$INSTDIR\${UNINSTALLER}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" \
    "DisplayIcon" "$INSTDIR\${CLIENT_ICON}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEBSITE}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_VENDOR}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\${CLIENT_EXE}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory" $DataDir

  ; Delete old installed Start Menu links for Current and All Users
  ; Current User,
  ; C:\Users\<user>\AppData\Roaming\Microsoft\Windows\Start Menu\Programs
  SetShellVarContext current
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir /r "$SMPROGRAMS\${PROJECT_NAME}"
  ; All Users, C:\ProgramData\Microsoft\Windows\Start Menu\Programs
  SetShellVarContext all
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
  RMDir /r "$SMPROGRAMS\${PROJECT_NAME}" ; = ${MENU_PATH}
  ; Start Menu
  CreateDirectory "${MENU_PATH}"
  CreateShortCut "${MENU_PATH}\Folding@home.lnk" "$INSTDIR\HideConsole.exe" \
    '"$INSTDIR\${CLIENT_EXE}" --open-web-control' "$INSTDIR\${CLIENT_ICON}"
  CreateShortCut "${MENU_PATH}\Data Directory.lnk" "$DataDir"
  CreateShortCut "${MENU_PATH}\Homepage.lnk" "$INSTDIR\Homepage.url"
  CreateShortCut "${MENU_PATH}\Uninstall.lnk" "$INSTDIR\${UNINSTALLER}"

  ; Internet shortcuts
  WriteIniStr "$INSTDIR\Homepage.url" "InternetShortcut" "URL" \
    "${PRODUCT_WEBSITE}"

  ; Delete old Autostart link for Current and All Users
  SetShellVarContext current
  Delete "$SMSTARTUP\Folding@home.lnk"
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"
  SetShellVarContext all
  Delete "$SMSTARTUP\Folding@home.lnk"
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"
  ; Autostart
  ${If} $AutoStart == ${BST_CHECKED}
    CreateShortCut "$SMSTARTUP\Folding@home.lnk" "$INSTDIR\HideConsole.exe" \
      "$INSTDIR\${CLIENT_EXE}" "$INSTDIR\${CLIENT_ICON}"
  ${EndIf}

  ; Refresh desktop to cleanup any deleted desktop icons
  ${RefreshShellIcons}

  ; Set working directory for starting Folding@home, if selected on Finish page
  SetOutPath "$DataDir"

  Return

abort:
  Abort
SectionEnd


Section -un.Program
  ; Shutdown running client
  DetailPrint "Shutting down any local clients"

  ; Terminate
  Call un.CloseApps

  ; Menu
  RMDir /r "${MENU_PATH}"

  ; Autostart
  Delete "$SMSTARTUP\Folding@home.lnk"

  ; Desktop
  Delete "$DESKTOP\Folding @Home.lnk"

  ; Remove from PATH
  ${un.EnvVarUpdate} $0 "PATH" "R" "HKCU" $INSTDIR

  ; Registry
  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

  ; Refresh desktop to cleanup any deleted desktop icons
  ${un.RefreshShellIcons}

  ; Ensure the working path is different than the INSTDIR that may be deleted
  SetOutPath $TEMP

  ; Program directory
  Delete "$INSTDIR\ChangeLog.txt"
  Delete "$INSTDIR\FAHClient.exe"
  Delete "$INSTDIR\FAHClient.ico"
  Delete "$INSTDIR\HideConsole.exe"
  Delete "$INSTDIR\Homepage.url"
  Delete "$INSTDIR\License.txt"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\Uninstall.exe"
  ; Allow leaving the Install folder if the Data folder was stored in it
  ; Only remove INSTDIR when empty. Avoid recursive remove to INSTDIR
  RMDir "$INSTDIR"
SectionEnd


Section /o un.Data
  ; Remove sub-folders recursively
  RMDir /r "$DataDir\cores"
  RMDir /r "$DataDir\credits"
  RMDir /r "$DataDir\logs"
  RMDir /r "$DataDir\work"
  ; Remove additional FAH v7 copied data
  RMDir /r "$DataDir\configs"
  Delete "$DataDir\*"
  ; Only remove DataDir when empty. Avoid recursive remove to DataDir
  RMDir "$DataDir"
SectionEnd


; Functions
Function .onInit
  ${IfNot} ${AtLeastWin7}
    MessageBox MB_OK "Windows 7 or newer required"
    Quit
  ${EndIf}

  ; 32/64 bit registry
  SetRegView %(PACKAGE_ARCH)s

  SetShellVarContext all

  ; Ensure correct path. Overwriting FAH v7.x typically uses 32 bit path
  StrCpy "$INSTDIR" "$PROGRAMFILES%(PACKAGE_ARCH)s\${PRODUCT_NAME}"

  ; Language selection page
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd


!macro EndProcess un
Function ${un}EndProcess
  Exch $R1
  Push $R0
  Push $R2

  Retry:
    nsProcess::_FindProcess /NOUNLOAD "$R1"
    Pop $R0
    IntCmp $R0 0 0 0 End

    nsProcess::_KillProcess /NOUNLOAD "$R1"
    Pop $R0

    StrCpy $R2 0
    CheckAgain:
      Sleep 250 ; 1/4s
      nsProcess::_FindProcess /NOUNLOAD "$R1"
      Pop $R0
      IntCmp $R0 0 0 0 End
      IntOp $R2 $R2 + 1
      IntCmp $R2 120 0 CheckAgain ; Max 30s wait

    MessageBox MB_RETRYCANCEL "Please close $R1, and press 'Retry'. \
        $\r$\n$\r$\nNote: Folding@home maybe running in the system tray \
        in the lower right-hand corner of your screen." \
        /SD IDCANCEL IDCANCEL End IDRETRY Retry

  End:
  Pop $R2
  Pop $R0
  Pop $R1
  nsProcess::_Unload
FunctionEnd
!macroend

!insertmacro EndProcess ""
!insertmacro EndProcess "un."


Function CloseApps
  Push "${CLIENT_EXE}"
  Call EndProcess

  Push "FAHControl.exe"
  Call EndProcess
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


Function OnInstallPageEnter
  ClearErrors
  ; Init
  ${If} $DataDir == ""
    ReadRegStr $DataDir HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory"
    StrCpy $DataDir "$APPDATA\${PRODUCT_NAME}"
    StrCpy $AutoStart ${BST_CHECKED}
  ${EndIf}

  ; Page Title: "Folding@home" + ": Installation Options"
  ; Text from language strings in: ${NSISDIR}\Contrib\Language files\*.nlf
  !insertmacro MUI_HEADER_TEXT "Folding@home$(^ComponentsSubCaption)" ""

  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}

  ; Client Startup
  ${NSD_CreateCheckbox} 4%% 35u 98%% 24u \
    "Automatically start at login (Recommended)$\r$\n$(StartupText)"
  Pop $0
  ${NSD_SetState} $0 $AutoStart
  ${NSD_OnClick} $0 OnAutoStartChange

  ; Data Path: "Data" + "Destination Folder"
  ${NSD_CreateGroupBox} 0 76u 100%% 35u "Data: $(DataText) $(^DirSubText)"
  Pop $0

  ${NSD_CreateDirRequest} 4%% 90u 69%% 12u "$DataDir"
  Pop $DataDirText

  ${NSD_CreateBrowseButton} 76%% 88u 20%% 15u $(^BrowseBtn)
  Pop $0
  ${NSD_OnClick} $0 OnDataDirBrowse

  nsDialogs::Show
FunctionEnd


Function OnDataDirBrowse
  ClearErrors
  ${NSD_GetText} $DataDirText $4
  ; Select Data Directory: "Destination Folder"
  nsDialogs::SelectFolderDialog $(^DirSubText) "$4"
  Pop $4
  ${If} $4 != error
    ${NSD_SetText} $DataDirText "$4"
    StrCpy $DataDir $4
  ${EndIf}
FunctionEnd


Function OnInstallPageLeave
  ClearErrors
  ${NSD_GetText} $DataDirText $4
  ; Validate data dir from the text box
  Push $4
  Call ValidPath
  ; Try to create the Data directory, to avoid allowing an invalid drive
  CreateDirectory "$4"
  ${If} ${FileExists} "$4\*.*"
    StrCpy $DataDir $4
  ${Else}
    MessageBox MB_OK "$(^ErrorCreating)$4"
    Abort
  ${EndIf}
FunctionEnd


Function OnAutoStartChange
  Pop $0
  ${NSD_GetState} $0 $AutoStart
FunctionEnd


Function un.onInit
  ; 32/64 bit registry
  SetRegView %(PACKAGE_ARCH)s

  SetShellVarContext all

  ; Get Data Directory
  ReadRegStr $DataDir HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory"

  ; Use same language as installer
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd


Function un.CloseApps
  Push "${CLIENT_EXE}"
  Call un.EndProcess
FunctionEnd
