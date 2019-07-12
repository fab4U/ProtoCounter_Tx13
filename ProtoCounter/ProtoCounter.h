/*
 * ProtoCounter.h
 *
 */ 

/**********************************************************************************

Author:				Frank Andre
Copyright 2015:		Frank Andre
License:			see "license.md"
Disclaimer:			This software is provided by the copyright holder "as is" and any 
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


#ifndef PROTOCOUNTER_H_
#define PROTOCOUNTER_H_

#include <inttypes.h>


/*************
 * constants *
 *************/

// display
#define ANODE_PORT		PORTB
#define ANODE_DDR		DDRB
#define ANODE1			7		// display anode 1 (left digit)
#define ANODE2			6		// display anode 2 (center digit)
#define ANODE3			5		// display anode 3 (right digit)

// push buttons
#define BTN_PORT		PORTB
#define BTN_PIN			PINB
#define BTN_DDR			DDRB
#define BTN_COM			6		// bit number of common line
#define BTN1_BIT		5		// bit number of upper push-button
#define BTN2_BIT		7		// bit number of lower push-button

// serial interface
// To use the hardware serial interface one needs to modify the tracks on 
// the ProtoCounter PCB in such a way that pins PB0, PB1 take the role of 
// pins PD0, PD1 controlling led segments A and B.
// This frees pins PD0, PD1 (RXD, TXD) for use as a serial interface.
//
// Out-comment the following line if you have made such a modification.
// #define SWAP_PINS_PD01_FOR_PB01

// external shift register
#define SH_REG_PORT		PORTB	// output register
#define SH_REG_PIN		PINB	// input register
#define SH_REG_DDR		DDRB	// data direction register
#define SH_REG_OUT_BIT	5		// data out bit number
#define SH_REG_IN_BIT	6		// data in bit number
#define SH_REG_CLK_BIT	7		// clock bit number
#define SH_REG_LD_PORT	PORTB	// shift register load signal
#define SH_REG_LD_DDR	DDRB
#define SH_REG_LD_BIT	0

// display
#define DIMMING			4		// default dimming value (0 = no dimming)
								// use dimming to reduce brightness and current consumption
#define MAX_DIGITS		3		// number of digits
#define DECIMAL_PLACES	0		// default number of decimal places (0..2)
#define MAX_DECIMAL		999		// largest decimal number that can be displayed
#define MIN_DECIMAL		-99		// smallest decimal number that can be displayed

// external shift register
// Data is shifted out with MSB first.
// To disable the external shift register set BITCOUNT to 0
#define SH_REG_IN_BITCOUNT	8	// number of input shift register bits (range 0..32)
#define SH_REG_OUT_BITCOUNT	8	// number of output shift register bits (range 0..32)
#define SH_REG_MSB_MASK			((sr_out_data_t)1 << (SH_REG_OUT_BITCOUNT - 1))

// analog knob
#define ANALOG_ENABLE			// Enable analog knob. Out-comment to disable.
// The time constant of a RC combination on pin AIN1 is measured.
// For this purpose timer0 (prescaler 1:64) is used as a timebase.
// With the controller running at 8 MHz you can achieve a resolution of
// approx. 120 steps.
// R = 1k + 470k potentiometer, C = 15 nF
#define ANALOG_MAX_RESOLUTION	0
#define ANALOG_129_DETENT_STEPS	1
#define ANALOG_86_DETENT_STEPS	2
#define ANALOG_65_DETENT_STEPS	3
#define ANALOG_43_DETENT_STEPS	4
#define ANALOG_33_DETENT_STEPS	5
#define ANALOG_22_DETENT_STEPS	6
#define ANALOG_OFF				7

// push buttons
// For Arduino: When ProtoCounter runs at 8 MHz the update cycle is approx. 2 ms.
#define BTN_SAMPLE_INTERVAL	10		// buttons are sampled every n-th update cycle
#define BTN_LONGPRESS_DELAY	50		// time (as number of sample intervals)
									// after which a longpress event is generated

// bit masks for push buttons (do not change)
#define BUTTON1				(1<<BTN1_BIT)
#define BUTTON2				(1<<BTN2_BIT)
#define BOTH_BTNS			(BUTTON1|BUTTON2)
#define BTN_MASK			(BUTTON1|BUTTON2)	// mask to extract button state
#define PB_PRESS			(1<<1)				// 1 = pressed, 0 = released
#define PB_RELEASE			(0<<1)
#define PB_LONG				(1<<2)				// 1 = pb_timer has elapsed
#define PB_LONGPRESS		(PB_LONG|PB_PRESS)	// 1 = long press, 0 = short press
#define PB_ACK				(1<<3)				// acknowledge, 1 = key event has been processed

// push button events (do not change)
#define BTN1_PRESSED		(BUTTON1 | PB_PRESS)
#define BTN1_RELEASED		(BUTTON1 | PB_RELEASE)
#define BTN1_LONGPRESSED	(BUTTON1 | PB_LONGPRESS)
#define BTN2_PRESSED		(BUTTON2 | PB_PRESS)
#define BTN2_RELEASED		(BUTTON2 | PB_RELEASE)
#define BTN2_LONGPRESSED	(BUTTON2 | PB_LONGPRESS)
#define BOTH_PRESSED		(BOTH_BTNS | PB_PRESS)
#define BOTH_RELEASED		(BOTH_BTNS | PB_RELEASE)
#define BOTH_LONGPRESSED	(BOTH_BTNS | PB_LONGPRESS)


/**************
 * data types *
 **************/

#if SH_REG_IN_BITCOUNT <= 8
	typedef uint8_t sr_in_data_t;
#elif SH_REG_IN_BITCOUNT <= 16
	typedef uint16_t sr_in_data_t;
#elif SH_REG_IN_BITCOUNT <= 32
	typedef uint32_t sr_in_data_t;
#else
	error maximum supported input shift register width (32 bits) exceeded
#endif

#if SH_REG_OUT_BITCOUNT <= 8
	typedef uint8_t sr_out_data_t;
#elif SH_REG_OUT_BITCOUNT <= 16
	typedef uint16_t sr_out_data_t;
#elif SH_REG_OUT_BITCOUNT <= 32
	typedef uint32_t sr_out_data_t;
#else
	error maximum supported output shift register width (32 bits) exceeded
#endif


/********************
 * class definition *
 ********************/

class ProtoCounter
{
public:
	static void init();
	static void clearDisplay();
	static uint8_t getDisplay(uint8_t pos);
	static void setDisplay(uint8_t led_pattern, uint8_t pos);
	static void writeChar(uint8_t ascii_code, uint8_t pos);
	static void writeString_P(const char* st);
	static void writeInt(int16_t val);
	static void writeHex(uint8_t val);
	static void	setDimming(uint8_t dim);
	static void	setDecimalPlaces(uint8_t decimals);
	static void	setAnalogResolution(uint8_t ana_res);
	static uint8_t getButton();
	static void buttonAck();
	static sr_in_data_t readShiftRegister();
	static void writeShiftRegister(sr_out_data_t out_data);
	static uint8_t getAnalog();
	static void	update();
	static inline void updateAnalog();

private:
	static uint8_t dimming;
	static uint8_t decimal_places;
	static uint8_t analog_resolution;
	static uint8_t display[MAX_DIGITS];		// display[0] = rightmost digit
	static uint8_t button;					// button event
	static uint8_t pb_timer;				// push button timer
	static uint8_t pb_delay_timer;			// push button delay timer
	static volatile sr_in_data_t  sh_reg_in_data;	// data read from shift registers
	static volatile sr_out_data_t sh_reg_out_data;	// data to be written to shift registers
	static volatile uint8_t analog;			// stores the last analog value
	static volatile uint8_t start_time;		// start time of analog ramp-up
	static void updateShiftRegister();
	static void sampleButtons();
};


#endif /* PROTOCOUNTER_H_ */
