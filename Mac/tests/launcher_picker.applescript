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
            if operation is "bounds" then return (position of window 1) & (size of window 1)
            if operation is "inspect" then
                set details to (name of every window as text) & return
                tell splitter group 1 of window 1
                    set details to details & "labels=" & (value of every static text as text) & return
                    set details to details & "buttons=" & (name of every button as text) & return
                    set details to details & "fields=" & (value of every text field as text)
                end tell
                set entries to entire contents of splitter group 1 of splitter group 1 of window 1
                repeat with entry in entries
                    try
                        set details to details & return & (role of entry) & " | " & (name of entry as text) & " | " & (value of entry as text)
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
                if (item 5 of argv) is "Choose a world from this modpack's saves folder" then
                    keystroke (item 6 of argv)
                else
                    keystroke (item 3 of argv)
                end if
                key code 36
                repeat 30 times
                    if not (exists sheet 1 of window 1) then exit repeat
                    delay 0.1
                end repeat
                if exists sheet 1 of window 1 then error "Go to Folder did not finish."
                if (item 5 of argv) is "Choose a world from this modpack's saves folder" then
                keystroke "2" using command down
                delay 0.5
                set foundRow to false
                set entries to entire contents of splitter group 1 of splitter group 1 of window 1
                repeat with entry in entries
                    try
                        if (value of entry) is (item 7 of argv) then
                            set rowElement to contents of entry
                            repeat 5 times
                                if (role of rowElement) is "AXRow" then exit repeat
                                set rowElement to value of attribute "AXParent" of rowElement
                            end repeat
                            if (role of rowElement) is not "AXRow" then error "Filename has no native row."
                            select rowElement
                            set foundRow to true
                            exit repeat
                        end if
                    end try
                end repeat
                if not foundRow then error "The selected folder/file row is absent: " & (item 7 of argv)
                end if
                delay 0.3
                set panelPosition to position of window 1
                set panelSize to size of window 1
                set panelRectangle to ((item 1 of panelPosition) as text) & "," & (item 2 of panelPosition) & "," & (item 1 of panelSize) & "," & (item 2 of panelSize)
                do shell script "/usr/sbin/screencapture -x -R" & panelRectangle & " " & quoted form of (item 8 of argv)
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
