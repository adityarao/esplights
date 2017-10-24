#include <ESP8266WiFi.h>
#include <TM1637Display.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <TimeLib.h>

#define ssid  "harisarvottama"
#define password "9885104058"

#define MAX_ATTEMPTS_FOR_TIME 5
#define MAX_CONNECTION_ATTEMPTS 30

#define RELAY D1
#define BLUE D2
#define RED D3

 
const int CLK = D5; //Set the CLK pin connection to the display
const int DIO = D4; //Set the DIO pin connection to the display

String localIP = "";

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
 
TM1637Display display(CLK, DIO); //set up the 4-Digit Display.
time_t timeVal; 

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

const uint8_t SEG_SYNC[] = {
	SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,   // S
	SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,  // y
	SEG_C | SEG_E | SEG_G,                  // n
	SEG_A | SEG_D | SEG_E | SEG_F           // C
};

void setup()
{
 	int attempt = 0;
 	Serial.begin(115200);

 	showDateCounter = showMsgCounter = millis();

 	// set the LEDs & relay
 	pinMode(RELAY, OUTPUT);
 	pinMode(BLUE, OUTPUT);
 	pinMode(RED, OUTPUT);

 	display.setBrightness(0x0f); //set the diplay to maximum brightness
	timeVal = 0;

 	// attempt connection 
 	display.setSegments(SEG_CONN);

	if(connectToWiFi()) {
		while(attempt < MAX_ATTEMPTS_FOR_TIME) {
			timeVal = getTime();
			if(timeVal) // got time !
				break;

			attempt++; 
		}
	
		Serial.print("Get time value : ");
		Serial.println(timeVal);
	}

	if(timeVal > 0) {
		setTime(timeVal);
	} else { 
		display.setSegments(SEG_ERR);
	}

	setSyncProvider(getTime);
	setSyncInterval(180);  
}
 
 
void loop()
{
	unsigned int t = hour() * 100 + minute();
	display.showNumberDecEx(t, second()%2?0xff:0x00, true);

	if(minute()%2) {
		digitalWrite(RELAY, HIGH);
		digitalWrite(RED, LOW);
		digitalWrite(BLUE, !digitalRead(BLUE));	
	} else {
		digitalWrite(RELAY, LOW);	
		digitalWrite(BLUE, LOW);	
		digitalWrite(RED, !digitalRead(RED));
	}

	delay(500);
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
	
	localIP = WiFi.localIP().toString();

	Serial.println(localIP);

	Serial.println("Starting UDP");
	udp.begin(localPort);
	Serial.print("Local port: ");

	Serial.println(udp.localPort());

	return true;
}

time_t getTime() 
{
	display.setSegments(SEG_SYNC);

	WiFi.hostByName(ntpServerName, timeServerIP); 
	sendNTPpacket(timeServerIP); // send an NTP packet to a time server

	// wait to see if a reply is available
	delay(4000);

	int cb = udp.parsePacket();

	if (!cb) {
		Serial.println("no packet yet, recursively trying again");
		return 0;
	}
	else {
		Serial.print("packet received, length=");
		Serial.println(cb);
		// We've received a packet, read the data from it
		udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

		//the timestamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, esxtract the two words:
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;

		Serial.print("Seconds since Jan 1 1900 = " );
		Serial.println(secsSince1900);

		// now convert NTP time into everyday time:
		Serial.print("Unix time = ");
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;
		// print Unix time:
		Serial.println(epoch);

		epoch += 5*60*60 + 30*60; // IST ahead by 5 hrs 30 mins

		return epoch;
	}	

}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

