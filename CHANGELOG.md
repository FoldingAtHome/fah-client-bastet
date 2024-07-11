Folding@home Client Changelog
=============================

## v8.4.0
 - Don't add client install path to ``PATH`` when running cores on Windows.

## v8.3.18
 - Windows installer fixes.

## v8.3.17
 - Fixes for account (un)linking and node changes.
 - Fix for repeated "No active" exception.

## v8.3.16
 - Fix Linux battery detection.  #240

## v8.3.15
 - Fix crash caused by failed DNS server. #235
 - Improved Linux battery detection.  #240

## v8.3.14
 - Fix Windows crash. #235

## v8.3.13
 - Fix for log updates to Web Control.

## v8.3.12
 - Fix Windows and macOS crashing.
 - Removed PDB file from Windows release mode installer.

## v8.3.11
 - Use LogTracker to follow log instead of reading back files.
 - Fixed core log following on macOS.  #234
 - Fix Windows crash.  #235
 - Fix Windows log rotation.  #233

## v8.3.10
 - Logging bug fixes.

## v8.3.9
 - Fixed ``'wu' not found`` error.  #231, #232
 - Other bug fixes.

## v8.3.7
 - Better DNS error handling and support for IPv6 name servers

## v8.3.6
 - Generate new client ID if machine ID has changed. #216
 - Rewrite of domain name lookup code. #223

## v8.3.5
 - Fix AMD GPU detection. #137
 - Add Lithuania macOS installer translation (muziqaz)
 - Fix for Windows battery status detection. #139
 - Fix for pause on battery.

## v8.3.4
 - Beta release

## v8.3.3
 - Fixed start failure on Windows #210

## v8.3.2
 - Added folding on battery option
 - Added keep awake option
 - Show correct count for systems with more than 64 logical CPUs on Windows.
 - Fix remote monitoring of log after log rotation.
 - Redirect old v7 Web Control
 - MacOS installer updates (kbernhagen)
 - Debian installer updates (mmello)

## v8.3.1
 - Updated copyrights.

## v8.3.0
 - Return of resource groups
 - Separate fold/pause buttons in Windows sys-tray
 - Fixed saving of local account config
 - Log dump record to correct directory
 - Fixed SSL error, cannot find SHA-256.
 - Fixed node broadcast messages.
 - 'Paused by user' -> 'Paused'
 - Provide full OS version in info
 - Set default 'cpus' closes #180

## v8.2.4
 - Organize credit logs by year/month. #59 (Kevin Bernhagen)
 - RPM build config. (Marcos Mello, Kevin Bernhagen)
 - Debian package improvements. (Marcos Mello, Kevin Bernhagen)

## v8.2.3
 - Potential fix for account link/unlink getting stuck.
 - Linux build uses older glibc for wider compatibility.

## v8.2.2
 - Windows: Fixed fail to start from installer or desktop icon.
 - Windows: Fixed "vector subscript out of range" crash.
 - Debian: Remove old systemd unit file if it exists.

## v8.2.1
 - Folding@home account login.
 - Remote access to machines via account and fah-node.
 - Removed resource groups feature.
 - Removed peers feature.  Replaced by fah-node access.
 - Added GPU specific beta mode and project key.
 - Improved Debian package.
 - Improved GPU detection.
 - Don't automatically reserve a CPU for each GPU

## v8.1.19
 - Close remote connection on Websocket close.

## v8.1.18
 - Added keep alive message to Websocket.

## v8.1.17
 - Fix GPU resource avaialble check. #135
 - Don't show ``1B of 1B`` for completed up/download size. #130
 - Only reset retry count if WU has run for more than 5 minutes. #134

## v8.1.16
 - Fix core download retry logic.
 - Only add client executable directory to lib path on Windows.
 - Retry WU if core crashes. #127
 - Fix CPU allocation when there are more GPUs than CPUs. #129
 - Don't reserve a CPU for each disabled GPUs.

## v8.1.15
 - Fix CUDA/OpenCL driver mixup from v8.1.14.
 - Improved OpenCL PCI info detection.

## v8.1.14
 - Print down/upload sizes in progress log. #113
 - Show user and team on WU request log line.
 - Show GPU PCI device/vendor and cuda/opencl support in log.
 - Add ``fah-client`` user to groups ``video`` and ``render`` on Linux. #121
 - Close log file before rotation to avoid problems on Windows. #120
 - Show unsupported GPUs.
 - Improved GPU detection.

## v8.1.13
 - Handle cores with ``.exe`` ending in Windows.

## v8.1.12
 - Rotate logs daily. #92
 - Keep up to 90 old logs by default.
 - Add ``fah-client.service`` to Linux tar.bz2 distribution.
 - Fix CPU reallocation bug during GPU WU assignment. #106

## v8.1.11
 - Added end screen to macOS install. (Kevin Bernhagen)
 - Prevent install on macOS if Safari is the only browser. (Kevin Bernhagen)
 - Fixed Windows systray pause.  #96
 - Pause client and prompt user if no user settings and not fold-anon.  #32

## v8.1.10
 - Fixed copyright and version display in Windows about screen.  #94
 - Fixed bug in Windows/macOS networking timeout code.  #78

## v8.1.9
 - Delay AS DNS lookup to avoid startup problems with no network. #84

## v8.1.8
 - Get a new assignment after ``HTTP_SERVICE_UNAVAILABLE``.
 - Removal of old logs fixed.  (Kevin Bernhagen)
 - Fixed network timeout error in Windows and macOS. #78
 - Retry WS assignment indefinately until WU paused. #79
 - Retry WU upload or dump up to 50 times.
 - Enable Linux service start on boot at install time. #81 (Kevin Bernhagen)

## v8.1.7
 - Fixed client ID generation.
 - Thread safe Windows event loop.  Fixes NULL pointer exception. #74
 - Fix assignment data corruption which causes ``HTTP_NOT_ACCEPTABLE`` error.
 - Fix problem with WUs moving to root resource group after restart. #68
 - Fix some bugs related to removing resource groups.

## v8.1.6
 - Fixed a memory leak in Linux builds.
 - Fixes for on idle handling.
 - Window installer improvements.  (Jeff Moreland)
 - Fixes for on idle for OSX.  (Kevin Bernhagen)
 - Force usage of older GLibC to allow Linux binaries to run on older systems.
 - Prevent RGs from loading new WUs before GPU detection is complete.

## v8.1.5
 - Fix data folder selection in Window installer.  (Jeff Moreland)

## v8.1.4
 - Fix "Finish" handling.
 - Stop wait timers on pause.
 - Use v7 settings in Windows install for all users. #45 (Jeff Moreland)
 - Windows installer translations. #48 (Jeff Moreland)
 - Don't delete other files on Windows uninstall. (Jeff Moreland)
 - Other Windows installer improvements. (Jeff Moreland)
 - Added resource groups feature. (Similar to slots in v7)

## v8.1.3
 - Load team and other numerical options from old ``config.xml`` correctly.
 - Fix for failing core downloads.
 - Fix for Windows install for all users.
 - Try CS if upload to WS fails.
 - Fixed WU stall after dns lookup failure.
 - Adjust CPU allocation rather than request new WU.

## v8.1.2
 - Fix Windows CPU features reporting.
 - Report CPU family, model and stepping to AS.

## v8.1.0
 - Front-end API changes.
 - Bug fixes
 - AS API changes.

## v8.0.0
 - Rewrite
