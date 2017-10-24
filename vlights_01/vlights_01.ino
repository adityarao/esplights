#include <ESP8266WiFi.h>
#include <TM1637Display.h>

#define RELAY D1
#define BLUE D2
#define RED D3

unsigned int numCounter = 0;

const int CLK = D5; //Set the CLK pin connection to the display
const int DIO = D4; //Set the DIO pin connection to the display

TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

void setup()
{
	pinMode(RELAY, OUTPUT);
	pinMode(BLUE, OUTPUT);
	pinMode(RED, OUTPUT);	
	display.setBrightness(0x0a);
}

void loop()
{
	digitalWrite(RELAY, LOW);
	digitalWrite(BLUE, LOW);
	digitalWrite(RED, HIGH);
	delay(500);
	digitalWrite(RELAY, HIGH);
	digitalWrite(BLUE, HIGH);
	digitalWrite(RED, LOW);
	delay(500);	

	display.showNumberDec(numCounter++%9999, true);

}