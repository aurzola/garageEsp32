#include <WiFi.h>
#include <esp_wifi.h>
#include "SPIFFS.h"
#include <WebServer.h>


// GPIO pin we're using
const int pinmac = 13;   //start prog. store trusted macs
const int pin = 23;
const int pin2 = 22;
#define PIN5V 5
#define PINGREEN 19
#define PINBLUE 21
#define PINRED 18

const char* ssid     = "ESP32";
const char* password = "00000000";

bool    spiffsActive = false;
#define MAXCLIENTS 6
#define PRGMACTIMEOUT 10000
#define TESTFILE "/macs.txt"
WebServer server(80);
// Variable to store the HTTP request
String header;
static boolean prgMac = false;
static uint32_t lastMillis = 0;

void IRAM_ATTR isr() {
  prgMac = true;
  lastMillis = millis();
  digitalWrite(PINBLUE, LOW); //Led ON! 
  Serial.println("prgMac!");
}

void blinkOk(){
 // Serial.println("blink blue");
  for(int i=0;i<5;i++){
    digitalWrite(PINBLUE, HIGH);
    delay(100);
    digitalWrite(PINBLUE, LOW);
    delay(100);
  }
}

void blinkWarn(){
 
  for(int i=0;i<5;i++){
    digitalWrite(PINRED, HIGH);
    delay(100);
    digitalWrite(PINRED, LOW);
    delay(100);
  }
  digitalWrite(PINRED, HIGH);
}


void setup() {
 Serial.begin(115200);
 delay(10);
 // set pin to output
 pinMode(pin, OUTPUT);
 digitalWrite(pin, LOW);
 pinMode(pin2, OUTPUT);
 digitalWrite(pin2, LOW);
 pinMode(PIN5V, OUTPUT);
 digitalWrite(PIN5V, HIGH);
 pinMode(PINBLUE, OUTPUT);
 digitalWrite(PINBLUE, HIGH);
 pinMode(PINRED, OUTPUT);
 digitalWrite(PINRED, HIGH);
 pinMode(PINGREEN, OUTPUT);
 digitalWrite(PINGREEN, HIGH);
 WiFi.mode(WIFI_AP);
 while(!WiFi.softAP(ssid, password))
  {
   Serial.println(".");
    delay(100);
  }
  
  Serial.print("AP Started");
  Serial.println(ssid);
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());
  
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisConnected, SYSTEM_EVENT_AP_STADISCONNECTED);
 
  // Start filing subsystem
  if (SPIFFS.begin()) {
      Serial.println("SPIFFS Active");
      Serial.println();
      spiffsActive = true;
     
  } else {
      Serial.println("Unable to activate SPIFFS");
  }
  // Start the server
  server.on("/", handle_OnConnect);
  server.on("/led1on", handle_led1on);
  server.on("/led2on", handle_led2on);
  server.onNotFound(handle_NotFound);

  pinMode(pinmac, INPUT_PULLUP);
  attachInterrupt(pinmac, isr, FALLING);
  
  server.begin();
  Serial.println("Server started (80)");

}


void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
   
  Serial.println("Station connected.");
  if ( prgMac ) {
    Serial.println(" -> Storing MAC:");
    addMac(info.sta_connected.mac);
    blinkOk();
  }
  if ( !macRegistered(info.sta_connected.mac) )  {
    esp_wifi_deauth_sta(info.sta_connected.aid);
    Serial.println(" Client not registered-> disconnected!");
    blinkWarn();
  }
  
}

void WiFiStationDisConnected(WiFiEvent_t event, WiFiEventInfo_t info){
 Serial.println("Station disconnected.");
  
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void addMac(const unsigned char* mac){  
    if ( macRegistered(mac) )
      return;
   
     File f = SPIFFS.open(TESTFILE, "a");
      if (!f) {
        Serial.print("Unable To Open '");
        Serial.print(TESTFILE);
        Serial.println("' for Appending");
        Serial.println();
      } else {
        Serial.print("Appending line to file '");
        Serial.print(TESTFILE);
        Serial.println("'");
        Serial.println();
        f.println(macToString(mac));
        f.close();
      }
}


boolean macRegistered(const unsigned char* mac){
   String sMac = macToString(mac);
   boolean registered=false;
   if (spiffsActive) {
    if (SPIFFS.exists(TESTFILE)) {
      File f = SPIFFS.open(TESTFILE, "r");
      if (!f) {
        Serial.print("Unable To Open '");
        Serial.print(TESTFILE);
        Serial.println("' for Reading");
        Serial.println();
      } else {
        String s;
        while (f.position()<f.size())
        {
          s=f.readStringUntil('\n');
          s.trim();
          if ( sMac == s  ) 
            registered=true;
        } 
        f.close(); 
      }
      if (registered ) 
        Serial.println("MAC Already Registered" );
    }
    
   }
  return registered; 
}


void loop(){
  server.handleClient();
  //Cancel prgMac after 1 Minute
  if (prgMac && (millis() - lastMillis > PRGMACTIMEOUT )) {
     prgMac = false;
     digitalWrite(PINBLUE, HIGH);
     Serial.println("prgMac cancelled!");
  }
}


void handle_OnConnect() {
  Serial.print("OnConnect IP ");
  Serial.println(server.client().remoteIP().toString());
  server.send(200, "text/html", SendHTML()); 
}

void handle_led1on() {
  digitalWrite(pin, HIGH);
  delay(1000);
  // open transistor (button release)
  digitalWrite(pin, LOW);
  Serial.println("gate 1: toogle");
  server.send(200, "text/html", SendHTML()); 
}


void handle_led2on() {
  digitalWrite(pin2, HIGH);
  delay(1000);
  // open transistor (button release)
  digitalWrite(pin2, LOW);
  
  Serial.println("gate 2: toogle");
  server.send(200, "text/html", SendHTML()); 
}


void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>LED Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ESP32 Web Server</h1>\n";
  ptr +="<h3>Using Access Point(AP) Mode</h3>\n";
  
  
  {ptr +="<a class=\"button button-on\" href=\"/led1on\">Principal</a>\n";}
  
  
  {ptr +="<a class=\"button button-on\" href=\"/led2on\">Interna</a>\n";}
  
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
