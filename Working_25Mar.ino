/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient/releases/latest
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   http://tiny.cc/tiny-gsm-readme
 *
 **************************************************************
 * Use Mosquitto client tools to work with MQTT
 *   Ubuntu/Linux: sudo apt-get install mosquitto-clients
 *   Windows:      https://mosquitto.org/download/
 *
 * Subscribe for messages:
 *   mosquitto_sub -h test.mosquitto.org -t GsmClientTest/init -t GsmClientTest/ledStatus -q 1
 * Toggle led:
 *   mosquitto_pub -h test.mosquitto.org -t GsmClientTest/led -q 1 -m "toggle"
 *
 * You can use Node-RED for wiring together MQTT-enabled devices
 *   https://nodered.org/
 * Also, take a look at these additional Node-RED modules:
 *   node-red-contrib-blynk-websockets
 *   node-red-dashboard
 *
 **************************************************************/

// Select your modem:
#define TINY_GSM_MODEM_SIM800
// #define TINY_GSM_MODEM_SIM808
// #define TINY_GSM_MODEM_SIM900
// #define TINY_GSM_MODEM_A6
// #define TINY_GSM_MODEM_A7
// #define TINY_GSM_MODEM_M590
// #define TINY_GSM_MODEM_ESP8266
// #define TINY_GSM_MODEM_XBEE

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include "wiring_private.h"
#include <RTCZero.h>
#include <math.h>

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "airtelgprs.com";
const char user[] = "";
const char pass[] = "";

// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial

// or Software Serial on Uno, Nano
//#include <SoftwareSerial.h>
//SoftwareSerial SerialAT(2, 3); // RX, TX
/*!
  We're using a MAX485-compatible RS485 Transceiver.
  Rx/Tx is hooked up to the hardware serial port at 'Serial'.
  The Data Enable and Receiver Enable pins are hooked up as follows:
*/
#define MAX485_DE      9
#define MAX485_RE_NEG  9

/*
 * Local data structires
 */

#define EM1200_NUM_OF_REG 18

enum format {
    FORMAT_FLOAT=0,
    FORMAT_STRING,
    FORMAT_INT,
    FORMAT_ULONG,
};

struct Emeter_reg {
    uint16_t reg;
    uint16_t num;
    enum format fmt;
    char name[50];
};

struct Emeter_reg EM1200[] = {
  {3051, 2, FORMAT_FLOAT, "Active power total"},
  {3003, 2, FORMAT_FLOAT, "Active power phase 1"},
  {3019, 2, FORMAT_FLOAT, "Active power phase 2"},
  {3035, 2, FORMAT_FLOAT, "Active power phase 3"},
  {3053, 2, FORMAT_FLOAT, "Reactive power total"},
  {3005, 2, FORMAT_FLOAT, "Reactive power phase 1"},
  {3021, 2, FORMAT_FLOAT, "Reactive power phase 2"},
  {3037, 2, FORMAT_FLOAT, "Reactive power phase 3"},
  {3049, 2, FORMAT_FLOAT, "Apparent power total"},
  {3001, 2, FORMAT_FLOAT, "Apparent power phase 1"},
  {3265, 2, FORMAT_ULONG, "Forward run seconds"},
  {3267, 2, FORMAT_ULONG, "ON Seconds"},
  {3055, 2, FORMAT_FLOAT, "Power factor total"},
  {3007, 2, FORMAT_FLOAT, "Power factor phase 1"},
  {3023, 2, FORMAT_FLOAT, "Power factor phase 2"},
  {3039, 2, FORMAT_FLOAT, "Power factor phase 3"},
  {3201, 2, FORMAT_FLOAT, "Forward apparent energy"},
  {3203, 2, FORMAT_FLOAT, "Forward active energy"},
   
};
 

// instantiate ModbusMaster object
ModbusMaster node;

void preTransmission()
{
  digitalWrite(MAX485_RE_NEG, 1);
 // digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
  digitalWrite(MAX485_RE_NEG, 0);
 // digitalWrite(MAX485_DE, 0);
}

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);
RTCZero rtc;

const char* broker = "m13.cloudmqtt.com";
const char *thingsBordServer = "demo.thingsboard.io";
#define THINGS_BOARD_TOKEN "uU23brvS8bRNG0NURfgw"
const char *api = "v1/devices/me/telemetry";

const char* topicLed = "water";
const char* topicInit = "water";
const char* topicLedStatus = "water";

#define LED_PIN 13
int ledStatus = LOW;

long lastReconnectAttempt = 0;
union {
  unsigned long data_l;
  float data_f;
}data_format;

String splitString(String input, char deLim, String *output)
{
  int len = input.length();
  int pos = 0;
  pos = input.indexOf(deLim, 0);
  *output=input.substring(pos+1, len);
  return input.substring(0, pos);
  
}
String date[6];
int getTime(String input)
{
  String output;
  String temp;

  SerialUSB.println("getTime");
  SerialUSB.println(input);
  splitString(input, '"', &output);
  SerialUSB.println("here"+output);
  temp = output;
  #if 1
  date[0] =  splitString(temp, '/', &output);
  SerialUSB.println("here"+date[0]);
  temp = output;
  date[1] =  splitString(temp, '/', &output);
  SerialUSB.println("here"+date[1]);
  temp = output;
  date[2] =  splitString(temp, ',', &output);
  SerialUSB.println("here"+date[2]);
  temp = output;
  date[3] =  splitString(temp, ':', &output);
  SerialUSB.println("here"+date[3]);
  temp = output;
  date[4] =  splitString(temp, ':', &output);
  SerialUSB.println("here"+date[4]);
  temp = output;
  date[5] =  splitString(temp, '+', &output);
  SerialUSB.println("here"+date[5]);
  #endif
}

void setup() {
 // pinMode(LED_PIN, OUTPUT);

  // Set console baud rate
  SerialUSB.begin(115200);

  while (!SerialUSB) {
    ;
  }

  pinPeripheral(9, PIO_OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  //pinMode(MAX485_DE, OUTPUT);
  // Init in receive mode
  digitalWrite(MAX485_RE_NEG, 0);
  //digitalWrite(MAX485_DE, 0);

  // Modbus communication runs at 115200 baud
  Serial1.begin(19200, SERIAL_8E1);

  // Modbus slave ID 1
  node.begin(9, Serial1);
  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  delay(10);

  // Set GSM module baud rate
  SerialAT.begin(115200);
  delay(3000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialUSB.println("Initializing modem...");
  

  String modemInfo = modem.getModemInfo();
  SerialUSB.print("Modem: ");
  SerialUSB.println(modemInfo);

  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");
  #if 0
  SerialAT.println("AT+CLTS=1");
  while (!SerialAT.available());
  SerialUSB.println(SerialAT.readString());

  SerialAT.println("AT&W");
  while (!SerialAT.available());
  SerialUSB.println(SerialAT.readString());


  SerialUSB.println("Initializing modem...");
  modem.restart();
#endif  
  
  SerialUSB.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialUSB.println(" fail");
    while (true);
  }
  SerialUSB.println(" OK");





  SerialUSB.print("Connecting to ");
  SerialUSB.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    SerialUSB.println(" fail");
    while (true);
  }

  delay(1000);
#if 0
SerialAT.println("AT+CNTPCID=1");
while (!SerialAT.available());
SerialUSB.println(SerialAT.readString());
SerialUSB.println("here1");
SerialAT.println("AT+CNTP=\"in.pool.ntp.org\",32");
while (!SerialAT.available());
SerialUSB.println(SerialAT.readString());
delay(100);
SerialUSB.println("here");

SerialAT.println("AT+CNTP");
while (!SerialAT.available());


SerialUSB.println(SerialAT.readString());
 while (!SerialAT.available());
SerialUSB.println(SerialAT.readString());
#endif

  SerialUSB.println("OK");
SerialAT.println("AT+CCLK?");
while (!SerialAT.available());
String input = SerialAT.readString();
  SerialUSB.println(input);

  getTime(input);

  rtc.begin(); // initialize RTC
#if 0
   uint8_t hours = date[3].toInt() + 5;
   if (hours > 23) {
       hours = hours % 24;
   }
   
   uint8_t minutes = date[4].toInt() + 30;
#endif
uint8_t hours = date[3].toInt();
uint8_t minutes = date[4].toInt();

   if (minutes > 59) {
    minutes = minutes % 60;
    hours++;
    
   }

 
  // Set the time
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(date[5].toInt());

  // Set the date
  rtc.setDay(date[2].toInt());
  rtc.setMonth(date[1].toInt());
  rtc.setYear(date[0].toInt());

  
  
  SerialUSB.println(date[0]);

 

  // MQTT Broker setup
  mqtt.setServer(thingsBordServer, /*11267*/1883);
  mqtt.setCallback(mqttCallback);
}

boolean mqttConnect() {
  SerialUSB.print("Connecting to ");
  SerialUSB.print(broker);
  if (!mqtt.connect("ByPlayIT"/*"GsmClientTest"*/, THINGS_BOARD_TOKEN/*"jluoxxcf"*/, NULL /*"xhsdwPeC0k3s"*/)) {
    SerialUSB.println(" fail");
    return false;
  }
  SerialUSB.println(" OK");
  #if 0
  mqtt.publish(topicInit, "GsmClientTest started");
  mqtt.subscribe(topicLed);
  #endif
  return mqtt.connected();
}

void print2digits(int number) {
  if (number < 10) {
    SerialUSB.print("0"); // print a 0 before if the number is < than 10
  }
  SerialUSB.print(number);
}

void loop() {
	if (mqtt.connected()) {
		  uint8_t result;
		  uint16_t data[6];
      //ßuint8_t buff[10];
		  uint32_t data_int;
		  uint32_t l = 0;
      float my_val = 0.0;
		  uint8_t i;
      
		  while (l < EM1200_NUM_OF_REG) {
      
			result = node.readHoldingRegisters(EM1200[l].reg - 1, EM1200[l].num);
			if (result == node.ku8MBSuccess)
			{
        uint32_t epoch = rtc.getEpoch();
        String post_string = "{\"ts\":" + String(epoch) +"000";
        post_string += ", \"values\":";
        post_string += "{";
				if (EM1200[l].fmt == FORMAT_FLOAT) {
					for (i = 0; i < 2; i++) {
						data[i] = node.getResponseBuffer(i);
					}
					data_format.data_l = (data[0] << 16) | data[1];
        
          post_string = post_string + "\"" + EM1200[l].name + "\"" + ":" + String(data_format.data_f);
					SerialUSB.println(data_format.data_f);
				}else if (EM1200[l].fmt == FORMAT_ULONG) {
					for (i = 0; i < 2; i++) {
						data[i] = node.getResponseBuffer(i);
					}
					data_format.data_l = (data[0] << 16) | data[1];
					SerialUSB.println(data_format.data_l);
          post_string = post_string + "\"" + EM1200[l].name + "\"" +  ":" + data_format.data_l;
				}
        post_string = post_string +"}" + "}";
        char d[100];
        //sprintf(d, "\"{\"ts\":%d, \"values\":{\"mathew\":25, \"sinoj\":24}\"",rtc.getEpoch()); 
        sprintf(d, "{\"ts\":%d000, \"values\":{\"key1\":\"value1\", \"key2\":\"value2\"}}", rtc.getEpoch() );
        //String t = "{\"ts\":" + String(rtc.getEpoch()).toInt();
        //t+= ",\"values\":{\"mathew\":23, \"sinoj\":25}}";
        //SerialUSB.println(d);
        SerialUSB.println(post_string.c_str());/*1451649600512*/
       mqtt.publish("v1/devices/me/telemetry", /*d*/post_string.c_str());
        l++;
		   // mqtt.publish(topicLedStatus, s.c_str());
			} else {
        #if 0
        // Print date...
      print2digits(rtc.getDay());
      SerialUSB.print("/");
      print2digits(rtc.getMonth());
      SerialUSB.print("/");
      print2digits(rtc.getYear());
      SerialUSB.print(" ");

      // ...and time
      print2digits(rtc.getHours());
      SerialUSB.print(":");
      print2digits(rtc.getMinutes());
      SerialUSB.print(":");
      print2digits(rtc.getSeconds());
				SerialUSB.println(result, HEX);
       #endif
			}
     
		}
   
   /*{"Active power total":0.00,"Active power phase 1":0.00,"Active power phase 2":0.00,"Active power phase 3":0.00,"Reactive power total":0.00,"Reactive power phase 1":0.00,"Reactive power phase 2":0.00,"Reactive power phase 3":0.00,"Apparent power total":0.00,"Apparent power phase 1":0.00,"Forward run seconds":9189,"ON Seconds":6917241,"Power factor total":0.00,"Power factor phase 1":0.00,"Power factor phase 2":0.00,"Power factor phase 3":0.00,"Forward apparent energy":0.00,"Forward active energy":34.16}*/
   
   
   
   //mqtt.publish("v1/devices/me/telemetry", attributes);
  	delay(10000);
    mqtt.loop();
  	} else {
    	// Reconnect every 10 seconds
    	unsigned long t = millis();
    	if (t - lastReconnectAttempt > 10000L) {
      		lastReconnectAttempt = t;
      		if (mqttConnect()) {
        		lastReconnectAttempt = 0;
      		}
    	}
  }

}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialUSB.print("Message arrived [");
  SerialUSB.print(topic);
  SerialUSB.print("]: ");
  SerialUSB.write(payload, len);
  SerialUSB.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLed) {
   // ledStatus = !ledStatus;
    //digitalWrite(LED_PIN, ledStatus);
    
   // mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

