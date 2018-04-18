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

#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>

char server[] = "platform.ioeye.com";
int port = 80; // port 80 is the default for HTTP
// Your GPRS credentials
// Leave empty, if missing user or pass
//const char apn[]  = "airtelgprs.com";
const char apn[]  = "bsnlnet";
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
    
};

struct Emeter_reg EM1200[] = {
  {3051, 2, FORMAT_FLOAT},
  {3003, 2, FORMAT_FLOAT},
  {3019, 2, FORMAT_FLOAT},
  {3035, 2, FORMAT_FLOAT},
  {3053, 2, FORMAT_FLOAT},
  {3005, 2, FORMAT_FLOAT},
  {3021, 2, FORMAT_FLOAT},
  {3037, 2, FORMAT_FLOAT},
  {3049, 2, FORMAT_FLOAT},
  {3001, 2, FORMAT_FLOAT},
  {3265, 2, FORMAT_ULONG},
  {3267, 2, FORMAT_ULONG},
  {3055, 2, FORMAT_FLOAT},
  {3007, 2, FORMAT_FLOAT},
  {3023, 2, FORMAT_FLOAT},
  {3039, 2, FORMAT_FLOAT},
  {3201, 2, FORMAT_FLOAT},
  {3203, 2, FORMAT_FLOAT},
   
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
//GSMClient client;
TinyGsmClient client(modem);
HttpClient http(client, server, port);

RTCZero rtc;

//const char* broker = "m13.cloudmqtt.com";
//const char *thingsBordServer = "demo.thingsboard.io";
//#define THINGS_BOARD_TOKEN "uU23brvS8bRNG0NURfgw"
//const char *api = "v1/devices/me/telemetry";

//const char* topicLed = "water";
//const char* topicInit = "water";
//const char* topicLedStatus = "water";

#define LED_PIN 13
int ledStatus = LOW;

long lastReconnectAttempt = 0;
union {
  unsigned long data_l;
  float data_f;
}data_format;
String data_j;
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
  String output = String();
  String temp = String();

 
  splitString(input, '"', &output);
 
  temp = output;
  #if 1
  date[0] =  splitString(temp, '/', &output);
 
  temp = output;
  date[1] =  splitString(temp, '/', &output);
 
  temp = output;
  date[2] =  splitString(temp, ',', &output);
   temp = output;
  date[3] =  splitString(temp, ':', &output);
  
  temp = output;
  date[4] =  splitString(temp, ':', &output);
  
  temp = output;
  date[5] =  splitString(temp, '+', &output);
  
  #endif
}
 String date_t[5];
void setup() {
 // pinMode(LED_PIN, OUTPUT);

  // Set console baud rate
  SerialUSB.begin(115200);
#if 0
  while (!SerialUSB) {
    ;
  }
#endif
  delay(1000);
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
  do {
    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    SerialUSB.println("Initializing modem...");
    modem.restart();

    String modemInfo = modem.getModemInfo();
    SerialUSB.print("Modem: ");
    SerialUSB.println(modemInfo);

  
  
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


    SerialUSB.println("OK");
    SerialAT.println("AT+CCLK?");
    while (!SerialAT.available());
    String input = SerialAT.readString();

    delay(1000);
    SerialUSB.println(input);

    getTime(input);
   }while (date[0].toInt() < 18);

  rtc.begin(); // initialize RTC

   uint8_t hours = date[3].toInt() + 5;
   if (hours > 23) {
       hours = hours % 24;
   }
   
   uint8_t minutes = date[4].toInt() + 30;
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

}


void print2digits(int number) {
  if (number < 10) {
    SerialUSB.print("0"); // print a 0 before if the number is < than 10
  }
  SerialUSB.print(number);
}

void postsense(String sensedata) {
  //Serial.println("post cmd2");
  // initialize serial communications and wait for port to open:
  //GSM.begin();
  int x;

  String sensetime;
        sensetime+= "20"; 
        sensetime+= rtc.getYear();
        if (rtc.getMonth() < 10) {
          sensetime += "0";
        }
        sensetime+= rtc.getMonth();
        if (rtc.getDay() < 10) {
          sensetime += "0";
        }
        sensetime+= rtc.getDay();
        
        if (rtc.getHours() < 10) {
          sensetime += "0";
        }
        sensetime+= rtc.getHours();
        if (rtc.getMinutes() < 10) {
          sensetime += "0";
        }
        sensetime+= rtc.getMinutes();
        if (rtc.getSeconds() < 10) {
          sensetime += "0";
        }
        sensetime+= rtc.getSeconds();
  
        String RTCN2;
        String RTCN3;
        String RTCN4;
#if 1
  RTCN2 = sensetime.substring(0,8);
  RTCN3 = "9";
  RTCN4 = sensetime.substring(8,14);

  String RTCN5 = RTCN2 + RTCN3 + RTCN4;
  #endif
  sensedata = RTCN5 + sensedata;
  
  SerialUSB.println(RTCN5);
  SerialUSB.println("Starting Arduino web client.");
  delay(100);
  
   data_j = "";
  StaticJsonBuffer<1000> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["topic"] ="bbbfe350-92cf-11e7-a670-bc764e106405/data" ;
  root["manufacturer"] = "Raspberrypi";
  root["client_id"]="bbbfe350-92cf-11e7-a670-bc764e1064051";
  root["data"]=sensedata;
  root["cmd"]="SAVEN-INDIA-INFO";
  root["id"]="862462038381554";
  root["time"]= RTCN5;
  root["offset"]="0";
  root["v"]="1";
  //root["data"] = sensedata;
  JsonObject& metadata = root.createNestedObject("metadata");
  metadata["protocol"]="csv";
  
  

  metadata["file_id"]="QUAL1";

  
  delay(100);
  
 root.printTo(data_j);
 delay(100);

  http.beginRequest();
  http.post("/api/v1/data");
  //  http.post("/api/v1/data HTTP/1.1");
   http.sendHeader("Host: platform.ioeye.com");
   http.sendHeader("Authorization: Bearer lhdsfljksda34ewrwernvjxcvclkcxj");
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Cache-Control: no-cache");
  http.sendHeader("Postman-Token: bf4e4a8d-c3d0-b639-21ac-4ae5c83f67fa");
 // http.sendHeader("Postman-Token: febb1d5d-c59e-c456-ede1-4f6b013aac66");
  //http.sendHeader("Content-Length", String(root.measureLength()));
 
  http.sendHeader("Content-Length", String(data_j.length()));
  http.beginBody();
//  http.print("{"topic":"bbbfe350-92cf-11e7-a670-bc764e106405/data","manufacturer":"Raspberrypi","client_id":"bbbfe350-92cf-11e7-a670-bc764e1064051","data":"1803282045539,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,9189,6917241,0.00,0.00,0.00,0.00,0.00,34.16","cmd":"SAVEN-INDIA-INFO","id":"862462038381554","time":"1803282045539","offset":"0","v":"1","metadata":{"protocol":"csv","file_id":"QUAL1"}}");
  //root.printTo(http);
  http.print(data_j);
  
  http.endRequest();

 

  SerialUSB.println("Posted data");

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();
  
  SerialUSB.print("Status code: ");
  SerialUSB.println(statusCode);
  SerialUSB.print("Response: ");
  SerialUSB.println(response);
  statusCode  = 0;
  client.stop();
  delay(10000);
  
  modem.gprsDisconnect();
 delay(100);
    if (!modem.gprsConnect(apn, user, pass)) {
      SerialUSB.println(" fail");
      while (true);
    }
    SerialUSB.println("Modem connected back");
 
  delay(100);


}


void loop() {

      uint8_t result;
      uint16_t data[6];
      
      uint32_t data_int;
      uint32_t l = 0;
      
      uint8_t i;
      String value = ",";
     
      
      while (l < EM1200_NUM_OF_REG) {
        
      
      result = node.readHoldingRegisters(EM1200[l].reg - 1, EM1200[l].num);
      if (result == node.ku8MBSuccess)
      {
        
        if (EM1200[l].fmt == FORMAT_FLOAT) {
          for (i = 0; i < 2; i++) {
            data[i] = node.getResponseBuffer(i);
          }
          data_format.data_l = (data[0] << 16) | data[1];
          value += String(data_format.data_f);
          
          
        }else if (EM1200[l].fmt == FORMAT_ULONG) {
          for (i = 0; i < 2; i++) {
            data[i] = node.getResponseBuffer(i);
          }
          data_format.data_l = (data[0] << 16) | data[1];
          
          
          value += String(data_format.data_l);
        }
        if (l < 17)  {
          value += ",";
        }
        l++;
       
      } else {

        delay(10);
       // SerialUSB.println(result, HEX);
    
      }
     
    }
   
   
   
   
   postsense(value);
   
   
    delay(60000);
    
}




