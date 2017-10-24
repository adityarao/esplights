#include <ESP8266WiFi.h>
#include <TM1637Display.h>

// Display the LDR read value

#define RELAY D1
#define BLUE D2
#define RED D3

// define the intervals at which things should be checked.
unsigned long check_interval = 0;
unsigned long display_value = 0;

unsigned int ldr_value = 0;

const int CLK = D5; //Set the CLK pin connection to the display
const int DIO = D4; //Set the DIO pin connection to the display

TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

void setup()
{
	pinMode(RELAY, OUTPUT);
	pinMode(BLUE, OUTPUT);
	pinMode(RED, OUTPUT);	

	display.setBrightness(0x0a);
	display_value = check_interval = millis();
}

void loop()
{
	ldr_value = analogRead(A0);
	if(millis() - display_value > 500){
		display.showNumberDec(ldr_value, true);
		display_value = millis();
	}

	if(millis() - check_interval > 2000) { 
		if(ldr_value < 150) {
			digitalWrite(RELAY, HIGH);
		} else {
			digitalWrite(RELAY, LOW);
		}

		check_interval = millis();
	}
}