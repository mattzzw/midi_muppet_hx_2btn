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

  SCROLL Mode:   up/dn switches prog patches
               long press dn: TUNER
               long press up: SNAPSHOT
               up + dn: toggle FS4/FS5 and SCROLL mode
  TUNER Mode:    up or dn back to prev Mode
  SNAPSHOT Mode: up/dn switches snapshot?
               long press dn: TUNER
               long press up: FS mode
  FS Mode:       up/dn emulate FS4/FS5
               long press up: TUNER
               long press dn: back to SCROLL mode

**************************************************************************/

#include <Wire.h>
#include <OneButton.h>
#include <EEPROM.h>

#define BTN_UP 2
#define BTN_DN 3
#define LED_GRN 4
#define LED_RED 5

// Adjust red LED brightness 0-255 (full on was way too bright for me)
#define LED_RED_MAX_BRIGHTNESS 25

OneButton btnUp(2, true);
OneButton btnDn(3, true);

enum modes_t {SCROLL, SNAPSHOT, FS, LOOPER, TUNER};       // modes of operation
static modes_t MODE;       // current mode
static modes_t LAST_MODE;  // last mode

enum lmodes_t {PLAY, RECORD, OVERDUB, STOP};   // Looper modes
static lmodes_t LPR_MODE;

bool led = true;

void setup() {

  uint8_t eeprom_val;

  // LEDs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GRN, OUTPUT);
  
  // say hello (aka show that reset occured)
  flashLED(3, LED_RED);

  // Buttons:
  btnUp.setClickTicks(100);
  btnUp.attachClick(upClick);
  btnUp.attachLongPressStart(upLongPressStart);
  //  btnUp.attachLongPressStop(upLongPressStop);
  //  btnUp.attachDuringLongPress(upLongPress);

  btnDn.setClickTicks(100);
  btnDn.attachClick(dnClick);
  btnDn.attachLongPressStart(dnLongPressStart);
  //  btnDn.attachLongPressStop(dnLongPressStop);
  //  btnDn.attachDuringLongPress(dnLongPress);

  // Set MIDI baud rate:
  Serial.begin(31250);

  // get last used mode from eeprom
  eeprom_val = EEPROM.read(0);

  // BTN_DN held at power up? Toggle Looper Mode
  if (digitalRead(BTN_DN) == 0) {
   if (MODE == LOOPER) {
      MODE = SCROLL;
      flashLED(10, LED_RED);
   }
    else {
      MODE = LOOPER;
      // blink red/green to show we are in looper mode
      flashRedGreen(10);
    }
    EEPROM.update(0, MODE);
  }
  // BTN_UP held at power up? Toggle FS  Mode
  else if (digitalRead(BTN_UP) == 0) {
    if ( MODE == FS) {
      MODE = SCROLL;
      flashLED(10, LED_RED);
    } else {
      MODE = FS;
      flashLED(10, LED_GRN);
    }
    EEPROM.update(0, MODE);
  }
  else {
    // no buttons pressed,
    // restore last mode, if possible (= valid value)
    if (eeprom_val < 4)
      MODE = (modes_t)eeprom_val;
    else
      // no valid value in eeprom found. (Maybe this is the first power up ever?)
      MODE = SCROLL;

    // restore mode on HX Stomp as well
    if (MODE == SNAPSHOT)
      midiCtrlChange(71, 3); // set snapshot mode
    else if (MODE == FS)
      midiCtrlChange(71, 0); // set stomp mode
    else if (MODE == SCROLL)
      midiCtrlChange(71, 0); // set stomp mode

  // indicate mode via LEDs 
   if (MODE == LOOPER)
      flashRedGreen(5);
   else
      flashLED(5, LED_RED);
 
  } 
  // Looper default state
  LPR_MODE = STOP;
}

void loop() {

  btnUp.tick();
  btnDn.tick();

  handle_leds();
}

/* ------------------------------------------------- */
/* ---       Button Callback Routines             ---*/
/* ------------------------------------------------- */

void dnClick() {
  switch (MODE)
  {
    case SCROLL:
      patchDown();
      flashLED(2, LED_RED);
      break;
    case TUNER:
      midiCtrlChange(68, 0); // toggle tuner
      flashLED(2, LED_RED);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      midiCtrlChange(69, 9);  // prev snapshot
      flashLED(2, LED_RED);
      break;
    case FS:
      midiCtrlChange(52, 0); // emulate FS 4
      flashLED(2, LED_RED);
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
  }
}
void upClick() {
  switch (MODE)
  {
    case SCROLL:
      patchUp();
      flashLED(2, LED_RED);
      break;
    case TUNER:
      midiCtrlChange(68, 0); // toggle tuner
      flashLED(2, LED_RED);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      flashLED(2, LED_RED);
      midiCtrlChange(69, 8);  // next snapshot
      break;
    case FS:
      flashLED(2, LED_RED);
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
  }
}

void dnLongPressStart() {
  switch (MODE)
  { case TUNER:
      break;
    case SCROLL:
    case SNAPSHOT:
      midiCtrlChange(68, 0); // toggle tuner
      flashLED(2, LED_RED);
      LAST_MODE = MODE;
      MODE = TUNER;
      break;
    case FS:
      break;
    case LOOPER:
      switch (LPR_MODE) {
        case PLAY:
        case STOP:
          midiCtrlChange(63, 127); // looper undo/redo
          flashLED(3, LED_RED);
          break;
        case RECORD:
        case OVERDUB:
        break;
      }
      break;
  }
}

void upLongPressStart() {
  switch (MODE)
  {
    case TUNER:
      break;
    case SCROLL:
      midiCtrlChange(71, 3); // set snapshot mode
      flashLED(5, LED_RED);
      MODE = SNAPSHOT;
      EEPROM.update(0, MODE);
      break;
    case SNAPSHOT:
      midiCtrlChange(71, 0); // set stomp mode
      flashLED(5, LED_RED);
      MODE = SCROLL;
      EEPROM.update(0, MODE);
      break;
    case FS:
      break;
    case LOOPER:
      break;
  }
}


/* ------------------------------------------------- */
/* ---      Midi Routines                         ---*/
/* ------------------------------------------------- */

// HX stomp does not have a native patch up/dn midi command
// so we are switching to scroll mode and emulating a FS1/2
// button press.
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

void midiProgChange(uint8_t p) {
  Serial.write(0xc0); // PC message
  Serial.write(p); // prog
}
void midiCtrlChange(uint8_t c, uint8_t v) {
  Serial.write(0xb0); // CC message
  Serial.write(c); // controller
  Serial.write(v); // value
}

/* ------------------------------------------------- */
/* ---      Misc Stuff                            ---*/
/* ------------------------------------------------- */
void flashLED(uint8_t i, uint8_t led)
{
  uint8_t last_state;
  last_state = digitalRead(led);

  for (uint8_t j = 0; j < i; j++) {
    digitalWrite(led, HIGH);
    delay(30);
    digitalWrite(led, LOW);
    delay(30);
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
    analogWrite(LED_RED, LED_RED_MAX_BRIGHTNESS);
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
      analogWrite(LED_RED, LED_RED_MAX_BRIGHTNESS);
      //digitalWrite(LED_RED, HIGH);
      break;
    case SNAPSHOT:
      // solid green
      digitalWrite(LED_GRN, HIGH);
      digitalWrite(LED_RED, LOW);
      break;
    case FS:
      // blink red
      if (millis() - next > 500) {
        next += 500;
        led = !led;
      }
      if (led)
        analogWrite(LED_RED, LED_RED_MAX_BRIGHTNESS);
      else
        digitalWrite(LED_RED, 0);
      digitalWrite(LED_GRN, LOW);
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
          analogWrite(LED_RED, LED_RED_MAX_BRIGHTNESS);
          break;
        case OVERDUB:
          // blink red
          if (millis() - next > 500) {
            next += 500;
            led = !led;
          }
          if (led)
            analogWrite(LED_RED, LED_RED_MAX_BRIGHTNESS);
          else
            digitalWrite(LED_RED, 0);
          digitalWrite(LED_GRN, LOW);
          break;
      }
      break;
  }
}
