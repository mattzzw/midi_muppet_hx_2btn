
/**************************************************************************

  BSD 3-Clause License

  Copyright (c) 2020, Matthias Wientapper
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


  Midi Foot Switch for HX Stomp
  =============================
  - Button Up/Down on pins D2, D3
  - LED green/red          D4, D5
  - requires OneButton lib https://github.com/mathertel/OneButton
  - requires JC_Button lib https://github.com/JChristensen/JC_Button

  We are using two different button libraries for different purposes:
    - OneButton to detect a long press without detecting a short press first.
      This means the action will be evaluated on releasing the button,
      which is ok for most cases but bad for timing critical actions
      like when we are in looper mode.
    - JC_Button to detect a short press as soon as the button is pressed down.
      This button library is used in looper mode only.

  SCROLL Mode:   up/dn switches prog patches
                 long press dn: TUNER
                 long press up: SNAPSHOT mode
                 up + dn: FS mode
  FS Mode:       up/dn emulate FS4/FS5
                 long press dn: TUNER
                 long press up: SNAPSHOT mode
                 up + dn: SCROLL mode
  SNAPSHOT Mode: up/dn switches snapshot
                 long press dn: TUNER
                 long press up: FS mode
  TUNER Mode:    up or dn back to prev mode
  LOOPER Mode:   dn toggles record/overdub
                 up toggles play/stop
                 long press up toggles undo/redo

**************************************************************************/

#include <Wire.h>
#include <OneButton.h>
#include <JC_Button.h>

#include <EEPROM.h>

// GPIO pins used
#define BTN_UP 2
#define BTN_DN 3
#define LED_GRN 4
#define LED_RED 5

// EEPROM addresses
#define OP_MODE_ADDR      0  // stores looper mode of operation
#define LOOPER_MODE_ADDR  1  // stores if looper control is enabled
#define MIDI_CHANNEL_ADDR 2  // stores the midi channel 

// Adjust red LED brightness 0-255 (full on was way too bright for me)
#define LED_RED_BRIGHTNESS 25
// on/off delay when we flash a LED
#define LED_FLASH_DELAY    30  

OneButton btnUp(BTN_UP, true);
OneButton btnDn(BTN_DN, true);

Button jc_btnUp(BTN_UP);
Button jc_btnDn(BTN_DN);

enum modes_t {SCROLL, SNAPSHOT, FS, LOOPER, TUNER, CHANNEL_CFG, LOOPER_CFG};       // modes of operation
static modes_t MODE;       // current mode
static modes_t LAST_MODE;  // last mode
static modes_t MODE_BEFORE_SNAPSHOT; // last mode before snap shot mode

enum lmodes_t {PLAY, RECORD, OVERDUB, STOP};   // Looper modes
static lmodes_t LPR_MODE;

bool with_looper = true;
uint8_t midi_channel = 0;

void (*Reset)(void) = 0;

void setup() {

  // LEDs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  // Buttons:
  btnUp.setClickTicks(50);
  btnUp.attachClick(upClick);
  btnUp.attachLongPressStart(upLongPressStart);

  btnDn.setClickTicks(50);
  btnDn.attachClick(dnClick);
  btnDn.attachLongPressStart(dnLongPressStart);

  // Set MIDI baud rate:
  Serial.begin(31250);


  // restore MODE from EEPROM
  MODE = (modes_t) EEPROM.read(OP_MODE_ADDR);
  if (MODE > 4){
    // no valid value in eeprom found. (Maybe this is the first power up ever?)
    MODE = SCROLL;
  }

// restore looper config from adr. 1
  with_looper = EEPROM.read(LOOPER_MODE_ADDR);
  if (with_looper > 1)
    with_looper = false;

 // restore MIDI channel
  midi_channel = EEPROM.read(MIDI_CHANNEL_ADDR);
  if (midi_channel > 15){ 
    // set channel to 0 if data not valid
    EEPROM.update(MIDI_CHANNEL_ADDR, 0);
    midi_channel = 0;
  } 

 if (digitalRead(BTN_DN) == 0 && digitalRead(BTN_UP) == 1) {
    // btn dn pressed: configure MIDI channel
      MODE = CHANNEL_CFG;
    flashLED(20, LED_GRN, LED_FLASH_DELAY);
    // 'display' configure MIDI channel
    delay(1000);
    flashLED(midi_channel + 1, LED_GRN, 500);
  }

  if (digitalRead(BTN_DN) == 1 && digitalRead(BTN_UP) == 0) {
    // btn up pressed: toggle looper ctrl enabled/disabled
    MODE = LOOPER_CFG;
    flashLED(20, LED_RED, LED_FLASH_DELAY);
    delay(1000);
    if (with_looper)
      flashLED(1, LED_GRN, 500);
    else
      flashLED(1, LED_RED, 500);
  }


  // restore mode on HX Stomp as well
  if (MODE == SNAPSHOT)
    midiCtrlChange(71, 3); // set snapshot mode
  else if (MODE == FS)
    midiCtrlChange(71, 0); // set stomp mode
  else if (MODE == SCROLL)
    midiCtrlChange(71, 0); // set stomp mode

  MODE_BEFORE_SNAPSHOT = MODE;

  // indicate mode via LEDs
  if (MODE == LOOPER) {
    flashRedGreen(10);
    // we are in looper mode, so we are using the jc_button class for action on button press
    // (OneButton acts on button release)
    jc_btnUp.begin();
    jc_btnDn.begin();
  }
  else if (MODE == SCROLL)
    flashLED(10, LED_RED, LED_FLASH_DELAY);
  else if (MODE == FS)
    flashLED(10, LED_GRN, LED_FLASH_DELAY);

  // Looper default state
  LPR_MODE = STOP;

}


void loop() {

  if (MODE == LOOPER) {
    jc_btnDn.read();                   // DN Button handled by JC_Button
    btnUp.tick();                   // Up Button handled by OneButton

    if (jc_btnDn.wasPressed() && digitalRead(BTN_UP) == 1)          // attach handler
      jc_dnClick();

  } else {
    btnUp.tick();                   // both buttons handled by OneButton
    btnDn.tick();
  }

  handle_leds();
}

/* ------------------------------------------------- */
/* ---       OneButton Callback Routines          ---*/
/* ------------------------------------------------- */

void dnClick() {
  switch (MODE)
  {
    case SCROLL:
      patchDown();
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      break;
    case TUNER:
      midiCtrlChange(68, 0); // toggle tuner
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      midiCtrlChange(69, 9);  // prev snapshot
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      break;
    case FS:
      midiCtrlChange(52, 0); // emulate FS 4
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      break;
    case LOOPER:
      switch (LPR_MODE) {
        case STOP:
          LPR_MODE = RECORD;
          midiCtrlChange(60, 127);  // Looper record
          break;
        case RECORD:
          LPR_MODE = PLAY;
          midiCtrlChange(61, 127); // Looper play
          break;
        case PLAY:
          LPR_MODE = OVERDUB;
          midiCtrlChange(60, 0);    // Looper overdub
          break;
        case OVERDUB:
          LPR_MODE = PLAY;
          midiCtrlChange(61, 127); // Looper play
          break;
      }
      break;
    case CHANNEL_CFG:
      if(midi_channel > 0)
        midi_channel--;
      flashLED(midi_channel + 1, LED_GRN, 500);
      EEPROM.update(MIDI_CHANNEL_ADDR, midi_channel);    
    break;
    case LOOPER_CFG:
      with_looper = false;
      flashLED(1, LED_RED, 500);
      EEPROM.update(LOOPER_MODE_ADDR, with_looper);    
    break;
  }
}
void upClick() {
  switch (MODE)
  {
    case SCROLL:
      patchUp();
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      break;
    case TUNER:
      midiCtrlChange(68, 0); // toggle tuner
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      midiCtrlChange(69, 8);  // next snapshot
      break;
    case FS:
      flashLED(2, LED_RED, LED_FLASH_DELAY);
      midiCtrlChange(53, 0); // emulate FS 5
      break;
    case LOOPER:
      switch (LPR_MODE) {
        case STOP:
          LPR_MODE = PLAY;
          midiCtrlChange(61, 127); // Looper play
          break;
        case PLAY:
        case RECORD:
        case OVERDUB:
          LPR_MODE = STOP;
          midiCtrlChange(61, 0); // Looper stop
          break;
      }
      break;
    case CHANNEL_CFG:
      if(midi_channel < 15)
        midi_channel++;
      flashLED(midi_channel + 1, LED_GRN, 500);
      EEPROM.update(MIDI_CHANNEL_ADDR, midi_channel);
    break;
    case LOOPER_CFG:
      with_looper = true;
      flashLED(1, LED_GRN, 500);
      EEPROM.update(LOOPER_MODE_ADDR, with_looper);    
    break;

  }
}

void dnLongPressStart() {
  if (digitalRead(BTN_UP) == 1) {
    switch (MODE)
    {
      case TUNER:
        break;
      case SCROLL:
      case SNAPSHOT:
      case FS:
        midiCtrlChange(68, 0); // toggle tuner
        flashLED(2, LED_RED, LED_FLASH_DELAY);
        LAST_MODE = MODE;
        MODE = TUNER;
        break;
      case LOOPER:
        break;
    }
  }
}

void upLongPressStart() {

  if (digitalRead(BTN_DN) == 0) {
    // yay, both buttons pressed!
    // Toggle through modes
    switch (MODE) {
      case SCROLL:
        MODE = FS;
        break;
      case FS:
        if (with_looper)
          MODE = LOOPER;
        else
          MODE = SCROLL;
        break;
      case LOOPER:
        // make sure to switch off looper
        midiCtrlChange(61, 0); // Looper stop
        MODE = SCROLL;
        break;
      case SNAPSHOT:
        if (with_looper)
          MODE = LOOPER;
        else
          MODE = SCROLL;
        break;
      case TUNER:
        break;
      case CHANNEL_CFG:
        MODE = LAST_MODE;
      break;
    }
    EEPROM.update(OP_MODE_ADDR, MODE);
    // reset the device to reboot in new mode
    Reset();
  } else {
    // regular long press event:
    switch (MODE)
    {
      case TUNER:
        break;
      case SCROLL:
        // save mode where we entered snapshot mode from
        MODE_BEFORE_SNAPSHOT = MODE;
        midiCtrlChange(71, 3); // set snapshot mode
        flashLED(5, LED_RED, LED_FLASH_DELAY);
        MODE = SNAPSHOT;
        EEPROM.update(0, MODE);
        break;
      case SNAPSHOT:
        if (MODE_BEFORE_SNAPSHOT == FS) {
          midiCtrlChange(71, 0); // set stomp mode
          flashLED(5, LED_RED, LED_FLASH_DELAY);
          MODE = FS;
        } else {
          if (MODE_BEFORE_SNAPSHOT == SCROLL) {
            midiCtrlChange(71, 0); // set stomp mode
            flashLED(5, LED_RED, LED_FLASH_DELAY);
            MODE = SCROLL;
          }
        }
        EEPROM.update(0, MODE);
        break;
      case FS:
        // save mode where we entered snapshot mode from
        MODE_BEFORE_SNAPSHOT = MODE;
        midiCtrlChange(71, 3); // set snapshot mode
        flashLED(5, LED_RED, LED_FLASH_DELAY);
        MODE = SNAPSHOT;
        EEPROM.update(0, MODE);
        break;
      case LOOPER:
        switch (LPR_MODE) {
          case PLAY:
          case STOP:
            midiCtrlChange(63, 127); // looper undo/redo
            flashLED(3, LED_RED, LED_FLASH_DELAY);
            break;
          case RECORD:
          case OVERDUB:
            break;
        }

        break;
    }
  }
}

/* ------------------------------------------------- */
/* ---       JC_Button Callback Routines          ---*/
/* ------------------------------------------------- */

void jc_dnClick() {
  switch (LPR_MODE) {
    case STOP:
      LPR_MODE = RECORD;
      midiCtrlChange(60, 127);  // Looper record
      break;
    case RECORD:
      LPR_MODE = PLAY;
      midiCtrlChange(61, 127); // Looper play
      break;
    case PLAY:
      LPR_MODE = OVERDUB;
      midiCtrlChange(60, 0);    // Looper overdub
      break;
    case OVERDUB:
      LPR_MODE = PLAY;
      midiCtrlChange(61, 127); // Looper play
      break;
  }
}



/* ------------------------------------------------- */
/* ---      Midi Routines                         ---*/
/* ------------------------------------------------- */

// Use these routines if you are using firmware 3.01 or older:

// HX stomp does not have a native patch up/dn midi command
// so we are switching to scroll mode and emulating a FS1/2
// button press.
/*
void patchUp() {
  midiCtrlChange(71, 1); // HX scroll mode
  delay(30);
  midiCtrlChange(50, 127); // FS 2 (up)
  midiCtrlChange(71, 0);  // HX stomp mode
}

void patchDown() {
  midiCtrlChange(71, 1);   // HX scroll mode
  delay(30);
  midiCtrlChange(49, 127); // FS 1 (down)
  midiCtrlChange(71, 0);   // HX stomp mode
}
*/

// Added in Firmware 3.10:
//New MIDI message: CC 72 value 64-127 = next preset, value 0-63 = previous preset
void patchUp() {
  midiCtrlChange(72, 127); // next preset
}

void patchDown() {
  midiCtrlChange(72, 0);   // prev preset
}


void midiProgChange(uint8_t p) {
  Serial.write(0xc0 | midi_channel); // PC message
  Serial.write(p); // prog
}
void midiCtrlChange(uint8_t c, uint8_t v) {
  Serial.write(0xb0 | midi_channel); // CC message
  Serial.write(c); // controller
  Serial.write(v); // value
}

/* ------------------------------------------------- */
/* ---      Misc Stuff                            ---*/
/* ------------------------------------------------- */
void flashLED(uint8_t i, uint8_t led, uint8_t del)
{
  uint8_t last_state;
  last_state = digitalRead(led);

  for (uint8_t j = 0; j < i; j++) {
    digitalWrite(led, HIGH);
    delay(del);
    digitalWrite(led, LOW);
    delay(del);
  }
  digitalWrite(led, last_state);
}

void flashRedGreen(uint8_t i) {
  uint8_t last_state_r;
  uint8_t last_state_g;
  last_state_r = digitalRead(LED_RED);
  last_state_g = digitalRead(LED_GRN);


  for (uint8_t j = 0; j < i; j++) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GRN, HIGH);
    delay(75);
    analogWrite(LED_RED, LED_RED_BRIGHTNESS);
    digitalWrite(LED_GRN, LOW);
    delay(75);
  }
  digitalWrite(LED_RED, last_state_r);
  digitalWrite(LED_GRN, last_state_g);
}

void handle_leds() {
  static unsigned long next = millis();

  switch (MODE) {
    case SCROLL:
      // solid red
      digitalWrite(LED_GRN, LOW);
      analogWrite(LED_RED, LED_RED_BRIGHTNESS);
      //digitalWrite(LED_RED, HIGH);
      break;
    case SNAPSHOT:
      // solid green
      digitalWrite(LED_GRN, HIGH);
      digitalWrite(LED_RED, LOW);
      break;
    case FS:
      // solid green
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GRN, HIGH);
      break;
    case TUNER:
      // blink green
      if (millis() - next > 500) {
        next += 500;
        digitalWrite(LED_GRN, !digitalRead(LED_GRN));
      }
      break;
    case LOOPER:
      switch (LPR_MODE) {
        case STOP:
          digitalWrite(LED_GRN, LOW);
          digitalWrite(LED_RED, LOW);
          break;
        case PLAY:
          digitalWrite(LED_GRN, HIGH);
          digitalWrite(LED_RED, LOW);
          break;
        case RECORD:
          digitalWrite(LED_GRN, LOW);
          analogWrite(LED_RED, LED_RED_BRIGHTNESS);
          break;
        case OVERDUB:
          // yellow
          digitalWrite(LED_GRN, HIGH);
          analogWrite(LED_RED, LED_RED_BRIGHTNESS);
          break;
      }
      break;
  }
}
