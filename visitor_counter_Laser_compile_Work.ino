#include <mqtt.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7

int sensA;                      //    Declaring VARIABLES
int sensB;
const int thresh = 700;
int PersonsCountedin;
int PersonsCountedout;

String ISO8601;
String IDModulo ;
byte data2;
String ATComRsp;
char atCommand[50];
byte mqttMessage[250];
int mqttMessageLength = 0;

//Definir pines de Software Serial.

SoftwareSerial GSMSrl(7, 8); //Rx, TX Arduino --> Tx, Rx En SIM800

//definir  parametros de conexion a red GSM
byte GPRSserialByte;

//Banderas de verificacion
boolean CheckSim800 = false;
boolean mqttSent = false;

//definir  parametros de conexion a servicio de MQTT
const char* server = "iotarduinodaygt.flatbox.io";
const char* port =  "1883";
char* clientId = "d:qnu3pg:Countbox:countbox001";
char * topic = "lasercount";
char payload[250];


void setup() {
  String Res;
  Serial.begin(19200);
  delay(5000);
  //iniciar puerto para transmicion de data GSM
  GSMSrl.begin(9600);
  //Encender Modem
  Res = GPRScommnad ("AT");
  Serial.println(Res);
  delay(1000);
  //3.2.35 AT+CSQ Signal Quality Report
  Res = GPRScommnad ("AT+CSQ");
  Serial.println(Res);
  delay(1000);
  // 7.2.1 AT+CGATT Attach or Detach from GPRS Service 
   Res = GPRScommnad ("AT+CGATT?");
  Serial.println (Res);
  delay(1000);
  //7.2.10 AT+CGREG Network Registration Status
  Res = GPRScommnad ("AT+CGREG?");
  Serial.println (Res);
  delay(1000);
  Res = GPRScommnad ("AT+CSTT=\"broadband.tigo.gt\",\"\",\"\"");
  Serial.println (Res);
  delay(1000);
  Res = GPRScommnad ("AT+CIICR");
  Serial.println (Res);
  delay(1000);
  Res = GPRScommnad ("AT+CIFSR");
  Serial.println (Res);
  delay(1000);
  Res = GPRScommnad ("AT+CLTS=1");
  Serial.println (Res);
  delay(1000);
  GetIMEI();
  ISO8601 = GetTime ();
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
  IDModulo = imei;
  Serial.println(IDModulo);
  delay(1000);
}

String GPRScommnad (String comm) {
  GSMSrl.listen();
  String ATComRsp, response;
  Serial.println("command:" + comm);
  GSMSrl.println(comm);
  delay(800);
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
    ISO8601 = GetTime ();
    delay(1000);
    buildJson(IDModulo, ISO8601, PersonsCountedout, "Salida");
    delay(1000);
    buildJson(IDModulo, ISO8601, PersonsCountedin, "Entrada");
    delay(1000);
    clearSerial();
    PersonsCountedout = 0;
    PersonsCountedin = 0;
    nextsendtime = Starttime + 5 * 60 * 1000UL;
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

void CloseTCPConnection (){
  GSMSrl.println(F("AT+CIPCLOSE"));
  Serial.println(F("AT+CIPCLOSE"));
  delay(1000);
}
