; Defines
!define PRODUCT_NAME            "FAHClient"
!define PROJECT_NAME            "Folding@home"
!define DISPLAY_NAME            "Folding@home Client"

!define WEB_CLIENT_URL          "https://app.foldingathome.org/"

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
!define PRODUCT_VERSION         "%(version)s"
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

!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start Folding@home"
!define MUI_FINISHPAGE_RUN_FUNCTION OnRunFAH
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
;!define MUI_FINISHPAGE_NOAUTOCLOSE ; uncomment for install details
!insertmacro MUI_PAGE_FINISH

!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_TITLE "Instructions"
!define MUI_COMPONENTSPAGE_TEXT_DESCRIPTION_INFO \
  "You may choose to save your configuration and work unit data for later \
   use or completely uninstall."
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English" ; (default language listed first)
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Norwegian"
!insertmacro MUI_LANGUAGE "NorwegianNynorsk"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "Greek"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Croatian"
!insertmacro MUI_LANGUAGE "Bulgarian"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Latvian"
!insertmacro MUI_LANGUAGE "Macedonian"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Slovenian"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Farsi"
!insertmacro MUI_LANGUAGE "Hebrew"
!insertmacro MUI_LANGUAGE "Indonesian"
!insertmacro MUI_LANGUAGE "Mongolian"
!insertmacro MUI_LANGUAGE "Luxembourgish"
!insertmacro MUI_LANGUAGE "Albanian"
!insertmacro MUI_LANGUAGE "Breton"
!insertmacro MUI_LANGUAGE "Belarusian"
!insertmacro MUI_LANGUAGE "Icelandic"
!insertmacro MUI_LANGUAGE "Malay"
!insertmacro MUI_LANGUAGE "Bosnian"
!insertmacro MUI_LANGUAGE "Kurdish"
!insertmacro MUI_LANGUAGE "Irish"
!insertmacro MUI_LANGUAGE "Uzbek"
!insertmacro MUI_LANGUAGE "Galician"
!insertmacro MUI_LANGUAGE "Afrikaans"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Esperanto"
!insertmacro MUI_LANGUAGE "Asturian"
!insertmacro MUI_LANGUAGE "Basque"
!insertmacro MUI_LANGUAGE "Pashto"
!insertmacro MUI_LANGUAGE "ScotsGaelic"
!insertmacro MUI_LANGUAGE "Georgian"
!insertmacro MUI_LANGUAGE "Vietnamese"
!insertmacro MUI_LANGUAGE "Welsh"
!insertmacro MUI_LANGUAGE "Armenian"
!insertmacro MUI_LANGUAGE "Corsican"
!insertmacro MUI_LANGUAGE "Tatar"
!insertmacro MUI_LANGUAGE "Hindi"
!insertmacro MUI_RESERVEFILE_LANGDLL

LicenseLangString MUILicense ${LANG_ENGLISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_FRENCH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_GERMAN} ".\Resources\GPL3_German_de.rtf"
LicenseLangString MUILicense ${LANG_SPANISH} ".\Resources\GPL3_Spanish_es.rtf"
LicenseLangString MUILicense ${LANG_SPANISHINTERNATIONAL} ".\Resources\GPL3_Spanish_es.rtf"
LicenseLangString MUILicense ${LANG_SIMPCHINESE} ".\Resources\GPL3_SimpChinese_zh-cn.rtf"
LicenseLangString MUILicense ${LANG_TRADCHINESE} ".\Resources\GPL3_TradChinese_zh-tw.rtf"
LicenseLangString MUILicense ${LANG_JAPANESE} ".\Resources\GPL3_Japanese_ja.rtf"
LicenseLangString MUILicense ${LANG_KOREAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ITALIAN} ".\Resources\GPL3_Italian_it.rtf"
LicenseLangString MUILicense ${LANG_DUTCH} ".\Resources\GPL3_Dutch_nl.rtf"
LicenseLangString MUILicense ${LANG_DANISH} ".\Resources\GPL3_Danish_da.rtf"
LicenseLangString MUILicense ${LANG_SWEDISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_NORWEGIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_NORWEGIANNYNORSK} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_FINNISH} ".\Resources\GPL3_Finnish_fi.rtf"
LicenseLangString MUILicense ${LANG_GREEK} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_RUSSIAN} ".\Resources\GPL3_Russian_ru.rtf"
LicenseLangString MUILicense ${LANG_PORTUGUESE} ".\Resources\GPL3_PortugueseBR_pt-br.rtf"
LicenseLangString MUILicense ${LANG_PORTUGUESEBR} ".\Resources\GPL3_PortugueseBR_pt-br.rtf"
LicenseLangString MUILicense ${LANG_POLISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_UKRAINIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_CZECH} ".\Resources\GPL3_Czech_cs.rtf"
LicenseLangString MUILicense ${LANG_SLOVAK} ".\Resources\GPL3_Slovak_sk.rtf"
LicenseLangString MUILicense ${LANG_CROATIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_BULGARIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_HUNGARIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_THAI} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ROMANIAN} ".\Resources\GPL3_Romanian_ro.rtf"
LicenseLangString MUILicense ${LANG_LATVIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_MACEDONIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ESTONIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_TURKISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_LITHUANIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_SLOVENIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_SERBIAN} ".\Resources\GPL3_Serbian_sr.rtf"
LicenseLangString MUILicense ${LANG_SERBIANLATIN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ARABIC} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_FARSI} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_HEBREW} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_INDONESIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_MONGOLIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_LUXEMBOURGISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ALBANIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_BRETON} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_BELARUSIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ICELANDIC} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_MALAY} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_BOSNIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_KURDISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_IRISH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_UZBEK} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_GALICIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_AFRIKAANS} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_CATALAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ESPERANTO} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ASTURIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_BASQUE} ".\Resources\GPL3_Basque_eu.rtf"
LicenseLangString MUILicense ${LANG_PASHTO} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_SCOTSGAELIC} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_GEORGIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_VIETNAMESE} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_WELSH} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_ARMENIAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_CORSICAN} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_TATAR} "${PRODUCT_LICENSE}"
LicenseLangString MUILicense ${LANG_HINDI} "${PRODUCT_LICENSE}"

; [Use UTF-8 BOM encoding in Notepad++] Translated: "Automatically start at login (Recommended)"
LangString StartupText ${LANG_ENGLISH} "Adds: HideConsole.exe to Windows Startup Apps" ; Additional info for English
LangString StartupText ${LANG_FRENCH} "Démarrer automatiquement à la connexion (recommandé)"
LangString StartupText ${LANG_GERMAN} "Beim Login automatisch starten (empfohlen)"
LangString StartupText ${LANG_SPANISH} "Iniciar automáticamente al iniciar sesión (recomendado)"
LangString StartupText ${LANG_SPANISHINTERNATIONAL} "Iniciar automáticamente al iniciar sesión (recomendado)"
LangString StartupText ${LANG_SIMPCHINESE} "登录时自动启动（推荐）"
LangString StartupText ${LANG_TRADCHINESE} "登錄時自動啟動（推薦）"
LangString StartupText ${LANG_JAPANESE} "ログイン時に自動的に開始 (推奨)"
LangString StartupText ${LANG_KOREAN} "로그인 시 자동 시작(권장)"
LangString StartupText ${LANG_ITALIAN} "Avvia automaticamente all'accesso (consigliato)"
LangString StartupText ${LANG_DUTCH} "Automatisch starten bij inloggen (aanbevolen)"
LangString StartupText ${LANG_DANISH} "Start automatisk ved login (anbefales)"
LangString StartupText ${LANG_SWEDISH} "Starta automatiskt vid inloggning (rekommenderas)"
LangString StartupText ${LANG_NORWEGIAN} "Start automatisk ved pålogging (anbefalt)"
LangString StartupText ${LANG_NORWEGIANNYNORSK} "Start automatisk ved pålogging (anbefalt)"
LangString StartupText ${LANG_FINNISH} "Aloita automaattisesti sisäänkirjautumisen yhteydessä (suositus)"
LangString StartupText ${LANG_GREEK} "Αυτόματη έναρξη κατά τη σύνδεση (Συνιστάται)"
LangString StartupText ${LANG_RUSSIAN} "Автоматически запускать при входе в систему (рекомендуется)"
LangString StartupText ${LANG_PORTUGUESE} "Iniciar automaticamente no login (recomendado)"
LangString StartupText ${LANG_PORTUGUESEBR} "Iniciar automaticamente no login (recomendado)"
LangString StartupText ${LANG_POLISH} "Automatycznie uruchamiaj przy logowaniu (zalecane)"
LangString StartupText ${LANG_UKRAINIAN} "Автоматично запускати під час входу (рекомендовано)"
LangString StartupText ${LANG_CZECH} "Automaticky spustit při přihlášení (doporučeno)"
LangString StartupText ${LANG_SLOVAK} "Automaticky spustiť pri prihlásení (odporúča sa)"
LangString StartupText ${LANG_CROATIAN} "Automatski pokreni pri prijavi (preporučeno)"
LangString StartupText ${LANG_BULGARIAN} "Автоматично стартиране при влизане (препоръчително)"
LangString StartupText ${LANG_HUNGARIAN} "Automatikus indítás bejelentkezéskor (ajánlott)"
LangString StartupText ${LANG_THAI} "เริ่มต้นโดยอัตโนมัติเมื่อเข้าสู่ระบบ (แนะนำ)"
LangString StartupText ${LANG_ROMANIAN} "Începe automat la conectare (recomandat)"
LangString StartupText ${LANG_LATVIAN} "Automātiski sākt, piesakoties (ieteicams)"
LangString StartupText ${LANG_MACEDONIAN} "Автоматски старт при најавување (препорачано)"
LangString StartupText ${LANG_ESTONIAN} "Automaatne käivitamine sisselogimisel (soovitatav)"
LangString StartupText ${LANG_TURKISH} "Oturum açıldığında otomatik olarak başlat (Önerilen)"
LangString StartupText ${LANG_LITHUANIAN} "Automatiškai pradėti prisijungus (rekomenduojama)"
LangString StartupText ${LANG_SLOVENIAN} "Samodejni zagon ob prijavi (priporočeno)"
LangString StartupText ${LANG_SERBIAN} "Аутоматски почни при пријављивању (препоручено)"
LangString StartupText ${LANG_SERBIANLATIN} "Automatice incipit a login (commendatur)"
LangString StartupText ${LANG_ARABIC} "ابدأ تلقائيًا عند تسجيل الدخول (موصى به)"
LangString StartupText ${LANG_FARSI} "شروع خودکار هنگام ورود به سیستم (توصیه می شود)"
LangString StartupText ${LANG_HEBREW} "התחל אוטומטית בכניסה (מומלץ)"
LangString StartupText ${LANG_INDONESIAN} "Mulai secara otomatis saat masuk (Disarankan)"
LangString StartupText ${LANG_MONGOLIAN} "Нэвтрэх үед автоматаар эхлэх (Санал болгосон)"
LangString StartupText ${LANG_LUXEMBOURGISH} "Start automatesch beim Login (recommandéiert)"
LangString StartupText ${LANG_ALBANIAN} "Filloni automatikisht në hyrje (rekomandohet)"
LangString StartupText ${LANG_BRETON} "Loc' hañ emgefreek pa ereañ (Doarezet)"
LangString StartupText ${LANG_BELARUSIAN} "Аўтаматычны запуск пры ўваходзе (рэкамендуецца)"
LangString StartupText ${LANG_ICELANDIC} "Byrja sjálfkrafa við innskráningu (ráðlagt)"
LangString StartupText ${LANG_MALAY} "Mula secara automatik semasa log masuk (Disyorkan)"
LangString StartupText ${LANG_BOSNIAN} "Automatski start pri prijavi (preporučeno)"
LangString StartupText ${LANG_KURDISH} "Di têketinê de bixweber dest pê bike (Pêşniyar kirin)"
LangString StartupText ${LANG_IRISH} "Tosaigh go huathoibríoch ag logáil isteach (Molta)"
LangString StartupText ${LANG_UZBEK} "Kirish paytida avtomatik ishga tushirish (Tavsiya etiladi)"
LangString StartupText ${LANG_GALICIAN} "Iniciar automaticamente ao iniciar sesión (recomendado)"
LangString StartupText ${LANG_AFRIKAANS} "Begin outomaties by aanmelding (aanbeveel)"
LangString StartupText ${LANG_CATALAN} "Comença automàticament a l'inici de sessió (recomanat)"
LangString StartupText ${LANG_ESPERANTO} "Aŭtomate komenci ĉe ensaluto (Rekomendita)"
LangString StartupText ${LANG_ASTURIAN} "Aniciar automáticamente al aniciar sesión (Recomendáu)"
LangString StartupText ${LANG_BASQUE} "Hasi automatikoki saioa hasten denean (gomendatua)"
LangString StartupText ${LANG_PASHTO} "په اوتومات ډول په ننوتلو پیل وکړئ (سپارښتنه)"
LangString StartupText ${LANG_SCOTSGAELIC} "Tòisich gu fèin-ghluasadach aig logadh a-steach (Air a mholadh)"
LangString StartupText ${LANG_GEORGIAN} "ავტომატური დაწყება შესვლისას (რეკომენდებულია)"
LangString StartupText ${LANG_VIETNAMESE} "Tự động bắt đầu khi đăng nhập (Được khuyến nghị)"
LangString StartupText ${LANG_WELSH} "Cychwyn yn awtomatig wrth fewngofnodi (Argymhellir)"
LangString StartupText ${LANG_ARMENIAN} "Ավտոմատ կերպով սկսել մուտքից (Առաջարկվում է)"
LangString StartupText ${LANG_CORSICAN} "Cumincià automaticamente à u login (Consigliatu)"
LangString StartupText ${LANG_TATAR} "Логиннан автоматик рәвештә башлау (Тәкъдим ителә)"
LangString StartupText ${LANG_HINDI} "लॉगिन पर स्वचालित रूप से प्रारंभ करें (अनुशंसित)"

; [Use UTF-8 BOM encoding in Notepad++] Translated: "Data". If the same, then use " "
LangString DataText ${LANG_ENGLISH} "Storage" ; Additional info for English
LangString DataText ${LANG_FRENCH} "Données"
LangString DataText ${LANG_GERMAN} "Daten"
LangString DataText ${LANG_SPANISH} "Datos"
LangString DataText ${LANG_SPANISHINTERNATIONAL} "Datos"
LangString DataText ${LANG_SIMPCHINESE} "数据"
LangString DataText ${LANG_TRADCHINESE} "數據"
LangString DataText ${LANG_JAPANESE} "データ"
LangString DataText ${LANG_KOREAN} "데이터"
LangString DataText ${LANG_ITALIAN} "Dati"
LangString DataText ${LANG_DUTCH} "Gegevens"
LangString DataText ${LANG_DANISH} " "
LangString DataText ${LANG_SWEDISH} " "
LangString DataText ${LANG_NORWEGIAN} " "
LangString DataText ${LANG_NORWEGIANNYNORSK} " "
LangString DataText ${LANG_FINNISH} " "
LangString DataText ${LANG_GREEK} "Δεδομένα"
LangString DataText ${LANG_RUSSIAN} "Данные"
LangString DataText ${LANG_PORTUGUESE} "Dados"
LangString DataText ${LANG_PORTUGUESEBR} "Dados"
LangString DataText ${LANG_POLISH} "Dane"
LangString DataText ${LANG_UKRAINIAN} "дані"
LangString DataText ${LANG_CZECH} " "
LangString DataText ${LANG_SLOVAK} "Údaje"
LangString DataText ${LANG_CROATIAN} "Podaci"
LangString DataText ${LANG_BULGARIAN} "Данни"
LangString DataText ${LANG_HUNGARIAN} "Adat"
LangString DataText ${LANG_THAI} "ข้อมูล"
LangString DataText ${LANG_ROMANIAN} "Date"
LangString DataText ${LANG_LATVIAN} "Dati"
LangString DataText ${LANG_MACEDONIAN} "Податоци"
LangString DataText ${LANG_ESTONIAN} "Andmed"
LangString DataText ${LANG_TURKISH} "Veri"
LangString DataText ${LANG_LITHUANIAN} "Duomenys"
LangString DataText ${LANG_SLOVENIAN} "podatki"
LangString DataText ${LANG_SERBIAN} "Подаци"
LangString DataText ${LANG_SERBIANLATIN} " "
LangString DataText ${LANG_ARABIC} "بيانات"
LangString DataText ${LANG_FARSI} "داده ها"
LangString DataText ${LANG_HEBREW} "נתונים"
LangString DataText ${LANG_INDONESIAN} " "
LangString DataText ${LANG_MONGOLIAN} "Өгөгдөл"
LangString DataText ${LANG_LUXEMBOURGISH} "Daten"
LangString DataText ${LANG_ALBANIAN} "Të dhënat"
LangString DataText ${LANG_BRETON} "Roadoù"
LangString DataText ${LANG_BELARUSIAN} "даныя"
LangString DataText ${LANG_ICELANDIC} "Gögn"
LangString DataText ${LANG_MALAY} " "
LangString DataText ${LANG_BOSNIAN} "Podaci"
LangString DataText ${LANG_KURDISH} "Jimare"
LangString DataText ${LANG_IRISH} "Sonraí"
LangString DataText ${LANG_UZBEK} "Ma'lumotlar"
LangString DataText ${LANG_GALICIAN} "Datos"
LangString DataText ${LANG_AFRIKAANS} " "
LangString DataText ${LANG_CATALAN} "Dades"
LangString DataText ${LANG_ESPERANTO} "Datumoj"
LangString DataText ${LANG_ASTURIAN} "Datos"
LangString DataText ${LANG_BASQUE} "Datuak"
LangString DataText ${LANG_PASHTO} "ډاټا"
LangString DataText ${LANG_SCOTSGAELIC} "Dàta"
LangString DataText ${LANG_GEORGIAN} "მონაცემები"
LangString DataText ${LANG_VIETNAMESE} "Dữ liệu"
LangString DataText ${LANG_WELSH} " "
LangString DataText ${LANG_ARMENIAN} "Տվյալներ"
LangString DataText ${LANG_CORSICAN} "Dati"
LangString DataText ${LANG_TATAR} "Мәгълүмат"
LangString DataText ${LANG_HINDI} "जानकारी"

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
    DetailPrint "Removing service, if previously installed. (This can take awhile)"
    nsExec::Exec '"$UninstDir\${CLIENT_EXE}" --stop-service'
    nsExec::Exec '"$UninstDir\${CLIENT_EXE}" --uninstall-service'
skip_remove:

  ; Terminate
  Call CloseApps

  ; Remove Autostart
  Delete "$SMSTARTUP\${CLIENT_NAME}.lnk"
  Delete "$SMSTARTUP\FAHControl.lnk"

  ; Data directory
  CreateDirectory $DataDir
  ; Set working directory for files, etc. Do before AccessControl::GrantOnFile
  SetOutPath $DataDir
  AccessControl::GrantOnFile "$DataDir" "(S-1-5-32-545)" "FullAccess"

  ; Uninstall old software
  ; Avoid simply removing the whole directory as that causes subsequent file
  ; writes to fail for several seconds.
  DetailPrint "Uninstalling any conflicting Folding@home software"
  StrCmp $3 "v7" 0 skip_v7cleanup
  ; Copy old FAH v7 settings to new v8 DataDir, if in different locations
  StrCmp $DataDir $UnDataDir skip_copy_settings
    IfFileExists "$UnDataDir\config.xml" 0 skip_copy_settings
      DetailPrint "Copy old FAH settings to new location"
      CopyFiles "$UnDataDir\config.xml" "$DataDir\config.xml"
skip_copy_settings:

  MessageBox MB_YESNO "$(^RemoveFolder)$\r$\n$\r$\nFolding@home Data: $(DataText) \
    $\r$\n$UnDataDir" /SD IDYES IDNO skip_data_remove
  ; Remove sub-folders recursively
  DetailPrint "Removing old Folding@home data"
  RMDir /r "$UnDataDir\configs"
  RMDir /r "$UnDataDir\cores"
  RMDir /r "$UnDataDir\logs"
  RMDir /r "$UnDataDir\themes"
  RMDir /r "$UnDataDir\work"
  Delete "$UnDataDir\*"
  ; Only remove DataDir when empty. Avoid recursive remove to DataDir
  RMDir "$UnDataDir"
skip_data_remove:

  DetailPrint "Remove Folding@home program files"
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
    MessageBox MB_RETRYCANCEL "Failed to install files.  Most likely some \
        software, possibly Folding@home is currently using one or more files \
        that the installer is trying to upgrade.  Please stop all running \
        Folding@home software and quit any applications that are using files \
        from the Folding@home installation.  Note, complete shutdown can take \
        a little while after the application has closed." \
        IDRETRY install_files IDCANCEL abort

  ; Add to PATH
  ${EnvVarUpdate} $0 "PATH" "A" "HKCU" $INSTDIR

  ; Set working directory for shortcuts, etc.
  SetOutPath $DataDir

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
    MessageBox MB_ABORTRETRYIGNORE "Failed to create uninstaller" \
      IDABORT abort IDRETRY write_uninstaller

  ; Save uninstall information
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\${UNINSTALLER}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${CLIENT_ICON}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEBSITE}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_VENDOR}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\${CLIENT_EXE}"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory" $DataDir

  ; Delete old installed Start Menu links for Current and All Users
  ; Current User, C:\Users\%user%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs
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

  ; Program directory
remove_dir:
  ClearErrors
  Delete "$INSTDIR\*"
  ; Only remove INSTDIR when empty. Avoid recursive remove to INSTDIR
  RMDir "$INSTDIR"
  IfErrors 0 +3
    IfSilent +2
    MessageBox MB_RETRYCANCEL "Failed to remove $INSTDIR.  Please stop all \
      running Folding@home software." IDRETRY remove_dir
SectionEnd


Section /o un.Data
  ; Remove sub-folders recursively
  RMDir /r "$DataDir\cores"
  RMDir /r "$DataDir\credits"
  RMDir /r "$DataDir\logs"
  RMDir /r "$DataDir\work"
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
  ; Init
  ${If} $DataDir == ""
    ReadRegStr $DataDir HKLM "${PRODUCT_UNINST_KEY}" "DataDirectory"
    StrCpy $DataDir "$APPDATA\${PRODUCT_NAME}"
    StrCpy $AutoStart ${BST_CHECKED}
  ${EndIf}

  ; Page Title: "Custom" + ": Installation Options"
  ; Text from language strings in: ${NSISDIR}\Contrib\Language files\*.nlf
  !insertmacro MUI_HEADER_TEXT "$(^Custom)$(^ComponentsSubCaption)" ""

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
  ${NSD_GetText} $DataDirText $0
  ;Select Data Directory: "Destination Folder"
  nsDialogs::SelectFolderDialog $(^DirSubText) "$0"
  Pop $0
  ${If} $0 != error
    ${NSD_SetText} $DataDirText "$0"
  ${EndIf}
FunctionEnd


Function OnInstallPageLeave
  ; Validate install dir
  Push $INSTDIR
  Call ValidPath

  ; Validate data dir
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


Function OnAutoStartChange
  Pop $0
  ${NSD_GetState} $0 $AutoStart
FunctionEnd


Function OnRunFAH
  ; Also opens Web Control
  ExecShell "open" "${MENU_PATH}\Folding@home.lnk"
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
