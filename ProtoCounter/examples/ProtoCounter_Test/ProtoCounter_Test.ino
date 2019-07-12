/**********************************************************************************

Title:    Test Software for the ProtoCounter Tx13 Board

CPU:      ATtiny4313 processor running at 8 MHz.
          
Author:           Frank Andre
Copyright 2015:   Frank Andre
License:          see "license.md"
Disclaimer:
          This software is provided by the copyright holder "as is" and any 
          express or implied warranties, including, but not limited to, the 
          implied warranties of merchantability and fitness for a particular 
          purpose are disclaimed. In no event shall the copyright owner or 
          contributors be liable for any direct, indirect, incidental, 
          special, exemplary, or consequential damages (including, but not 
          limited to, procurement of substitute goods or services; loss of 
          use, data, or profits; or business interruption) however caused 
          and on any theory of liability, whether in contract, strict 
          liability, or tort (including negligence or otherwise) arising 
          in any way out of the use of this software, even if advised of 
          the possibility of such damage.

Description:
          This software may be used to test that the ProtoCounter is 
          working properly.

          First it shows a short display test. Then you have three modes 
          of operation:
            - manual counter
            - automatic counter
            - analog knob

          The system starts in manual mode. You can press the upper / lower
          button to increase / decrease the counter value.

          Pressing the upper button for more than 1 second switches to 
          automatic mode. The counter is counting at a high speed. When it 
          reaches the limit it will reverse the counting direction. You can 
          reverse the counting direction manually by pressing the upper button. 
          Pressing the lower button resets the counter value to 0. Holding 
          the upper button pressed changes back to manual mode.

          Pressing the lower button for more than 1 second switches to 
          analog knob mode. If you have an analog knob attached to the 
          ProtoCounter (see ProtoCounter documentation for details) a 
          value corresponding to its position is displayed. This can be 
          used to find good values for the capacitor and potentiometer.

          Bonus feature:
          If there is an external output shift register attached to the 
          ProtoCounter with leds connected to its outputs it will show 
          a "Knight Rider" animation. See the ProtoCounter documentation 
          ("I/O expansion") for details.
          
          
**********************************************************************************/

/* Bedienungsanleitung
 *
 * Es gibt drei Modi: Handzähler, Automatikzähler, Analogknopf
 *
 * Gestartet wird als Handzähler. Ein langer Druck auf den oberen Knopf (BTN1)
 * wechselt zwischen Handzähler und Automatikzähler. Ein langer Druck auf den
 * unteren Knopf (BTN2) wechselt zwischen Handzähler und Analogknopf.
 *
 * Auf einem externen Schieberegister läuft das "Knight Rider"-Lauflicht.
 */


#include <ProtoCounter.h>

// modes
#define MANUAL  0
#define AUTO  1
#define ANALOG  2


byte   mode;
byte   scanner, scan_dir;
int    value;
byte   count_up;

ProtoCounter pc;


void segment_test()
{
  byte i, led;

  led = 1;
  for(i = 0; i < 13; i++) {
    pc.setDisplay(led, 0);
    pc.setDisplay(led, 1);
    pc.setDisplay(led, 2);
    delay(100);
    led <<= 1;
    if (led == 0x40) { led = 1; }
  }
  pc.setDisplay(0x7F, 0);
  pc.setDisplay(0x7F, 1);
  pc.setDisplay(0x7F, 2);
  delay(1000);
}


void setup() {
  // put your setup code here, to run once:
  pc.init();
  segment_test();
  mode = MANUAL;
  value = 0;
  count_up = 1;
  scanner = 1;
  scan_dir = 1;
}


void loop() {
  // put your main code here, to run repeatedly:

  // ----- push buttons -----
  if (pc.getButton() == BTN1_RELEASED) {
    pc.buttonAck();
    if (mode == MANUAL) {
      value++;
    }
    else if (mode == AUTO) {
      if (count_up == 1) {
        pc.writeString_P(PSTR("dn "));
        count_up = 0;
      }
      else {
        pc.writeString_P(PSTR("UP "));
        count_up = 1;
      }
      delay(500);
    }
  }

  if (pc.getButton() == BTN2_RELEASED) {
    pc.buttonAck();
    if (mode == MANUAL) {
      value--;
    }
    else if (mode == AUTO) {
      pc.writeString_P(PSTR("rES"));
      value = 0;
      delay(500);
      segment_test();
    }
  }

  if (pc.getButton() == BTN1_LONGPRESSED) {
    pc.buttonAck();
    if (mode == AUTO) {
      mode = MANUAL;
      pc.writeString_P(PSTR("cnt"));
      delay(500);
    }
    else {
      mode = AUTO;
      pc.writeString_P(PSTR("run"));
      delay(500);
    }
  }

  if (pc.getButton() == BTN2_LONGPRESSED) {
    pc.buttonAck();
    if (mode == ANALOG) {
      mode = MANUAL;
      pc.writeString_P(PSTR("cnt"));
      delay(500);
    }
    else {
      mode = ANALOG;
      pc.writeString_P(PSTR("AnA"));
      delay(500);
    }
  }

  // ----- display -----
  if (mode == MANUAL) {
    pc.writeInt(value);
  }
  else if (mode == AUTO) {
    if (count_up == 1) {
      value++;
      if (value == MAX_DECIMAL) {
        count_up = 0;
      }
    }
    else {
      value--;
      if (value == MIN_DECIMAL) {
        count_up = 1;
      }
    }
    pc.writeInt(value);
  }
  else if (mode == ANALOG) {
    pc.writeInt(pc.getAnalog());
  }

  // ----- scanner -----
  if (scan_dir == 1) {
    if (scanner == 0x40) {
      scan_dir = 0;
    }
    scanner <<= 1;
  }
  else {
    if (scanner == 0x02) {
      scan_dir = 1;
    }
    scanner >>= 1;
  }
  pc.writeShiftRegister(scanner);

  delay(70);
}


/**********************
 * interrupt routines *
 **********************/

// ProtoCounter uses timer0 OC0B interrupt routine.
ISR(TIMER0_COMPB_vect)
{
	sei();		// re-enable interrupts so that the ProtoCounter routine can be 
				// interrupted by other, more time-critical interrupts.
	pc.update();
}
