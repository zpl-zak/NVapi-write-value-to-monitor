#SingleInstance Force     ; Allow only one copy of this script
#NoEnv
#Persistent

; Prevent multiple concurrent runs of the same executable
Global writeIsRunning := false

^!d::  ; Ctrl + Alt + D for DisplayPort
    if (writeIsRunning)
        return
    writeIsRunning := true
    RunWait, %ComSpec% /c .\writeValueToDisplay.exe -1 0xD0 0xF4 0x50, , Hide
    writeIsRunning := false
return

^!m::  ; Ctrl + Alt + M for HDMI
    if (writeIsRunning)
        return
    writeIsRunning := true
    RunWait, %ComSpec% /c .\writeValueToDisplay.exe -1 0x90 0xF4 0x50, , Hide
    writeIsRunning := false
return

^!k::  ; Ctrl + Alt + K for HDMI-2
    if (writeIsRunning)
        return
    writeIsRunning := true
    RunWait, %ComSpec% /c .\writeValueToDisplay.exe -1 0x91 0xF4 0x50, , Hide
    writeIsRunning := false
return
