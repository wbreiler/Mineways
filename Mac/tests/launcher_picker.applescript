-- CI only. All UI actions target the one process started by the Python driver.
on run argv
    set operation to item 1 of argv
    if operation is "preflight" then
        tell application "System Events"
            if not UI elements enabled then error "Accessibility is unavailable. Use an already-authorized disposable runner. Do not change TCC."
            get name of menu bar 1 of process "Finder"
        end tell
        return "authorized"
    end if
    set targetPID to (item 2 of argv) as integer
    tell application "System Events"
        set matches to application processes whose unix id is targetPID
        if (count matches) is not 1 then error "The test process is absent."
        set targetProcess to item 1 of matches
        tell targetProcess
            if operation is "title" then return name of window 1
            if operation is "inspect" then
                set details to (name of every window as text) & return
                set elements to entire contents of window 1
                repeat with element in elements
                    try
                        set details to details & (role of element) & " | " & (name of element as text) & " | " & (value of element as text) & return
                    end try
                    if (length of details) > 11000 then exit repeat
                end repeat
                return details
            end if
            if operation is "menu" then
                set frontmost to true
                click menu item (item 3 of argv) of menu 1 of menu bar item "File" of menu bar 1
                return "clicked"
            end if
            if operation is "choose" then
                set frontmost to true
                if not frontmost then error "The test process is not frontmost."
                set panelGroup to splitter group 1 of window 1
                if not (exists static text (item 5 of argv) of panelGroup) then error "The expected picker caption is absent."
                set openButton to button "Open" of panelGroup
                keystroke "g" using {command down, shift down}
                delay 0.5
                if not (exists sheet 1 of window 1) then error "The Go to Folder sheet did not open."
                keystroke "a" using command down
                keystroke (item 3 of argv)
                key code 36
                delay 0.6
                click openButton
                return "selected"
            end if
            if operation is "poll-export" then
                set windowName to name of window 1
                if windowName contains "Original exporter test" then return "main"
                if windowName is "Export done" then
                    click button "OK" of window 1
                    return "dismissed-export"
                end if
                -- The app may still show its busy progress dialog during import.
                if windowName contains "Preparing" or windowName contains "Exporting" then return "busy: " & windowName
                error "Unexpected app dialog: " & windowName
            end if
        end tell
    end tell
    error "Unknown test operation."
end run
