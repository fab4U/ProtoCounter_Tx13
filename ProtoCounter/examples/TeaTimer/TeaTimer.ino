#include <ProtoCounter.h>

/**********************************************************************************

Description:    A simple count-down timer.

          - Press upper button for 2 seconds to enter "set time" mode.
            Upper button changes minutes, lower button changes seconds.
            When done press upper button for 2 seconds again to switch 
            back to run mode.
          - In "run" mode the lower button starts and stops the timer.
            The upper button resets the time.
          - When the time has elapsed an alarm will be signaled. Press 
            any key to leave "alarm" mode.

          Use funtions "enter_running" and "leave_running" when you want 
          to activate a device while the timer is running.
          Use funtions "enter_alarm" and "leave_alarm" when you want 
          to activate a device after the timer has elapsed.

Hardware: ProtoCounter Tx13 with an ATtiny4313 processor
          Close solder jumper J1 in position B to turn on a separating dot 
          between minutes and seconds.
          
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
#define SET_TIME  0   // set count-down time
#define HALT      1   // timer has been halted
#define RUNNING   2   // timer is running
#define ALARM     3   // time has elapsed


/********************
 * global variables *
 ********************/

const unsigned long tick_duration = 1000;   // duration of 1 clock tick (in milliseconds)

ProtoCounter pc;

byte   start_min, start_sec;   // start time: minutes, seconds
byte   minutes, seconds;       // current time: minutes, seconds
byte   mode;
unsigned long current_millis, previous_millis;
int value;


/*************
 * functions *
 *************/

void reset_timer()
{
  if (mode == RUNNING) {
    leave_running();
  }
  else if (mode == ALARM) {
    leave_alarm();
  }
  minutes = start_min;
  seconds = start_sec;
  show_time(minutes, seconds);
}


void show_time(byte m, byte s)
{
  if ((m > 9) || (s > 59)) {
    pc.writeString_P(PSTR("---"));
    return;
  }
  pc.writeInt(s);
  pc.writeChar(m, 2);
}


void enter_running()
// This routine is called when the timer enters the RUNNING state.
{

}


void leave_running()
// This routine is called when the timer leaves the RUNNING state.
{

}


void enter_alarm()
// This routine is called when the timer has elapsed.
{
  delay(500);
  pc.clearDisplay();
  delay(500);
  pc.writeString_P(PSTR(" AL"));
  delay(500);
  pc.clearDisplay();
  delay(500);
  pc.writeString_P(PSTR(" AL"));
  delay(500);
  pc.clearDisplay();
  delay(500);
  pc.writeString_P(PSTR(" AL"));
  delay(500);
  pc.clearDisplay();
  delay(1000);
  show_time(minutes, seconds);
}


void leave_alarm()
// This routine is called when the alarm is reset.
{

}


/*********
 * setup *
 *********/

void setup() {
  // put your setup code here, to run once:

  pc.init();
  pc.setDecimalPlaces(2);
  
  start_min = 2;
  start_sec = 0;
  reset_timer();
  mode = HALT;
}


/********
 * loop *
 ********/

void loop() {
  // put your main code here, to run repeatedly:

  if (pc.getButton() == BTN1_RELEASED) {
    pc.buttonAck();
    if (mode == SET_TIME) {
      if (start_min < 9) {
        start_min++;
      } else {
        start_min = 0;
      }
      show_time(start_min, start_sec);
    }
    else {
      reset_timer();
      mode = HALT;  
    }
  }

  if (pc.getButton() == BTN2_RELEASED) {
    pc.buttonAck();
    if (mode == SET_TIME) {
      if (start_sec < 50) {
        start_sec += 10;
      } else {
        start_sec = 0;
      }
      show_time(start_min, start_sec);
    }
    else if (mode == HALT) {
      previous_millis = millis();
      enter_running();
      mode = RUNNING;
    }
    else if (mode == RUNNING) {
      leave_running();
      mode = HALT;
    }
    else {
      reset_timer();
      mode = HALT;  
    }
  }

  if (pc.getButton() == BTN1_LONGPRESSED) {
    pc.buttonAck();
    if (mode == SET_TIME) {
      pc.writeString_P(PSTR("RUN"));
      delay(500);
      reset_timer();
      mode = HALT;
    }
    else {
      pc.writeString_P(PSTR("SET"));
      delay(500);
      reset_timer();
      mode = SET_TIME;
    }
  }

  if (pc.getButton() == BTN2_LONGPRESSED) {
    pc.buttonAck();
  }

  current_millis = millis();

  if (mode == RUNNING) {
    if ((current_millis - previous_millis) >= tick_duration) {
      previous_millis = current_millis;
      if (seconds > 0) {
        seconds--;
      }
      else if (minutes > 0) {
        minutes--;
        seconds = 59;
      }
      show_time(minutes, seconds);
      if ((seconds == 0) && (minutes == 0)) {
        leave_running();
        enter_alarm();
        mode = ALARM;
      }
    }
  }

  delay(20);
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
