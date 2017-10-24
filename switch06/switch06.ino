/*
	Copyright 2017 Aditya Rao (c) 

	Permission is hereby granted, free of charge, to any person obtaining a 
	copy of this software and associated documentation files (the "Software"), 
	to deal in the Software without restriction, including without limitation 
	the rights to use, copy, modify, merge, publish, distribute, sublicense, 
	and/or sell copies of the Software, and to permit persons to whom the Software 
	is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included 
	in all copies or substantial portions of the Software.
*/

// basic includes
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
// controls the 7 segment display
#include <TM1637Display.h>

// Wifi parameters / auth
#define ssid  ""
#define password "$!23"

// set some parameters to define time delays
#define MAX_CONNECTION_ATTEMPTS 30

// define the pins for RELAY & LED
#define RELAY D1
#define BLUE D2
#define RED D3

// Index page - hard-coded, ideally should be encased in PROGMEM (https://www.arduino.cc/en/Reference/PROGMEM)
const String INDEX_PAGE  = "<!DOCTYPE html>\r\n<html>\r\n<head> \r\n<title>On/Off Switch </title>\r\n</head>\r\n<style> \r\n.label_chk {\r\n  padding: 15px 25px;\r\n  font-size: 24px;\r\n  font-size: 4.0vw;\r\n  margin-bottom:25px;\r\n  text-align: center;\r\n}\r\n.button_on {\r\n  display: block;\r\n  height:100px;\r\n  width:80%;\r\n  padding: 15px 25px;\r\n  font-size: 24px;\r\n  font-size: 4.0vw;\r\n  margin-left:auto;\r\n  margin-right:auto;\r\n  margin-bottom:25px;\r\n  margin-top:auto;  \r\n  cursor: pointer;\r\n  text-align: center;\r\n  text-decoration: none;\r\n  outline: none;\r\n  color: #fff;\r\n  background-color: GREEN;\r\n  border: none;\r\n  border-radius: 15px;\r\n  box-shadow: 0 9px #999;\r\n}\r\n\r\n.button_off {\r\n  display: block;\r\n  height:100px;\r\n  width:80%;  \r\n  padding: 15px 25px;\r\n  font-size: 24px;\r\n  font-size: 4.0vw;\r\n  margin-left:auto;\r\n  margin-right:auto;  \r\n  margin-bottom:auto;\r\n  margin-top:auto;  \r\n  cursor: pointer;\r\n  text-align: center;\r\n  text-decoration: none;\r\n  outline: none;\r\n  color: #fff;\r\n  background-color: RED;\r\n  border: none;\r\n  border-radius: 15px;\r\n  box-shadow: 0 9px #999;\r\n}\r\n.button_on:hover {background-color: #3e8e41}\r\n.button_off:hover {background-color: #ff3341}\r\n\r\n.button_off:active {\r\n  background-color: #ff3341;\r\n  box-shadow: 0 5px #666;\r\n  transform: translateY(4px);\r\n\r\n}\r\n\r\n.button_on:active {\r\n  background-color: #3e8e41;\r\n  box-shadow: 0 5px #666;\r\n  transform: translateY(4px);\r\n}\r\n\r\n.ckhbox {\r\n  height: 30px;\r\n  width: 30px;\r\n}\r\n\r\n\r\n</style>\r\n<script type=\"text/javascript\">\r\n  function updateTextInput(val) {\r\n          document.getElementById('textInput').value=val; \r\n        }       \r\n</script>\r\n<body>\r\n\r\n<form action=\"/\" method=\"post\">\r\n<table>\r\n  <button name=\"submit\" align=\"center\" class=\"button_on\" type=\"submit\" value=\"ON\"> ON </button>\r\n  <button name=\"submit\" align=\"center\" class=\"button_off\" type=\"submit\" value=\"OFF\"> OFF </button> \r\n </table>\r\n <br/>\r\n\r\n</form> \r\n</body>\r\n</html>";

const int CLK = D5; //Set the CLK pin connection to the display
const int DIO = D4; //Set the DIO pin connection to the display

// 4 segment display 
TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

time_t timeVal; 

unsigned long int t_SENSE_TEMP = 0;

const uint8_t SEG_CONN[] = {
	SEG_A | SEG_D | SEG_E | SEG_F,                   // C
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_C | SEG_E | SEG_G,           				 // n
};

const uint8_t SEG_ERR[] = {
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
	SEG_E | SEG_G,   						// r
	SEG_E | SEG_G,                         // r
	0    									
};

const uint8_t SEG_ON[] = {
	0,   
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F, // O
	SEG_C | SEG_E | SEG_G, // n
	0          
};

const uint8_t SEG_OFF[] = {
	0,  
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
	SEG_A | SEG_E | SEG_F | SEG_G,    // F
	SEG_A | SEG_E | SEG_F | SEG_G     // F
};

// Create a WebServer Object
ESP8266WebServer server;


bool connectToWiFi(); // connect to WiFi
unsigned long sendNTPpacket(IPAddress&);

void handleSubmit();
void handleRoot();

// the main set up function
void setup()
{
 	Serial.begin(115200);

 	// set the LEDs & relay
 	pinMode(RELAY, OUTPUT);
 	pinMode(BLUE, OUTPUT);
 	pinMode(RED, OUTPUT);

 	display.setBrightness(0x0f); //set the diplay to maximum brightness
	timeVal = 0;

 	// attempt connection 
 	display.setSegments(SEG_CONN);

 	// connect to Wifi
	if(connectToWiFi()) {
		Serial.print("Successfully connected to WiFi ...");
		digitalWrite(RELAY, HIGH);
		display.setSegments(SEG_ON);
	}
}

// the main loop function 
void loop()
{
    server.handleClient(); 
}

// connects to Wifi and sets up the WebServer !
bool connectToWiFi() {
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	int count = 0;

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		if(count++ > MAX_CONNECTION_ATTEMPTS) // 30 attempts
			return false; 
		display.showNumberDecEx(count, 0x00, true);
		digitalWrite(BLUE, HIGH);
		digitalWrite(RED, HIGH);		
		delay(100);
		digitalWrite(BLUE, LOW);
		digitalWrite(RED, LOW);
		delay(100);				
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	
	Serial.println(WiFi.localIP().toString());

	// start the server at the root 
	server.on("/", handleRoot);
	server.begin();

	return true;
}

// handle all server requests
void handleRoot()
{
  if (server.hasArg("submit")) {
    handleSubmit();
  }
  else {
    Serial.print("Going to index page, current value of relay : ");
    Serial.println(digitalRead(RELAY));
    server.send(200, "text/html", INDEX_PAGE);
  }
}

// handle the submit button
void handleSubmit()
{
  String submit = server.arg("submit");

  if(submit == "ON") {
    Serial.print("Got message : ");
    Serial.println(submit);
    digitalWrite(RELAY, HIGH); 
    digitalWrite(BLUE, HIGH);
    digitalWrite(RED, LOW);
	display.setSegments(SEG_ON);    
  } else if (submit == "OFF") {
    Serial.print("Got message : ");
    Serial.println(submit);
    digitalWrite(RELAY, LOW); 
	digitalWrite(BLUE, LOW);  
	digitalWrite(RED, HIGH);
	display.setSegments(SEG_OFF);    	
  }

  server.send(200, "text/html", INDEX_PAGE);
}
