#SingleInstance Force     ; Allow only one copy of this script
#NoEnv
#Persistent

; Prevent multiple concurrent runs of the same executable
Global writeIsRunning := false

^!d::  ; Ctrl + Alt + D for DisplayPort
    if (writeIsRunning)
        return
    writeIsRunning := true
    RunWait, %ComSpec% /c .\writeValueToDisplay.exe 0 0xD0 0xF4 0x50, , Hide
    writeIsRunning := false
return

^!m::  ; Ctrl + Alt + M for HDMI
    if (writeIsRunning)
        return
    writeIsRunning := true
    RunWait, %ComSpec% /c .\writeValueToDisplay.exe 0 0x90 0xF4 0x50, , Hide
    writeIsRunning := false
return
