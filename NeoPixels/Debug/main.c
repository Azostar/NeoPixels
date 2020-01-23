/*
 * NeoPixels.c
 *
 * Created: 01/02/2018 23:35:29
 * Author : Aaron Dunford
 */

/*
	--TIMING--
	According to the datasheet for AdaFruits Neo Pixel, each bit we send should take a maximum of 1.3us to transfer.
	Each LED requires 24 bits transfered to change what is emitted, which gives us a total of 31.2us per LED.
	
	TODO: Include functionality to check whether or not we will need to use the reset function. 

*/

#define F_CPU	20E6		// 20MHz frequency

#include <avr/io.h>			// IO library for AVR
#include <avr/interrupt.h>	// Interrupt library for AVR
#include <util/delay.h>		// Delay library
#include <math.h>			// Math Library

#define POut			PORTD		// Output port for LED strips
#define POutState		DDRD		// Variable defining which pins will output on the port
#define PIn				PORTC		// Input port for LED controls
#define StripLength		8			// Number of LEDs on each strip
#define NumOfStrips		8			// Number of strips on our output port
#define Accuracy		256			// Accuracy of the ADMUX

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
	char displayNum;	// The amount of LEDs to light up.
};

typedef struct struct_Strip Strip;	// Define our structure as a new data type so we can pass each one as a pointer
int volatile ADCflag = 0;			// Set up a flag to be used by our ADC
int volatile vSize = 0;				// Create variable for holding voltage

/* 
	Waits for the period of time we need to reset the LED inputs 
*/
void reset()
{
	_delay_us(50);
}

/*	
	Sends a byte of data across a given pin, the timing for each output depends on the bit we want to send, this 
	information can be found in the data sheet For the pixels 
*/
void sendByte(char byte, char pin)
{
	char comp = 0x00;	// Initializes the comparison variable
	
	for(int j = 0; j < 8; j++)	// Loop through our bit array
	{
		comp = byte & bit[j];	// Compare our byte output to the current bit in the array, AKA bitmask
		
		if( comp == bit[j] )	// If our comparison and our bit are the same, it means our byte has a 1 in position j and we should output 1 to the led
		{
			POut = 1<<pin;
			_delay_us(0.7);
			POut = 0<<pin;
			_delay_us(0.6);
		}
		else					// Otherwise, we have a 0 in position j, and we should output 0 to the LED
		{
			POut = 1<<pin;
			_delay_us(0.35);
			POut = 0<<pin;
			_delay_us(0.8);
		}
	}
}

/*
	Takes the values we'd like for an LED colour and sends it across the strip.
*/
void sendCol(char r, char g, char b, char pin)
{
	sendByte(g, pin);
	sendByte(r, pin);
	sendByte(b, pin);
}

void displayUpdate(Strip strip)
{
	/*
		Get data from struct before we pass through.
		Originally I was just passing the pointer along, but it seems reading straight from a pointer pointing to an array entry
		caused timing issues, you'd think it were the other way around.
	*/
	
	for(char i = 0; i < strip.numLEDs; i++) // loop through the number of LEDs on the strip
	{
		if(i < strip.displayNum)			// If i is less than the number of LEDs to display, send out the display data
		{
			sendCol(0x00, 0xFF, 0x00, strip.pin);
		}
		else								// Otherwise we want to send a blank LED data
		{
			sendCol(0x00, 0x00, 0x00, strip.pin);
		}
	}
}

void setMUX(char pin){
	// Reset the MUX pins
	ADMUX &= ~(1 << MUX0) & ~(1 << MUX1) & ~(1 << MUX2);
	
	for(int j = 0; j < 3; j++)		// Loop through each bit of our pin and update the register with the information.
	{
		char check = pin & bit[j];	// create a check character to compare again our bit
		
		if (check == bit[j])		// If they match set the MUX accordingly
		{
			switch(j)
			{
				case 0:
					ADMUX = ADMUX | (1 << MUX0);
					break;
				
				case 1:
					ADMUX = ADMUX | (1 << MUX1);
					break;
				
				case 2:
					ADMUX = ADMUX | (1 << MUX2);
					break;
			}
		}
		
	}
	
}

int main(void)
{
	Strip stripArr[NumOfStrips];				// Create an array of strips depending on the number of strips we have
	
	for(int i = 0; i < NumOfStrips; i++)		// Initialize our array based on the number of strips
	{
		POutState = POutState | (1 << i);		// Set the port output state using bitshift.
		stripArr[i].pin = i;					// The pin of current strip will be equal to it's position in the strip array
		stripArr[i].numLEDs = StripLength;		// The number of LEDs will be equal to the length of strip defined in the header
		stripArr[i].displayNum = i + 1;			// Set the current number of displayed LED's to 0 (i + 1 for testing)
	}
	
	ADCSRA = ADCSRA | (1 << ADEN) | (1 << ADIE);	// Turn on ADC and enable interrupts
	ADMUX = ADMUX | (1 << ADLAR) | (1 << REFS0);					// Set result to be left adjusted, we don't need much accuracy
	sei();											// Enable global interrupts
	
	/*
		Test code
	*/

	
    while (1) 
    {
		for(int i = 0; i < NumOfStrips; i++)
		{
			
			// Set MUX register
			setMUX(stripArr[i].pin);
			
			// Start a conversion
			ADCSRA = ADCSRA | (1 << ADSC);
			
			// Wait until ADC has been read
			while(ADCflag == 0)	
			{
				asm("nop");
			}
			
			// Reset the ADC flag
			ADCflag = 0;
			
			// Get the data from the register and use to determine the amount of LEDs to display
			if (vSize >= 1)
			{
				stripArr[i].displayNum = vSize/(Accuracy/StripLength - 1); // This will give us a number to display between 0 and 8
			}
			else
			{
				stripArr[i].displayNum = 0;
			}
						
			// Update the display
			displayUpdate(stripArr[i]);
		}
    }
}

ISR(ADC_vect)
{
	ADCflag = 1;	// Set the ADC Flag
	vSize = ADCH;	// Read the data from ADCH
}