//======================================================================
// $RCSfile: main.c,v $
// $Revision: 1.8 $
// $Date: 2022/05/15 13:26:41 $
// $Author: claude $
//
//	BLINK:	Default blink test application @ 1s interval
//
//	BOARD:	Arduino Nano v3 (Abra Electronics)
//		16MHz external clock
//
//      WIRING:
//              LED:    PB5
//
//	Calculations for TIMER1
//		16MHz / 1024            = 15625         -- 1s
//		16MHz / 1024  / 20      = 781           -- 50ms
//		16MHz / 1024  / 100     = 156           -- 10ms
//		16MHz / 1024  / 1000    = 16            -- 1ms
//
//		TNCT1 = 65536 - 15625 = 49911		-- 1s timer
//
//======================================================================
// (c) 2022 Claude Benoit - All Right Reserved
//======================================================================
#include <avr/io.h>
#include <avr/interrupt.h>

//-----------------------------------------------------------
//---- Defines 
//-----------------------------------------------------------
#define	TM1_COUNT	49911		// See calculations above
#define	BIT_LED		0b00100000	// PB5 (Default LED)

//-----------------------------------------------------------
//---- ISR - TIMER 1 Interrupt Service Routine
//-----------------------------------------------------------
ISR( TIMER1_OVF_vect )
{
	TCNT1  = TM1_COUNT;	// Reset count for TIMER 1 (16 bits)
	PORTB ^= BIT_LED;	// Toggle LED pin
}

//===========================================================
//====
//==== Main -- Initialize TIMER1, LED(PB5), and loop
//====
//===========================================================
int main()
{
	DDRB	|= BIT_LED;	// Enable LED pin as OUTPUT
	TCNT1	= TM1_COUNT;	// Reset count for TIMER 1 (16 bits)
	TCCR1A	= 0b00000000;	// Timer/Counter 1 Register A - Reset
	TCCR1B	= 0b00000101;	// Timer/Control 1 Register B - Set 1024 prescaler
	TIMSK1	= 0b00000001;	// Timer/Counter 1 Interrupt Mask Resigter - Overflow Interrupt Overflow Enabled
	sei();			// Enable interrupts

	//---- Infinite loop, waiting for interrupts
	for(;;) {}
}
