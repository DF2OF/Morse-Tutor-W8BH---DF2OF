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
                
04-2026:           Keyer Update:
                -  Added Ultimatic keyer mode (last-paddle priority).
                -  Updated Config/Key menu: Mode selection via Squeeze added.
                -  Rewrote paddle logic (paddleInput, getPatternFromPaddles) to support Ultimatic.
                -  Increased buffer sizes in mimic functions to prevent overflows with long QSO sentences.
                -  Last Menu setting is saved for next power on.

10-2026            Generic code improvements with Claude AI by DC0CM Claus Moessner:

                List of modified places (search for "Claude:" in the code):
                - Header (this section)
                - Variable inputDelay (initial value)                     [C2]
                - keyUp(), keyDown(), new function setToneFrequency()     [C7]
                - characterSpace(), wordSpace()                           [C4]
                - dit(), dah()                                            [C3]
                - sendMorseWord()                                         [C5]
                - straightKeyInput()                                      [C9]
                - copyWords(), headCopy(), new function pickCopyWord()    [C5]
                - hitTone(), missTone()                                   [C6]
                - loadConfig()                                            [C2]
                - checkConfig()                                           [C8]
                - useDefaults()                                           [C7]
                - setPitch()                                              [C7]
                - splashScreen()                                          [C11]
                - getPatternFromPaddles()                                 [C1, C10]
                - initMorse()                                             [C7]
                - Constant ULTIMATIC, variable lastPaddle                 [C12]
                - New functions updatePaddlePriority(), nextElement()     [C12]
                - dit(), dah(), paddleInput(), getPatternFromPaddles()    [C12]
                - configKey(), checkConfig()                              [C12]
                - Variables lastExercise, lastPractice, STARTUP_LAST      [C13]
                - saveConfig(), loadConfig(), checkConfig(), useDefaults()[C13]
                - runPracticeMenu(), loop(), new function isExercise()    [C13]
                - selectionString(), showMenuChoice(), changeStartup(),
                  new function validStartup()                             [C13, C14]
                - getPatternFromPaddles(), new function
                  getPatternFromStraightKey()                             [C15]
                - #include BLE..., variable useBluetooth, menu2 ("Bluetooth"),
                  new section "Bluetooth" before setup(), setup(), loop(),
                  saveConfig(), loadConfig(), checkConfig(), validStartup(),
                  STARTUP_LAST                                            [C16]
                - Bluetooth section: BtSecurityCallbacks, BtServerCallbacks,
                  startBluetooth(), setBluetooth(), setup()               [C16a-e]
                - displayFrame(), subMenu(), new function showSubMenuItem() [C17]
                - #define USE_SERIAL / DBG, setup(), printConfig()        [C18]
                - setup()                                                 [C19]
                - Variables callRaw, callWasReset, loadConfig(), checkConfig(),
                  setCallsign(), setup(), new function showCallRaw()      [C20]
                - startBluetooth(), BtServerCallbacks::onConnect()        [C21]
                - displayFrame(), subMenu(), showSubMenuItem(),
                  new function setupSubMenuGeometry()                     [C22]
                Note: requires the ESP32 Arduino core 3.3 or later (core 3.x was already
                      required for ledcAttach(); additionally uses ledcChangeFrequency() and the
                      revised BLE library of core 3.3).
                
                
              
