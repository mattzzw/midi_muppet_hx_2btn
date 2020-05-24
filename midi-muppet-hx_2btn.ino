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

NORMAL Mode:   up/dn switches prog patches
               long press dn: TUNER
               long press up: SNAPSHOT
               up + dn: toggle FS4/FS5 and NORMAL mode
TUNER Mode:    up or dn back to prev Mode
SNAPSHOT Mode: up/dn switches snapshot?
               long press dn: TUNER
               long press up: FS mode
FS Mode:       up/dn emulate FS4/FS5
               long press up: TUNER
               long press dn: back to NORMAL mode

**************************************************************************/

#include <Wire.h>
#include <OneButton.h>

#define BTN_UP 2
#define BTN_DN 3
#define LED_GRN 4
#define LED_RED 5

OneButton btnUp(2, true);
OneButton btnDn(3, true);

enum modes_t {ACTIVE, TUNER, SNAPSHOT, FS};       // modes of operation
static modes_t MODE;       // current mode
static modes_t LAST_MODE;  // last mode

void setup() {
 // LEDs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  // Buttons:
  btnUp.setClickTicks(200);
  btnUp.attachClick(upClick);
  btnUp.attachLongPressStart(upLongPressStart);
  //  btnUp.attachLongPressStop(upLongPressStop);
  //  btnUp.attachDuringLongPress(upLongPress);

  btnDn.setClickTicks(200);
  btnDn.attachClick(dnClick);
  btnDn.attachLongPressStart(dnLongPressStart);
  //  btnDn.attachLongPressStop(dnLongPressStop);
  //  btnDn.attachDuringLongPress(dnLongPress);

  // Set MIDI baud rate:
  Serial.begin(31250);

  MODE = ACTIVE;

   // say hello (aka reset occured)
  flashLED(10);
}

void loop() {
  btnUp.tick();
  btnDn.tick();


   // LED indicates FS mode
   if (MODE == FS)
      digitalWrite(LED_GRN, HIGH);
   else
      digitalWrite(LED_GRN, LOW);

   if (MODE == SNAPSHOT)
      digitalWrite(LED_RED, HIGH);
   else
      digitalWrite(LED_RED, LOW);
   

}

// --- Button Callback Routines ---

void dnClick() {
  switch (MODE)
  {
    case ACTIVE:
      patchDown();
      flashLED(2);
      break;
    case TUNER:
      midiCtrlChange(68,0);  // toggle tuner
      flashLED(2);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      midiCtrlChange(69, 9);  // prev snapshot
      flashLED(2);
      break;
    case FS:
      midiCtrlChange(52,0);  // emulate FS 4
      flashLED(2);
      break;

  }
}
void upClick() {
  switch (MODE)
  {
    case ACTIVE:
      patchUp();
      flashLED(2);
      break;
    case TUNER:
      midiCtrlChange(68,0);  // toggle tuner
      flashLED(2);
      MODE = LAST_MODE;
      break;
    case SNAPSHOT:
      flashLED(2);
      midiCtrlChange(69, 8);  // next snapshot
      break;
    case FS:
      flashLED(2);
      midiCtrlChange(53,0);  // emulate FS 5
      break;
  }
}

void dnLongPressStart(){
 switch (MODE)
  {
    case ACTIVE:
    case SNAPSHOT:
    case FS:
      midiCtrlChange(68,0);  // toggle tuner
      flashLED(2);
      LAST_MODE = MODE;
      MODE = TUNER;
      break;
  }
}

void upLongPressStart(){
 switch (MODE)
  {
    case ACTIVE:
      midiCtrlChange(71,3);  // set snapshot mode
      flashLED(5);
      MODE = SNAPSHOT;
      break;
    case SNAPSHOT:
      midiCtrlChange(71,0);  // set stomp mode
      flashLED(5);
      MODE = FS;
      break;
    case FS:
      flashLED(5);
      MODE = ACTIVE;
      break;
  }  
}


// --- Midi Routines ---


// HX stomp does not have a native patch up/dn midi command
// so we are switching to scroll mode and emulating a FS1/2 
// button press.
void patchUp(){
 midiCtrlChange(71,1);  // HX scroll mode
 delay(30);
 midiCtrlChange(50,127); // FS 2 (up)
 midiCtrlChange(71,0);   // HX stomp mode
}

void patchDown(){
 midiCtrlChange(71,1);    // HX scroll mode
 delay(30);
 midiCtrlChange(49,127);  // FS 1 (down)
 midiCtrlChange(71,0);    // HX stomp mode
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

// --- misc stuff ---

void flashLED(uint8_t i)
{
  uint8_t last_state;
  last_state = digitalRead(LED_RED);
  
  for (uint8_t j = 0; j < i; j++) {
    digitalWrite(LED_RED, HIGH);
    delay(30);
    digitalWrite(LED_RED, LOW);
    delay(30);
  }
  digitalWrite(LED_RED, last_state);
}
