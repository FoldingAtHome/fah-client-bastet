Folding@home Client Changelog
=============================

## v8.1.10
 - Fixed copyright and version display in Windows about screen.  #94
 - Fixed bug in Windows/macOS networking timeout code.  #78
 - Added end screen to macOS install. (Kevin Bernhagen)
 - Fixed Windows systray pause.  #96

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
