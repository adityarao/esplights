#include <ESP8266WiFi.h>
// controls the 7 segment display
#include <TM1637Display.h>

// enables UDP
#include <WiFiUdp.h>

// time management libraries - uses millis() to manage time
#include <Time.h>
#include <TimeLib.h>

// DHT library to manage the temperature sensor
#include <SimpleDHT.h>

// JSON management library 
#include <ArduinoJson.h>

// Wifi parameters / auth
#define ssid  ""
#define password "$!23"

// 
#define MAX_ATTEMPTS_FOR_TIME 5
#define MAX_CONNECTION_ATTEMPTS 30

// define the pins for RELAY & LED
#define RELAY D1
#define BLUE D2
#define RED D3
#define DHT22_PIN D6

// http port 
#define HTTP_PORT 80

const size_t MAX_CONTENT_SIZE = 512;

// timezonedb host & URLs 
const char *timezonedbhost = "api.timezonedb.com";
const char *timezoneparams = "/v2/get-time-zone?key=&by=zone&zone=Asia/Kolkata&format=json";
 
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

// 4 segment display 
TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

time_t timeVal; 

bool getTemperatureHumidity(float&, float&); // get temp
bool connectToWiFi(); // connect to WiFi
unsigned long sendNTPpacket(IPAddress&);
time_t getTime();
time_t getTimeFromTimeZoneDB(const char*, const char*);

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

const uint8_t SEG_SYNC[] = {
	SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,   // S
	SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,  // y
	SEG_C | SEG_E | SEG_G,                  // n
	SEG_A | SEG_D | SEG_E | SEG_F           // C
};

const uint8_t LETTER_C = SEG_A | SEG_D | SEG_E | SEG_F ; // 'C'
const uint8_t LETTER_H = SEG_C | SEG_E | SEG_F | SEG_G ; // 'H'

// define the temperture object
SimpleDHT22 dht22;

void setup()
{
 	int attempt = 0;
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
		Serial.println("Starting UDP");
		udp.begin(localPort);
		Serial.print("Local port: ");

		Serial.println(udp.localPort());

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

	// set up sync provider for getting time every 180 seconds
	setSyncProvider(getTime);
	setSyncInterval(30); 

	t_SENSE_TEMP = millis(); 
}
 
 
void loop()
{
	float temperature, humidity;
	unsigned int t = hour() * 100 + minute();
	display.showNumberDecEx(t, second()%2?0xff:0x00, true);
	
	if(second()%4) {
		digitalWrite(BLUE, !digitalRead(BLUE));	
		digitalWrite(RED, !digitalRead(RED));
	}

	// turn on the LED during an ODD minute every 4 seconds
	if(minute()%2 && !(second()%4)) {
		digitalWrite(RELAY, HIGH);
	} else {
		digitalWrite(RELAY, LOW);	
	}

	if(millis() - t_SENSE_TEMP > 10000) {
		Serial.print("Time : ");
		Serial.println(t);

		t_SENSE_TEMP = millis();
		if(getTemperatureHumidity(temperature, humidity)) {
			// data to display on the 7 segment display
			uint8_t data[] = { 0xff, 0xff, 0xff, 0xff }; // all on		

			// first display the temperture
			data[0] = temperature < 0 ? SEG_G : 0; // '-' sign

			data[1] = display.encodeDigit((int)temperature / 10); // first digit
			data[2] = display.encodeDigit((int)temperature % 10); // second digit
			data[3] = LETTER_C; // 'C' centigrade

			display.setSegments(data);

			// wait for 2 secs
			delay(2000);
		} else {
			display.setSegments(SEG_ERR);
		}
	}
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

	return true;
}

time_t getTime() 
{
	display.setSegments(SEG_SYNC);

	WiFi.hostByName(ntpServerName, timeServerIP); 
	sendNTPpacket(timeServerIP); // send an NTP packet to a time server

	// wait to see if a reply is available
	delay(3000);

	int cb = udp.parsePacket();

	// introduce a back up for time as its critical for the overall operation
	if (!cb) {
		Serial.println("no packet yet, trying timezonedb as back up!");
		return getTimeBackup();
		// even if this fails, you must connect the system to an RTC module
		// set up time and obtain it. 
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

// API to get your temperature and humidity
bool getTemperatureHumidity(float &temp, float &hum) {
  // read without samples.
  float temperature = 0;
  float humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(DHT22_PIN, &temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Reading DHT22 failed - may not be connected, err="); 
    Serial.println(err);
    return false;
  }

  temp = temperature;
  hum = humidity;

  Serial.print("Temperature - ");
  Serial.println(temp);

  Serial.print("Humidity - ");
  Serial.println(hum);

  Serial.println();

  return true;
}

// wrapper API for getTimeFromTimeZoneDB
time_t getTimeBackup() 
{
	return getTimeFromTimeZoneDB(timezonedbhost, timezoneparams);
}

// connect to timezone db !
time_t getTimeFromTimeZoneDB(const char* host, const char* params) 
{
  Serial.print("Trying to connect to ");
  Serial.println(host);

  // Use WiFiClient for timezonedb 
  WiFiClient client;
  if (!client.connect(host, HTTP_PORT)) {
    Serial.print("Failed to connect to :");
    Serial.println(host);
    return 0;
  }

  Serial.println("connected !....");

  // send a GET request
  client.print(String("GET ") + timezoneparams + " HTTP/1.1\r\n" +
             "Host: " + host + "\r\n" +
             "User-Agent: ESP8266\r\n" +
             "Accept: */*\r\n" +
             "Connection: close\r\n\r\n");

  // bypass HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Header: ");
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  // get the length component
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print( "Body Length: ");
    Serial.println(line);    
      break;
  } 

  String line = "";

  // get the actual body, which has the JSON content
  while (client.connected()) {
    line = client.readStringUntil('\n');
    Serial.print( "Json body: ");
    Serial.println(line);    
    break;
  }   

  // Use Arduino JSON libraries parse the JSON object
  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(8)    // the root object has 8 elements
      + MAX_CONTENT_SIZE;    // additional space for strings

  // Allocate a temporary memory pool
  DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

  JsonObject& root = jsonBuffer.parseObject(line);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return -1;
  }  

  Serial.println("Parsing a success ! -- ");

  // 'timestamp' has the exact UNIX time for the zone specified by zone param
  Serial.println((long)root["timestamp"]);

  return (time_t)root["timestamp"];
}

