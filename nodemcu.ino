#include <LiquidCrystal_I2C.h>  // display library
#include <Wire.h>               // I2C library
#include "secrets.h"
#include <ArduinoJson.h>

#define DISPLAY_CHARS 16     // number of characters on a line
#define DISPLAY_LINES 2      // number of display lines
#define DISPLAY_ADDR 0x27    // display address on I2C bus

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object

#include <DHT.h>
#include <MQTT.h>

// include WiFi library
#include "ESP8266WiFi.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#define RSSI_THRESHOLD -60   // button debounce time in ms

//SERVO MOTOR
#include <Servo.h>
#define SERVO_PIN D8
#define SERVO_PWM_MIN 500   // minimum PWM pulse in microsecond
#define SERVO_PWM_MAX 2500  // maximum PWM pulse in microsecond
Servo servo; 
#define DELAY 20

// weather api
const char weather_server[] = "api.openweathermap.org";
const char weather_query[] = "GET /data/2.5/weather?q=%s,%s&units=metric&APPID=%s";

//WIFI Config.
char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password
char mysql_user[] = MYSQL_USER;              // MySQL user login username
char mysql_password[] = SECRET_MYSQL_PASS;        // MySQL user login password
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress gateway(GATEWAY);
WiFiClient client;
ESP8266WebServer server(80);
char mqtt_broker[] = "INSERT IP";

// Client MQTT and WiFI
MQTTClient mqttClient;

IPAddress server_addr(IP);

MySQL_Connection conn((Client *)&client);

char query[128];
String INSERT_DATA = "INSERT INTO assignment1.master (temp, hum, rssi, allarm) VALUES ('";

// DHT sensor
#define DHTPIN D4        // sensor I/O pin, eg. D1 (D0 and D4 are already used by board LEDs)
#define DHTTYPE DHT11    // sensor type DHT 11 

DHT dht = DHT(DHTPIN, DHTTYPE);

// BUZZER
#define BUZZER D5

//LEDs
#define LED_EXTERNAL D6

//BUTTONS
#define BUTTON D3
#define BUTTON_DEBOUNCE_DELAY 20    // button debounce time in ms
#define BUTTON2 D7

//PHOTORESISTOR
#define PHOTORESISTOR A0             // photoresistor pin
#define PHOTORESISTOR_THRESHOLD 128  // turn led on for light values lesser than this

#include <ESP8266TelegramBOT.h>
void Bot_ExecMessages();
TelegramBOT bot(BOTtoken, BOTname, BOTusername);

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been d
bool Start = false;

//SAFETY VALUES
float tempMax = 30.0;
int humidityMax = 70;
int rssiMax = 100;
int lightMax = 1000;
bool alerting=HIGH;
int cicleNumber=0;
bool state=true;
unsigned long timeA;
unsigned long timeB;
unsigned long lastQueryTime;
unsigned long lastSlaveTime;

String lastAllarm = "None";
bool activation=false;
bool secondState=HIGH;
String light_str;
char light_char[50];
bool mqtt_activation = false;
bool checkConfig = false;

unsigned long timeC;

void setup() {
  Serial.begin(115200);
  
  //WEB SERVER
  WiFi.mode(WIFI_STA);
  server.on("/", handle_root); 
  server.on("/ON", handle_allarmOn); 
  server.on("/OFF", handle_allarmOff);
  server.onNotFound(handle_NotFound);
  server.begin();
  
  //LCD
  Serial.println("\n\nCheck LCD connection...");
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println("LCD found.");
    lcd.begin(DISPLAY_CHARS, 2);   // initialize the lcd
    lcd.setBacklight(255);   // set backlight to maximum
  } else {
    Serial.print("LCD not found. Error ");
    Serial.println(error);
    Serial.println("Check connections and configuration. Reset to try again!");
    while (true);
  }
  lcd.print("TUTTO OK        ");
  
  //DHT
  Serial.println("\n\nCheck DHT...");
  dht.begin();
  Serial.println("\n\nCheck DHT: OK...");
  
  //BUZZER
  Serial.println("\n\nCheck BUZZER...");
  pinMode(BUZZER, OUTPUT);// set buzzer pin as outputs
  digitalWrite(BUZZER, HIGH); // turn buzzer off
  Serial.println("\n\nCheck BUZZER: OK...");

  //LEDs
  pinMode(LED_EXTERNAL, OUTPUT); // set LED pin as outputs
  digitalWrite(LED_EXTERNAL, HIGH); // turn led off

  //BUTTONS
  pinMode(BUTTON, INPUT_PULLUP); // set BUTTON pin as input with pull-up
   printWifiStatus();  
  connectToWiFi();
  //SERVO MOTOR
  Serial.println("\n\nCheck SERVO MOTOR...");
  servo.attach(SERVO_PIN, SERVO_PWM_MIN, SERVO_PWM_MAX);
  servo.write(0);
  Serial.println("\n\nCheck SERVO MOTOR: OK...");
   mqttClient.begin(mqtt_broker, 1883, client);
  Serial.println("\n\nSetup completed.\n\n");
  //configuro lo slave
  config_slave(); //in set up
  
  // FINE SETUP //
}
    float temp;
    float hum;
    long rssi;
    unsigned int light;
    
void loop() {
    
 if (!(millis() % 5000)){  //Ogni 5s controllo valori e li stampo sul display
    Serial.println(activation);
    mqttClient.subscribe("config");
    temp = tempCheck();
    hum = humidityCheck();
    rssi = rssiCheck();  

    cicleNumber++;
    switch (cicleNumber) {
      case 1:
        print(0,temp);
        break;
      case 2:
        print(1,hum);
        break;
      case 3:
        print(2,rssi);
        break;
      default:
        ;
      }
      if(cicleNumber>3){
        cicleNumber=0;
        
      }
    bot.getUpdates(bot.message[0][1]);   // launch API GetUpdates up to xxx message
    Bot_ExecMessages();   // reply to message with Echo  
 }
 //con bottone gestisco slave
 if(isButtonPressed()){
    mqtt_activation=true;
 }
 if(isButton2Pressed()){
    mqtt_activation=false;
 }
 if(mqtt_activation && checkConfig){
    slave();
 }
 
 //webserver 
 server.handleClient(); // listening for clients on port 80
    
 if (activation) {
   if (temp>tempMax){
      alerting=LOW;
      alertTemp();
      lastAllarm = "Temperature";
   }
   if(hum>humidityMax){
      alerting=LOW;
      alertHumidity();
      lastAllarm = "Humidity";
   }
   if((-rssi)>rssiMax){
      alerting=LOW;
      alertRssi();
      lastAllarm = "rssi";
   }
   if(alerting==LOW){
       if (!(millis() % 10000)){
        WriteMultiToDB(temp,hum,(int)rssi,1);
       }
      allarm(alerting);
   }
 }else{        
    alerting=HIGH;     
    allarm(alerting);
 }
    
 //Write to MySQL
 timeA = millis();
 if (timeA < lastQueryTime) { 
    lastQueryTime = 0L;
 }
 if ( (timeA - lastQueryTime) > 60000 ) { //Sends a query every 60000 milliseconds (60 seconds)
    printCurrentWeather();
    WriteMultiToDB(temp, hum, (int)rssi, 0);
    lastQueryTime = timeA;
 }
 
   ////FINE LOOP/////
}


void Bot_ExecMessages() {
 
  for (int i = 1; i < bot.message[0][0].toInt() + 1; i++)      {
     
    String message_rvd = bot.message[i][5];
    Serial.println(message_rvd);
    if (message_rvd.equals("/turnAllarmON")) {
      activation = true;
      bot.sendMessage(bot.message[i][4], "Allarm is ON", "");
    }
    if (message_rvd.equals("/turnAllarmOFF")) {
      activation = false;
      bot.sendMessage(bot.message[i][4], "Allarm is OFF", "");
    }
   if (message_rvd.equals("/lastAllarm")) {
      bot.sendMessage(bot.message[i][4], lastAllarm, "");
    }
   if (message_rvd.equals("/slaveStatus")) {
    if (mqtt_activation && checkConfig){
      bot.sendMessage(bot.message[i][4], "Connected", "");
    }else{
      bot.sendMessage(bot.message[i][4], "NOT connected", "");
    }
   }
    if (message_rvd.equals("/start")) {
      
      String wellcome = "Wellcome from MonitoringBot, your personal Bot on ESP8266 board";
      String wellcome1 = "/turnAllarmON : to switch the Allarm ON";
      String wellcome2 = "/turnAllarmOFF : to switch the Allarm OFF";
      String wellcome3 = "/lastAllarm : to see the last Allarm that has been triggered";
      String wellcome4 = "/slaveStatus : to see if the slave is connected or not";
      bot.sendMessage(bot.message[i][4], wellcome, "");
      bot.sendMessage(bot.message[i][4], wellcome1, "");
      bot.sendMessage(bot.message[i][4], wellcome2, "");
      bot.sendMessage(bot.message[i][4], wellcome3, "");
      bot.sendMessage(bot.message[i][4], wellcome4, "");
      Start = true;
    }
  }
  bot.message[0][0] = "";   // All messages have been replied - reset new messages
}


void printCurrentWeather() {
  // Current weather api documentation at: https://openweathermap.org/current
  Serial.println(F("\n=== Current weather ==="));

  // call API for current weather
  if (client.connect(weather_server, 80)) {
    char request[100];
    sprintf(request, weather_query, WEATHER_CITY, WEATHER_COUNTRY, WEATHER_API_KEY);
    client.println(request);
    client.println(F("Host: api.openweathermap.org"));
    client.println(F("User-Agent: ArduinoWiFi/1.1"));
    client.println(F("Connection: close"));
    client.println();
  } 
  else {
    Serial.println(F("Connection to api.openweathermap.org failed!\n"));
  }

  while(client.connected() && !client.available()) delay(1);  // wait for data
  String result;
  while (client.connected() || client.available()) {   // read data
    char c = client.read();
    result = result + c;
  }
  
  client.stop();  // end communication
  //Serial.println(result);  // print JSON

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonArray);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  Serial.print(F("Location: "));
  Serial.println(doc["name"].as<String>());
  Serial.print(F("Country: "));
  Serial.println(doc["sys"]["country"].as<String>());
  Serial.print(F("Temperature (°C): "));
  Serial.println((float) doc["main"]["temp"]);
  Serial.print(F("Humidity (%): "));
  Serial.println((float) doc["main"]["humidity"]);
  Serial.print(F("Weather: "));
  Serial.println(doc["weather"][0]["main"].as<String>());
  Serial.print(F("Weather description: "));
  Serial.println(doc["weather"][0]["description"].as<String>());
  Serial.print(F("Pressure (hPa): "));
  Serial.println((float) doc["main"]["pressure"]);
  Serial.print(F("Sunrise (UNIX timestamp): "));
  Serial.println((float) doc["sys"]["sunrise"]);
  Serial.print(F("Sunset (UNIX timestamp): "));
  Serial.println((float) doc["sys"]["sunset"]);
  Serial.print(F("Temperature min. (°C): "));
  Serial.println((float) doc["main"]["temp_min"]);
  Serial.print(F("Temperature max. (°C): "));
  Serial.println((float) doc["main"]["temp_max"]);
  Serial.print(F("Wind speed (m/s): "));
  Serial.println((float) doc["wind"]["speed"]);
  Serial.print(F("Wind angle: "));
  Serial.println((float) doc["visibility"]);
  Serial.print(F("Visibility (m): "));
  Serial.println((float) doc["wind"]["deg"]);

  Serial.println(F("==============================\n"));
  
  //temperature and humidity in Milan can trigger the allarm
  if ((float) doc["main"]["temp"]>35 && activation){
    alerting = LOW;
    lastAllarm = "temperature Milan";
  }
  if ((float) doc["main"]["humidity"]>90 && activation){
    alerting = LOW;
    lastAllarm = "humidity Milan";
  }
}

void config_slave(){
    if (!mqttClient.connected()) {
      connect_to_mqtt();
    }    
    mqttClient.loop(); //MQTT client loop
    const int capacity = JSON_OBJECT_SIZE(1); 
    StaticJsonDocument<capacity> doc;
    doc["configurazione"] = "luce";
    char buffer[128];
    size_t n = serializeJson(doc, buffer);
    Serial.print("JSON message: ");
    Serial.println(buffer);
    mqttClient.publish("config", buffer, n);
    mqttClient.onMessage(messageReceived); 
}

void slave(){
    if (!mqttClient.connected()) {
      connect_to_mqtt();
    }
    mqttClient.loop(); //MQTT client loop
    light = lightCheck();
    mqttClient.begin(mqtt_broker, 1883, client);
    light_str = String(light); //converting light (the float variable above) to a string
    light_str.toCharArray(light_char, light_str.length() + 1); //packaging up the data to publish to mqtt.
    const int capacity = JSON_OBJECT_SIZE(1); 
    StaticJsonDocument<capacity> doc;
    doc["light"] = lightCheck();
    char buffer[128];     
   timeB = millis();
   if (timeB < lastSlaveTime) { 
      lastSlaveTime = 0L;
   }
   if ( (timeB - lastSlaveTime) > 60000 ) { //Sends a query every 60000 milliseconds (60 seconds)
      size_t n = serializeJson(doc, buffer);
      Serial.print("JSON message: ");
      Serial.println(buffer);
      mqttClient.publish("values", buffer, n);
      lastSlaveTime = timeB;
   }
   if(light>lightMax && activation){
      alerting=LOW;
      alertLight();
      lastAllarm = "Light";
      size_t n = serializeJson(doc, buffer);
      Serial.print("JSON message: ");
      Serial.println(buffer);
      mqttClient.publish("values", buffer, n);
   } 
    mqttClient.onMessage(messageReceived);    
}

void messageReceived(String &topic, String &payload) {
  
    Serial.println("incoming: " + topic + " - " + payload);
    StaticJsonDocument<128> doc;
    deserializeJson(doc, payload);
    if("luce" == doc["configurazione"]){
      
      const int capacity = JSON_OBJECT_SIZE(1); 
      StaticJsonDocument<capacity> docu;
      docu["configurazione"] = "values";
      char buffer[128];
      size_t n = serializeJson(docu, buffer);
      Serial.print("JSON message: ");
      Serial.println(buffer);
      mqttClient.publish("config", buffer, n);
      
    }
    if("values" == doc["configurazione"]){
      mqttClient.subscribe("values");
      Serial.println("Configuration DONE!");
      checkConfig = true;
    }
  
}

void connect_to_mqtt() {

  Serial.print("\nconnecting to MQTT broker...");
  while (!mqttClient.connect("esp8266", "", "")) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
  mqttClient.subscribe("config");
  Serial.println("\nsubscribed to config topic!");
}

void handle_root() { //ogni volta che visito la pagina IP Address
  Serial.print("New Client with IP: ");
  Serial.println(server.client().remoteIP().toString());
  server.send(200, "text/html", SendHTML(alerting)); 
}

void handle_allarmOn() { 
  activation=HIGH;
  server.send(200, "text/html", SendHTML(activation));  
}

void handle_allarmOff() {
  activation=LOW;
  server.send(200, "text/html", SendHTML(activation)); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}


//il client riceve questo script html e il client attraverso un browser lo visualizza
String SendHTML(uint8_t activation){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta http-equiv=\"refresh\" content=\"30\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"; //refresh ogni 30s
  ptr +="<title>Web LED Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #1abc9c;}\n";
  ptr +=".button-on:active {background-color: #16a085;}\n";
  ptr +=".button-off {background-color: #ff4133;}\n";
  ptr +=".button-off:active {background-color: #d00000;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>Turning Allarm ON/OFF </h1>\n";
  ptr +="Last Allarm: " + lastAllarm;
  //parte dinamica
   if(activation)
  {ptr +="<p>Current Allarm Status: ON</p><a class=\"button button-off\" href=\"/OFF\">OFF</a>\n";}
  else
  {ptr +="<p>Current Allarm Status: OFF</p><a class=\"button button-on\" href=\"/ON\">ON</a>\n";}
  if(mqtt_activation && checkConfig){
    ptr +="Slave connected: yes";
  }else{
    ptr +="Slave connected: no";
  }

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

boolean isButtonPressed() {
  static byte lastState = digitalRead(BUTTON);       // the previous reading from the input pin
  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON) == lastState) return false;
    delay(1);
  }
  lastState = !lastState;
  return lastState == HIGH ? false : true;
}
boolean isButton2Pressed() {
  static byte lastState2 = digitalRead(BUTTON2);       // the previous reading from the input pin
  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON2) == lastState2) return false;
    delay(1);
  }
  lastState2 = !lastState2;
  return lastState2 == HIGH ? false : true;
}
void allarm(bool state){
  digitalWrite(BUZZER, state);
  digitalWrite(LED_EXTERNAL, state);
  if(state==LOW && secondState==HIGH){
    for (int pos = 0; pos <= 180; pos++) {   // from 0 degrees to 180 degrees
      servo.write(pos);   // set position
      delay(DELAY);       // waits for the servo to reach the position
    }
  }else if(state==HIGH && secondState==LOW){
    for (int pos = 180; pos >= 0; pos--) { // from 180 degrees to 0 degrees
       servo.write(pos);
       delay(DELAY);
    }
  }
  secondState=state; //per evitare che il motore continui a girare in loop
}

void alertHumidity(){
  lcd.setCursor(0, 0);
    lcd.print("HUMIDITY TOO HIGH!");
}
void alertTemp(){
  lcd.setCursor(0, 0);
    lcd.print("TEMP TOO HIGH!");
}
void alertRssi(){
  lcd.setCursor(0, 0);
    lcd.print("RSSI TOO LOW!");
}
void alertLight(){
  lcd.setCursor(0, 0);
    lcd.print("LIGHT TOO HIGH!");
}

float tempCheck(){
  float temp = dht.readTemperature();
  return temp;
}
int humidityCheck(){
  float hum = dht.readHumidity();
  return hum;
}
int lightCheck(){
  unsigned int light = analogRead(PHOTORESISTOR);   // read analog value (range 0-1023)
  return light;
}
int rssiCheck(){
  long rssi = connectToWiFi();
  return rssi;
}


void print(int thing, int value){
    lcd.setCursor(0, 1);
    switch(thing){
      case 0:
        //print temperature
        lcd.print("Temperature:");
        if(value < 10)
        {
        lcd.print(" ");
        }
        lcd.print(value);
        lcd.print(" C");
        break;  
      case 1:
          //print humidity level
        lcd.print("Humid level:");
        if(value < 5)
        {
        lcd.print(" ");
        }
        lcd.print(value);
        lcd.print(".%");
        break;
      case 2:
        //print RSSI level
        lcd.print("WIFI level:");
        if(value < 5)
        {
        lcd.print(" ");
        }
        lcd.print(value);
        lcd.print("dBm");
        break;
      case 3:
        //print light level
        lcd.print("Light level:");
        if(value < 5)
        {
        lcd.print(" ");
        }
        lcd.print(value);
        lcd.print("Lu");
        break;
      default:
          Serial.println(value);
    }
}

long connectToWiFi() {
  long rssi_strenght;
  // connect to WiFi (if not already connected)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);
    //WiFi.config(ip, dns, gateway, subnet);   // by default network is configured using DHCP
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(250);
    }
    Serial.println("\nConnected!");
    rssi_strenght = WiFi.RSSI(); //get wifi signal strenght
  }
  else {
    rssi_strenght = WiFi.RSSI(); //get wifi signal strenght
  }
 
  return rssi_strenght;
}


int WriteMultiToDB(float field1, float field2, int field3, int field4) {
  int error = conn.connect(server_addr, 3306, MYSQL_USER, SECRET_MYSQL_PASS);
  if (error) {
    Serial.println("MySQL connection established.");

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    Serial.println(query);
    // Execute the query
    cur_mem->execute(String(INSERT_DATA + String(field1) + String("', ") + String(field2) + String(", ") + String(field3)
    + String(", ") + String(field4) + String(")")).c_str());

    delete cur_mem;
   Serial.println("Data recorded on MySQL");
   Serial.println("---------------------------------------------------------------------+");

    conn.close();
  }
  else {
    Serial.println("MySQL connection failed: " + String(error));
  }

  return error;
}


void printWifiStatus() {
  Serial.println("\n=== WiFi connection status ===");

  // SSID
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // signal strength
  Serial.print("Signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  // current IP
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // subnet mask
  Serial.print("Subnet mask: ");
  Serial.println(WiFi.subnetMask());

  // gateway
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());

  // DNS
  Serial.print("DNS IP: ");
  Serial.println(WiFi.dnsIP());

  Serial.println("==============================\n");
}
