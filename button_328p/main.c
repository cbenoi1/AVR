//======================================================================
// $RCSfile: main.c,v $
// $Revision: 1.6 $
// $Date: 2022/06/16 01:50:24 $
// $Author: claude $
//======================================================================
//
//	BUTTON:	Debounced multiple buttons using a timer + LED 4 x 8
//
//	BOARD:	Arduino Uno v3 (Abra Electronics)
//		16MHz external clock
//
//	SHIELD: Arduino Multifunction Shield
//
//	WIRING:
//		DEFAULT LED:	PB5
//		LEDS: 			PB2...4
//		BUTTONS:		PC1...3
//		DIGITS:			CLK(PD7) + CS(PD4) + DO(PB0)
//
//      Calculations for TIMERs
//              16MHz / 1024            = 15625      -- 1s
//              16MHz / 1024  / 20      = 781        -- 50ms
//              16MHz / 1024  / 100     = 156        -- 10ms
//              16MHz / 1024  / 1000    = 16         -- 1ms
//
//              TNCT0 = 256   - 16    = 240          -- 1ms timer
//              TNCT1 = 65536 - 15625 = 49911        -- 1s timer
//
//======================================================================
// (c) 2022 Claude Benoit - All Right Reserved
//======================================================================
//#define F_CPU 16000000UL	// 16MHz -- passed by compiler -D
#include <avr/io.h>
#include <avr/interrupt.h>

//--------------------------------
//---- Defines
//--------------------------------
#define	TM0_COUNT		240		// 10ms - See calculations above
#define	TM1_COUNT		49911		// 1s   - See calculations above
#define	CNT_DEBOUNCE		50		// Debounce delay (# of 1ms)
#define	CNT_LED4X8		5		// LED4X8 update rate (# of 1ms)
#define	BIT_DEFAULT_LED		0b00100000	// DEFAULT LED -- Port B, Bit 5
#define	BIT_LED1		0b00000100	// LED1        -- Port B, Bit 2
#define	BIT_LED2		0b00001000	// LED2        -- Port B, Bit 3
#define	BIT_LED3		0b00010000	// LED3        -- Port B, Bit 4
#define	BIT_BUT1		0b00000010	// BUTTON1 -- Port C, Bit 1
#define	BIT_BUT2		0b00000100	// BUTTON2 -- Port C, Bit 2
#define	BIT_BUT3		0b00001000	// BUTTON3 -- Port C, Bit 3
#define	BIT_LED_ALL		(BIT_LED1 | BIT_LED2 | BIT_LED3)
#define	BIT_BUT_ALL		(BIT_BUT1 | BIT_BUT2 | BIT_BUT3)

//--------------------------------
//---- Macros
//--------------------------------
#define	LED4X8_PIN_CLK_LOW()	PORTD &= 0b01111111		// PD7
#define	LED4X8_PIN_CLK_HIGH()	PORTD |= 0b10000000
#define	LED4X8_PIN_CS_LOW()	PORTD &= 0b11101111		// PD4
#define	LED4X8_PIN_CS_HIGH()	PORTD |= 0b00010000
#define	LED4X8_PIN_DO_LOW()	PORTB &= 0b11111110		// PB0
#define	LED4X8_PIN_DO_HIGH()	PORTB |= 0b00000001
#define	LED4X8_SETVALUE(x,y)	g_uDigit[ (x) ] = (uint8_t) ( (y) < 0 ? 0xFF : g_uLED[ (y) ] );

//--------------------------------
//---- Global variables
//--------------------------------
static volatile uint8_t		g_uButtonsPulse;
static const	uint8_t		g_uLED[]   = { 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90 };
static const	uint8_t		g_uSEGS[]  = { 0xF1, 0xF2, 0xF4, 0xF8 };
static volatile	uint8_t		g_uDigit[4];
static volatile	uint8_t		g_uCurrDigit;
static volatile	uint8_t		g_uLed4x8Pulse;

//--------------------------------
//---- Static functions
//--------------------------------
static	void led4x8_shift_out( uint8_t );
static	void button_process( uint8_t in_uState );

//======================================================================
//==== ISR - TIMER 0 Interrupt Service Routine
//======================================================================
ISR( TIMER0_OVF_vect )
{
	TCNT0  = TM0_COUNT;			// Reset count
	if ( g_uButtonsPulse )	g_uButtonsPulse--;
	if ( g_uLed4x8Pulse )	g_uLed4x8Pulse--;
}

//======================================================================
//==== ISR - TIMER 1 Interrupt Service Routine
//======================================================================
ISR( TIMER1_OVF_vect )
{
	TCNT1  = TM1_COUNT;		// Reset count
	PORTB ^= BIT_DEFAULT_LED;	// Toggle DEFAULT LED
}

//======================================================================
//==== MAIN
//======================================================================
int main()
{
	//---- Initialize TIMER 0
	TCNT0	= TM0_COUNT;	// Reset count for TIMER 0 (8 bits)
	TCCR0A	= 0b00000000;	// Timer/Counter 1 Register A - Reset
	TCCR0B	= 0b00000101;	// Timer/Control 1 Register B - Set 1024 prescaler
	TIMSK0	= 0b00000001;	// Timer/Counter 1 Interrupt Mask Resigter - Overflow Interrupt Overflow Enabled

	//---- Initialize TIMER 1
	TCNT1	= TM1_COUNT;	// Reset count for TIMER 1 (16 bits)
	TCCR1A	= 0b00000000;	// Timer/Counter 1 Register A - Reset
	TCCR1B	= 0b00000101;	// Timer/Control 1 Register B - Set 1024 prescaler
	TIMSK1	= 0b00000001;	// Timer/Counter 1 Interrupt Mask Resigter - Overflow Interrupt Overflow Enabled

	//---- Setup DEFAULT LED
	DDRB	|= BIT_LED_ALL | BIT_DEFAULT_LED;	// Enable LEDs as OUTPUT
	PORTB	|= BIT_LED_ALL;						// Set LEDs to OFF

	//---- Setup LED4X8
	DDRB	|= 0b00000001;		// DO(PB0)
	DDRD	|= 0b10010000;		// CLK(PD7) + CS(PD4)
	LED4X8_PIN_CLK_LOW();
	LED4X8_PIN_CS_HIGH();
	g_uDigit[0]	= 0xFF;
	g_uDigit[1]	= 0xFF;
	g_uDigit[2]	= 0xFF;
	g_uDigit[3]	= 0xFF;
	g_uCurrDigit	= 0;
	g_uLed4x8Pulse	= 0;

	//---- Setup Digits.
	LED4X8_SETVALUE( 0, 0 );
	LED4X8_SETVALUE( 1, 1 );
	LED4X8_SETVALUE( 2, 2 );
	LED4X8_SETVALUE( 3, 3 );

	//---- Setup BUTTONs
	uint8_t l_uOldState = PINC & BIT_BUT_ALL;
	g_uButtonsPulse = 0;

	//--------------------------------------------------
	//---- LOOP - Infinite loop.
	//--------------------------------------------------
	sei();					// Enable interrupts
	for(;;)
	{
		//----
		//---- SOME OTHER IMPORTANT PROCESSING HERE
		//----


		//----
		//---- LED4X8 PROCESSING
		//----
		if ( g_uLed4x8Pulse == 0 )
		{
			LED4X8_PIN_CS_LOW();
			led4x8_shift_out( g_uDigit[ g_uCurrDigit ] );
			led4x8_shift_out( g_uSEGS[ g_uCurrDigit ] );
			LED4X8_PIN_CS_HIGH();
			g_uCurrDigit++;
			g_uCurrDigit = g_uCurrDigit % 4;
			g_uLed4x8Pulse = CNT_LED4X8;
		}

		//----
		//---- BUTTON PROCESSING
		//----
		if ( g_uButtonsPulse == 0 )
		{
			//---- Check button
			uint8_t l_uNewState = PINC & BIT_BUT_ALL;
			if ( l_uNewState != l_uOldState )
			{
				//---- Changed, so start a blackout period and process
				l_uOldState			= l_uNewState;
				g_uButtonsPulse		= CNT_DEBOUNCE;
				button_process( l_uOldState );
			}
		}
	}
}

//======================================================================
//==== Button process - Turn on/off LEDs to indicate status
//======================================================================
static void button_process( uint8_t in_uState )
{
	if ( in_uState & BIT_BUT1 )		// BUTTON 1
		PORTB |= BIT_LED1;
	else
		PORTB &= ~BIT_LED1;

	if ( in_uState & BIT_BUT2 )		// BUTTON 2
		PORTB |= BIT_LED2;
	else
		PORTB &= ~BIT_LED2;

	if ( in_uState & BIT_BUT3 )		// BUTTON 3
		PORTB |= BIT_LED3;
	else
		PORTB &= ~BIT_LED3;
}

//======================================================================
//====
//======================================================================
static void led4x8_shift_out( uint8_t in_uData )
{
	register uint8_t	i = 8;
	register uint8_t	msk = 0x80;
	do {
		if ( in_uData & msk )
			LED4X8_PIN_DO_HIGH();
		else
			LED4X8_PIN_DO_LOW();
		LED4X8_PIN_CLK_HIGH();
		LED4X8_PIN_CLK_LOW();
		msk >>= 1;
		i--;
	} while( i );
}
