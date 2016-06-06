#include <mqtt.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7

int sensA;                      //    Declaring VARIABLES
int sensB;
const int thresh = 700;
int PersonsCountedin;
int PersonsCountedout;

String ISO8601;
int IDModulo = 949567;
byte data2;
String ATComRsp;

char atCommand[50];
byte mqttMessage[250];
int mqttMessageLength = 0;

//Definir pines de Software Serial.

SoftwareSerial GSMSrl(7, 8); //Rx, TX Arduino --> Tx, Rx En SIM800

//definir  parametros de conexion a red GSM
byte GPRSserialByte;

//-------- IBM Bluemix Info ---------//
#define ORG "qnu3pg"
#define DEVICE_TYPE "Countbox"
#define DEVICE_ID "countbox001"

//Banderas de verificacion
boolean CheckSim800 = false;
boolean mqttSent = false;

//definir  parametros de conexion a servicio de MQtt
const char* server = "iotarduinodaygt.flatbox.io";
const char* port =  "1883";
char* clientId = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;
char * topic = "lasercount";
char payload[250];

boolean isGPRSReady() {
  Serial.println(F("AT"));
  GSMSrl.println(F("AT"));
  GSMSrl.println(F("AT+CGATT?"));
  GPRSread();
  GSMSrl.println(F("AT+CGREG?"));
  //GPRSread();
  //Serial.print(F("data2:"));
  //Serial.println(data2);
  delay (200);
  //Serial.println(F("Check OK"));
  //Serial.print(F("AT command RESP:"));
  //Serial.println(ATComRsp);
  delay(100);
  if (data2 > -1) {
    Serial.println(F("GPRS OK"));
    return true;
  }
  else {
    Serial.println(F("GPRS NOT OK"));
    return false;
  }
}

void setup() {
  Serial.begin(19200);
  GSMSrl.begin(9600);
  CheckSim800 = isGPRSReady();
  delay(500);
  wakeUpModem ();
  ConnectToAPN();
  BringUpGPRS();
  GetIPAddress();
  GetIMEI();
  Serial.println(F("GSM Ready"));
}

String GetTime () {
  String result;
  result = GPRScommnad ("AT+CCLK?");
  int firstindex = result.indexOf('"');
  int secondindex = result.indexOf('"', firstindex + 1);
  String command = result.substring(0, firstindex);
  String Clock = result.substring(firstindex + 1, secondindex);
  return Clock;
}

void GetIMEI () {
  String result;
  result = GPRScommnad ("AT+CGSN");
  int firstindex = result.indexOf('|');
  int secondindex = result.indexOf('|', firstindex + 1);
  String command = result.substring(0, firstindex);
  String imei = result.substring(firstindex + 1, secondindex);
  IDModulo = imei.toInt();
  Serial.println(IDModulo);
  delay(1000);
}
String GPRScommnad (String comm) {
  GSMSrl.listen();
  String ATComRsp, response;
  Serial.println("command:" + comm);
  GSMSrl.println(comm);
  delay(500);
  while (GSMSrl.available() > 0) {
    char c = GSMSrl.read();
    if (c == '\n') {
      response += ATComRsp + "|";
      ATComRsp = "";
    } else {
      ATComRsp += c;
    }
  }
  return response;
  ATComRsp = "";
  GSMSrl.flush();
  Serial.flush();
}

void wakeUpModem () {
  GSMSrl.println(F("AT")); // Sends AT command to wake up cell phone
  //GPRSread();
  delay(800); // Wait a second
}

void ConnectToAPN() {
  GSMSrl.println(F("AT+CSTT=\"broadband.tigo.gt\",\"\",\"\"")); // Puts phone into GPRS mode
  GPRSread();
  delay(1000); // Wait a second
}

void BringUpGPRS() {
  GSMSrl.println(F("AT+CIICR"));
  //GPRSread();
  delay(1000);
}

void GetIPAddress() {
  GSMSrl.println(F("AT+CIFSR"));
  GPRSread();
  delay(1000);
}

void clearSerial() {
  while (Serial.read() >= 0) {
    ; // do nothing
  }
  Serial.flush();
  while (GSMSrl.read() >= 0) {
    ; // do nothing
  }
  GSMSrl.flush();
}

unsigned long Starttime;
unsigned long nextsendtime = 30 * 1000UL;

void loop() {
  Starttime = millis();
  sensA = analogRead(A0); // READ SENSOR A
  sensB = analogRead(A1); // READ SENSOR B
  //Serial.println(sensA);
  //Serial.println(sensB);
  InorOut ();

  if (Starttime >= nextsendtime) {
    char bufS[32];
    char bufE[32];
    //Serial.print(F("     persons in :  "));
    // Serial.print(PersonsCountedin);
    //Serial.print(F("     persons out: "));
    //Serial.println(PersonsCountedout);
    sprintf(bufS, "CB%dS", IDModulo);
    sprintf(bufE, "CB%dE", IDModulo);
    //Serial.println(bufS);
    String NodeIDSalida = bufS;
    //Serial.print(F("NodeID Salida"));
    //Serial.println(NodeIDSalida);
    delay(500);
    String NodeIDEntrada = bufE ;
    //Serial.print(F("NodeID Entrada:"));
    //Serial.println(NodeIDEntrada);
    delay(500);
    ISO8601 = GetTime ();
    buildJson(NodeIDEntrada, ISO8601, PersonsCountedout, "Salida");
    delay(2000);
    buildJson(NodeIDSalida, ISO8601, PersonsCountedin, "Entrada");
    delay(1000);
    clearSerial();
    PersonsCountedout = 0;
    PersonsCountedin = 0;
    nextsendtime = Starttime + 5 * 6 * 1000UL;
  }
}

void InorOut () {
  if (sensA < thresh && sensB > thresh) {
    PersonsCountedin = PersonsCountedin + 1; //  IN
    delay(500);
  }
  else {
  }
  if (sensA > thresh && sensB < thresh) {
    PersonsCountedout = PersonsCountedout + 1; // OUT
    delay(500);
  }
  else {
  }
}

void buildJson(String Sid, String tStamp, int Pcount, String InOut) {

  StaticJsonBuffer<500> jsonbuffer;
  JsonObject& root = jsonbuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& data = d.createNestedObject("data");
  data["device_name"] = Sid;
  data["timestamp"] = tStamp;
  data["tipo"] = InOut;
  data["personas"] = Pcount;
  root.printTo(payload, sizeof(payload));
  Serial.println(F("publishing device metadata:"));
  Serial.println(payload);
  StablishTCPconnection ();
  SendMqttConnectMesage();
  sendMqttMessage();
  CloseTCPConnection();
}

void StablishTCPconnection () {
  strcpy(atCommand, "AT+CIPSTART=\"TCP\",\"");
  strcat(atCommand, server);
  strcat(atCommand, "\",\"");
  strcat(atCommand, port);
  strcat(atCommand, "\"");
  GSMSrl.println(atCommand);
  //GPRSread();
  delay(1000);
}

void SendMqttConnectMesage () {
  GSMSrl.println(F("AT+CIPSEND"));
  //Serial.println(F("AT+CIPSEND"));
  delay(1000);
  mqttMessageLength = 16 + strlen(clientId);
  //Serial.println(mqttMessageLength);
  delay(100);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++) {
    GSMSrl.write(mqttMessage[j]); // Message contents
    //Serial.write(mqttMessage[j]); // Message contents
    //Serial.println("");
  }
  GSMSrl.write(byte(26)); // (signals end of message)
  //Serial.println(F("Sent"));
  delay(1000);
}

void sendMqttMessage () {
  GSMSrl.println(F("AT+CIPSEND"));
  //Serial.println(F("AT+CIPSEND"));
  delay(1000);
  mqttMessageLength = 4 + strlen(topic) + strlen(payload);
  Serial.println(mqttMessageLength);
  delay(100);
  mqtt_publish_message(mqttMessage, topic, payload);
  for (int k = 0; k < mqttMessageLength; k++) {
    GSMSrl.write(mqttMessage[k]);
    Serial.write((byte)mqttMessage[k]);
  }
  GSMSrl.write(byte(26)); // (signals end of message)
  Serial.println(F(""));
  Serial.println(F("-------------Sent-------------")); // Message contents
  delay(1000);
}

void CloseTCPConnection () {
  GSMSrl.println(F("AT+CIPCLOSE"));
  //Serial.println(F("AT+CIPCLOSE"));
  delay(1000);
}

void GPRSread () {
  if (GSMSrl.available()) {
    while (GSMSrl.available() > 0) {
      data2 = (char)GSMSrl.read();
      Serial.write(data2);
      ATComRsp += char(data2);
    }
    Serial.println(ATComRsp);
    delay(200);
  }
  ATComRsp = "";
  GSMSrl.flush();
  Serial.flush();
}
