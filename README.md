# Morse-Tutor-W8BH---DF2OF
Device to learn Morse Code easily. Based on the orininal Design from W8BH with ESP32 Wroom Dev Kit.
It uses the latest PCB from W8BH and the parts list.
A major update has been done to the Software, adapting it in parts to EU and German CWOPS course requirements.
New menues have been added to allow for Farnsworth timing, inputdelay and extra word spacing as well as other improvements.

A suitable case for the Morse Tutor can be found on Thingiverse:
https://www.thingiverse.com/thing:5635482

Improvements/Changes to the original Software:

2025:              Adapted to run on ESP DEV Kit V3 ESP32-WROOM-32 
                -  SD card option deleted.

01-2026:           Major functional updates:
                -  Seperate Farnsworth and Speed entry and use it in Receive and Send. 
                -  Display of Farnsworth/Speed setting implemented.
                -  Interruption of output (e.g. QSO) enabled by pressing encoder. 
                -  Entry timing depends on Farnsworth/Speed (allows more time).
                -  Added EU calls and German names.
                -  Input Delay function added. 
                -  Farnsworth - Char Speed delta defined: max. 19 WPM
                -  Cleanup: Removed obsolete Wifi and Two-way menu/code.
                -  Fixed exit behavior in Practice Sending (immediate return on button press).
                -  Removed intrusive Speed/Farnsworth popup window (values shown in status bar only).
                -  Menu structure: Moved 'Flashcards' to Receive menu.
                -  Menu structure: Added sub-menu for Practice (Select: Koch Trainer vs. Free Sending).
                -  New Feature: 'Free Sending' mode implemented (validates Morse input, Green=OK / Red=Error).
                -  UI: Increased font size to 3 for 'Copy' exercises and 'Free Sending'.
                -  UI: Optimized line wrapping and screen clearing for larger fonts to maximize screen usage.
                -  Arduino IDE 2.3.7

02-2026:           Minor updates:
                -  Squeeze function added to FREEPRACTICE menu 
                -  Fixed screen reset in receive mode
                -  Updated word list (most used QSO expessions) 
