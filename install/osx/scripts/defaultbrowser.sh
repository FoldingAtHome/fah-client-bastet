#!/bin/bash
# https://discussions.apple.com/thread/251602703

# return url-encoded default browser name that would open the URL

function default_browser () {
    osascript <<-AS
    use framework "AppKit"
    use AppleScript version "2.4"
    use scripting additions

    property NSWorkspace : a reference to current application's NSWorkspace
    property NSURL : a reference to current application's NSURL

    set wurl to NSURL's URLWithString:"https://www.apple.com"
    set thisBrowser to (NSWorkspace's sharedWorkspace)'s ¬
                        URLForApplicationToOpenURL:wurl
    set appname to (thisBrowser's absoluteString)'s lastPathComponent()'s ¬
                    stringByDeletingPathExtension() as text
    return appname as text
AS
    return
}

printf '%s\n' $(default_browser)
exit 0

