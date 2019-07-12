#include <ProtoCounter.h>

/**********************************************************************************

Description:    A two input event counter

          - Press upper button for 2 seconds to enter "set limit" mode.
            Upper button increases, lower button decreases limit value.
            When done press upper button for 2 seconds again to switch 
            back to "counting" mode.
          - Hold the lower button pressed for 2 seconds to change the 
            mode of input B between "add" and "subtract".
          - In "counting" mode a high-to-low transition on input A increases 
            the counter value by one. A high-to-low transition on input B 
            either increases or decreases the counter value by one depending 
            on its input mode.
          - Once a high-to-low transition has been detected on one of the 
            inputs this input becomes inactive for a certain time span 
            (deadtime).
          - When in "counting" mode pressing the lower button resets the 
            counter value to zero while pressing the upper button sets the 
            counter to the limit value.
          - If the counter equals the limit value the output is activated 
            for a certain duration (outputDuration).

Hardware: ProtoCounter Tx13 with an ATtiny4313 processor
          PB2 = input A
          PB3 = input B
          PB4 = limit output
          
Author:           Frank Andre
Copyright 2016:   Frank Andre
License:          see "license.md"
Disclaimer:     This software is provided by the copyright holder "as is" and any 
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
          
**********************************************************************************/


// modes
#define COUNTING  0
#define SET_LIMIT 1   // set limit value


/********************
 * global variables *
 ********************/

const byte inputA = 11;     // use PB2 as inputA
const byte inputB = 12;     // use PB3 as inputB
const byte output = 13;     // use PB4 as limit output
const unsigned long outputDuration = 1000;  // time for which the output is activated (in milliseconds)
const unsigned long deadtime = 100;         // time for which the input is disabled (in milliseconds)
const int  limitIncrement = 50;         // amount by which the limit value can be increased or decreased

ProtoCounter pc;

int           counter;                 // current counter value
int           limit;                   // limit value
int           increment;               // increment for inputB (either +1 or -1)
byte          inA, inB;
boolean       deadA, deadB;
unsigned long eventTimeA, eventTimeB;
unsigned long outputTime;
boolean       outputActive;
byte          mode;


/*************
 * functions *
 *************/

void checkLimit()
// This routine is called whenever the counter or limit value has changed.
{
  if (counter == limit) {
    digitalWrite(output, HIGH);
    outputTime = millis();
    outputActive = true;
  }
}


/*********
 * setup *
 *********/

void setup() {
  // put your setup code here, to run once:

  pc.init();

  counter = 0;
  limit = 100;
  increment = +1;

  pinMode(inputA, INPUT_PULLUP);
  pinMode(inputB, INPUT_PULLUP);
  pinMode(output, OUTPUT);
  digitalWrite(output, LOW);
  inA = 1;
  inB = 1;
  deadA = false;
  deadB = false;
  outputActive = false;
  checkLimit();
  mode = COUNTING;
  pc.writeInt(counter);
}


/********
 * loop *
 ********/

void loop() {
  // put your main code here, to run repeatedly:

  byte input;

  if (mode == COUNTING) {
    if (deadA) {
      if ((millis() - eventTimeA) >= deadtime) {    // deadtime elapsed ?
        deadA = false;                              // -> rearm input
      }
    }
    else {                                  // ---- sample input A --------------------
      input = digitalRead(inputA);
      if ((inA == 1) && (input == 0)) {     // high-to-low transition ?
        if (counter < MAX_DECIMAL) {
          eventTimeA = millis();            // start deadtime
          deadA = true;                     // disable input
          counter++;
          pc.writeInt(counter);
          checkLimit();
        }
      }
      inA = input;
    }

    if (deadB) {
      if ((millis() - eventTimeB) >= deadtime) {    // deadtime elapsed ?
        deadB = false;                              // -> rearm input
      }
    }
    else {                                  // ---- sample input B --------------------
      input = digitalRead(inputB);
      if ((inB == 1) && (input == 0)) {     // high-to-low transition ?
        if ((counter > MIN_DECIMAL) && (counter < MAX_DECIMAL)) {
          eventTimeB = millis();            // start deadtime
          deadB = true;                     // disable input
          counter += increment;
          pc.writeInt(counter);
          checkLimit();
        }
      }
      inB = input;
    }
  }   // end of if (mode == COUNTING)

  if (outputActive) {                        // ---- output ----------------------------
    if ((millis() - outputTime) >= outputDuration) {    // output duration elapsed ?
      digitalWrite(output, LOW);
      outputActive = false;
    }
  }

  
  if (pc.getButton() == BTN1_RELEASED) {    // ---- upper button (short press) ---------
    pc.buttonAck();
    if (mode == SET_LIMIT) {
      if (limit <= (MAX_DECIMAL - limitIncrement)) {
        limit += limitIncrement;
      } else {
        limit = 0;
      }
      pc.writeInt(limit);
    }
    else {
      counter = limit;
      pc.writeInt(counter);
    }
  }

  if (pc.getButton() == BTN2_RELEASED) {    // ---- lower button (short press) ---------
    pc.buttonAck();
    if (mode == SET_LIMIT) {
      if (limit >= (MIN_DECIMAL + limitIncrement)) {
        limit -= limitIncrement;
      } else {
        limit = 0;
      }
      pc.writeInt(limit);
    }
    else {
      counter = 0;
      pc.writeInt(counter);
    }
  }

  if (pc.getButton() == BTN1_LONGPRESSED) {  // ---- upper button (long press) ----------
    pc.buttonAck();
    if (mode == SET_LIMIT) {
      pc.writeString_P(PSTR("CNT"));
      delay(500);
      checkLimit();
      pc.writeInt(counter);
      mode = COUNTING;
    }
    else {
      pc.writeString_P(PSTR("SET"));
      delay(500);
      pc.writeInt(limit);
      mode = SET_LIMIT;
    }
  }

  if (pc.getButton() == BTN2_LONGPRESSED) {  // ---- lower button (long press) ----------
    pc.buttonAck();
    if (increment == +1) {
      increment = -1;
      pc.writeString_P(PSTR("SUB"));
    }
    else {
      increment = +1;
      pc.writeString_P(PSTR("ADD"));
    }
    delay(500);
    if (mode == SET_LIMIT) {
      pc.writeInt(limit);
    }
    else {
      pc.writeInt(counter);
    }
  }

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
