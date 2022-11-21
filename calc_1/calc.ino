//======================================================================
// $RCSfile: calc.ino $
// $Revision: $
// $Date:  $
// $Author:  $
//======================================================================
//
//  CALC: Programmer's Calculator
//
//  BOARD:  Arduino Nano (Abra Electronics)
//          16MHz external clock
//
//  SHIELD: Custom KB 7x4 + DFRobot I2C 16x2 LCD Display
//
//  WIRING:
//    DEFAULT LED:  D13
//    KEYPAD:       Rows 0...6 -- D2...D9
//                  Cols 0...3 -- D9...D12
//    LCD I2C:      A4 (SDA), A5 (SCK)
//
//======================================================================
// (c) 2022 Claude Benoit - All Right Reserved
//======================================================================

//*******************************
//  INCLUDES
//*******************************
#include <stdint.h>
#include <avr/pgmspace.h> 
#include "Keypad.h"
#include "DFRobot_RGBLCD1602.h"

//*******************************
//  DEFINES
//*******************************
#define	LED_RED			A3
#define	LED_BLUE		A2
#define	KB_ROWS			7
#define	KB_COLS			4 
#define	CALC_NB_STACK	4
#define	CALC_NB_STORE	4
#define	CALC_DIGITS		8

//*******************************
//  MACROS
//*******************************

//*******************************
//  TYPEDEFS
//*******************************

//*******************************
//  STATIC CONSTANTS
//*******************************

//*******************************
//  STATIC VARIABLES
//*******************************
static	bool		g_bSecond = false;
static	bool		g_bPushed = false;
static	bool		g_bSTO = false;
static	bool		g_bRCL = false;
static	bool		g_bError = false;
static	bool		g_bRefreshLCD = true;
static	uint32_t	g_uLastX = 0;

static	uint32_t	g_uCurrent;

static	uint32_t	g_uStack[CALC_NB_STACK];
static	uint32_t	g_uStore[CALC_NB_STORE];

static  DFRobot_RGBLCD1602    g_sLCD( 16, 2 );

const	byte	g_bPinsRow[KB_ROWS] = { 2, 3, 4, 5, 6, 7, 8 }; 
const	byte	g_bPinsCol[KB_COLS] = { 9, 10, 11, 12 }; 
const	byte	g_cKeyMap[KB_ROWS][KB_COLS] = {
                    { 0xEE, 0x10, 0x81, 0xA0},	// 0
                    { 0x11, 0x12, 0x13, 0xA1},	// 1
                    { 0x14, 0x15, 0x16, 0xA2},	// 2
                    { 0x17, 0x18, 0x19, 0xA3},	// 3
                    { 0x1A, 0x1B, 0x1C, 0xA4},	// 4
                    { 0x1D, 0x1E, 0x1F, 0xA5},	// 5
                    { 0x22, 0xCE, 0xB8, 0xA6}};	// 6
                    
static	Keypad		g_sKeypad= Keypad( makeKeymap(g_cKeyMap), g_bPinsRow, g_bPinsCol, KB_ROWS, KB_COLS);

//*******************************
//  STATIC FUNCTIONS
//*******************************

//******************************************************
//  $$L setup()
//
//  Initializes the MCU and peripherals.
//******************************************************
void setup()
{
    g_sLCD.init();
    g_sLCD.setRGB(255, 255, 255);

	Serial.begin(9600);  // initializing serail monitor

	pinMode(LED_BUILTIN, INPUT);
	digitalWrite(LED_BUILTIN, LOW);
  
	pinMode(LED_RED, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	digitalWrite(LED_RED, LOW);	
	digitalWrite(LED_BLUE, LOW);	
  
	calc_clearall();
	disp_clear();
	disp_update( NO_KEY );
	g_uCurrent = 0;
}

//******************************************************
//  $$L loop()
//
//  The main processing loop
//******************************************************
void loop()
{
	//----
	//---- Get keyboard input
	//----
	byte	l_bKeyPressed = (byte) g_sKeypad.getKey();
	if ( l_bKeyPressed == NO_KEY )
		return;

	process_key( l_bKeyPressed );
	disp_update( l_bKeyPressed );
	digitalWrite(LED_RED, g_bSecond ? HIGH : LOW );	// f key
	digitalWrite(LED_BLUE, (g_bSTO | g_bRCL) ? HIGH : LOW );	// f key + other

    dump();
}

//******************************************************
//  $$L process_key()
//
//  Process the input keys
//******************************************************
void process_key( byte in_bKey )
{ 

//	Serial.println(keypressed);

	//---- SECOND
	if ( in_bKey == 0x22 )
	{
		g_bSecond = !g_bSecond;
		if ( !g_bSecond )
		{
			g_bSTO = g_bRCL = false;
		}
		return;
	}

	//---- CLEAR / CLEARALL
	if ( in_bKey == 0xCE )
	{
		disp_clear();
		if ( g_bSecond )
			calc_clearall();
		g_bSecond = false;
		g_bPushed = false;
		g_bSTO = false;
		g_bRCL = false;
		g_bError = false;
		return;
	}

	if ( g_bSecond )
	{
		g_bSecond = false;
		
		//---- STO
		if ( in_bKey == 0x1D )
		{
			g_bSTO = true;
			return;
		}

		//---- RCL
		if ( in_bKey == 0x1E )
		{
			g_bRCL = true;
			return;
		}

		//---- LST X
		if ( in_bKey == 0x12 )
		{
			calc_push( g_uStack[0] );
			disp_setvalue( g_uStack[0] );
			return;
		}

		//---- SWP X Y
		if ( in_bKey == 0x13 )
		{
			uint32_t	l_uVal = g_uStack[1];
			g_uStack[1] = g_uStack[0];
			g_uStack[0] = l_uVal;
			disp_setvalue( g_uStack[0] );
			return;
		}

		//---- ROLL DOWN
		if ( in_bKey == 0x17 )
		{
			g_uStack[ CALC_NB_STACK - 1 ] = calc_pop();
			disp_setvalue( g_uStack[0] );
			return;
		}

		//---- ROLL UP
		if ( in_bKey == 0x17 )
		{
			calc_push( g_uStack[ CALC_NB_STACK - 1 ] );
			disp_setvalue( g_uStack[0] );
			return;
		}
	}

	//---- BITS / MODE
	if ( in_bKey == 0xB8 )
	{
		//---- no mode / bits yet
		if ( g_bSecond )
			return;
		return;
	}

	//----
	//---- STORAGE MODE
	//----
	if ( g_bSTO )
	{
		g_bSTO = false;
		g_bSecond = false;
		if ( (in_bKey >= 0x10) || (in_bKey <= (0x10 + CALC_NB_STORE ) ) )
		{
			g_uStore[ in_bKey - 0x10 ] = g_uCurrent; // disp_getvalue();
		}
		return;
	}
	if ( g_bRCL )
	{
		g_bRCL = false;
		g_bSecond = false;
		if ( (in_bKey >= 0x10) || (in_bKey <= (0x10 + CALC_NB_STORE ) ) )
		{
			calc_push( g_uStore[ in_bKey - 0x10 ] );
			disp_setvalue( g_uStack[0] );
		}
		return;
	}

	//----
	//---- 0 ... F
	//----
	if ( (in_bKey >= 0x10) && (in_bKey <= 0x1F) )
	{
		if ( g_bPushed )
		{
			g_uCurrent = (uint32_t) in_bKey - 0x00000010L;
			g_bPushed = false;
			return;
		}
		g_uCurrent = (g_uCurrent << 4) + (uint32_t) in_bKey - 0x00000010L;
		return;
	}

	//----
	//---- ENTER
	//----
	if ( in_bKey == 0xEE )
	{
		//---- Push value in the display
		calc_push( disp_getvalue() );
		g_bSecond = false;
		g_bPushed = true;
		return;
	}

	//---- OPERATIONS
	if ( (in_bKey >= 0xA0) && (in_bKey <= 0xA6) )
	{
		uint32_t	l_uRes, l_uVal_0, l_uVal_1;

		if ( g_bPushed )
		{
			//---- Need to pop
			l_uVal_0 = g_uStack[0];
			l_uVal_1 = calc_pop();
		}
		else
		{
			//---- No pop. Take from display
			l_uVal_0 = disp_getvalue();
			l_uVal_1 = g_uStack[0];
		}
		g_uLastX = g_uStack[0];

		if ( g_bSecond )
		{
			//---- PLUS
			if ( in_bKey == 0xA0 )
				l_uRes = l_uVal_1 + l_uVal_0;

			//---- MINUS
			else if ( in_bKey == 0xA1 )
				l_uRes = l_uVal_1 - l_uVal_0;

			//---- SHIFT LEFT
			else if ( in_bKey == 0xA2 )
				l_uRes = l_uVal_1 << l_uVal_0;

			//---- SHIFT RIGHT
			else if ( in_bKey == 0xA3 )
				l_uRes = l_uVal_1 >> l_uVal_0;

			//---- XOR
			else if ( in_bKey == 0xA4 )
				l_uRes = l_uVal_1 ^ l_uVal_0;

			//---- MODULO
			else if ( in_bKey == 0xA5 )
				l_uRes = l_uVal_1 % l_uVal_0;

			//---- CHS (remapped to NOT)
			else if ( in_bKey == 0xA6 )
				l_uRes = ~ l_uVal_0;
		}
		else
		{
			//---- PLUS
			if ( in_bKey == 0xA0 )
				l_uRes = l_uVal_1 + l_uVal_0;

			//---- MINUS
			else if ( in_bKey == 0xA1 )
				l_uRes = l_uVal_1 - l_uVal_0;

			//---- MULT
			else if ( in_bKey == 0xA2 )
				l_uRes = l_uVal_1 * l_uVal_0;

			//---- DIVIDE
			else if ( in_bKey == 0xA3 )
			{
				if ( l_uVal_0 == 0 )
				{
					g_bError = true;
					l_uRes = l_uVal_0;
				}
				else
					l_uRes = l_uVal_1 / l_uVal_0;
			}

			//---- OR
			else if ( in_bKey == 0xA4 )
				l_uRes = l_uVal_1 | l_uVal_0;

			//---- AND
			else if ( in_bKey == 0xA5 )
				l_uRes = l_uVal_1 & l_uVal_0;

			//---- NOT
			else if ( in_bKey == 0xA6 )
				l_uRes = ~ l_uVal_0;
		}
		
		g_uStack[0] = l_uRes;
		disp_setvalue( l_uRes );
		g_bPushed = true;
		g_bSecond = false;
		return;
	}

	//---- ERASE LAST
	if ( in_bKey == 0x81 )
	{
		g_uCurrent = (g_uCurrent >> 4) & 0x0FFFFFFFL;
		return;
	}
}

//==========================================================
//==== DISP
//==========================================================
static	char		g_cHex[] = "0123456789ABCDEF";

static void disp_clear( void )
{
	g_uCurrent = 0;
}

static void disp_setvalue( uint32_t in_uVal )
{
	g_uCurrent = in_uVal;
}

static uint32_t disp_getvalue( void )
{
	return( g_uCurrent );
}

static void disp_to_str( uint32_t in_uVal, uint8_t * in_pString )
{
	register int8_t i;
	for ( i=CALC_DIGITS-1; i>=0; i-- )
	{
		in_pString[i] = g_cHex[ (uint8_t) ( in_uVal & 0xF ) ];
		in_uVal = in_uVal >> 4;
	}
	in_pString[ CALC_DIGITS ] = '\0';
	for ( i=0; i<CALC_DIGITS-1; i++)
	{
		if ( in_pString[i] != '0' )
			break;
		in_pString[i] = ' ';
	}
	
}


static void disp_update( byte in_uKeypressed )
{
	char	cc[16];

	cc[ 0] = 'X';
	cc[ 1] = ':';
	cc[ 2] = ' ';
	cc[11] = '\0';
	cc[12] = '\0';
	cc[13] = '\0';
	cc[14] = '\0';
	cc[15] = '\0';
	disp_to_str( disp_getvalue(), &cc[3] );
	g_sLCD.setCursor( 5, 1 );
    g_sLCD.print( cc );

	cc[ 0] = 'Y';
	disp_to_str( g_uStack[1], &cc[3] );
	g_sLCD.setCursor( 5, 0 );
    g_sLCD.print( cc );
    

	g_sLCD.setCursor( 0, 0 );
    g_sLCD.print( g_bSecond ? 'f' : ' ' );

	g_sLCD.setCursor( 2, 1 );
	if ( g_bError )
		g_sLCD.print( "Err" );
	else
		g_sLCD.print( "   " );

	g_sLCD.setCursor( 0, 0 );

    g_bRefreshLCD = false;
    g_bError = false;
}

//==========================================================
//==== CALC
//==========================================================
static void calc_clearall( void )
{
    for( uint8_t i=0; i<CALC_NB_STACK; i++ )
		g_uStack[i] = 0;
    for( uint8_t i=0; i<CALC_NB_STORE; i++ )
		g_uStore[i] = 0;
}

static void calc_push( uint32_t in_uVal )
{
	for (uint8_t i = CALC_NB_STACK - 1; i > 0; i--)
		g_uStack[i] = g_uStack[i - 1];
	g_uStack[0] = in_uVal;
}

static uint32_t calc_pop( void )
{
	uint32_t	l_uTop = g_uStack[0];
	for (uint8_t i = 0; i < CALC_NB_STACK - 1; i++)
		g_uStack[i] = g_uStack[i + 1];
	return( l_uTop );
}


static void dump( void )
{
	Serial.println("===================================\n");
	for ( int i=0; i<CALC_NB_STACK; i++)
		Serial.println( g_uStack[i], HEX );

	Serial.println("\n\n\n");
	
}
