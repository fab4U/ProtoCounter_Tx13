/*
 * ProtoCounter.cpp
 *
 */ 

/**********************************************************************************

Description:		Basic routines for operating the ProtoCounter
					- multiplexing the display
					- evaluation of the push buttons
					- i/o extension via shift registers
					- reading of analog knob
					
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

// check controller type
#if 	defined (__AVR_ATtiny2313__)  || \
		defined (__AVR_ATtiny2313A__) || \
		defined (__AVR_ATtiny4313__)
#define PROTOCOUNTER_TX13
#else
#error "incompatible microcontroller type (must be ATtiny2313 or 4313)"
#endif

// check CPU frequency
#if F_CPU != 8000000
#warning "cpu frequency deviates from assumed standard frequency (8 MHz)"
#endif


/************
 * includes *
 ************/
 
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "ProtoCounter.h"

#ifdef ARDUINO
	#include "Arduino.h"
#else
	#include <util/delay.h>
#endif


/**********
 * macros *
 **********/

#define swap(x)                                            \
 ({                                                        \
    unsigned char __x__ = (unsigned char) x;               \
    asm volatile ("swap %0" : "=r" (__x__) : "0" (__x__)); \
    __x__;                                                 \
  })


/**************************
 * static class variables *
 **************************/

// Member variable "display" contains the led patterns (0=on, 1=off)
// for each digit of the display.
// display[0] is the rightmost digit.

uint8_t ProtoCounter::display[MAX_DIGITS];
uint8_t ProtoCounter::button;
uint8_t ProtoCounter::dimming;
uint8_t ProtoCounter::analog_resolution;
uint8_t ProtoCounter::decimal_places;
uint8_t ProtoCounter::pb_timer;
uint8_t ProtoCounter::pb_delay_timer;

volatile sr_in_data_t  ProtoCounter::sh_reg_in_data;
volatile sr_out_data_t ProtoCounter::sh_reg_out_data;

volatile uint8_t ProtoCounter::analog;
volatile uint8_t ProtoCounter::start_time;


/***********
 * methods *
 ***********/

void ProtoCounter::init()
{
	clearDisplay();
	button = 0;
	dimming = DIMMING;
	decimal_places = DECIMAL_PLACES;

	ANODE_PORT |= (1<<ANODE1)|(1<<ANODE2)|(1<<ANODE3);	// all anodes off
	ANODE_DDR  |= (1<<ANODE1)|(1<<ANODE2)|(1<<ANODE3);	// make all anodes outputs

#ifdef SWAP_PINS_PD01_FOR_PB01
	PORTD |= 0b01111100;	// all segments off
	DDRD  |= 0b01111100;	// make all segments outputs
	PORTB |= 0b00000011;
	DDRB  |= 0b00000011;
#else
	PORTD = 0b01111111;		// all segments off
	DDRD  = 0b01111111;		// make all segments outputs
#endif

#if SH_REG_IN_BITCOUNT > 0
	sh_reg_in_data = 0;
#endif

#if SH_REG_OUT_BITCOUNT > 0
	sh_reg_out_data = 0;
#endif

#if (SH_REG_IN_BITCOUNT > 0) || (SH_REG_OUT_BITCOUNT > 0)
	SH_REG_LD_PORT &= ~(1<<SH_REG_LD_BIT);		// low
	SH_REG_LD_DDR  |=  (1<<SH_REG_LD_BIT);		// output
#endif

#ifdef ANALOG_ENABLE
	AIN1_PORT &= ~(1<<AIN1_BIT);		// output LOW on AIN1 to discharge capacitor
	AIN1_DDR  |=  (1<<AIN1_BIT);
	ACSR = (1<<ACBG) | (1<<ACI)| (2<<ACIS0);	// use bandgap reference, int on falling edge
	DIDR = (1<<AIN1D);					// disable digital input on AIN1
	analog_resolution = ANALOG_MAX_RESOLUTION;
	analog = 0;
#endif

#ifdef ARDUINO
	// use timer0 compare B interrupt for ProtoCounter
	OCR0B = 125;						// an arbitrary value
	TIMSK |= (1 << OCIE0B);				// enable OC0B interrupt
#endif
}


void ProtoCounter::clearDisplay()
{
	for(uint8_t i = 0; i < MAX_DIGITS; i++) {
		display[i] = 0xFF;
	}
}


uint8_t ProtoCounter::getDisplay(uint8_t pos)
// return content of display position
// as a bit pattern (0 = led off, 1 = led on)
{
	return (~display[pos]);
}


void ProtoCounter::setDisplay(uint8_t led_pattern, uint8_t pos)
// write led bit pattern (0=off, 1=on) into display position (0 = rightmost digit)
{
	if (pos >= MAX_DIGITS) { return; }
	display[pos] = ~led_pattern;
}


void ProtoCounter::writeChar(uint8_t ascii_code, uint8_t pos)
// write an ASCII character at given display position (0 = rightmost digit)
{
	// character generator (decimal to 7-segment)
	static const uint8_t char_gen[] PROGMEM = {	0x00, 0x30, 0x22, 0x14, 0x2D, 0x1B, 0x70, 0x20,
												0x39, 0x0F, 0x63, 0x46, 0x10, 0x40, 0x80, 0x52,
												0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
												0x7F, 0x6F, 0x09, 0x36, 0x58, 0x48, 0x4C, 0x53,
												0x5F, 0x77, 0x7C, 0x58, 0x5E, 0x79, 0x71, 0x3D,
												0x74, 0x10, 0x1E, 0x76, 0x38, 0x37, 0x54, 0x5C,
												0x73, 0x67, 0x50, 0x6D, 0x78, 0x1C, 0x3E, 0x2A,
												0x49, 0x6E, 0x5B, 0x31, 0x64, 0x0E, 0x23, 0x08};

	// ASCII codes 0 to 15 are mapped to '0'..'9', 'A'..'F'.
	// Small letters are mapped to capital letters.
	// Character codes 16 to 31 and codes above 127 are mapped to <space>.
	if (ascii_code <=  9)		{ ascii_code += 16; }
	else if (ascii_code <= 15)	{ ascii_code += 23; }
	else if (ascii_code <= 31)	{ ascii_code  =  0; }
	else if (ascii_code <= 95)	{ ascii_code -= 32; }
	else if (ascii_code <= 127)	{ ascii_code -= 64; }
	else						{ ascii_code  =  0; }
	setDisplay(pgm_read_byte( &char_gen[ascii_code] ), pos);
}


void ProtoCounter::writeString_P(const char* st)
// write a string that is stored in flash memory to the display
{
	char	ch;
	uint8_t pos;

	pos = MAX_DIGITS;
	ch = pgm_read_byte(st);
	while (ch && pos)
	{
		pos--;
		writeChar(ch, pos);
		st++;
		ch = pgm_read_byte(st);
	}
}


void ProtoCounter::writeInt(int16_t val)
// Convert an integer from -99..999 to a decimal number and display it.
{
    uint8_t i, ii, d;
	uint8_t digit[3] = {0, 0, 0};

	// range check
	if (val > MAX_DECIMAL) {
		writeString_P(PSTR("0FL"));		// write "OFL" = overflow
		return;
	}
	if (val < MIN_DECIMAL) {
		writeString_P(PSTR("VFL"));		// write "UFL" = underflow
		return;
	}

	// write sign
	ii = MAX_DIGITS;					// number of digits to display
	if (val < 0) {
		val = -val;
		writeChar('-', MAX_DIGITS-1);	// write minus sign to leftmost digit
		ii--;							// decrement number of digits
	}
	
	// convert value to digits
	digit[0] = (val >> 8) & 11;			// load bit9..8
	for (i=0; i<8; i++) {				// convert binary value to decimal digits
		digit[0] <<= 1;					// double each digit
		digit[1] <<= 1;
		digit[2] <<= 1;
		if (val & 0x80) {				// bit7 = 1 ?
			digit[0]++;
		}
		if (digit[0] > 9) {				// correct digit 0
			digit[0] -= 10;
			digit[1]++;
		}
		if (digit[1] > 9) {				// correct digit 1
			digit[1] -= 10;
			digit[2]++;
		}
		val <<= 1;						// next bit of binary value
	}

	// suppress leading zeroes and display result
	d = ' ';
	do {
		ii--;
		if ((d != ' ') || (ii == decimal_places) || (digit[ii] > 0)) {
			d = digit[ii];
		}
		writeChar(d, ii);
	} while (ii > 0);
}


void ProtoCounter::writeHex(uint8_t val)
{
	uint8_t nibble;

	nibble = swap(val);
	nibble &= 0x0F;					// upper nibble
	if (nibble > 9)	{ nibble += ('A' - 10); }
	else			{ nibble += ('0'); }
	writeChar(nibble, 2);

	nibble = val;
	nibble &= 0x0F;					// lower nibble
	if (nibble > 9)	{ nibble += ('A' - 10); }
	else			{ nibble += ('0'); }
	writeChar(nibble, 1);

	writeChar('h', 0);				// write 'h' to indicate a hex number
}


void ProtoCounter::setDimming(uint8_t dim)
// set dimming level (0 = no dimming)
{
	dimming = dim;
}


void ProtoCounter::setDecimalPlaces(uint8_t decimals)
{
	if (decimals < MAX_DIGITS) {
		decimal_places = decimals;
	}
}


void ProtoCounter::setAnalogResolution(uint8_t ana_res)
{
	if (ana_res <= ANALOG_OFF) {
		analog_resolution = ana_res;
	}
}


uint8_t ProtoCounter::getButton()
// get button event
{
	return(button);
}


void ProtoCounter::buttonAck()
// acknowledge button event
{
	button |= PB_ACK;					// set acknowledge flag
}


sr_in_data_t ProtoCounter::readShiftRegister()
{
	sr_in_data_t temp;

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		temp = sh_reg_in_data;
	}
	return(temp);
}


void ProtoCounter::writeShiftRegister(sr_out_data_t out_data)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		sh_reg_out_data = out_data;
	}
}


void ProtoCounter::updateShiftRegister()
// shift out data to external shift registers (MSB first)
// shift in data from external shift registers (MSB first)
// precondition: IN, OUT and CLK pins must be high
{
#if (SH_REG_IN_BITCOUNT > 0) || (SH_REG_OUT_BITCOUNT > 0)

	uint8_t 		bit;

#if SH_REG_OUT_BITCOUNT > SH_REG_IN_BITCOUNT
	sr_out_data_t	sr_data;
#else
	sr_in_data_t	sr_data;
#endif

#if SH_REG_OUT_BITCOUNT > 0							// ----- shift data out -----
	sr_data = sh_reg_out_data;
	for (bit = 0; bit < SH_REG_OUT_BITCOUNT; bit++) {
		if (sr_data & SH_REG_MSB_MASK) {			// set OUT = MSB
			SH_REG_PORT |=  (1<<SH_REG_OUT_BIT);
		} else {
			SH_REG_PORT &= ~(1<<SH_REG_OUT_BIT);
		}
		sr_data <<= 1;
		SH_REG_PORT &= ~(1<<SH_REG_CLK_BIT);		// clock pulse
		SH_REG_PORT |=  (1<<SH_REG_CLK_BIT);
	}
	// turn anode off to avoid ghost images
	SH_REG_PORT |=  (1<<SH_REG_OUT_BIT);			// set OUT = high
#endif

	// load outputs and sample inputs
	SH_REG_LD_PORT &= ~(1<<SH_REG_LD_BIT);
	SH_REG_LD_PORT |=  (1<<SH_REG_LD_BIT);

#if SH_REG_IN_BITCOUNT > 0							// ----- shift data in -----
	sr_data = 0;
	SH_REG_DDR &= ~(1<<SH_REG_IN_BIT);				// make IN an input
	for (bit = 0; bit < SH_REG_IN_BITCOUNT; bit++) {
		sr_data <<= 1;
		if (SH_REG_PIN & (1<<SH_REG_IN_BIT)) {		// set LSB = IN
			sr_data |= 1;
		}
		SH_REG_PORT &= ~(1<<SH_REG_CLK_BIT);		// clock pulse
		SH_REG_PORT |=  (1<<SH_REG_CLK_BIT);
	}
	sh_reg_in_data = sr_data;
	SH_REG_DDR |= (1<<SH_REG_IN_BIT);				// switch IN back to output
#endif

#endif
}


void ProtoCounter::sampleButtons()
// precondition: all button pins must be high
{
	uint8_t pb;

	// sample push buttons
	BTN_DDR &= ~(1 << BTN1_BIT);				// switch button pins to input
	BTN_DDR &= ~(1 << BTN2_BIT);

	BTN_PORT &= ~(1<<BTN_COM);					// set common line = low

#ifdef ARDUINO
	delayMicroseconds(1);
#else
	_delay_us(1);
#endif

	pb = ~BTN_PIN;								// read buttons
	pb &= BTN_MASK;
	BTN_PORT |= (1<<BTN_COM);					// set common line = high
	BTN_DDR |= (1 << BTN1_BIT);					// switch back to outputs
	BTN_DDR |= (1 << BTN2_BIT);

	if (pb == 0) {								// --- no button pressed ---
		if (button & PB_PRESS) {				// former state = pressed ?
			if ((button & (PB_ACK|PB_LONG)) == (PB_ACK|PB_LONG)) {	// in case of an acknowledged longpress event
				button &= ~(PB_ACK | PB_PRESS);		// -> issue long release event (clear ack and press)
			} else {
				button &= ~(PB_ACK | PB_LONGPRESS);	// -> issue release event (clear ack, long and press)
			}
		}
	}
	else {										// --- button pressed ---
		if ((button & PB_PRESS) == 0) {			// former state = all buttons released ?
			button = pb | PB_PRESS;				// -> issue press event
			// start push button timer
			pb_delay_timer = BTN_LONGPRESS_DELAY;
		} else {
			if ((button & (BTN_MASK|PB_LONG)) == pb) {	// holding same button pressed?
				if (pb_delay_timer == 0) {			// if push button timer has elapsed
					button = pb | PB_LONGPRESS;	// issue longpress event
				}
			} else {
				if ((pb == BOTH_BTNS) && ((button & PB_LONG) == 0)) {
					button = BOTH_PRESSED;		// -> issue "both pressed" event
				}
			}
		}
	}
	if (pb_delay_timer) { pb_delay_timer--; }
}


void ProtoCounter::update()
// should be called periodically, e. g. every 1 ms
{
	static uint8_t current_pos = 0;
	static const uint8_t col_bit[MAX_DIGITS] PROGMEM =
								{(1<<ANODE3), (1<<ANODE2), (1<<ANODE1)};
	uint8_t	anode;

	ANODE_PORT |= (1<<ANODE1)|(1<<ANODE2)|(1<<ANODE3);	// all anodes off

#ifdef SWAP_PINS_PD01_FOR_PB01
	PORTD |= 0b01111100;	// all led segments off
	PORTB |= 0b00000011;
#else
	PORTD = 0b01111111;		// all led segments off
#endif

#ifdef ANALOG_ENABLE
	if (current_pos == 0) {
		ACSR = (1<<ACBG) | (1<<ACI)| (1<<ACIE)| (2<<ACIS0);	// enable analog comparator interrupt
		AIN1_DDR &= ~(1<<AIN1_BIT);			// make AIN1 input (without pull-up)
		start_time = TCNT0;					// remember start time
	}
	else {
		AIN1_DDR |= (1<<AIN1_BIT);			// output LOW on AIN1 to discharge capacitor
	}
#endif

#if (SH_REG_IN_BITCOUNT > 0) || (SH_REG_OUT_BITCOUNT > 0)
	updateShiftRegister();
#endif

	pb_timer--;
	if (pb_timer == 0) {
		pb_timer = BTN_SAMPLE_INTERVAL;
		sampleButtons();
	}

	if (current_pos == 0) {
		current_pos = (MAX_DIGITS+1 + dimming);	// add an extra cycle for analog reading
	}

	current_pos--;								// next position
	if (current_pos < MAX_DIGITS) {
		anode = pgm_read_byte( &col_bit[current_pos] );
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			ANODE_PORT &= ~anode;				// turn new anode on
		}
		
#ifdef SWAP_PINS_PD01_FOR_PB01
		PORTD |= 0b11111100;					// set led segments
		PORTD &= (display[current_pos] | 0b00000011);
		PORTB |= 0b00000011;
		PORTB &= (display[current_pos] | 0b11111100);
#else
		PORTD = display[current_pos];			// set led segments
#endif
	}
}


uint8_t ProtoCounter::getAnalog()
{
	return (analog);
}


inline void ProtoCounter::updateAnalog()
{
#ifdef ANALOG_ENABLE

	uint16_t ana;
	uint8_t rc;
	uint8_t resolution;

	rc = TCNT0 - start_time - 1;
	resolution = analog_resolution;
	if (resolution >= ANALOG_OFF) {
		analog = 0;
	}
	else if (resolution == ANALOG_MAX_RESOLUTION) {
		analog = rc;
	}
	else {
		ana = rc;
		if (resolution == ANALOG_129_DETENT_STEPS) {
			ana += (rc >> 1);
		}
		else if (resolution == ANALOG_65_DETENT_STEPS) {
			ana -= (rc >> 2);
		}
		else if (resolution == ANALOG_43_DETENT_STEPS) {
			ana -= (rc >> 1);
		}
		else if (resolution == ANALOG_33_DETENT_STEPS) {
			ana += rc;
			ana += rc;
			ana >>= 3;
		}
		else if (resolution == ANALOG_22_DETENT_STEPS) {
			ana >>= 2;
		}
		ana += analog + 2;
		analog = (uint8_t)(ana >> 2);
	}
	ACSR = (1<<ACBG) | (1<<ACI)| (0<<ACIE)| (2<<ACIS0);	// disable interrupt
	
#endif
}


/**********************
 * interrupt routines *
 **********************/

#ifdef ANALOG_ENABLE
ISR(ANA_COMP_vect)
// analog comparator interrupt
{
	ProtoCounter::updateAnalog();
}
#endif
