/*
 * NeoPixels.c
 *
 * Created: 01/02/2018 23:35:29
 * Author : Aaron Dunford
 */

#define F_CPU	20E6		// 20MHz frequency

#include <avr/io.h>			// IO library for AVR
#include <avr/interrupt.h>	// Interrupt library for AVR
#include <util/delay.h>		// Delay library

#define LEDPort			PORTD		// Output port for LED strips
#define LEDState		DDRD		// Variable defining which pins will output on the port D
#define ControlPort		PORTB		// Output port for control outputs
#define ControlState    DDRB		// Variable defining which pins will output on port A
#define StripLength		8			// Number of LEDs on each strip
#define NumOfStrips		8			// Number of strips on our output port
#define Accuracy		255			// Accuracy of the ADMUX
#define Brightness		255			// Brightness of the LED
#define Compare			2169		// Compare value for the timer


/* 
	Bit array, used for data transfer so we can read each bit of our hex codes using these values as a bit mask 
	before we send the data 
*/
char bit[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/* 
	Struct of various variables used for our LED strip
*/
struct struct_Strip {
	char pin;			// The pin the strip is placed on.
	char numLEDs;		// Number of LEDs on the strip
	int displayNum;		// The amount of LEDs to light up.
};
typedef struct struct_Strip Strip;						// Define our structure as a new data type so we can pass each one as a variable

int volatile ADCflag = 0;								// Set up a flag to be used by our ADC
char volatile vSize = 0;								// Create variable for holding voltage
char unsigned volatile rCol[3] = { 0, 0, 0 };			// Create an array to store rainbow colours for our second mode
	
enum mode												// Create an enum for our modes
{
	DEFAULT,					// Default mode uses standard equaliser display techniques
	RAINBOW,					// Rainbow mode uses non-standard equaliser display techniques
	SHINE						// Fading brightness depending on the height of the display
} volatile cMode;				// Set our current mode to default


/*	
	Sends a byte of data across a given pin, the timing for each output depends on the bit we want to send, this 
	information can be found in the data sheet For the pixels 

	According to the datasheet for AdaFruits Neo Pixel, each bit we send should take a maximum of 1.3us to transfer.
	Each LED requires 24 bits transfered to change what colour is emitted, which gives us a total of 31.2us per LED.
*/
void sendByte(char byte, char pin)
{
	char comp = 0x00;		// Initializes the comparison variable
	
	// Loop through our bit array
	for(int j = 7; j > -1; j--)
	{
		comp = byte & bit[j];		// AND our byte output to the current bit in the array, AKA bitmask
		
		/*
			If our comparison and our bit are the same, it means our byte has a 1 in position j and we should output 1 to the led
			Otherwise, we have a 0 in position j, and we should output 0 to the LED
		*/
		if( comp == bit[j] )	
		{
			LEDPort = ( 1 << pin );
			_delay_us(0.7);
			LEDPort = 0;
			_delay_us(0.6);
		}
		else
		{
			LEDPort = ( 1 << pin );
			_delay_us(0.35);
			LEDPort = 0;
			_delay_us(0.8);
		}
		
	}
}


/*
	Takes the values we'd like for an LED colour and sends it across the strip.
*/
void sendCol(char r, char g, char b, char pin)
{
	sendByte( r, pin );
	sendByte( g, pin );
	sendByte( b, pin );
}

/*
	Updates what our strip will display.
*/
void displayUpdate(Strip strip)
{
	/*
		Get data from struct before we pass through.
		Originally I was just passing the pointer along, but it seems reading straight from a pointer pointing to an array entry
		caused timing issues, you'd think it were the other way around.
	*/
	char sFactor = 0;			// sFactor for the Shine mode
	char j = 0;					// Counter for various modes
	sFactor = Brightness / 9;	// Create a fraction of brightness used for the shine mode
	
	cli();					// Disable global interrupts while we transfer data
	switch (cMode)
	{
		case DEFAULT:
			// loop through the number of LEDs on the strip
			for(char i = 0; i < strip.numLEDs; i++)
			{
				// If i is less than the number of LEDs to display send out the display data, otherwise we want to send a blank LED data
				if( i < strip.displayNum )
				{
					if			( i < strip.numLEDs -3){ sendCol( 0, Brightness, 0, strip.pin ); }
					else if		( i < strip.numLEDs -1){ sendCol( Brightness, Brightness, 0, strip.pin ); }
					else		{ sendCol( Brightness, 0, 0, strip.pin ); }
				}
				else
				{
					sendCol( 0, 0, 0, strip.pin );
				}
						
			}
		break;
				
		case SHINE:
			// loop through the number of LEDs on the strip
			for(char i = 0; i < strip.numLEDs; i++)
			{
				// If i is less than the number of LEDs to display send out the display data, otherwise we want to send a blank LED data
				if( i < strip.displayNum )
				{
					if (i < strip.displayNum - 3)	{ sendCol( 0, 0, 0, strip.pin ); }
					else if (strip.displayNum > 2)  { j++; sendCol( sFactor * (j * j), sFactor * (j * j), sFactor * (j * j), strip.pin ); }
					else							{ j = 4; sendCol( sFactor * j * (i + 1), sFactor * j * (i + 1), sFactor * j * (i + 1), strip.pin ); }
				}
				else
				{
					sendCol( 0, 0, 0, strip.pin );
				}
						
			}
		break;
				
		case RAINBOW:
			// loop through the number of LEDs on the strip
			for(char i = 0; i < strip.numLEDs; i++)
			{
				// If i is less than the number of LEDs to display send out the display data, otherwise we want to send a blank LED data
				if( i < strip.displayNum )
				{
					sendCol( rCol[0], rCol[1], rCol[2], strip.pin );
				}
				else
				{
					sendCol( 0, 0, 0, strip.pin );
				}
						
			}
		break;
	}
	sei();
}

/*
	DEPRECIATED
	No longer needed as we now use one pin for each read, still a very useful function to keep around none the less.
*/
void setMUX(char pin){
	ADMUX &= ~( 1 << MUX0 ) & ~( 1 << MUX1 ) & ~( 1 << MUX2 );		// Reset the MUX pins
	
	// Loop through each bit of our pin and update the register with the information.
	for(int j = 0; j < 3; j++)		
	{
		char check = pin & bit[j];		// create a check character to compare again our bit
		
		// If they match set the MUX accordingly
		if ( check == bit[j] )		
		{
			switch(j)
			{
				case 0:
					ADMUX |= ( 1 << MUX0 );
					break;
				
				case 1:
					ADMUX |= ( 1 << MUX1 );
					break;
				
				case 2:
					ADMUX |= ( 1 << MUX2 );
					break;
			}
		}
		
		
	}
}


/*
	A function that sets the display mode of the LEDs
*/
void setMode(char MODE)
{
	cMode = MODE;		// Set the current mode to the selected mode
	
	switch(cMode)
	{
		case RAINBOW:
			TIMSK1 |= ( 1 << OCIE1A );		// Enable CTC timer interrupts
			rCol[1] = Brightness;			// Set initial color
		break;
			
		default:
			TIMSK1 &= ~( 1 << OCIE1A );		// Disable CTC timer interrupts
		break;
	}
	
}


/*
	NEEDS TESTING
	This function strobes the equaliser in order to switch the bandpass range that is being read.
*/
void strobeEqualiser(){
		ControlPort |= (1 << PB0);		// Send strobe high
		_delay_us(18);					// Wait 18us
		ControlPort &= ~(1 << PB0);		// Send strobe low
		ADCSRA |= ( 1 << ADSC );		// Start a conversion
		_delay_us(54);
};


int main(void)
{
	Strip stripArr[ NumOfStrips ];				// Create an array of strips depending on the number of strips we have
	int numLED = 0;								// Create an int to store the number of LEDs to display
	
	for(int i = 0; i < NumOfStrips; i++)		// Initialize our array based on the number of strips
	{
		LEDState |= ( 1 << i );				// Set the port output state.
		stripArr[i].pin = i;					// The pin of current strip will be equal to it's position in the strip array
		stripArr[i].numLEDs = StripLength;		// The number of LEDs will be equal to the length of strip defined in the header
		stripArr[i].displayNum = 0;				// Set the current number of displayed LED's to 0 (i + 1 for testing)
	}
	
	ControlState |= (1 << PA0);		// Set pin 0 on port A as an output
	
	ADCSRA |= ( 1 << ADEN ) | ( 1 << ADIE ) | ( 1 << ADPS1 ) | ( 1 << ADPS2 );						// Turn on ADC & set prescaler x64 which gives 41.6us per conversion
	ADMUX |= ( 1 << ADLAR ) | ( 1 << REFS0 );														// Left adjust result and set reference voltage
	
	OCR1A = Compare;																				// Set our timer comparison int
	TCCR1B |= ( 1 << WGM12 ) | ( 1 << CS12 );														// Enable CTC timer with 64x prescale
	
	sei();																							// Enable global interrupts
	
	setMode( RAINBOW );		// Set the display mode

	/*
		Before we do anything we're going to do our first conversion, this way we don't have to wait in a loop until the ADC finished before
		continuing on in the future. We can set the ADC running and come back to it when we need it.
	*/
	strobeEqualiser();
	
    while (1) 
    {
		// Cycle through each strip on the output pins
		for(int i = 0; i < NumOfStrips; i++)
		{
			// Wait until ADC has been read, if it's still working
			while( ADCflag == 0 )
			{
				// do nothing
			}
			
			ADCflag = 0;		// Reset the ADC flag
			
			numLED = vSize / ( Accuracy / StripLength );	// Set the number of LEDs to display based on the previous conversion
			strobeEqualiser();									// Start an ADC conversion for the next strip
			
			// Check if the number of LEDs are already being displayed, if they are, save cycles. Or if the current mode is rainbow, carry on.
			if( ( numLED != stripArr[i].displayNum ) | ( cMode == RAINBOW ) )
			{
				stripArr[i].displayNum = numLED;		// Set the current strips display number to the calculated display number
				displayUpdate( stripArr[i] );			// Update the strips display
			}
			
		}
    }
}


/*
	ADC interrupt
*/
ISR(ADC_vect)
{
	ADCflag = 1;	// Set the ADC Flag
	vSize = ADCH;	// Set vSize to the ADC result
}

/*
	Timer1 interrupt, used to control the rainbow pixel display
*/
ISR(TIMER1_COMPA_vect)
{
	/*
		This cycles the colour values up to the brightness defined in the header, once a colour reaches it's maximum brightness, it
		decreases the previous colours value before increasing the next colours value.
	*/
	if( rCol[0] >= Brightness )
	{ 
		if( rCol[2] > 0 ){ rCol[2]--; }
		else{ rCol[1]++; } 
	}
	
	if( rCol[1] >= Brightness )
	{ 
		if( rCol[0] > 0 ){ rCol[0]--; }
		else{ rCol[2]++; } 
	}
	
	if( rCol[2] >= Brightness )
	{ 
		if( rCol[1] > 0 ){ rCol[1]--; }
		else{ rCol[0]++; } 
	}
	
}