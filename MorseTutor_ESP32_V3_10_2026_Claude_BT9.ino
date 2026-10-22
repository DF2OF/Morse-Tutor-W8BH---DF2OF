/**************************************************************************
       Title:   Morse Tutor ESP32						   
      Author:   Bruce E. Hall, w8bh.net
        Date:   07 Jan 2022
    Hardware:   ESP32 DevBoard "HiLetGo", ILI9341 TFT display
    Software:   Arduino IDE 1.8.19
                ESP32 by Expressif Systems 1.0.6
                Adafruit GFX Library 1.5.3
                Adafruit ILI9341 1.5.6
       Legal:   Copyright (c) 2020  Bruce E. Hall.
                Open Source under the terms of the MIT License. 

                ----------------------------------------------------------------------------------
                Modifications by DF2OF, Matthias Rauhut

                >> 2025: Adapted to run on ESP DEV Kit V3 ESP32-WROOM-32 
                -  SD card option deleted.
                
                >> 01-2026: Major functional updates:
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
                >> 02-2026: Minor update
                -  Squeeze function added to FREEPRACTICE menu 
                -  Fixed screen reset in receive mode
                -  Word list in Receive mode updated (most used expressions in QSO)
                ----------------------------------------------------------------------------------
                >> 10-2026: Timing review and fixes by Claude (Anthropic)
                   on behalf of Claus Moessner. All modified places in the code
                   are marked with "Claude:".
                ----------------------------------------------------------------------------------

 ==================================================================================
  CLAUDE: GENERAL RATIONALE AND LIST OF CHANGES (10-2026)
 ==================================================================================

  Original problem: when sending, characters were sometimes decoded wrongly,
  and on playback the timing (dit/dah/spacing) was not always correct.
  The Morse table morse[] was checked character by character and is correct.
  The causes were in the timing and in some side effects:

  [C1] Character end in getPatternFromPaddles() (Free Sending, Koch Trainer send)
       Before: character end only after ditPeriod * inputDelay/5 (default 25 -> 5 dits)
       plus 1 dit already contained in dit()/dah() = approx. 6 dits of pause.
       A correct letter space, however, is only 3 dits. Cleanly sent letters
       were therefore merged into ONE pattern -> red "?".
       In addition, inputDelay meant something completely different in
       paddleInput() (word end after 25 dits). Two decoders, two completely
       different thresholds.
       New: character end fixed at 1.5 dits of additional pause (approx. 2.5 dits
       total, i.e. safely between the 1 dit element space and the 3 dit letter
       space). inputDelay/5 now only determines when a space (word end) is
       shown in Free Sending.

  [C2] Initial value inputDelay = 1
       With a blank EEPROM checkConfig() was never called, so inputDelay stayed 1.
       Result: in paddleInput() input ended after only 1 dit of pause, and in
       getPatternFromPaddles() characters fell apart (factor 0.2).
       New: initial value 25 (as in useDefaults), and loadConfig() always
       calls checkConfig().

  [C3] checkForSpeedChange() removed from dit() and dah()
       a) If the encoder was turned during an element, displayWPM() drew to the
          display via SPI (several ms). Tone or pause were stretched by this
          -> uneven elements.
       b) While sending yourself, isSending == false. A button press was
          "consumed" in checkForSpeedChange() (button_pressed = false) and
          silently toggled gap mode. Leaving an exercise was therefore
          unreliable.
       Speed changes are still handled in smartDelay() (between characters),
       in freePractice() and in practiceSending().
       Additionally: timing with unsigned long and differences (robust
       against millis() overflow) instead of "int finished = millis()+...".

  [C4] Farnsworth spacing corrected (characterSpace, wordSpace)
       extracharDit() returns the Farnsworth unit correctly (ARRL formula).
       Before: letter space = 1 dit + 2 Farnsworth units (correct: 3),
               word space   = 1 dit + 6 Farnsworth units (correct: 7).
       Example 18/10 WPM: letter space 480 ms instead of 621 ms (over 20% short).
       Without Farnsworth (character speed = overall speed) nothing changes.
       wordSpace() now uses smartDelay() instead of delay() -> can be aborted
       by button.

  [C5] Buffer overflow in copyWords() and headCopy()
       char text[10], but the word list contains e.g. "QTH IS BERLIN BERLIN"
       (20 characters). strcpy() wrote beyond the array onto the stack ->
       sporadic, unexplainable behaviour possible.
       Also, sendMorseWord() did not handle spaces (morse[-1] was sent), and
       words containing spaces could never be answered correctly in the Copy
       exercises (input ends at the first word end).
       New: larger buffer, safe copying, Copy exercises only pick words
       without spaces, sendMorseWord() handles spaces.

  [C6] hitTone() / missTone(): wrong LEDC API
       ledcWriteTone(0,...) / ledcWrite(0,0) come from ESP32 core 2.x
       (channel 0). In core 3.x the parameter is a PIN -> GPIO0: no tone,
       only error messages, but still 200-350 ms of blocking during which the
       paddles are not read. New: output via the AUDIO pin.

  [C7] Tone generation: set the frequency once, then only switch the duty cycle
       Before, keyDown() reconfigured the timer with ledcWriteTone() for EVERY
       element. New: setToneFrequency() sets the frequency once (startup,
       pitch changed, defaults); keyDown()/keyUp() only switch the duty
       cycle (50% / 0%). Less jitter, fewer clicks.

  [C8] checkConfig(): codeSpeed must not be greater than charSpeed
       Otherwise extracharDit() can become negative; smartDelay(negative) is
       interpreted as a huge unsigned long -> the device "hangs" until a
       button press. Additionally the maximum difference of 19 WPM is enforced.

  [C9] straightKeyInput(): variable "end" was not initialized
       The first key press could wrongly be discarded as contact bounce.

  [C10] getPatternFromPaddles(): after an abort due to an overly long character,
        ditRequest/dahRequest are cleared (no "phantom elements" afterwards).

  [C11] Splash screen: software date set to "Oct 2026" so that one can see
        after uploading that the new version is running.

  [C12] Ultimatic keyer adopted (idea from V3_04 by DF2OF, 04-2026)
        Ultimatic: on squeeze (both paddles pressed) the MOST RECENTLY pressed
        paddle wins and repeats. When it is released, the paddle still held
        takes over again. Selection in menu Config -> Key:
        dit = Iambic A, dah = Iambic B, squeeze = Ultimatic.
        Intentional differences from V3_04:
        - In Ultimatic mode the element memory is only set on a NEW press of
          the other paddle (edge). In V3_04 it was also set while the paddle
          was merely held; after releasing BOTH paddles an unwanted extra
          element followed (Iambic B behaviour).
        - The keyer decision lives in ONE function nextElement(), shared by
          paddleInput() and getPatternFromPaddles().
        - checkForSpeedChange() is still NOT called in dit()/dah()
          (see [C3]); V3_04 still had it there.
        - Squeeze detection in the Key menu uses a 250 ms window instead of
          30 ms, since two paddles can hardly be pressed within 30 ms.
        - If no paddle is pressed any more between the wait loop and the
          decision, getPatternFromPaddles() no longer counts an empty element.
        NOT adopted from V3_04 (only Ultimatic was requested):
        the modified encoder debouncing, saving of the last menu setting and
        the modified Copy Words / Head Copy functions (already solved here
        by [C5]).

  [C13] Remember last exercise and start it directly at power-on
        New startup option "Last Exercise" (Config -> Screen -> Startup).
        If selected, the tutor starts directly with the last used exercise
        after power-on. For "Practice" the last used sub-mode (Koch Trainer /
        Free Sending) is also remembered and started directly; in the
        Practice menu the cursor is always placed on the last used entry.
        Intentionally different from V3_04: there EVERY menu selection was
        saved as the start point, including config items. After
        "Config -> Defaults" the tutor would then have reset all settings at
        every power-on. Also, the fixed startup setting was constantly
        overwritten.
        Here only exercises from Receive and Send are remembered (never
        Config, never Exit), in separate EEPROM bytes (26, 27). The fixed
        startup choice is preserved. Data is only saved when it changes
        (reduces flash wear).

  [C14] Startup selection: invalid menu positions
        changeStartup() simply stepped from -1 to 27, including values
        without a menu entry (e.g. 9, 17, 18, 19). selectionString() then read
        beyond the end of the menu arrays (undefined memory, garbage on the
        display, a crash in the worst case). Invalid values are now skipped
        and selectionString() checks the bounds.

  [C15] Straight key in Free Sending and Koch Trainer (send)
        These two exercises read their input via getPatternFromPaddles().
        Already in the original (V3_02) this ALWAYS used the paddle keyer
        logic, even if a straight key was selected in Config -> Key. The keyer
        turns a held contact into a series of dits - so a dah could never be
        produced there with a straight key.
        (The Copy exercises use morseInput() -> straightKeyInput() and were
        not affected.)
        New: getPatternFromPaddles() branches to the new function
        getPatternFromStraightKey() when usePaddles == false. It measures the
        duration of each key press like straightKeyInput(): longer than
        2 dits = dah. Character end after 2 dits of pause, word end as with
        paddles (inputDelay/5). Short glitches below 10 ms are ignored
        (debouncing).

  [C16] Bluetooth keyboard (BLE): paddle states as Left Ctrl / Right Ctrl
        For programs such as "Remote CW Keyer" (DL4YHF) that read a paddle via
        the Ctrl keys of the PC keyboard. No driver and no additional software
        required: the tutor is paired once as Bluetooth keyboard "MorseTutor".
          Dit paddle (or straight key) = Left Ctrl, dah paddle = Right Ctrl.
        On/off: new menu item Config -> Bluetooth (turn encoder = ON/OFF,
        button = apply). On a change the tutor restarts. Default: OFF - then
        the Bluetooth stack is not started at all and the tutor behaves
        exactly as without this feature.
        Timing: the paddles are polled every 1 ms by a SEPARATE task on CPU
        core 0 (same value twice = debounced) and only changes are sent. The
        tutor code itself (core 1: keyer, tone, display, spacing) is NOT
        touched or delayed by this. The sidetone still comes directly from
        the tutor.
        Latency to the PC: typ. 10-30 ms (BLE connection interval); an
        interval of 7.5-15 ms is requested.
        Uses the BLE library included in ESP32 core 3.3 or later; no
        additional library needs to be installed.
        New EEPROM address 28 (useBluetooth). "Defaults" intentionally does
        not change the Bluetooth setting.
        The new menu item moves "Exit" in the Config menu; the startup option
        "Last Exercise" therefore now has the value 29 (previously 28). If
        "Last Exercise" was selected, the tutor starts once in the main menu
        after the update - simply select it again.
        "Exit" entries can no longer be selected as startup choice.
        [C16a] Fix: hid->manufacturer(String) sets the value of a
        characteristic that is only created by hid->manufacturer() ->
        access through an invalid pointer -> crash at startup with
        Bluetooth ON (reset loop). Now hid->manufacturer()->setValue().
        Emergency exit: holding the encoder button during power-on switches
        Bluetooth off.
        [C16b] Diagnostic output in the Serial Monitor (115200 baud):
        connected, disconnected (with reason), result of pairing.
        [C16d] The pairing setting changed in an interim version was
        reverted: "Secure Connections" falls back to legacy pairing anyway
        with Bluetooth 4.0 adapters; the change had only caused the tutor to
        no longer start encryption itself -> the connection dropped after a
        short time. Additionally the tutor now reports "NormallyConnectable"
        (hidInfo 0x03) so that Windows reconnects after power-on.
        [C16e] Diagnostics without a PC: menu Config -> Bluetooth shows
        "Conn:<number of connections> Auth:<OK/error code> Disc:<disconnect reason>".
        Tested: compiled with ESP32 core 3.3.11 (board "ESP32 Dev Module",
        default partition scheme). Core 3.2.x does NOT compile (the BLE
        library was revised in 3.3.0).

  [C17] Submenus: remnants of entries when scrolling
        Menus with more than 8 entries scroll (Receive already in the
        original, Config now because of "Bluetooth"). The entries differ in
        length and were overwritten without erasing -> remnants.
        New: showSubMenuItem() pads entries to 10 characters with spaces.

  [C18] Serial port can be disabled (#define USE_SERIAL, default 0 = off)
        With 0, Serial.begin() is not called, all output (EEPROM dump,
        Bluetooth diagnostics) is dropped and ESP-IDF system log messages are
        disabled. Uploading via USB still works (that is done by the
        bootloader, not the sketch). For diagnostics on the PC set USE_SERIAL
        to 1. Note: the ESP32 ROM bootloader always prints a few lines at
        power-on; this cannot be disabled in software and is harmless.

  [C19] Delayed Bluetooth start: Bluetooth is started at least 2 s after
        power-on (previously right after the "R"). At startup the ESP32
        calibrates its radio; if the supply ramps up slowly (e.g. switching
        regulator Mini360), calibration may happen while the voltage is not
        yet stable -> unreliable connection. The waiting time falls within
        the existing pause of the startup screen; power-on hardly takes
        longer than before.
        Note: software cannot delay the hardware reset itself; that would
        require a capacitor (1-10 uF) between the EN pin and GND.

  [C20] Callsign diagnostics (temporary): in menu Config -> Callsign the
        EEPROM bytes 8-17 are shown as hex at the bottom. If the callsign is
        found invalid at startup (-> W8BH), the startup screen shows
        "Call reset!" and the raw values read at the bottom.
        Also: myCall is always terminated with a null character.

  [C21] Maximum Bluetooth TX power: +9 dBm instead of +3 dBm (default of the
        BLE library), for advertising and connections. +6 dB roughly doubles
        the range. Higher peak current while transmitting (some 10 mA) - no
        problem with a clean 5 V supply.

  [C22] Submenus without scrolling: submenus may use the full screen height
        (9 rows at the normal row spacing) instead of stopping at y = 210.
        Receive and Config (9 entries each) now fit without scrolling. The
        9th row reaches the WPM line at the bottom; to avoid overlapping the
        WPM digits (x = 115..175), entries are padded only to the longest
        entry of the respective menu (Receive: 108 px wide at x = 0, Config
        starts at x = 200). MAXROW for text output is unchanged.

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
 ==================================================================================
    
 Description:   Practice sending & receiving morse code
                Inspired by Jack Purdum's "Morse Code Tutor"
                
                

 **************************************************************************/


//===================================  INCLUDES ========================================= 
#include "Adafruit_GFX.h"                         // Version 1.5.3
#include "Adafruit_ILI9341.h"                     // Version 1.5.6
#include "EEPROM.h"
#include <BLEDevice.h>                            // Claude: [C16] Bluetooth keyboard (library included in the ESP32 core)
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include "esp_log.h"                              // Claude: [C18]

// Claude: [C18] Enable/disable the serial port.
//   0 = completely off (default): Serial.begin() is not called, all
//       output is dropped and ESP-IDF system log messages are disabled.
//   1 = on (115200 baud) for diagnostics on the PC.
#define USE_SERIAL  0

#if USE_SERIAL
  #define DBG Serial
#else
  struct NoSerial {                               // placeholder: does nothing
    void begin(unsigned long) {}
    template<typename... T> void print(T...) {}
    template<typename... T> void println(T...) {}
    template<typename... T> void printf(T...) {}
  };
  static NoSerial noSerial;
  #define DBG noSerial
#endif

//===================================  Hardware Connections =============================
#define TFT_DC             21                     // Display "DC" pin
#define TFT_CS             05                     // Display "CS" pin
#define ENCODER_B          16                     // Rotary Encoder output A
#define ENCODER_A           4                     // Rotary Encoder output B
#define ENCODER_BUTTON     15                     // Rotary Encoder switch
#define PADDLE_A           32                     // Morse Paddle "dit"
#define PADDLE_B           33                     // Morse Paddle "dah"
#define AUDIO              13                     // Audio output
#define LED                 2                     // onboard LED pin
#define SCREEN_ROTATION     3                     // landscape mode: use '1' or '3'


//===================================  Morse Code Constants =============================
#define DEFAULTSPEED       13                     // character speed in Words per Minute
#define MAXSPEED           50                     // fastest morse speed in WPM
#define MINSPEED            3                     // slowest morse speed in WPM
#define DEFAULTPITCH     1200                     // default pitch in Hz of morse audio
#define MAXPITCH         2800                     // highest allowed pitch
#define MINPITCH          300                     // how low can you go
#define WORDSIZE            5                     // number of chars per random word
#define MAXWORDSPACES      99                     // maximum word delay, in spaces
#define FLASHCARDDELAY   2000                     // wait in mS between cards
// Rotary encoder resolution:
//   OneEdgePerCount   = every valid Gray-code transition is one count
//   TwoEdgesPerCount  = two transitions per count
//   FourEdgesPerCount = one count per complete 4-transition cycle
#define OneEdgePerCount     1
#define TwoEdgesPerCount    2
#define FourEdgesPerCount   4
#define ENCODER_RESOLUTION  FourEdgesPerCount
#define FNAMESIZE          15                     // max size of a filename
#define MAXFILES           20                     // max number of SD files recognized
#define IAMBIC_A            1                     // Iambic Keyer Mode A   (Claude: A/B comments were swapped)
#define IAMBIC_B            2                     // Iambic Keyer Mode B
#define ULTIMATIC           3                     // Claude: [C12] Ultimatic mode (from V3_04)
#define LONGPRESS        1000                     // hold-down time for long press, in mSec
#define SUPPRESSLED     false                     // if true, do not flash diagnostic LED

//===================================  Color Constants ==================================
#define BLACK          0x0000
#define BLUE           0x001F
#define NAVY           0x000F               
#define RED            0xF800
#define MAROON         0x7800 
#define GREEN          0x07E0
#define LIME           0x0400
#define CYAN           0x07FF
#define TEAL           0x03EF
#define PURPLE         0x780F                
#define PINK           0xF81F
#define YELLOW         0xFFE0
#define ORANGE         0xFD20
#define BROWN          0x79E0
#define WHITE          0xFFFF
#define OLIVE          0x7BE0
#define SILVER         0xC618        
#define GRAY           0x7BEF
#define DARKGRAY       0x1208

const word colors[] = {BLACK,BLUE,NAVY,RED,MAROON,GREEN,LIME,CYAN,TEAL,PURPLE,
  PINK,YELLOW,ORANGE,BROWN,WHITE,OLIVE,SILVER,GRAY,DARKGRAY};

// ==================================  Menu Constants ===================================
#define DISPLAYWIDTH      320                     // Number of LCD pixels in long-axis
#define DISPLAYHEIGHT     240                     // Number of LCD pixels in short-axis
#define TOPMARGIN          30                     // All submenus appear below top line
#define MENUSPACING       100                     // Width in pixels for each menu column

int     ROWSPACING = 22;
int     COLSPACING = 18;
int     MAXCOL;                                   // Number of characters per row    
int     MAXROW;                                   // Number of text-rows per screen
int     currentTextSize = 2;
#define FG              GREEN                     // Menu foreground color 
#define BG              BLACK                     // App background color
#define SELECTFG         BLUE                     // Selected Menu foreground color
#define SELECTBG        WHITE                     // Selected Menu background color
#define TEXTCOLOR      YELLOW                     // Default non-menu text color
#define TXCOLOR         WHITE                     // color of outgoing (sending) text
#define RXCOLOR         GREEN                     // color of incoming (received) text
#define ELEMENTS(x) (sizeof(x) / sizeof(x[0]))    // Handy macro for determining array sizes

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

//===================================  Rotary Encoder Variables =========================
volatile int      rotaryCounter    = 0;           // "position" of rotary encoder (increments CW) 
volatile boolean  button_pressed   = false;       // true if the button has been pushed
volatile boolean  button_released  = false;       // true if the button has been released (sets button_downtime)
volatile long     button_downtime  = 0L;          // ms the button was pushed before released
bool settingGapSpeed = false;
bool isSending = false;                           // remembers, if we do send

//===================================  Morse Code Variables =============================

                    // The following is a list of the 100 most-common English words, in frequency order.
                    // See: https://www.dictionary.com/e/common-words/
                    // from The Brown Corpus Standard Sample of Present-Day American English 
                    // (Providence, RI: Brown University Press, 1979)
                    
char *words[]     = {"AGN", "ANT", "BK", "BUERO", "B4", "CONDX", "CPI", "CPY", "CU", "CUAGN", "DE", "DR", "ES", "FB", "FER", "HI", "HR", "HW?", "IS", "NO", "NOT", "NIL", "OM", "PWR", "PSE", 
                     "R", "RPT", "NAME", "NAME?", "RIG", "RST", "SRI", "TEMP", "TNX", "TKS", "TU", "UR", "VY", "WX", "73", "AWDH", "LBR", "YL", "GM", "GE", "GD", "GL", "GD", "DX",
                     "QRG", "QRL", "QRU", "QRM", "QRN", "QRP", "QRS", "QRQ", "QRU", "QRV", "QRX", "QRZ", "QSB", "QSL", "QSO", "QSY", "QTH", "QTH?", "SUNNY", "CLEAR", "COLD", "DRY", "CLOUDY", "FINE", 
                     "HOT", "FOG", "WARM", "SUNNY", "STORMY", "FROST", "MINUS", "10C", "25C", "RST IS 5NN 5NN", "RST 559 55N", "CALL?", "QTH IS BERLIN BERLIN", "NR?", "LOC?", "RST?", "QTH?",
                     "QTH AGN", "PSE QTH AGN", "PSE NAME AGN"};
char *antenna[]   = {"YAGI", "DIPOLE", "VERTICAL", "HEXBEAM", "MAGLOOP"};
char *weather[]   = {"HOT", "SUNNY", "WARM", "CLOUDY", "RAINY", "COLD", "SNOWY", "CHILLY", "WINDY", "FOGGY"};
char *names[]     = {"STEFAN", "HANS", "THOMAS", "ULI", "ANDREAS", "PETER", "Uschi", "Claudia", "WAYNE", "TYE", "DARREN", "MICHAEL", "SARAH", "DOUG", "FERNANDO", "CHARLIE", "HOLLY",
                     "KEN", "SCOTT", "DAN", "ERVIN", "GENE", "PAUL", "VINCENT"};
char *cities[]    = {"BERLIN", "MUENCHEN", "HAMBURG", "KOELN", "WIEN", "ZURICH", "SONSBECK", "DAYTON, OH", "HADDONFIELD, NJ", "TOKYO", "TORONTO", "BUENOS AIRES", 
                     "BOULDER, CO", "FRIEDRICHSHAFEN", "MILANO", "MOSCOW", "COPENHAGEN", "BARCELONA"};
char *rigs[]      = {"YAESU FT-710", "KENWOOD TS-890S", "ELECRAFT K3", "HOMEBREW", "QRPLABS QMX", "ICOM 7410", "FLEX 6700"};
char punctuation[]= "!@$&()-+=,.:;'/";
char prefix[]     = {'A', 'W', 'K', 'N'};
char koch[]       = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X";
byte morse[] = {                                  // Each character is encoded into an 8-bit byte:
  0b01001010,        // ! exclamation        
  0b01101101,        // " quotation          
  0b01010111,        // # pound                   // No Morse, mapped to SK
  0b10110111,        // $ dollar or ~SX
  0b00000000,        // % percent
  0b00111101,        // & ampersand or ~AS
  0b01100001,        // ' apostrophe
  0b00110010,        // ( open paren
  0b01010010,        // ) close paren
  0b0,               // * asterisk                // No Morse
  0b00110101,        // + plus or ~AR
  0b01001100,        // , comma
  0b01011110,        // - hypen
  0b01010101,        // . period
  0b00110110,        // / slant   
  0b00100000,        // 0                         // Read the bits from RIGHT to left,   
  0b00100001,        // 1                         // with a "1"=dit and "0"=dah
  0b00100011,        // 2                         // example: 2 = 11000 or dit-dit-dah-dah-dah
  0b00100111,        // 3                         // the final bit is always 1 = stop bit.
  0b00101111,        // 4                         // see "sendElements" routine for more info.
  0b00111111,        // 5
  0b00111110,        // 6
  0b00111100,        // 7
  0b00111000,        // 8
  0b00110000,        // 9
  0b01111000,        // : colon
  0b01101010,        // ; semicolon
  0b0,               // <                         // No Morse
  0b00101110,        // = equals or ~BT
  0b0,               // >                         // No Morse
  0b01110011,        // ? question
  0b01101001,        // @ at or ~AC
  0b00000101,        // A 
  0b00011110,        // B
  0b00011010,        // C
  0b00001110,        // D
  0b00000011,        // E
  0b00011011,        // F
  0b00001100,        // G
  0b00011111,        // H
  0b00000111,        // I
  0b00010001,        // J
  0b00001010,        // K
  0b00011101,        // L
  0b00000100,        // M
  0b00000110,        // N
  0b00001000,        // O
  0b00011001,        // P
  0b00010100,        // Q
  0b00001101,        // R
  0b00001111,        // S
  0b00000010,        // T
  0b00001011,        // U
  0b00010111,        // V
  0b00001001,        // W
  0b00010110,        // X
  0b00010010,        // Y 
  0b00011100         // Z
};

int charSpeed   = DEFAULTSPEED;                   // speed at which characters are sent, in WPM
int codeSpeed   = DEFAULTSPEED;                   // overall code speed, in WPM
int ditPeriod   = 100;                            // length of a dit, in milliseconds
int ditPaddle   = PADDLE_A;                       // digital pin attached to dit paddle
int dahPaddle   = PADDLE_B;                       // digital pin attached to dah paddle
int pitch       = DEFAULTPITCH;                   // frequency of audio output, in Hz
int kochLevel   = 1;                              // current Koch lesson #
int score       = 0;                              // copy challange score
int hits        = 0;                              // copy challange correct #
int misses      = 0;                              // copy challange incorrect #
int xWordSpaces = 0;                              // extra spaces between words
int keyerMode   = IAMBIC_B;                       // current keyer mode
bool usePaddles = true;                          // if true, using paddles; if false, straight key
bool paused     = false;                          // if true, morse output is paused
bool ditRequest = false;                          // dit memory for iambic sending
bool dahRequest = false;                          // dah memory for iambic sending
byte lastPaddle = 0;                              // Claude: [C12] last newly pressed paddle: 1 = dit, 2 = dah
bool inStartup  = true;                           // startup flag
char myCall[10] = "W8BH";
byte callRaw[10];                                 // Claude: [C20] raw EEPROM bytes 8..17 at startup (diagnostics)
bool callWasReset = false;                        // Claude: [C20] callsign invalid at startup -> W8BH
int textColor   = TEXTCOLOR;                      // foreground (text) color
int bgColor     = BG;                             // background (screen) color
int brightness  = 100;                            // backlight level (range 0-100%)
int startItem   = -1;                              // startup activity.  0 = main menu
int inputDelay  = 25;                             // Input Delay
                                                  // Claude: [C2] initial value was 1 -> with a blank EEPROM
                                                  // input ended after 1 dit / characters fell apart.



// Claude: [C13] Remember last exercise
#define STARTUP_LAST       29                     // startup value "Last Exercise"  (Claude: [C16] was 28; 28 is now Config-Exit)
int lastExercise = -1;                            // last used exercise (0..7, 10..15), -1 = none
int lastPractice = 0;                             // Practice sub-mode: 0 = Koch Trainer, 1 = Free Sending
bool useBluetooth = false;                        // Claude: [C16] Bluetooth keyboard on/off
volatile int  btLastDisc  = -1;                   // Claude: [C16e] last disconnect reason (-1 = none)
volatile int  btLastAuth  = -1;                   // Claude: [C16e] last pairing: 0 = OK, else error code
volatile int  btConnCount = 0;                    // Claude: [C16e] number of connections since startup
volatile bool btConnected = false;               // Claude: [C16] Bluetooth connection active

//===================================  Menu Variables ===================================
int  menuCol=2, textRow=0, textCol=0;
char *mainMenu[] = {" Receive ", "  Send  ", "Config "};        
char *menu0[]    = {" Koch    ", " Letters ", " Words   ", " Numbers ", " Mixed   ", " QSO ", " Callsign", " Flashcrd", " Exit "};
char *menu1[]    = {" Practice", " Copy One", " Copy Two", " Cpy Word", " Cpy Call",  " Head Cpy", " Exit "};
char *menu2[]    = {" Speed Cfg", " Tone ", " Key ", " Callsign ", " Screen ",  " InpDelay", " Defaults", " Bluetooth", " Exit "};  // Claude: [C16] "Bluetooth" added




//===================================  Rotary Encoder Code  =============================

/* 
   Rotary Encoder Button Interrupt Service Routine ----------
   Process encoder button presses and releases, including debouncing (extra "presses" from 
   noisy switch contacts). If button is pressed, the button_pressed flag is set to true. If 
   button is released, the button_released flag is set to true, and button_downtime will 
   contain the duration of the button press in ms.  Manually reset flags after handling event. 
*/

void buttonISR()
{  
  static boolean button_state = false;
  static unsigned long start, end;  
  boolean pinState = digitalRead(ENCODER_BUTTON);
  
  if ((pinState==LOW) && (button_state==false))                     
  {                                               // Button was up, but is now down
    start = millis();                             // mark time of button down
    if (start > (end + 10))                       // was button up for 10mS?
    {
      button_state = true;                        // yes, so change state
      button_pressed = true;
    }
  }
  else if ((pinState==HIGH) && (button_state==true))                       
  {                                               // Button was down, but now up
    end = millis();                               // mark time of release
    if (end > (start + 10))                       // was button down for 10mS?
    {
      button_state = false;                       // yes, so change state
      button_released = true;
      button_downtime = end - start;              // and record how long button was down
    }
  }
}

/* 
   Rotary Encoder Interrupt Service Routine ---------------
   This is an alternative to rotaryISR() above.
   It gives twice the resolution at a cost of using an additional interrupt line
   This function will runs when either encoder pin A or B changes state.
   The states array maps each transition 0000..1111 into CW/CCW rotation (or invalid).
   The rotary "position" is held in rotary_counter, increasing for CW rotation, decreasing 
   for CCW rotation. If the position changes, rotary_change will be set true. 
   You should set this to false after handling the change.
   To implement, attachInterrupts to encoder pin A *and* pin B 
*/

// Encoder lookup tables.
//
// The 4-bit table index consists of:
//   bits 3..2 = previous AB state
//   bits 1..0 = current AB state
//
// The signs are chosen to preserve the original Morse Tutor direction:
// +1 = clockwise, -1 = counter-clockwise.

static const int8_t encoderTable_1edge[16] =
{
   0,  // 0000 : 00 -> 00
  +1,  // 0001 : 00 -> 01
  -1,  // 0010 : 00 -> 10
   0,  // 0011 : 00 -> 11   illegal

  -1,  // 0100 : 01 -> 00
   0,  // 0101 : 01 -> 01
   0,  // 0110 : 01 -> 10   illegal
  +1,  // 0111 : 01 -> 11

  +1,  // 1000 : 10 -> 00
   0,  // 1001 : 10 -> 01   illegal
   0,  // 1010 : 10 -> 10
  -1,  // 1011 : 10 -> 11

   0,  // 1100 : 11 -> 00   illegal
  -1,  // 1101 : 11 -> 01
  +1,  // 1110 : 11 -> 10
   0   // 1111 : 11 -> 11
};


static const int8_t encoderTable_2edges[16] =
{
   0,  // 0000 : 00 -> 00
  +1,  // 0001 : 00 -> 01   count
   0,  // 0010 : 00 -> 10
   0,  // 0011 : 00 -> 11   illegal

  -1,  // 0100 : 01 -> 00   count
   0,  // 0101 : 01 -> 01
   0,  // 0110 : 01 -> 10   illegal
   0,  // 0111 : 01 -> 11

   0,  // 1000 : 10 -> 00
   0,  // 1001 : 10 -> 01   illegal
   0,  // 1010 : 10 -> 10
  -1,  // 1011 : 10 -> 11   count

   0,  // 1100 : 11 -> 00   illegal
   0,  // 1101 : 11 -> 01
  +1,  // 1110 : 11 -> 10   count
   0   // 1111 : 11 -> 11
};


// FourEdgesPerCount:
// Use ONE reversible transition pair (01 <-> 11).
// This gives one count per complete quadrature cycle.
//
// Important on the ESP32:
// Both directions of the SAME electrical edge are included with opposite signs.
// Therefore contact bounce 01 <-> 11 produces +1/-1 and cancels instead of
// repeatedly counting in only one direction.
//
// This differs deliberately from the old MiniDDS "count when leaving 11"
// table, which worked with periodic polling but is unsuitable for CHANGE
// interrupts because every contact bounce can trigger another interrupt.
static const int8_t encoderTable_4edges[16] =
{
   0,  // 0000 : 00 -> 00
   0,  // 0001 : 00 -> 01
   0,  // 0010 : 00 -> 10
   0,  // 0011 : 00 -> 11

   0,  // 0100 : 01 -> 00
   0,  // 0101 : 01 -> 01
   0,  // 0110 : 01 -> 10
  +1,  // 0111 : 01 -> 11   count

   0,  // 1000 : 10 -> 00
   0,  // 1001 : 10 -> 01
   0,  // 1010 : 10 -> 10
   0,  // 1011 : 10 -> 11

   0,  // 1100 : 11 -> 00
  -1,  // 1101 : 11 -> 01   count
   0,  // 1110 : 11 -> 10
   0   // 1111 : 11 -> 11
};


// Previous 2-bit AB state. It is initialized in initEncoder() before the
// interrupts are enabled so that the first movement is decoded correctly.
volatile uint8_t encoderPrevState = 0;


void rotaryISR()
{
  uint8_t currentState = 0;
  currentState |= digitalRead(ENCODER_A);
  currentState |= (digitalRead(ENCODER_B) << 1);

  uint8_t transition = ((encoderPrevState << 2) | currentState) & 0x0F;
  encoderPrevState = currentState;

#if ENCODER_RESOLUTION == OneEdgePerCount

  rotaryCounter += encoderTable_1edge[transition];

#elif ENCODER_RESOLUTION == TwoEdgesPerCount

  rotaryCounter += encoderTable_2edges[transition];

#elif ENCODER_RESOLUTION == FourEdgesPerCount

  rotaryCounter += encoderTable_4edges[transition];

#else

  #error "Unknown ENCODER_RESOLUTION"

#endif
}

boolean buttonDown()                              // check CURRENT state of button
{
  return (digitalRead(ENCODER_BUTTON)==LOW);
}

void waitForButtonRelease()
{
  if (buttonDown())                               // make sure button is currently pressed.
  {
    button_released = false;                      // reset flag
    while (!button_released) ;                    // and wait for release                     
  }
}

void waitForButtonPress()
{
  if (!buttonDown())                              // make sure button is not pressed.
  {
    while (!button_pressed) ;                     // wait for press
    button_pressed = false;                       // and reset flag
  }  
}

bool longPress()
{
  waitForButtonRelease();                         // wait until button is released
  return (button_downtime > LONGPRESS);           // then check time is was held down
}

/* 
   readEncoder() returns 0 if no significant encoder movement since last call,
   +1 if clockwise rotation, and -1 for counter-clockwise rotation
*/

int readEncoder()
{
  static int prevCounter = 0;                     // last encoder position seen by this function
  int currentCounter = rotaryCounter;
  int change = currentCounter - prevCounter;

  if (change == 0)
    return 0;

  // Keep the original Morse Tutor behaviour:
  // accept the new encoder position at once and return only its direction.
  // This prevents an old accumulated difference from being replayed as a
  // long sequence of menu steps.
  prevCounter = currentCounter;
  return (change > 0) ? 1 : -1;
}


//===================================  Morse Routines ===================================

// Claude: [C7] The tone frequency is now set only once (setToneFrequency).
//         keyDown()/keyUp() only switch the duty cycle. Previously
//         ledcWriteTone() reconfigured the timer for every element
//         (lost time, jitter, clicks).
#define TONE_DUTY  512                            // Claude: 50% at 10-bit resolution

void setToneFrequency()                           // Claude: [C7] new
{
  ledcChangeFrequency(AUDIO, pitch, 10);          // set frequency once
  ledcWrite(AUDIO, 0);                            // tone off
}

void keyUp()
{
  digitalWrite(LED, 0);
  ledcWrite(AUDIO, 0);                            // Claude: [C7] duty cycle only: 0 = tone off
}

void keyDown()
{
  if (!SUPPRESSLED) digitalWrite(LED, 1);
  ledcWrite(AUDIO, TONE_DUTY);                    // Claude: [C7] duty cycle only: 50% = tone on
}

// Claude: [C12] Remembers which paddle was most recently NEWLY pressed
//   (for Ultimatic) and returns the new presses of this call:
//   bit 0 = dit newly pressed, bit 1 = dah newly pressed.
//   Must be called regularly so that releases are detected as well.
#define EDGE_DIT  1
#define EDGE_DAH  2
uint8_t updatePaddlePriority()
{
  static bool prevDit = false;
  static bool prevDah = false;
  uint8_t edges = 0;
  bool d = ditPressed();
  bool h = dahPressed();
  if (d && !prevDit) { lastPaddle = 1; edges |= EDGE_DIT; }
  if (h && !prevDah) { lastPaddle = 2; edges |= EDGE_DAH; }
  prevDit = d;
  prevDah = h;
  return edges;
}

// Claude: [C12] Common keyer decision for paddleInput() and
//   getPatternFromPaddles(). Returns: 1 = dit, 2 = dah, 0 = nothing.
uint8_t nextElement()
{
  updatePaddlePriority();
  bool d = ditPressed();
  bool h = dahPressed();

  if (keyerMode == ULTIMATIC) {
    if (d && h) return (lastPaddle == 1) ? 1 : 2; // squeeze: most recently pressed paddle wins
    if (ditRequest) return 1;                     // briefly tapped paddle (memory) first
    if (dahRequest) return 2;
    if (d) return 1;                              // only one paddle held
    if (h) return 2;
    return 0;
  }

  // Iambic A / B: logic as before
  if (ditRequest || (d && !dahRequest)) return 1;
  if (dahRequest || (h && !ditRequest)) return 2;
  return 0;
}

bool ditPressed()
{
  return (digitalRead(ditPaddle)==0);             // pin is active low
}

bool dahPressed()
{
  return (digitalRead(dahPaddle)==0);             // pin is active low
}

int extracharDit()
{
  return (3158/codeSpeed) - (1958/charSpeed); 
}

int intracharDit()
{
  return (1200/charSpeed);
}

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
                                            // while waiting, check if knob has been turned
    checkForSpeedChange(); 
    
                                            // If knob has been pressed, stop waitng (break)
    if (button_pressed) break; 
  }
}


void characterSpace()
{
  // Claude: [C4] Farnsworth character spacing corrected.
  //   Target: 3 Farnsworth units (extracharDit()).
  //   The last element has already produced 1 fast dit (ditPeriod) of pause,
  //   so 3*extracharDit() - ditPeriod are still missing.
  //   Previously only 2*extracharDit() were added -> spacing too short
  //   (e.g. 18/10 WPM: 480 ms instead of 621 ms).
  //   Without Farnsworth (extracharDit()==ditPeriod) this gives 2 dits as before.
  long rest = 3L*extracharDit() - ditPeriod;
  if (rest > 0) smartDelay(rest);                 // Claude: [C8] never pass a negative time to smartDelay()
}

void wordSpace()
{
  // Claude: [C4] Word spacing = 7 Farnsworth units. characterSpace() has
  //   already produced 3, so 4 are still missing.
  //   smartDelay() instead of delay(): can now be aborted by button here too.
  long rest = 4L*extracharDit();
  if (rest > 0) smartDelay(rest);
  if (xWordSpaces)                                // any user-specified delays?
    smartDelay(7L*extracharDit()*xWordSpaces);    // yes, so additional wait between words
}

// Claude: [C3] dit() and dah() reworked:
//   - checkForSpeedChange() is NO longer called here. Previously an
//     encoder movement during an element could redraw the display
//     (SPI, several ms) and thus stretch tone/pause. In addition, while
//     sending yourself (isSending==false) a button press was "consumed" and
//     gap mode was toggled -> exercises could not be left reliably.
//     (The old indentation was misleading: the call ran in EVERY loop pass.)
//   - Timing with unsigned long and difference (millis()-t0) instead of
//     "int finished = millis()+...": correct even when millis() overflows.

void dit() {                                      // send a dit
  ditRequest = false;                             // clear any pending request
  keyDown();                                      // turn on sound & led
  unsigned long t0 = millis();                    // Claude: [C3]
  while (millis() - t0 < (unsigned long)ditPeriod) { // check dah paddle while dit sounding
    uint8_t e = updatePaddlePriority();           // Claude: [C12]
    if (keyerMode == ULTIMATIC) {                 // Claude: [C12] Ultimatic: only a NEW press
      if (e & EDGE_DAH) dahRequest = true;        //   is stored
    }
    else if ((keyerMode==IAMBIC_B) && dahPressed()) // if iambic B & dah was pressed,
      dahRequest = true;                          // request it for next element
  }
  keyUp();                                        // turn off sound & led
  t0 = millis();                                  // wait before next element sent
  while (millis() - t0 < (unsigned long)ditPeriod) { // check dah paddle while waiting
    uint8_t e = updatePaddlePriority();           // Claude: [C12]
    if (keyerMode == ULTIMATIC) {
      if (e & EDGE_DAH) dahRequest = true;
    }
    else if (keyerMode && dahPressed())           // if iambic A or B & dah was pressed,
      dahRequest = true;                          // request it for next element
  }
}

void dah() {                                      // send a dah
  dahRequest = false;                             // clear any pending request
  keyDown();                                      // turn on sound & led
  unsigned long t0 = millis();                    // Claude: [C3]
  while (millis() - t0 < 3UL*ditPeriod) {         // check dit paddle while dah sounding
    uint8_t e = updatePaddlePriority();           // Claude: [C12]
    if (keyerMode == ULTIMATIC) {                 // Claude: [C12] Ultimatic: only a NEW press
      if (e & EDGE_DIT) ditRequest = true;        //   is stored
    }
    else if ((keyerMode==IAMBIC_B) && ditPressed()) // if iambic B & dit was pressed
      ditRequest = true;                          // request it for next element
  }
  keyUp();                                        // turn off sound & led
  t0 = millis();                                  // wait before next element sent
  while (millis() - t0 < (unsigned long)ditPeriod) { // check dit paddle while waiting
    uint8_t e = updatePaddlePriority();           // Claude: [C12]
    if (keyerMode == ULTIMATIC) {
      if (e & EDGE_DIT) ditRequest = true;
    }
    else if (keyerMode && ditPressed())           // if iambic A or B & dit was pressed,
      ditRequest = true;                          // request it for next element
  }
}

void sendElements(int x) {                        // send string of bits as Morse      
isSending = true;
  while (x>1) {                                   // stop when value is 1 (or less)
    if (x & 1) dit();                             // right-most bit is 1, so dit
    else dah();                                   // right-most bit is 0, so dah
    x >>= 1;                                      // rotate bits right
  }
  characterSpace();                               // add inter-character spacing, Farnsworth Pause
  isSending = false;
}

void roger() {
  dit(); dah(); dit();                            // audible (only) confirmation
}

/* 
   The following routine accepts a numberic input, where each bit represents a code element:
   1=dit and 0=dah.   The elements are read right-to-left.  The left-most bit is a stop bit.
   For example:  5 = binary 0b0101.  The right-most bit is 1 so the first element is a dit.
   The next element to the left is a 0, so a dah follows.  Only a 1 remains after that, so
   the character is complete: dit-dah = morse character 'A'. 
*/

void sendCharacter(char c) {
  isSending = true;
  if (button_pressed) return;         // break if button pressed

  if (c < 32) return;
  if (c > 96) c -= 32;
  if (c > 90) return;

  addCharacter(c); 

  if (c == 32) {
    wordSpace();
  } else {
    sendElements(morse[c - 33]);
  }
  isSending = false;
}

void sendString (char *ptr) {           
  isSending = true;    
  while (*ptr && !button_pressed) { 
    sendCharacter(*ptr++);  
    isSending = false;
  }
}

void sendMorseWord (char *ptr) {
  isSending = true;
  while (*ptr) {                                              // send the entire word
    char c = *ptr++;
    // Claude: [C5] Handle spaces and lower case. Previously, for
    //   ' ' the byte morse[-1] (outside the table) was sent.
    if (c > 96) c -= 32;                                      // lower case -> upper case
    if (c == ' ') { wordSpace(); continue; }                  // word space
    if (c < 33 || c > 90) continue;                           // not a Morse character
    sendElements(morse[c - 33]);                              // each char in Morse - no display
  }
  isSending = false;
}

void displayWPM() {
                                                              // 1. Secure old cursor-position
  int oldX = tft.getCursorX();
  int oldY = tft.getCursorY();
  
                                                              // 2. Draw WPM display
  int xPos = 115; 
  int yPos = 220;
  tft.fillRect(xPos, yPos, 100, 20, BLACK); 
  
  tft.setTextSize(2);                                         // Defines only the size of the WPM numbers
  tft.setCursor(xPos, yPos);
  
  tft.setTextColor(YELLOW);
  tft.print(charSpeed);
  tft.setTextColor(GRAY);
  tft.print("/");
  tft.setTextColor(GREEN);
  tft.print(codeSpeed);

                                                              // 3. Important: reset cursor
  tft.setCursor(oldX, oldY);
  
                                                              // 4. Re-establish the char size for the remaining text.
  
  if (isSending) {
     tft.setTextSize(2);                                      // Standard size for text send/receive
  } else {
     tft.setTextSize(2);                                      // Standard size for menues
  }
}


void checkForSpeedChange() {
  int dir = readEncoder();
  
                                                      // 1. button: change modus (only when not on send)
  if (button_pressed && !isSending) { 
    button_pressed = false;
    settingGapSpeed = !settingGapSpeed;
  }

                                                       // 2. Button turned?
  if (dir != 0) {
        if (isSending || !settingGapSpeed) {
                                                       // Remember current Gap setting
       int currentDiff = charSpeed - codeSpeed;
       charSpeed += dir;
                                                       // set same Gap for new Char speed
       codeSpeed = charSpeed - currentDiff;
    } else {
                                                       // GAP SPEED MODUS (only change Gap setting)
       codeSpeed += dir;
    }

    // ------------------------------------------------------------
    // 3. GLOBAL boundaries (Security net)
    // ------------------------------------------------------------
                                                                               // A. Keep absolute borders
    charSpeed = constrain(charSpeed, MINSPEED, MAXSPEED);
                                                                               // B. Gap never higher than Char Speed
    if (codeSpeed > charSpeed) codeSpeed = charSpeed;
                                                                               // C. Differnce max. 19 WPM
                                                                               // correct Gap >19, if set manually
    if ((charSpeed - codeSpeed) > 19) {
       codeSpeed = charSpeed - 19;
    }
                                                                               // D. CodeSpeed never less than minimum
    if (codeSpeed < MINSPEED) codeSpeed = MINSPEED;
      displayWPM();
    ditPeriod = intracharDit(); 
  }
}
   


void checkPause()
{
  bool userKeyed=(ditPressed() ^ dahPressed());   // did user press a key (not both)
  if (!paused && userKeyed)                       // was it during sending?
  {
    paused = true;                                // yes, so pause output
    delay(ditPeriod*10);                          // for at least for 10 dits
  }
  else if ((paused && userKeyed)                  // did user key during pause
  || button_pressed)                              // or cancel?
    paused = false;                               // yes, so resume output
}


//===================================  Koch Method  =====================================

void sendKochLesson(int lesson) 
{ 
  isSending = true;                                 // lock on
  const int maxCount = 175; 
  int charCount = 0;
  newScreen(); 
  
  while (!button_pressed && (charCount < maxCount)) 
  {                           
    for (int i=0; i<WORDSIZE; i++) 
    {
      if (button_pressed) break;
      int c = koch[random(lesson+1)]; 
      
                                                   // Sends the char with charSpeed and adds 
                                                   // the Farnsworth-pause (extracharDit):
      sendCharacter(c); 
      charCount ++; 
    }
                                                  // long pause after a word group (7 units codeSpeed)
    if (!button_pressed) sendCharacter(' '); 
    isSending = false;                            // lock off
  }
}

void introLesson(int lesson)                      // helper fn for getLessonNumber()
{
  newScreen();                                    // start with clean screen
  tft.print("You are in lesson ");
  tft.println(lesson);                            // show lesson number
  tft.println("\nCharacters: ");                  
  tft.setTextColor(CYAN);
  for (int i=0; i<=lesson; i++)                   // show characters in this lession
  {
    tft.print(koch[i]);
    tft.print(" ");
  }
  tft.setTextColor(textColor);
  tft.println("\n\nPress <dit> to begin");  
}

int getLessonNumber()
{
  int lesson = kochLevel;                         // start at current level
  introLesson(lesson);                            // display lesson number
  while (!button_pressed && !ditPressed())
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      lesson += dir;                              // ...so change speed up/down 
      if (lesson<1) lesson = 1;                   // dont go below 1 
      if (lesson>kochLevel) lesson = kochLevel;   // dont go above maximum
      introLesson(lesson);                        // show new lesson number
    }
  }
  return lesson;
}

void sendKoch()
{
  isSending = true;
  while (!button_pressed) {
    setTopMenu("Koch lesson");
    int lesson = getLessonNumber();               // allow user to select lesson
    if (button_pressed) return;                   // user quit, so sad
    sendKochLesson(lesson);                       // do the lesson                      
    setTopMenu("Get 90%? Dit=YES, Dah=NO");       // ask user to score lesson
    while (!button_pressed) {                     // wait for user response
       if (ditPressed())                          // dit = user advances to next level
       {  
         roger();                                 // acknowledge success
         if (kochLevel<ELEMENTS(koch))        
           kochLevel++;                           // advance to next level
         saveConfig();                            // save it in EEPROM
         delay(1000);                             // give time for user to release dit
         break;                                   // go to next lesson
       }
       if (dahPressed()) break;                   // dah = repeat same lesson
       
    }
    isSending = false;
  }
}


//===================================  Receive Menu  ====================================


void addCharacter(char c) {
                                                 // Security: If MAXROW was not calculated
  if (MAXROW <= 0) updateGrid(currentTextSize); 

  showCharacter(c, textRow, textCol); 
  textCol++; 

  if ((textCol >= MAXCOL) || ((c == ' ') && (textCol > MAXCOL - 7))) {
    textRow++; 
    textCol = 0;                                // Jump to left of new row
    
    if (textRow >= MAXROW) { 
       newScreen();                             // does position reset
    }
  }
}

void randomCallsign(char* call) {
  strcpy(call, ""); 

                                                                                                        // 1. List of  european prefixes (Germany, Austria, Switzerland, etc.)
  char *euroPrefixes[] = {"DL", "DJ", "DK", "DF", "DG", "DH", "OE", "HB", "PA", "ON", "G", "F", "I"};
  
                                                                      // 2. Decision: 70% Chance for EU, 30% International (K, W, N)
  if (random(0, 10) < 7) {
    strcat(call, euroPrefixes[random(0, ELEMENTS(euroPrefixes))]);
    
                                                                     // HB and OE do often need 2 numbers after the prefix
    if (strcmp(call, "HB") == 0 || strcmp(call, "OE") == 0) {
      addChar(call, '0' + random(1, 10)); 
    } else if (strlen(call) == 1) { 
                                                                    // G or F need one number as well
      addChar(call, '0' + random(1, 10));
    } else {
                                                                   // DL, DJ etc. get one number (often 0-9)
      addChar(call, '0' + random(0, 10));
    }
  } else {
                                                                  // classic US-prefixes as remaining chance
    char *usPrefixes[] = {"K", "W", "N", "AA", "AK"};
    strcat(call, usPrefixes[random(0, ELEMENTS(usPrefixes))]);
    addChar(call, '0' + random(1, 10));
  }

                                                   // 3. Generate suffix (2 to 3 letters)
  int suffixLen = random(2, 4); 
  for (int i = 0; i < suffixLen; i++) {
    addChar(call, 'A' + random(0, 26));
  }
}

void randomRST(char* rst)
{
  strcpy(rst,"");                                 // start with empty string                     
  addChar(rst,'0'+random(3,6));                   // readability 3-5                      
  addChar(rst,'0'+random(5,10));                  // strength: 6-9 
  addChar(rst,'9');                               // tone usually 9
}

void sendNumbers()                                
{ 
  isSending = true;
  while (!button_pressed) {
    for (int i=0; i<WORDSIZE; i++)                // break them up into "words"
      sendCharacter(randomNumber());              // send a number
    sendCharacter(' ');                           // send a space between number groups
    isSending = false;
  }
}

void sendLetters()                                
{ 
  isSending = true;
  while (!button_pressed) {
    for (int i=0; i<WORDSIZE; i++)                // group letters into "words"
      sendCharacter(randomLetter());              // send the letter 
    sendCharacter(' ');                           // send a space between words
    isSending = false;
  }
}

void sendMixedChars()                             // send letter/number groups...
{ 
  isSending = true;
  while (!button_pressed) {                            
    for (int i=0; i<WORDSIZE; i++)                // break them up into "words"
    {
      int c = '0' + random(43);                   // pick a random character
      sendCharacter(c);                           // and send it
    }
    sendCharacter(' ');                           // send a space between words
    isSending = false;
  }
}

void sendPunctuation()
{
  isSending = true;
  while (!button_pressed) {
    for (int i=0; i<WORDSIZE; i++)                // break them up into "words"
    {
      int j=random(0,ELEMENTS(punctuation));      // select a punctuation char
      sendCharacter(punctuation[j]);              // and send it.
    }
    sendCharacter(' ');                           // send a space between words
    isSending = false;
  }
}


void sendWords()
{ 
  isSending = true;
  while (!button_pressed) {
    int index=random(0, ELEMENTS(words));         // eeny, meany, miney, moe
    sendString(words[index]);                     // send the word
    sendCharacter(' ');                           // and a space between words
    isSending = false;
  }
}

void sendCallsigns()                              // send random US callsigns
{ 
  isSending = true;
  char call[8];                                   // need string to stuff callsign into
  while (!button_pressed) {
    randomCallsign(call);                         // make it
    sendString(call);                             // and send it
    sendCharacter(' ');                           // with a space between callsigns
    isSending = false;         
  }
}

void sendQSO()
{
  isSending = true;                               // lock on
  char otherCall[8];
  char temp[20];                               
  char qso[300];                                  
  randomCallsign(otherCall);
  strcpy(qso, myCall);                            
  strcat(qso, " de ");
  strcat(qso, otherCall);                         
  strcat(qso, " K  TNX FER CALL= UR RST ");
  
                                                    // RST Logic
  int r = random(3, 6); int s = random(5, 10);
  sprintf(temp, "%d%d9 ", r, s);
  strcat(qso, temp);
  
  strcat(qso, "= NAME HR IS ");
  strcat(qso, names[random(0, ELEMENTS(names))]); 
  strcat(qso, " = QTH IS ");
  strcat(qso, cities[random(0, ELEMENTS(cities))]);
  strcat(qso, " = HW CPY? ");
  strcat(qso, myCall);
  strcat(qso, " de ");
  strcat(qso, otherCall);
  strcat(qso, " KN");

  newScreen();
  sendString(qso);  
  isSending = false;                               // lock off                              
  button_pressed = false; 
}

void freePractice() {
  int oldCol = COLSPACING;
  int oldRow = ROWSPACING;
  int oldSize = currentTextSize;

  currentTextSize = 3; 
  COLSPACING = 22;      
  ROWSPACING = 36;      
  updateGrid(currentTextSize); 

  newScreen();
  tft.setTextSize(3); 
  tft.setCursor(0, 0);
  tft.println("Free Sending");
  tft.println("-------------");

  textRow = 2;
  textCol = 0; 
  bool spacePrinted = true; 

  while(true) {
      if (button_pressed) { 
          button_pressed = false;
          COLSPACING = oldCol;
          ROWSPACING = oldRow;
          currentTextSize = oldSize;
          return; 
      }
      
      checkForSpeedChange();
      uint8_t pattern = getPatternFromPaddles();

      if (button_pressed) { 
          button_pressed = false;
          COLSPACING = oldCol;
          ROWSPACING = oldRow;
          currentTextSize = oldSize;
          return; 
      }

      tft.setTextSize(3);
      char charToPrint = 0;
      int colorToUse = textColor;
      bool shouldPrint = false;

      if (pattern == 0) {
          if (!spacePrinted) {
              charToPrint = ' ';
              shouldPrint = true;
              spacePrinted = true;
          }
      } else {
          spacePrinted = false;
          shouldPrint = true;
          char decoded = '?';
          bool found = false;
          for (int i=0; i < ELEMENTS(morse); i++) {
              if (morse[i] == pattern) {
                  decoded = (char)(i + 33);
                  found = true;
                  break;
              }
          }
          if (found) {
              colorToUse = GREEN;
              charToPrint = decoded;
          } else {
              colorToUse = RED;
              charToPrint = '?';
              missTone();
          }
      }

      if (shouldPrint) {
          tft.setTextColor(colorToUse);
          showCharacter(charToPrint, textRow, textCol);
          textCol++;
          if (textCol >= MAXCOL) {
              textCol = 0;
              textRow++;
          }
          if (textRow >= MAXROW) {
              tft.fillScreen(bgColor);
              tft.setTextSize(3);
              tft.setTextColor(textColor);
              tft.setCursor(0, 0);
              tft.println("Free Sending");
              tft.println("-------------");
              textRow = 2;
              textCol = 0;
          }
          tft.setTextColor(textColor);
      }
  }
}

void runPracticeMenu(bool autoStart) {           // Claude: [C13] autoStart = start last used mode directly
  char *modes[] = {" Koch Trainer", " Free Sending", " Exit "};
  int count = 3;
  int index = lastPractice;                       // Claude: [C13] cursor on last used mode (was 0)
  bool redraw = true;                                     // Defines, if new draw has to be done

  if (autoStart) {                                // Claude: [C13] start directly (after power-on)
    button_pressed = false;
    if (lastPractice == 0) practiceSending();
    else freePractice();
    button_pressed = false;                       // don't pass the exit button press on to the menu
  }

  while (true) {

                                                          //draw menu
    if (redraw) {
       newScreen();
       tft.setTextSize(2);
       tft.setCursor(0,0);
       tft.println(" Select Mode:");
       tft.drawLine(0, 25, 320, 25, YELLOW);               // top yellow line

       for (int i=0; i<count; i++) {
          int y = 60 + i*40;                              // distance of entries
          int x = 40;
          
          if (i == index) {
                                                          // marked entry: blue on white
             tft.setTextColor(SELECTFG, SELECTBG); 
                                                          // draw small frame for optical impression
             tft.fillRect(x-5, y-5, 200, 30, SELECTBG);
          } else {
                                                          // normal entry: green on black
             tft.setTextColor(FG, BG); 
          }
          
          tft.setCursor(x, y);
          tft.print(modes[i]);
       }
       tft.setTextColor(FG, BG);                          // reset colour
       redraw = false;
    }

                                                          // processing entry
    button_pressed = false;
    
    while (!button_pressed) {
       int dir = readEncoder();
       if (dir != 0) {
          index += dir;
                                                         // scroll
          if (index < 0) index = count - 1;
          if (index >= count) index = 0;
          redraw = true;                                // initiate new draw
          break;                                        // leave inner loop
       }
    }

                                                        // run
    if (button_pressed) {
       button_pressed = false;
       
       if ((index <= 1) && (index != lastPractice)) { // Claude: [C13] remember sub-mode (only when changed)
          lastPractice = index;
          saveConfig();
       }
       if (index == 0) {
          practiceSending();                           // calls KOCH Trainer
          redraw = true;                               // redraw menu
       }
       else if (index == 1) {
          freePractice();                              // calls FREE Sending
          redraw = true;                               // redraw menu
       }
       else {
          return;                                      // Exit: back to main menu
       }
    }
  }
}


void displayFiles(char menu[][FNAMESIZE], int top, int itemCount)
{
  int x = 30;                                     // x-coordinate of this menu
  newScreen();                                    // clear screen below menu
  for (int i = 0; i < MAXROW; i++)                // for all items in the frame
  {
     int y = TOPMARGIN + i*ROWSPACING;            // calculate y coordinate
     int item = top + i;
     if (item<itemCount)                          // make sure item exists
       showMenuItem(menu[item],x,y,FG,BG);        // and show the item.
  } 
}

int fileMenu(char menu[][FNAMESIZE], int itemCount) // Display list of files & get user selection
{
  int index=0,top=0,pos=0,x=30,y; 
  button_pressed = false;                         // reset button flag
  displayFiles(menu,0,itemCount);                 // display as many files as possible
  showMenuItem(menu[0],x,TOPMARGIN,               // highlight first item
    SELECTFG,SELECTBG);
  while (!button_pressed)                         // exit on button press
  {
    int dir = readEncoder();                      // check for encoder movement
    if (dir)                                      // it moved!    
    {
      if ((dir>0) && (index==(itemCount-1)))      // dont try to go below last item
        continue;
      if ((dir<0) && (index==0))                  // dont try to go above first item
        continue;
      if ((dir>0) && (pos==(MAXROW-1)))           // does the frame need to move down?
      {                              
        top++;                                    // yes: move frame down,
        displayFiles(menu,top,itemCount);         // display it,
        index++;                                  // and select next item
      }
      else if ((dir<0) && (pos==0))               // does the frame need to move up?
      {
        top--;                                    // yes: move frame up,
        displayFiles(menu,top,itemCount);         // display it,
        index--;                                  // and select previous item
      }
      else                                        // we must be moving within the frame
      {
        y = TOPMARGIN + pos*ROWSPACING;           // calc y-coord of current item
        showMenuItem(menu[index],x,y,FG,BG);      // deselect current item
        index += dir;                             // go to next/prev item
      }
      pos = index-top;                            // posn of selected item in visible list
      y = TOPMARGIN + pos*ROWSPACING;             // calc y-coord of new item
      showMenuItem(menu[index],x,y,
        SELECTFG,SELECTBG);                       // select new item
    }
  }
  return index;  
}



//===================================  Send Menu  =======================================

void checkSpeed()
{
  long start=millis();
  sendString("PARIS ");                           // send "PARIS"                                     
  long elapsed=millis()-start;                    // see how long it took
  dit();                                          // sound out the end.
  float wpm = 60000.0/elapsed;                    // convert time to WPM
  tft.setTextSize(3);
  tft.setCursor(100,80);
  tft.print(wpm);                                 // display result
  tft.print(" WPM");
  while (!button_pressed) ;                       // wait for user
}

int decode(int code)                              // convert code to morse table index
{
  int i=0;                                        
  while (i<ELEMENTS(morse) && (morse[i]!=code))   // search table for the code
    i++;                                          // ...from beginning to end. 
  if (i<ELEMENTS(morse)) return i;                // found the code, so return index
  else return -1;                                 // didn't find the code, return -1
}

char paddleInput()                                // monitor paddles & return a decoded char
{
  int bit=0, code=0;
  unsigned long start = millis();
  while (!button_pressed)
  {
    uint8_t el = nextElement();                   // Claude: [C12] common keyer logic
    if (el == 1)                                  //   (Iambic A/B or Ultimatic)
    {
      dit();                                      // so sound it out
      code += (1<<bit++);                         // add a '1' element to code
      start = millis();                           // and reset timeout.
    }
    else if (el == 2)
    {
      dah();                                      // so sound it tout
      bit++;                                      // add '0' element to code
      start = millis();                           // and reset the timeout.
    }
    int wait = millis()-start;
    if (bit && (wait > ditPeriod))                // waited for more than a dit
    {                                             // so that must be end of character:
      code += (1<<bit);                           // add stop bit
      int result = decode(code);                  // look up code in morse array
      if (result<0) return ' ';                   // oops, didn't find it
      else return '!'+result;                     // found it! return result
    }
    if (wait > inputDelay*ditPeriod) return ' ';  // long pause = word space    (configurable 5 to 25)
  }
  return ' ';                                     // return on button press
}

char straightKeyInput()                           // decode straight key input
{
  int bit=0, code=0;
  bool keying = false;
  unsigned long start=0,end=0,timeUp,timeDown,timer;   // Claude: [C9] start/end initialized (end was undefined)
  timer = millis();                               // start character timer
  while (!button_pressed)
  {
    if (ditPressed()) timer = millis();           // dont count keydown time
    if (ditPressed() && (!keying))                // new key_down event
    {  
      start = millis();                           // mark time of key down
      timeUp = start-end;
      if (timeUp > 10)                            // was key up for 10mS?   
      {                                     
        keying = true;                            // mark key as down
        keyDown();                                // turn on sound & led 
      }
    }
    else if (!ditPressed() && keying)             // new key_up event
    {
      end = millis();                             // get time of release
      timeDown = end-start;                       // how long was key down?
      if (timeDown > 10)                          // was key down for 10mS?
      {
        keying = false;                           // not just noise: mark key as up
        keyUp();                                  // turn off sound & led
        if (timeDown > 2*ditPeriod)               // if longer than 2 dits, call it dah                  
          bit++;                                  // dah: add '0' element to code
        else code += (1<<bit++);                  // dit: add '1' element to code
      }
    }
    int wait = millis() - timer;                  // time since last element was sent
    if (bit && (wait > ditPeriod*2))              // waited for more than 2 dits
    {                                             // so that must be end of character:
      code += (1<<bit);                           // add stop bit
      int result = decode(code);                  // look up code in morse array
      if (result<0) return ' ';                   // oops, didn't find it
      else return '!'+result;                     // found it! return result
    }
    if (wait > inputDelay*ditPeriod) return ' ';  // long pause = word space                               
  }
  return ' ';                                     // return on button press   
}

char morseInput()                                 // get & decode user input from key 
{
  if (usePaddles) return paddleInput();           // it can be either paddle input
  else return straightKeyInput();                 // or straight key, depending on setting
}

void practiceSending() {
                                                  //  A loop, which runs until button_pressed is true
  while (true) {
    char targetChar = koch[random(kochLevel + 1)]; 
    newScreen();
    tft.setTextSize(3);
    tft.setCursor(20, 40);
    tft.print("Sende: ");
    tft.setTextColor(YELLOW);
    tft.println(targetChar);
    tft.setTextColor(textColor);

    long timeoutLimit = 150 * extracharDit(); 
    unsigned long startTime = millis();

                                                            // Waiting for first button_press
    while (!ditPressed() && !dahPressed()) {
                                                           // If the user does not react for a long time (timeout), training ends
      if (millis() - startTime > timeoutLimit) {
        tft.setTextColor(RED);
        tft.println("\n\nTIMEOUT - Ende");
        delay(1500);
        return; 
      }
      
                                                           // Check if button is pressed to break
           if (button_pressed) { 
        button_pressed = false;                           // reset flag 
        return;                                           // back to menu
      }
      checkForSpeedChange();
    }

                                                         // read bit-pattern from user
    uint8_t userPattern = getPatternFromPaddles();
    if (userPattern == 0) continue;                      // If fault next character

    uint8_t targetPattern = morse[targetChar - 33];
    tft.setCursor(20, 120);
    tft.setTextSize(4);

    if (userPattern == targetPattern) {
      tft.setTextColor(GREEN);
      tft.print("RICHTIG!");
      roger(); 
    } else {
      tft.setTextColor(RED);
      tft.print("FALSCH!");
      
      tft.setTextSize(2);
      tft.setCursor(20, 180);
      tft.setTextColor(WHITE);
      tft.print("Gehoert: ");
      
      bool found = false;
      for (int i=0; i<ELEMENTS(morse); i++) {
        if (morse[i] == userPattern) {
          tft.print((char)(i+33)); 
          found = true;
          break;
        }
      }
      if (!found) tft.print("?");
    }
    
                                                   // short delay, so the result can be red, then next character
    delay(1500);
  }
}


void copyCallsigns()                              // show a callsign & see if user can copy it
{
  char call[8];
  while (!button_pressed)                      
  {  
    randomCallsign(call);                         // make a random callsign       
    mimic(call);
  }
}

void copyOneChar()
{
  char text[8];  
  while (!button_pressed)                      
  {
    strcpy(text,"");                              // start with empty string  
    addChar(text,randomLetter());                 // add a random letter
    mimic(text);                                  // compare it to user input
  }
}

void copyTwoChars()
{
  char text[8];   
  while (!button_pressed)                      
  {
    strcpy(text,"");                              // start with empty string  
    addChar(text,randomLetter());                 // add a random letter
    addChar(text,randomLetter());                 // make that two random letters
    mimic(text);                                  // compare it to user input
  }
}

// Claude: [C5] Helper: picks a word WITHOUT spaces for the
//   Copy/Head Copy exercises and copies it safely into the buffer.
//   Previously: char text[10] + strcpy() with entries of up to 20 characters
//   ("QTH IS BERLIN BERLIN") -> buffer overflow on the stack.
//   Words containing spaces can also never be entered correctly,
//   because input stops at the first word end.
void pickCopyWord(char *text, int size)
{
  int index;
  do {
    index = random(0, ELEMENTS(words));           // eeny, meany, miney, moe
  } while (strchr(words[index], ' ') != NULL);    // only words without spaces
  strncpy(text, words[index], size - 1);          // copy safely
  text[size - 1] = '\0';
}

void copyWords()                                  // show a word & see if user can copy it
{
  char text[24];                                  // Claude: [C5] buffer enlarged (was 10)
  while (!button_pressed)
  {
    pickCopyWord(text, sizeof(text));             // Claude: [C5] instead of strcpy(text,words[index])
    mimic(text);                                  // and ask user to copy it
  }
}

void encourageUser()                              // helper fn for showScore()
{
  char *phrases[]= {"Good Job", "Keep Going",     // list of phrases to display
    "Amazing","Dont Stop","Impressive"};
  if (score==0) return;                           // 0 is not an encouraging score
  if (score%25) return;                           // set interval at every 25 points
  textCol=0; textRow=6;                           // place display below challange
  int choice = random(0,ELEMENTS(phrases));       // pick a random message
  sendString(phrases[choice]);                    // show it and send it  
}

void displayNumber(int num, int color,            // show large number on screen
    int x, int y, int wd, int ht)                 // specify x,y and width,height of box
{
  int textSize = (num>99)?5:6;                    // use smaller text for 3 digit scores
  tft.setCursor(x+15,y+20);                       // position text within colored box
  tft.setTextSize(textSize);                      // use big text,
  tft.setTextColor(BLACK,color);                  // in inverted font,
  tft.fillRect(x,y,wd,ht,color);                  // on selected background
  tft.print(num);                                 // show the number
  tft.setTextSize(2);                             // resume usual size
  tft.setTextColor(textColor,BLACK);              // resume usual colors  
}
  
void showScore()                                  // helper fn for mimic()
{
  const int x=200,y=50,wd=105,ht=80;              // posn & size of scorecard
  int bkColor = (score>0)?GREEN:RED;              // back-color green unless score 0
  displayNumber(score,bkColor,x,y,wd,ht);         // show it!
  encourageUser();                                // show encouraging message periodically                                
}

void mimic1(char *text) {
  int oldCol = COLSPACING;
  int oldRow = ROWSPACING;
  int oldSize = currentTextSize;

  currentTextSize = 3; 
  COLSPACING = 22;      
  ROWSPACING = 36;      
  updateGrid(currentTextSize); 

  tft.setTextSize(currentTextSize);
  char ch, response[20];
  
  textRow = 1; 
  textCol = 0; 
  
  for (int i = 0; text[i] != 0; i++) {
    if (button_pressed) break;
    char c = text[i];
    showCharacter(c, textRow, textCol);
    textCol++;
    if (c == ' ') wordSpace();
    else {
      int mIdx = c - 33;
      if (mIdx >= 0 && mIdx < (int)sizeof(morse)) {
        sendElements(morse[mIdx]);
      }
    }
  }

  strcpy(response, "");
  textRow = 2; 
  textCol = 0; 
  while (!button_pressed && !ditPressed() && !dahPressed());
  if (!button_pressed) {
    do {                                            
      ch = morseInput();
      if (ch != ' ') {
        addChar(response, ch);
        addCharacter(ch);
      }
    } while (ch != ' ');
  }

  COLSPACING = oldCol;
  ROWSPACING = oldRow;
  currentTextSize = oldSize;
  tft.setTextSize(currentTextSize);

  if (button_pressed) return; 
  if (!strcmp(text, response)) score++;
  else score = 0;
  
  showScore();
  delay(FLASHCARDDELAY);
  newScreen();
}

void showHitsAndMisses(int hits, int misses)      // helper fn for mimic2()
{                      
  const int x=200,y1=50,y2=140,wd=105,ht=80;      // posn & size of scorecard             
  displayNumber(hits,GREEN,x,y1,wd,ht);           // show the hits in green
  displayNumber(misses,RED,x,y2,wd,ht);           // show misses in red                               
}

void headCopy()                                  // send a word & see if user can copy it
{
  char text[24];                                  // Claude: [C5] buffer enlarged (was 10)
  while (!button_pressed)
  {
    pickCopyWord(text, sizeof(text));             // Claude: [C5] instead of strcpy(text,words[index])
    mimic2(text);                                 // and ask user to copy it
  }
}

// Claude: [C6] hitTone()/missTone() used ledcWriteTone(0,...)/ledcWrite(0,0)
//   from ESP32 core 2.x (channel 0). In core 3.x the first parameter is a
//   PIN -> GPIO0: no tone, but still blocking. Now uses the AUDIO pin.
//   Afterwards the normal pitch is restored ([C7]).
void hitTone()
{
  ledcWriteTone(AUDIO,440); delay(150);           // first tone
  ledcWriteTone(AUDIO,600); delay(200);           // second tone
  setToneFrequency();                             // audio off, restore Morse pitch
}

void missTone()
{
  ledcWriteTone(AUDIO,200); delay(200);           // single tone
  setToneFrequency();                             // audio off, restore Morse pitch
}

void mimic2(char *text)                           // used by head-copy feature
{
  char ch, response[20];
  bool correct,leave;
  
  // --- SETTINGS FOR HEAD COPY START ---
  int oldCol = COLSPACING;                        // remember old spacing
  int oldRow = ROWSPACING;
  COLSPACING = 24;                                // double space for size 4
  ROWSPACING = 28;
  
  tft.setTextSize(4);                             // size 4
  tft.setTextColor(YELLOW);                       // color yellow
  // -----------------------------------------

  do {                                            // repeat same word until correct                                            
    
    sendMorseWord(text);                          // morse the text, NO DISPLAY
    strcpy(response,"");                          // start with empty response
    
    textRow=2;
    textCol=1;                                    // Start further left (1 and not 6)
    
    while (!button_pressed && !ditPressed()       // wait until user is ready
      && !dahPressed()) ;
    do {                                          // user has started keying...
      ch = morseInput();                          // get a character
      if (ch!=' ') addChar(response,ch);          // add it to the response
      addCharacter(ch);                           // and put it on screen
    } while (ch!=' ');                            // space = word timeout
    
    if (button_pressed) break;                    // leave without scoring (break instead of return, for cleanup)
    
    correct = !strcmp(text,response);             // did user match the text?
    leave   = !strcmp(response,"=");              // user entered BT/break to skip
    if (correct) {                                // did user match the text?
      hits++;                                     // got it right!
      hitTone();
    }
    else {
      misses++;                                   // user muffed it
      missTone();  
    }
    showHitsAndMisses(hits,misses);               // display scores for user
    delay(1000);                                  // wait a sec, then
    
    // enlarged erase area for the large font:
    tft.fillRect(0, 50, 200, 100, BLACK);         // erase answer (wider & taller)
    
  } while (!correct && !leave);                   // repeat until correct or user breaks

  // --- RESTORE SETTINGS ---
  COLSPACING = oldCol;
  ROWSPACING = oldRow;
  tft.setTextSize(2);                             // reset to standard
  tft.setTextColor(textColor);                    // back to standard coulour
  // ----------------------------------
}

void mimic (char* text)
{
  if (button_downtime > LONGPRESS)                // did user hold button down > 1 sec?
    mimic2 (text);                                // yes, so use alternate scoring mode 
  else mimic1 (text);                             // no, so use original scoring mode
}

void flashcards()
{
    while (!button_pressed)
  {
     int index = random(0,ELEMENTS(morse));       // get a random character
     int code = morse[index];                     // convert to morse code
     if (!code) continue;                         // only do valid morse chars
     sendElements(code);                          // sound it out
     delay(1000);                                 // wait for user to guess
     tft.setCursor(120,70);
     tft.setTextSize(5);
     tft.print(char('!'+index));                  // show the answer
     delay(FLASHCARDDELAY);                       // wait a little
     newScreen();                                 // and start over.
  }
}



//===================================  Config Menu  =====================================

void printConfig()                                // debugging only; not called
{
  DBG.println("EEPROM contents");
  for (int i=0; i<25; i++) {
    int value = EEPROM.read(i);
    DBG.print(i); DBG.print(": ");
    DBG.println(value,HEX);
  }
  DBG.println();
}

void saveConfig()
{
  EEPROM.write(0,42);                             // the answer to everything
  EEPROM.write(1,charSpeed);                      // save the character speed in wpm
  EEPROM.write(2,codeSpeed);                      // save overall code speed in wpm
  EEPROM.write(3,pitch/10);                       // save pitch as 1/10 of value
  EEPROM.write(4,ditPaddle);                      // save pin corresponding to 'dit'
  EEPROM.write(5,kochLevel);                      // save current Koch lesson #
  EEPROM.write(6,usePaddles);                     // save key type
  EEPROM.write(7,xWordSpaces);                    // save extra word spaces
  for (int i=0; i<10; i++)                        // save callsign,
    EEPROM.write(8+i,myCall[i]);                  // one letter at a time
  EEPROM.write(18,keyerMode);                     // save keyer mode (1=A, 2=B)
  EEPROM.write(19,startItem);                     // save startup activity
  EEPROM.write(20,brightness);                    // save screen brightness
  EEPROM.write(21,highByte(textColor));           // save text color
  EEPROM.write(22,lowByte(textColor));
  EEPROM.write(23,highByte(bgColor));             // save background color
  EEPROM.write(24,lowByte(bgColor));
  EEPROM.write(25, inputDelay);
  EEPROM.write(26, (byte)lastExercise);           // Claude: [C13] last exercise (-1 -> 255)
  EEPROM.write(27, lastPractice);                 // Claude: [C13] Practice sub-mode
  EEPROM.write(28, useBluetooth);                 // Claude: [C16] Bluetooth on/off
  EEPROM.commit();                                // ESP32 only
}

void loadConfig()
{
  int flag = EEPROM.read(0);                      // saved values been saved before?
  if (flag==42)                                   // yes, so load saved parameters
  {
     charSpeed   = EEPROM.read(1);
     codeSpeed   = EEPROM.read(2);
     pitch       = EEPROM.read(3)*10;
     ditPaddle   = EEPROM.read(4);
     kochLevel   = EEPROM.read(5);
     usePaddles  = EEPROM.read(6);
     xWordSpaces = EEPROM.read(7);
     for (int i=0; i<10; i++)
       myCall[i] = EEPROM.read(8+i);
     keyerMode   = EEPROM.read(18);
     startItem   = EEPROM.read(19);
     brightness  = EEPROM.read(20);
     textColor   =(EEPROM.read(21)<<8)            // add color high byte
                 + EEPROM.read(22);               // and color low byte
     bgColor     =(EEPROM.read(23)<<8)
                 + EEPROM.read(24);
     inputDelay  = EEPROM.read(25);
     lastExercise = EEPROM.read(26);              // Claude: [C13] (255 = none, corrected in checkConfig)
     lastPractice = EEPROM.read(27);              // Claude: [C13]
     useBluetooth = (EEPROM.read(28) == 1);       // Claude: [C16] (old devices: 0xFF -> off)
  }
  for (int i=0; i<10; i++) callRaw[i] = EEPROM.read(8+i);  // Claude: [C20] diagnostics
  checkConfig();                                  // Claude: [C2] ALWAYS validate, also with a blank
                                                  //   EEPROM (previously only when flag==42)
}

void checkConfig()                                // ensure config settings are valid
{
  if ((charSpeed<MINSPEED)||(charSpeed>MAXSPEED)) // validate character speed
    charSpeed = DEFAULTSPEED;                     
  if ((codeSpeed<MINSPEED)||(codeSpeed>MAXSPEED)) // validate code speed
    codeSpeed = DEFAULTSPEED;
  // Claude: [C8] Farnsworth speed never higher than character speed
  //   and difference max. 19 WPM. Otherwise extracharDit() can become negative and
  //   smartDelay() waits (unsigned!) practically forever.
  if (codeSpeed > charSpeed) codeSpeed = charSpeed;
  if ((charSpeed - codeSpeed) > 19) codeSpeed = charSpeed - 19;
  if (codeSpeed < MINSPEED) codeSpeed = MINSPEED;
  if ((pitch<MINPITCH)||(pitch>MAXPITCH))         // validate pitch
    pitch = DEFAULTPITCH;
  if ((kochLevel<0)||(kochLevel>ELEMENTS(koch)))  // validate koch lesson number
    kochLevel = 0;
  if (xWordSpaces>MAXWORDSPACES)                  // validate word spaces
     xWordSpaces = 0;
  if (!isAlphaNumeric(myCall[0]))                 // validate callsign
  {
     strcpy(myCall,"W8BH");
     callWasReset = true;                         // Claude: [C20] remember for the startup display
  }
  myCall[9] = 0;                                  // Claude: [C20] make sure the string is terminated
  if ((keyerMode<0)||(keyerMode>3))               // validate keyer mode  (Claude: [C12] 3 = Ultimatic)
    keyerMode = IAMBIC_B;
  if ((brightness<10)||(brightness>100)) 
    brightness=100;                               // validate screen brightness
  if (textColor==bgColor)                         // text&bg colors must be different
  {
     textColor = CYAN;
     bgColor = BLACK;    
  }
  if (!validStartup(startItem))                   // Claude: [C13/C14] instead of (startItem<-1)||(startItem>27)
    startItem = -1;                               // validate startup screen
                                                  // Validation for Input Delay
                                                  // Allows values between 5 and 80 (if need to be changed later)
                                                  // standard is 25
  if (inputDelay < 5 || inputDelay > 80) inputDelay = 25;
  if (!isExercise(lastExercise)) lastExercise = -1;   // Claude: [C13] real exercises only
  if ((lastPractice < 0) || (lastPractice > 1)) lastPractice = 0;
}

void useDefaults()                                // if things get messed up...
{
  charSpeed   = DEFAULTSPEED;
  codeSpeed   = DEFAULTSPEED;
  pitch       = DEFAULTPITCH;
  ditPaddle   = PADDLE_A;
  dahPaddle   = PADDLE_B;
  kochLevel   = 1;
  usePaddles  = true;
  xWordSpaces = 0;
  strcpy(myCall,"W8BH");
  keyerMode   = IAMBIC_B;
  brightness  = 100;
  textColor   = TEXTCOLOR;
  bgColor     = BG;
  startItem   = -1;
  inputDelay  = 25;
  lastExercise = -1;                            // Claude: [C13]
  lastPractice = 0;
  ditPeriod   = intracharDit();               // Claude: dit length matching the default values
  setToneFrequency();                          // Claude: [C7] activate default pitch
  saveConfig();
  roger();
}

char *ltrim(char *str) {                          // quick n' dirty trim routine
  while (*str==' ') str++;                        // dangerous but efficient
  return str;
}

void selectionString(int choice, char *str)       // return menu item in str
{
  strcpy(str,"");
  if (choice<0) return;                           // validate input value
  int i = choice/10;                              // get menu from selection
  int j = choice%10;                              // get menu item from selection
  switch (i)                                      // Claude: [C14] check menu array bounds
  {
    case 0: if (j < (int)ELEMENTS(menu0)) strcpy(str,ltrim(menu0[j])); break;   // show choice from menu0
    case 1: if (j < (int)ELEMENTS(menu1)) strcpy(str,ltrim(menu1[j])); break;   // show choice from menu1
    case 2: if (j < (int)ELEMENTS(menu2)) strcpy(str,ltrim(menu2[j])); break;   // show choice from menu2
    default: ;                                    // oopsie
  }
}

// Claude: [C13] Is "choice" an exercise from Receive or Send (not Exit)?
bool isExercise(int choice)
{
  if (choice < 0) return false;
  int i = choice/10, j = choice%10;
  if (i == 0) return j < (int)ELEMENTS(menu0) - 1;  // last entry is Exit
  if (i == 1) return j < (int)ELEMENTS(menu1) - 1;
  return false;                                   // config items are never remembered
}

// Claude: [C14] Is there a menu entry for this startup value?
bool validStartup(int choice)
{
  if (choice == -1 || choice == STARTUP_LAST) return true;
  if (choice < 0 || choice > 29) return false;
  char str[12];
  selectionString(choice, str);
  return (str[0] != 0) && (strncmp(str, "Exit", 4) != 0);  // Claude: [C16] Exit is not a valid startup choice
}

void showMenuChoice(int choice)
{
  char str[12];
  const int x=180,y=40;                           // screen posn for startup display
  tft.fillRect(x,y,130,32,bgColor);               // erase any prior entry
  tft.setCursor(x,y);
  if (choice<0) tft.print("Main Menu");
  else if (choice == STARTUP_LAST) {              // Claude: [C13] new option
    tft.println("Last");
    tft.setCursor(x,y+16);
    tft.print("Exercise");
  }
  else {
    tft.println(ltrim(mainMenu[choice/10]));      // show top menu choice on line 1
    tft.setCursor(x,y+16);                        // go to next line
    selectionString(choice,str);                  // get menu item
    tft.print(str);                               // and show it
  }
}

void changeStartup()                              // choose startup activity
{
  tft.setTextSize(2);
  tft.println("\nStartup:");
  int i = startItem;
  if (!validStartup(i)) i=-1;                     // Claude: [C14] dont wig out on bad value
  showMenuChoice(i);                              // show current startup choice
  button_pressed = false;
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      // Claude: [C13/C14] up to STARTUP_LAST, skipping values without
      //   a menu entry (previously -1..27 without any check)
      int n = i;
      do { n += dir; } while (n > -1 && n < STARTUP_LAST && !validStartup(n));
      if (n >= -1 && n <= STARTUP_LAST) i = n;    // keep within bounds
      showMenuChoice(i);                          // display new startup choice
    }
  }
  startItem = i;                                  // update startup choice
}


void changeBackground()
{
  const int x=180,y=150;                          // screen posn for text display
  tft.setTextSize(2);
  tft.println("\n\n\nBackground:");
  tft.drawRect(x-6,y-6,134,49,WHITE);             // draw box around background
  button_pressed = false;
  int i = 0;
  tft.fillRect(x-5,y-5,131,46,colors[i]);         // show current background color
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      i += dir;                                   // change color up/down
      i = constrain(i,0,ELEMENTS(colors)-1);      // keep within given colors
      tft.fillRect(x-5,y-5,131,46,colors[i]);     // show new background color
    }
  }
  bgColor = colors[i];                            // save background color
}

void changeTextColor()
{
  const char sample[] = "ABCDE";
  const int x=180,y=150;                          // screen posn for text display
  tft.setTextSize(2);
  tft.println("Text Color:");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  int i = 7;                                      // start with cyan for fun
  tft.setTextColor(colors[i]);
  tft.print(sample);                              // display text in cyan
  button_pressed = false;
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      i += dir;                                   // change color up/down
      i = constrain(i,0,ELEMENTS(colors)-1);      // keep within given colors
      tft.setCursor(x,y);
      tft.setTextColor(colors[i],bgColor);        // show text in new color
      tft.print(sample);
    }
  }
  textColor = colors[i];                          // update text color
}

void changeBrightness()
{
  const int x=180,y=100;                           // screen position
  tft.println("\n\n\nBrightness:");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(brightness);                          // show current brightness
  button_pressed = true;                          // on ESP32, inhibit user input                 
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob
    {
      brightness += dir*5;                        // so change level up/down, 5% increments
      brightness = constrain(brightness,10,100);  // stay in range
      tft.fillRect(x,y,100,40,bgColor);           // erase old value
      tft.setCursor(x,y);
      tft.print(brightness);                      // and display new value
      setBrightness(brightness);                  // update screen brightness
    }
  }
}

void setScreen()
{
  changeStartup();                                // set startup screen
  changeBrightness();                             // set display brightness
  changeBackground();                             // set background color
  changeTextColor();                              // set text color
  saveConfig();                                   // save settings 
  roger();                                        // and acknowledge
  clearScreen();
}

void setCodeSpeed()
{
  const int x=240,y=50;                           
  tft.println("\nEnter");
  tft.print("Code Speed (WPM):");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(charSpeed);                           
  button_pressed = false;
  
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   
    {
                                                         // 1. save distance
      int currentDiff = charSpeed - codeSpeed;

                                                         // 2. change
      charSpeed += dir;                           
      charSpeed = constrain(charSpeed, MINSPEED, MAXSPEED);
      
                                                        // 3. reset difference (sync Gap)
      codeSpeed = charSpeed - currentDiff;

                                                        // 4. check borders (delta max 19)
      if ((charSpeed - codeSpeed) > 19) codeSpeed = charSpeed - 19;
      if (codeSpeed < MINSPEED) codeSpeed = MINSPEED;

      tft.fillRect(x,y,50,50,bgColor);            
      tft.setCursor(x,y);
      tft.print(charSpeed);                       
    }
  }
  ditPeriod = intracharDit();                     
}

void setFarnsworth()
{
  const int x=240,y=100;                          // screen posn for speed display
  
  // safety check at function start
  if (codeSpeed > charSpeed) codeSpeed = charSpeed;

  tft.setTextSize(2);
  tft.println("\n\n\nFarnsworth");
  tft.print("Speed (WPM):");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(codeSpeed);                           // display current speed
  button_pressed = false;
  
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      codeSpeed += dir;                           // ...so change speed up/down 
      
                                                  // calc of lower limit
      int minAllowed = charSpeed - 19;
      if (minAllowed < MINSPEED) minAllowed = MINSPEED;
      
                                                  // Limit: no less than minAllowed, not over charSpeed
      codeSpeed = constrain(codeSpeed, minAllowed, charSpeed);
      // ---------------------------------------

      tft.fillRect(x,y,50,50,bgColor);            // erase old speed
      tft.setCursor(x,y);
      tft.print(codeSpeed);                       // and show new speed    
    }
  }                
  ditPeriod = intracharDit();                     // adjust dit length
}

void setExtraWordDelay()                          // add extra word spacing
{
  const int x=240,y=150;                          // screen posn for speed display
  tft.setTextSize(2);
  tft.println("\n\n\nExtra Word Delay");
  tft.print("(Spaces):");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(xWordSpaces);                         // display current space
  button_pressed = false;
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob:
    {
      xWordSpaces += dir;                         // ...so change value up/down 
      xWordSpaces = constrain(xWordSpaces,        // stay in range 0..MAX
        0,MAXWORDSPACES);         
      tft.fillRect(x,y,50,50,bgColor);            // erase old value
      tft.setCursor(x,y);
      tft.print(xWordSpaces);                     // and show new value  
    }
  }                
}

void setInputDelay()
{
  const int x=240,y=150;                          
  
                                                               //Initiate screen
  tft.setTextSize(2);
  tft.println("\n\n\nInput Timeout");
  tft.print("(x dit):");
  
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(inputDelay);                                      // output of input delay
    delay(500);
    button_pressed = false;                                   
    while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir != 0)                                   
    {
      inputDelay += dir * 5;                      
      inputDelay = constrain(inputDelay, 5, 80); 
      
                                                             // refresh value
      tft.fillRect(x, y, 90, 50, bgColor);            
      tft.setCursor(x,y);
      tft.print(inputDelay);                      
    }
  }
  
  saveConfig();                                   
  roger();
}

void setSpeed()
{
  setCodeSpeed();                                 // get code speed
  setFarnsworth();                                // get farnsworth delay
  setExtraWordDelay();                            // Thank you Mark AJ6CU!
  saveConfig();                                   // save the new speed values
  roger();                                        // and acknowledge  
}

void setPitch()
{
  const int x=120,y=80;                           // screen posn for pitch display
  tft.print("Tone Frequency (Hz)");
  tft.setTextSize(4);
  tft.setCursor(x,y);
  tft.print(pitch);                               // show current pitch                 
  while (!button_pressed)
  {
    int dir = readEncoder();
    if (dir!=0)                                   // user rotated encoder knob
    {
      pitch += dir*50;                            // so change pitch up/down, 50Hz increments
      pitch=constrain(pitch,MINPITCH,MAXPITCH);   // stay in range
      setToneFrequency();                         // Claude: [C7] apply new pitch
      tft.fillRect(x,y,100,40,bgColor);           // erase old value
      tft.setCursor(x,y);
      tft.print(pitch);                           // and display new value
      keyDown();                                  // Tone on
      delay(ditPeriod);                           // Wait (blocking, without Encoder-Check!)
      keyUp();                                    // Tone off
      delay(ditPeriod);                           // short delay
    }
  }
  saveConfig();                                   // save the new pitch 
  roger();                                        // and acknowledge
}

void configKey()
{
  tft.print("Currently: ");                       // show current settings:
  if (!usePaddles)                                // using paddles?
    tft.print("Straight Key");                    // not on your life!
  else if (keyerMode == ULTIMATIC)                // Claude: [C12]
    tft.print("Ultimatic");
  else {
    tft.print("Iambic Mode ");                    // of course paddles, but which keyer mode?
    if (keyerMode==IAMBIC_B) tft.print("B");      // show iambic B or iambic A
    else tft.print("A");
  }
  tft.setCursor(0,60);
  tft.println("Send a dit NOW");                  // first, determine which input is dit
  while (!button_pressed &&
    !ditPressed() && !dahPressed()) ;             // wait for user input
  if (button_pressed) return;                     // user wants to exit
  ditPaddle = !digitalRead(PADDLE_A)?
    PADDLE_A: PADDLE_B;                           // dit = whatever pin went low.
  dahPaddle = !digitalRead(PADDLE_A)?
    PADDLE_B: PADDLE_A;                           // dah = opposite of dit.
  tft.println("OK.\n");
  saveConfig();                                   // save it
  roger();                                        // and acknowledge.

  tft.println("Send Dit again for Key");          // now, select key or paddle input
  tft.println("Send Dah for Paddles");
  while (!button_pressed &&
    !ditPressed() && !dahPressed()) ;             // wait for user input
  if (button_pressed) return;                     // user wants to exit
  usePaddles = dahPressed();                      // dah = paddle input
  tft.println("OK.\n");
  saveConfig();                                   // save it
  roger();

  if (!usePaddles) return;

  // Claude: [C12] Select keyer mode: dit = Iambic A, dah = Iambic B,
  //   squeeze (both paddles) = Ultimatic. Idea from V3_04.
  //   Instead of 30 ms (V3_04) there is a 250 ms window in which the
  //   second paddle may join - pressing both within 30 ms is hardly
  //   possible by hand.
  tft.println("Dit: Iambic A");
  tft.println("Dah: Iambic B");
  tft.println("Squeeze: Ultimatic");

  while ((ditPressed() || dahPressed()) && !button_pressed) ; // wait for release first (previous input)
  delay(30);                                      // let contact bounce settle

  while (!button_pressed && !ditPressed() && !dahPressed()) ; // wait for user input
  if (button_pressed) return;                     // user wants to exit

  bool sawDit = false, sawDah = false;
  unsigned long t0 = millis();
  while (millis() - t0 < 250) {                   // time window for squeeze
    if (ditPressed()) sawDit = true;
    if (dahPressed()) sawDah = true;
  }
  if (sawDit && sawDah) keyerMode = ULTIMATIC;
  else if (sawDit)      keyerMode = IAMBIC_A;
  else                  keyerMode = IAMBIC_B;

  while ((ditPressed() || dahPressed()) && !button_pressed) ; // wait for release
  delay(30);

  tft.println("OK.\n");
  saveConfig();                                   // save it
  if (keyerMode==IAMBIC_A) sendCharacter('A');
  else if (keyerMode==IAMBIC_B) sendCharacter('B');
  else sendCharacter('U');                        // Claude: [C12] Ultimatic
}

// Claude: [C20] Diagnostics: show EEPROM bytes 8..17 (callsign) as hex
//   in small font. On the startup screen: the values read before
//   validation; in the Callsign menu: the current EEPROM contents.
void showCallRaw(int x, int y, const char *label)
{
  tft.setTextSize(1);
  tft.setTextColor(YELLOW, bgColor);
  tft.setCursor(x, y);
  tft.print(label);
  for (int i=0; i<10; i++) {
    byte b = (y >= 228) ? callRaw[i] : EEPROM.read(8+i);
    char h[4]; snprintf(h, sizeof(h), " %02X", b);
    tft.print(h);
  }
  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(2);
}

void setCallsign() {
  int cx = tft.getCursorX(), cy = tft.getCursorY(); // Claude: [C20] remember cursor
  showCallRaw(0, 215, "EEPROM 8-17:");            // Claude: [C20] diagnostics
  tft.setCursor(cx, cy);                          // Claude: [C20] restore cursor
  tft.setTextSize(2);                             // Set font size for menu text [cite: 572]
  char ch, response[20];   
  tft.print("\n Enter Callsign:");                // Display prompt [cite: 572]
  tft.setTextColor(CYAN);                         // Set color for input [cite: 573]
  strcpy(response, "");                           // Start with empty string [cite: 573]
  
  // CENTER LOGIC
  // Standard callsigns are ~6 chars. For font size 2, 
  // we start at column 6 to appear centered.
  textRow = 2;                                    // Row below the prompt
  textCol = 6;                                    // Adjusted column for center appearance
  
  while (!button_pressed && !ditPressed() && !dahPressed()); // Wait for first key [cite: 574]
  
  do {                                            
    ch = morseInput();                            // Get character from paddles [cite: 575]
    if (ch != ' ') {
      addChar(response, ch);                      // Append to response string [cite: 576]
      addCharacter(ch);                           // Display char using global logic [cite: 577]
    }
  } while (ch != ' ');                            // Exit on word timeout [cite: 578]
  
  if (button_pressed) return;                     // Exit without saving on button press [cite: 578]
  
  strcpy(myCall, response);                       // Save input to global variable [cite: 579]
  delay(1000);                                    // Short delay for visual confirmation [cite: 579]
  saveConfig();                                   // Store to EEPROM [cite: 580]
  roger();                                        // Acknowledge with 'R' [cite: 580]
}

//===================================  Screen Routines ====================================

//  The screen is divided into 3 areas:  Menu, Icons, and Body
//
//  [------------------------------------------------------]
//  [     Menu                                      Icons  ]
//  [------------------------------------------------------]
//  [                                                      ]
//  [     Body                                             ]
//  [                                                      ]
//  [------------------------------------------------------]
//
//  "Menu" is where the main menu is displayed
//  "Icons" is for battery icon and other special flags (30px)
//  "Body" is the writable area of the screen  

void clearMenu()
{
  tft.fillRect(0,0,DISPLAYWIDTH-30,ROWSPACING,bgColor);   
}

void clearBody()
{
  tft.fillRect(0, TOPMARGIN, DISPLAYWIDTH, DISPLAYHEIGHT, bgColor);  
}

void clearScreen()
{
  tft.fillScreen(bgColor);                        // fill screen with background color
  tft.drawLine(0,TOPMARGIN-6,DISPLAYWIDTH,
    TOPMARGIN-6, YELLOW);                         // draw horizontal menu line  
} 

void newScreen() {
  tft.fillScreen(bgColor);
  
  // MANDATORY: reset internal variables
  textRow = 0;
  textCol = 0; 
  
  // MANDATORY: set library cursor to the start position
  tft.setCursor(0, TOPMARGIN); 

  displayWPM(); 
  updateGrid(currentTextSize);
  tft.setTextSize(currentTextSize); 
  tft.drawLine(0, TOPMARGIN - 6, DISPLAYWIDTH, TOPMARGIN - 6, YELLOW);
}


//===================================  Menu Routines ====================================

void showCharacter(char c, int row, int col)      // display a character at given row & column
{
  int x = col * COLSPACING;                       // convert column to x coordinate
  int y = TOPMARGIN + (row * ROWSPACING);         // convert row to y coordinate
  tft.setCursor(x,y);                             // position character on screen
  tft.print(c);                                   // and display it 
}



int getMenuSelection()                            // Display menu system & get user selection
{
  int item;
  newScreen();                                    // start with fresh screen
  menuCol = topMenu(mainMenu,ELEMENTS(mainMenu)); // show horiz menu & get user choice
  switch (menuCol)                                // now show menu that user selected:
  {
   case 0: 
     item = subMenu(menu0,ELEMENTS(menu0));       // "receive" dropdown menu
     break;
   case 1: 
     item = subMenu(menu1,ELEMENTS(menu1));       // "send" dropdown menu
     break;
   case 2: 
     item = subMenu(menu2,ELEMENTS(menu2));       // "config" dropdown menu
  }
  return (menuCol*10 + item);                     // return user's selection
}

// Claude: [C15] Straight key input for Free Sending and Koch Trainer.
//   Returns the same bit pattern as getPatternFromPaddles() (format of the
//   morse[] table), 0 = word end / no input / abort.
//   The straight key is connected to the "dit" input (as in straightKeyInput()).
uint8_t getPatternFromStraightKey() {
  uint8_t pattern = 0;
  uint8_t bitPos = 0;
  const unsigned long DEBOUNCE = 10;                               // ms, as in straightKeyInput()
  unsigned long charGapMs = 2UL * ditPeriod;                       // character end after 2 dits of pause
  unsigned long wordGapMs = (unsigned long)(ditPeriod * (inputDelay / 5.0)); // word end as with paddles
  unsigned long dahLimit  = 2UL * ditPeriod;                       // longer than 2 dits = dah
  unsigned long lastRelease = millis();

  while (true) {
    // wait for key down, or detect character/word end
    while (!ditPressed()) {
      unsigned long idle = millis() - lastRelease;
      if (bitPos == 0) {
        if (idle > wordGapMs) return 0;                            // no input -> word end
      } else if (idle > charGapMs) {
        return (pattern | (1 << bitPos));                          // character end: set stop bit
      }
      if (button_pressed) return 0;                                // abort via encoder button
    }

    unsigned long down = millis();                                 // key pressed
    keyDown();                                                     // sidetone on

    unsigned long up;
    while (true) {                                                 // wait for release (debounced)
      if (button_pressed) { keyUp(); return 0; }
      if (!ditPressed()) {
        up = millis();
        while (!ditPressed() && (millis() - up < DEBOUNCE)) ;      // really open for 10 ms?
        if (!ditPressed()) break;                                  // yes: released
      }
    }
    keyUp();                                                       // sidetone off

    unsigned long dur = up - down;
    if (dur < DEBOUNCE) continue;                                  // ignore glitch
    if (dur <= dahLimit) pattern |= (1 << bitPos);                 // Dit = 1, Dah = 0
    bitPos++;
    lastRelease = up;
    if (bitPos > 6) return 0;                                      // character too long
  }
}

uint8_t getPatternFromPaddles() {
  if (!usePaddles) return getPatternFromStraightKey();  // Claude: [C15] straight key selected?
                                                        //   (previously always the paddle keyer -> no dah possible)
  uint8_t pattern = 0;
  uint8_t bitPos = 0;
  unsigned long lastActivity = millis();

  // Claude: [C1] Separate thresholds for character end and word end.
  //   Previously: character end after ditPeriod * inputDelay/5 (default 5 dits) plus
  //   1 dit from dit()/dah() = approx. 6 dits. A correct letter space is
  //   only 3 dits, though -> two cleanly sent letters merged into one
  //   pattern -> red "?".
  //   New: character end after 1.5 dits of additional pause (approx. 2.5 dits total,
  //   safely between 1 dit element space and 3 dits letter space).
  //   inputDelay/5 now only determines when a space (word end) is reported
  //   without input. Default 25 -> 5 dits; together with the
  //   2.5 dits of the character end this gives approx. 7.5 dits = word space.
  unsigned long charGapMs = (ditPeriod * 3UL) / 2;                 // Claude: [C1]
  unsigned long wordGapMs = (unsigned long)(ditPeriod * (inputDelay / 5.0)); // Claude: [C1]

  while (true) {
    // wait for paddle press or timeout (end of character)
    while (!ditPressed() && !dahPressed() && !ditRequest && !dahRequest) {
      unsigned long idle = millis() - lastActivity;
      if (bitPos == 0) {
        if (idle > wordGapMs) return 0;          // Claude: [C1] no input -> word end / space
      } else {
        if (idle > charGapMs)                    // Claude: [C1] character end
          return (pattern | (1 << bitPos));
      }
      if (button_pressed) return 0; // abort via encoder button
      updatePaddlePriority();       // Claude: [C12] detect release (for Ultimatic edges)
    }

    // Claude: [C12] common keyer logic (Iambic A/B or Ultimatic)
    uint8_t el = nextElement();
    if (el == 1) {
      pattern |= (1 << bitPos); // set bit for dit
      dit(); // sends dit and checks for dah requests
    } else if (el == 2) {
      // dah is 0, bit stays 0
      dah(); // sends dah and checks for dit requests
    } else {
      continue;                 // Claude: [C12] paddle already released -> don't count an empty element
    }

    bitPos++;
    lastActivity = millis();
    if (bitPos > 6) {                             // safety against overly long characters
      ditRequest = false;                         // Claude: [C10] clear stored requests,
      dahRequest = false;                         //   otherwise "phantom elements" follow
      return 0;
    }
  }
}


void setTopMenu(char *str)                        // erase menu & replace with new text
{
  clearMenu();
  showMenuItem(str,0,0,FG,bgColor);
}

void showSelection(int choice)                    // display current activity on top menu
{
  char str[10];                                   // reserve room for string
  selectionString(choice,str);                    // get menu item that user chose
  setTopMenu(str);                                // and show it in menu bar
}

void showMenuItem(char *item, int x, int y, int fgColor, int bgColor)
{
  tft.setCursor(x,y);
  tft.setTextColor(fgColor, bgColor);
  tft.print(item);
}

int topMenu(char *menu[], int itemCount)          // Display a horiz menu & return user selection
{
  int index = menuCol;                            // start w/ current row
  button_pressed = false;                         // reset button flag
  tft.setTextSize(2);                             // Text size 2 for menu header
  for (int i = 0; i < itemCount; i++)             // for each item in menu                         
    showMenuItem(menu[i],i*MENUSPACING,0,FG,bgColor);  // display it 
  showMenuItem(menu[index],index*MENUSPACING,     // highlight current item
    0,SELECTFG,SELECTBG);

  while (!button_pressed)                         // loop for user input:
  {
    int dir = readEncoder();                      // check encoder
    if (dir) {                                    // did it move?
      showMenuItem(menu[index],index*MENUSPACING,
        0, FG,bgColor);                           // deselect current item
      index += dir;                               // go to next/prev item
      if (index > itemCount-1) index=0;           // dont go beyond last item
      if (index < 0) index = itemCount-1;         // dont go before first item
      showMenuItem(menu[index],index*MENUSPACING,
        0, SELECTFG,SELECTBG);                    // select new item         
    }  
  }
  return index;  
}

// Claude: [C17] Print a submenu entry with fixed width. Entries differ
//   in length; when scrolling (menus with more than MAXROW = 8
//   entries, e.g. Receive and now also Config) remnants of longer
//   entries remained on screen. Now padded with spaces.
#define SUBMENU_WIDTH  10                         // characters (longest entry " Speed Cfg")

// Claude: [C22] Submenus may use the full screen height (down to the bottom
//   edge), so menus with 9 entries (Receive, Config) fit without scrolling and
//   with unchanged row spacing. The 9th row then reaches into the WPM line
//   (y >= 220), but the WPM display only occupies x = 115..175 and the
//   submenu columns are placed so that they do not overlap it horizontally:
//   each entry is padded only to the length of the longest entry of THIS menu
//   (Receive: 9 chars = 108 px at x = 0; Config: 10 chars at x = 200).
//   MAXROW (used for text output) is not changed.
int menuRows  = 0;                                // rows available for the current submenu
int menuWidth = SUBMENU_WIDTH;                    // padding width for the current submenu

void setupSubMenuGeometry(char *menu[], int itemCount)
{
  menuRows = (DISPLAYHEIGHT - TOPMARGIN) / ROWSPACING;   // full height (9 rows at size 2)
  if (menuRows < 1) menuRows = 1;
  menuWidth = 1;
  for (int i = 0; i < itemCount; i++) {                  // longest entry of this menu
    int len = strlen(menu[i]);
    if (len > menuWidth) menuWidth = len;
  }
  if (menuWidth > SUBMENU_WIDTH) menuWidth = SUBMENU_WIDTH;
}

void showSubMenuItem(char *item, int x, int y, int fgColor, int bgColor)
{
  char buf[SUBMENU_WIDTH + 1];
  snprintf(buf, sizeof(buf), "%-*s", menuWidth, item);  // left-aligned, padded with blanks (Claude: [C22] per-menu width)
  showMenuItem(buf, x, y, fgColor, bgColor);
}

void displayFrame(char *menu[], int top, int itemCount)
{
  int x = menuCol * MENUSPACING;                  // x-coordinate of this menu
  for (int i = 0; i < menuRows; i++)              // for all items in the frame  (Claude: [C22] was MAXROW)
  {
     int y = TOPMARGIN + i*ROWSPACING;            // calculate y coordinate
     int item = top + i;
     if (item<itemCount)                          // make sure item exists
       showSubMenuItem(menu[item],x,y,FG,bgColor); // and show the item.  (Claude: [C17])
  } 
}

int subMenu(char *menu[], int itemCount)          // Display drop-down menu & return user selection
{
  tft.setTextSize(2);                             // normal (2) submenues
  int index=0,top=0,pos=0,x,y; 
  button_pressed = false;                         // reset button flag
  x = menuCol * MENUSPACING;                      // x-coordinate of this menu
  setupSubMenuGeometry(menu, itemCount);          // Claude: [C22] rows/width for this menu
  displayFrame(menu,0,itemCount);                 // display as many menu items as possible
  showSubMenuItem(menu[0],x,TOPMARGIN,            // highlight first item  (Claude: [C17])
    SELECTFG,SELECTBG);

  while (!button_pressed)                         // exit on button press
  {
    int dir = readEncoder();                      // check for encoder movement
    if (dir)                                      // it moved!    
    {
      if ((dir>0) && (index==(itemCount-1)))      // dont try to go below last item
        continue;
      if ((dir<0) && (index==0))                  // dont try to go above first item
        continue;
      if ((dir>0) && (pos==(menuRows-1)))         // does the frame need to move down?  (Claude: [C22] was MAXROW)
      {                              
        top++;                                    // yes: move frame down,
        displayFrame(menu,top,itemCount);         // display it,
        index++;                                  // and select next item
      }
      else if ((dir<0) && (pos==0))               // does the frame need to move up?
      {
        top--;                                    // yes: move frame up,
        displayFrame(menu,top,itemCount);         // display it,
        index--;                                  // and select previous item
      }
      else                                        // we must be moving within the frame
      {
        y = TOPMARGIN + pos*ROWSPACING;           // calc y-coord of current item
        showSubMenuItem(menu[index],x,y,FG,bgColor); // deselect current item  (Claude: [C17])
        index += dir;                             // go to next/prev item
      }
      pos = index-top;                            // posn of selected item in visible list
      y = TOPMARGIN + pos*ROWSPACING;             // calc y-coord of new item
      showSubMenuItem(menu[index],x,y,
        SELECTFG,SELECTBG);                       // select new item  (Claude: [C17])
    }
  }
  return index;  
}


//================================  Main Program Code ================================

void setBrightness(int level)                     // level 0 (off) to 100 (full on)       
{
                                                  // not implemented in ESP32 version at this time 
}

void initEncoder()
{
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);

  // Start the decoder with the encoder's actual electrical state.
  encoderPrevState  = digitalRead(ENCODER_A);
  encoderPrevState |= (digitalRead(ENCODER_B) << 1);

  attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON),buttonISR,CHANGE);   
  attachInterrupt(digitalPinToInterrupt(ENCODER_A),rotaryISR,CHANGE); 
  attachInterrupt(digitalPinToInterrupt(ENCODER_B),rotaryISR,CHANGE); 
}

void initMorse()
{
                                                    // replaces ledcSetup and ledcAttachPin:
                                                    // Parameter: (Pin, frequency, resolution)
  ledcAttach(AUDIO, pitch, 10);                    // Claude: [C7] start with the configured pitch
  setToneFrequency();                              // Claude: [C7] set frequency once, tone off
  pinMode(LED, OUTPUT);                            // LED, but could be keyer output instead
  pinMode(PADDLE_A, INPUT_PULLUP);                 // two Paddle-Inputs
  pinMode(PADDLE_B, INPUT_PULLUP);

  ditPeriod = intracharDit();                      // calculate timing
  dahPaddle = (ditPaddle == PADDLE_A) ? PADDLE_B : PADDLE_A;

  roger();                                         // confirm per Morse 'R'
}



void initScreen()
{
  tft.begin();                                    // initialize screen object
  tft.setRotation(SCREEN_ROTATION);               // landscape mode: use '1' or '3'
  tft.fillScreen(BLACK);                          // start with blank screen
  setBrightness(100);                             // start screen full brighness
}

void splashScreen()                               // not splashy at all!
{                                                 // have fun sprucing it up.
  tft.setTextSize(3);
  tft.setTextColor(YELLOW);
  tft.setCursor(100, 50); 
  tft.print(myCall);                              // display user's callsign
  tft.setTextColor(CYAN);
  tft.setCursor(15, 90);      
  tft.print("Morse Code Tutor");                  // add title
  tft.setTextSize(2);                             // Print SW Version
  tft.setTextColor(GREEN);                        
  tft.setCursor(80, 150);                         // position: centered below the title
  tft.print("SW: Oct 2026 BT9");                      // Claude: [C11] SW date (timing fix)
  tft.setTextSize(1);
  tft.setCursor(50,220);
  tft.setTextColor(WHITE);
  tft.print("Copyright (c) 2020, Bruce E. Hall"); // legal small print
  tft.setTextSize(2);
}

void updateGrid(int textSize) {
  currentTextSize = textSize;
  COLSPACING = 6 * textSize; 
  ROWSPACING = (8 * textSize) + (textSize * 3);
  
  MAXCOL = DISPLAYWIDTH / COLSPACING;
  // only write up to pixel 210 to leave room for WPM (from 220):
  MAXROW = (210 - TOPMARGIN) / ROWSPACING; 
}

void addChar(char* str, char c) {
  int len = strlen(str);
  if (len < 19) {
    str[len] = c;
    str[len + 1] = '\0';
  }
}

char randomLetter() {
  return 'A' + random(26);
}

char randomNumber() {
  return '0' + random(10);
}



//===================================  Bluetooth  ========================================
// Claude: [C16] Bluetooth keyboard (BLE HID). See header [C16] for details.

#define BT_DEVICE_NAME   "MorseTutor"             // name shown when pairing
#define BT_KEY_DIT       0x01                     // modifier bit Left Ctrl
#define BT_KEY_DAH       0x10                     // modifier bit Right Ctrl
#define BT_REPORT_ID     1

BLECharacteristic *btInput = nullptr;             // keyboard input report

static const uint8_t btReportMap[] = {            // standard keyboard with report ID 1
  0x05, 0x01,       // Usage Page (Generic Desktop)
  0x09, 0x06,       // Usage (Keyboard)
  0xA1, 0x01,       // Collection (Application)
  0x85, BT_REPORT_ID, //   Report ID
  0x05, 0x07,       //   Usage Page (Key Codes)
  0x19, 0xE0,       //   Usage Minimum (224 = Left Ctrl)
  0x29, 0xE7,       //   Usage Maximum (231 = Right GUI)
  0x15, 0x00,       //   Logical Minimum (0)
  0x25, 0x01,       //   Logical Maximum (1)
  0x75, 0x01,       //   Report Size (1)
  0x95, 0x08,       //   Report Count (8)
  0x81, 0x02,       //   Input (Data, Variable, Absolute)   ; modifier byte
  0x95, 0x01,       //   Report Count (1)
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x01,       //   Input (Constant)                   ; reserved
  0x95, 0x05,       //   Report Count (5)
  0x75, 0x01,       //   Report Size (1)
  0x05, 0x08,       //   Usage Page (LEDs)
  0x19, 0x01,       //   Usage Minimum (Num Lock)
  0x29, 0x05,       //   Usage Maximum (Kana)
  0x91, 0x02,       //   Output (Data, Variable, Absolute)  ; LED report
  0x95, 0x01,       //   Report Count (1)
  0x75, 0x03,       //   Report Size (3)
  0x91, 0x01,       //   Output (Constant)                  ; padding bits
  0x95, 0x06,       //   Report Count (6)
  0x75, 0x08,       //   Report Size (8)
  0x15, 0x00,       //   Logical Minimum (0)
  0x25, 0x65,       //   Logical Maximum (101)
  0x05, 0x07,       //   Usage Page (Key Codes)
  0x19, 0x00,       //   Usage Minimum (0)
  0x29, 0x65,       //   Usage Maximum (101)
  0x81, 0x00,       //   Input (Data, Array)                ; 6 keys
  0xC0              // End Collection
};

// Claude: [C16b] Reports the result of pairing/encryption (diagnostics)
class BtSecurityCallbacks : public BLESecurityCallbacks {
  bool onConfirmPIN(uint32_t pin) override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    btLastAuth = cmpl.success ? 0 : cmpl.fail_reason;   // Claude: [C16e] for the display
    if (cmpl.success) DBG.println("BT: pairing/encryption OK");
    else DBG.printf("BT: pairing FAILED (reason 0x%02X)\n", cmpl.fail_reason);
  }
};

class BtServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) override {
    btConnected = true;
    DBG.println("BT: connected");              // Claude: [C16b] diagnostics in the Serial Monitor
    btConnCount++;                                // Claude: [C16e]
    for (int h = 0; h <= 2; h++)                  // Claude: [C21] full power for the connection too
      esp_ble_tx_power_set((esp_ble_power_type_t)(ESP_BLE_PWR_TYPE_CONN_HDL0 + h), ESP_PWR_LVL_P9);
    // request a short connection interval: 6..12 x 1.25 ms = 7.5..15 ms,
    // latency 0, timeout 4 s (400 x 10 ms). The PC makes the final decision.
    pServer->updateConnParams(param->connect.remote_bda, 6, 12, 0, 400);
  }
  void onDisconnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) override {
    btConnected = false;                          // advertising restarts automatically
    DBG.printf("BT: disconnected (reason 0x%02X)\n", param->disconnect.reason); // Claude: [C16b]
    btLastDisc = param->disconnect.reason;        // Claude: [C16e]
  }
};

void btSendState(uint8_t mods)                    // send modifier state to the PC
{
  if (!btConnected || (btInput == nullptr)) return;
  uint8_t report[8] = {mods, 0, 0, 0, 0, 0, 0, 0};
  btInput->setValue(report, sizeof(report));
  btInput->notify();
}

// Separate task on core 0: polls the paddles every millisecond and reports
// only changes. The tutor code on core 1 is not affected.
void btPaddleTask(void *arg)
{
  uint8_t last = 0, sent = 0;
  for (;;) {
    uint8_t now = 0;
    if (digitalRead(ditPaddle) == 0) now |= BT_KEY_DIT;       // active low
    if (usePaddles && (digitalRead(dahPaddle) == 0)) now |= BT_KEY_DAH;
    if (!btConnected) sent = 0;                   // after reconnecting: state "released"
    else if ((now == last) && (now != sent)) {    // same value read twice = debounced
      btSendState(now);
      sent = now;
    }
    last = now;
    vTaskDelay(1);                                // 1 ms (tick rate 1000 Hz)
  }
}

void startBluetooth()
{
  BLEDevice::init(BT_DEVICE_NAME);
  // Claude: [C21] Maximum TX power (+9 dBm instead of the default +3 dBm),
  //   for advertising (visibility/reconnect) and all connections.
  //   +6 dB roughly doubles the range.
  BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_DEFAULT);
  BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);
  BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_SCAN);
  BLEDevice::setSecurityCallbacks(new BtSecurityCallbacks());   // Claude: [C16b]
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new BtServerCallbacks());
  server->advertiseOnDisconnect(true);            // visible again after disconnect

  BLEHIDDevice *hid = new BLEHIDDevice(server);
  btInput = hid->inputReport(BT_REPORT_ID);
  hid->outputReport(BT_REPORT_ID);                // LED report (ignored)
  hid->manufacturer()->setValue("DIY");           // Claude: [C16a] create the characteristic first, then set it
                                                  //   (manufacturer(String) alone -> invalid pointer -> crash/reset)
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x03);                       // Claude: [C16d] RemoteWake + NormallyConnectable (was 0x01):
                                                  //   tells the PC that it should reconnect

  BLESecurity *security = new BLESecurity();      // pairing with bonding, no PIN
  // Claude: [C16d] Bonding, no MITM, Secure Connections (automatically falls back
  //   to legacy pairing if the PC does not support SC). This variant
  //   also makes the tutor start encryption ITSELF after connecting -
  //   important for a stable connection.
  //   (Changing this in [C16b] to setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND)
  //   was a mistake: same pairing mode, but without starting encryption.)
  security->setAuthenticationMode(true, false, true);
  security->setCapability(ESP_IO_CAP_NONE);

  hid->reportMap((uint8_t *)btReportMap, sizeof(btReportMap));
  hid->startServices();
  hid->setBatteryLevel(100);

  BLEAdvertising *adv = server->getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->start();

  xTaskCreatePinnedToCore(btPaddleTask, "btPaddle", 3072, NULL, 2, NULL, 0); // core 0
}

void showBtState(bool on)                         // ON/OFF gross anzeigen
{
  const int x=240, y=150;
  tft.setTextSize(4);
  tft.fillRect(x, y, 80, 40, bgColor);
  tft.setCursor(x, y);
  tft.print(on ? "ON" : "OFF");
  tft.setTextSize(2);
}

void setBluetooth()                               // menu Config -> Bluetooth
{
  bool newState = useBluetooth;
  tft.setTextSize(2);
  tft.println("\nBluetooth Keyboard");
  tft.println("Name: " BT_DEVICE_NAME);
  tft.println("Dit = Left Ctrl");
  tft.println("Dah = Right Ctrl");
  tft.print("Status: ");
  if (!useBluetooth) tft.println("off");
  else {
    tft.println(btConnected ? "connected" : "not connected");
    // Claude: [C16e] Diagnostics without Serial Monitor (e.g. on mains power):
    //   number of connections, last pairing, last disconnect reason (hex)
    tft.printf("Conn:%d Auth:", btConnCount);
    if (btLastAuth < 0) tft.print("-"); else if (btLastAuth == 0) tft.print("OK"); else tft.printf("%02X", btLastAuth);
    tft.print(" Disc:");
    if (btLastDisc < 0) tft.println("-"); else tft.printf("%02X\n", btLastDisc);
  }
  showBtState(newState);
  delay(300);
  button_pressed = false;
  while (!button_pressed) {
    if (readEncoder() != 0) {                     // turning = toggle
      newState = !newState;
      showBtState(newState);
    }
  }
  if (newState != useBluetooth) {                 // changed: save and restart
    useBluetooth = newState;
    saveConfig();
    tft.setCursor(0, 210);
    tft.print("Restart...");
    delay(1000);
    ESP.restart();
  }
  roger();
}


void setup() {
#if USE_SERIAL                                    // Claude: [C18]
  Serial.begin(115200);
#else
  esp_log_level_set("*", ESP_LOG_NONE);           // Claude: [C18] system log messages off as well
#endif
  initScreen();
  updateGrid(2); // initializes MAXCOL and MAXROW
  
  EEPROM.begin(64);
  loadConfig();
  // Claude: [C16a] Emergency exit: hold the encoder button during power-on
  //   -> Bluetooth is switched off (in case it causes problems).
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  delay(5);
  if (useBluetooth && (digitalRead(ENCODER_BUTTON) == LOW)) {
    useBluetooth = false;
    saveConfig();
  }
  splashScreen();
  if (callWasReset) showCallRaw(0, 228, "Call reset! EEPROM 8-17:");  // Claude: [C20]
  initEncoder();
  initMorse();
  // Claude: [C19] Start Bluetooth only once the supply is surely stable
  //   (at least BT_START_DELAY ms after power-on). At startup the ESP32
  //   calibrates its radio; if the voltage is not yet stable (slowly
  //   ramping switching regulator) it may calibrate wrongly -> poor
  //   connection. The waiting time falls within the existing pause of the
  //   startup screen, so power-on hardly takes longer.
  const unsigned long BT_START_DELAY = 2000;
  while (millis() < BT_START_DELAY) delay(10);    // keep startup screen visible
  if (useBluetooth) startBluetooth();             // Claude: [C16] only if enabled
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) delay(10);         // rest of the former 2 s pause
  clearScreen();
}


void loop()
{
  // Claude: [C13] Extended startup logic:
  //   - STARTUP_LAST: start last used exercise (if any)
  //   - only exercises (Receive/Send) are remembered, never Config or Exit
  bool autoStart = inStartup;                     // Claude: [C13] this pass comes from power-on
  int selection = startItem;                      // start with user specified startup screen
  if (inStartup && (startItem == STARTUP_LAST))   // Claude: [C13] "Last Exercise"
    selection = lastExercise;                     //   (-1 if no exercise remembered yet)
  if (!inStartup || (selection<0))                // but, if there isn't one,
  {
    selection = getMenuSelection();               // get menu selection from user instead
    autoStart = false;                            // Claude: [C13] chosen by the user
  }
  inStartup = false;                              // reset startup flag

  if (isExercise(selection) && (selection != lastExercise)) { // Claude: [C13] remember exercise,
    lastExercise = selection;                     //   only when changed (reduces flash wear)
    saveConfig();
  }

  showSelection(selection);                       // display users selection at top of screen
  newScreen();                                    // and clear the screen below it.
  textRow = 0;
  textCol = 0;
  button_pressed = false;                         // reset flag for new presses
  randomSeed(millis());                           // randomize!
  score=0; hits=0; misses=0;                      // restart score for copy challenges  
  switch(selection)                               // do action requested by user
  {
    case 00: sendKoch(); break;
    case 01: sendLetters(); break;
    case 02: sendWords(); break;
    case 03: sendNumbers(); break;
    case 04: sendMixedChars(); break;
    case 05: sendQSO(); break;
    case 06: sendCallsigns(); break;
    case 07: flashcards(); break;
    case 10: runPracticeMenu(autoStart && (startItem == STARTUP_LAST)); break; // Claude: [C13]
    case 11: copyOneChar(); break;
    case 12: copyTwoChars(); break;
    case 13: copyWords(); break;
    case 14: copyCallsigns(); break;
    case 15: headCopy(); break;
    case 20: setSpeed(); break;
    case 21: setPitch(); break;
    case 22: configKey(); break;
    case 23: setCallsign(); break;
    case 24: setScreen(); break;
    case 25: setInputDelay(); break;
    case 26: useDefaults(); break;
    case 27: setBluetooth(); break;                // Claude: [C16]
            
    default: ;
  }  
}

