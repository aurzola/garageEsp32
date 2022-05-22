#include <WiFi.h>
#include <esp_wifi.h>
//#include <esp_softap.h>
#include "SPIFFS.h"
#include <WebServer.h>

#define APPHEADING "<h1>WiFi Abrete el Garage</h1>\n";
// GPIO pin we're using
const int pinmac = 13;   //start prog. store trusted macs
const int pin = 23;
const int pin2 = 22;
//rgb led
#define PIN5V 5
#define PINGREEN 19
#define PINBLUE 21
#define PINRED 18

const char* ssid     = "ESP32";
const char* password = "00000000";

bool    spiffsActive = false;
#define MAXCLIENTS 6
#define PRGMACTIMEOUT 10000
#define TEMP_PATH "/tmpmacs.txt"
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
  server.on("/gate1on", handle_gate1on);
  server.on("/gate2on", handle_gate2on);
  server.on("/listmacs", handle_OnListMacs);
  server.on("/delmac", handle_OnDelMac);
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

boolean delMac(String sMac){
  Serial.println("Mac a eliminar " + sMac); 
  if (spiffsActive) {
    if (SPIFFS.exists(TESTFILE)) {
      File f = SPIFFS.open(TESTFILE, "r");
      if (!f) {
        Serial.printf("Unable To Open '%s' for reading", TESTFILE);
        return false;
      } 
      else {
         File temporary = SPIFFS.open(TEMP_PATH, "w+");
         if (!temporary){
          Serial.println("-- failed to open temporary file "); 
          return false;
         }
         else {        
            String s;
            while (f.position()<f.size()){
              s=f.readStringUntil('\n');
              s.trim();
              if ( sMac != s  ) {
                Serial.println(s + " copied to temporary file "); 
                temporary.println(s);
              }
            } 
            f.close(); 
            temporary.close();
            if (SPIFFS.remove(TESTFILE)) {
                Serial.println("Old file succesfully deleted");
            }
            else{
                Serial.println("Couldn't delete file");
                return false;
            }
            if (SPIFFS.rename(TEMP_PATH,TESTFILE)){
                Serial.println("Succesfully renamed");
            }
            else{
                Serial.println("Couldn't rename file");
                return false;
             } 
             return true;
          }
      }
    } 
    else {
      Serial.println(String(TESTFILE) + " does not exists!"); 
      return false;
    }
  }
  else {
    Serial.println( " SPIFFS not active");
    return false;
  }
}

/*
 * Line number of mac in txt file
 */
int lineNumInMacs(String sMac){
  int pos=-1;
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
        int i=0;
        while (f.position()<f.size())
        {
          s=f.readStringUntil('\n');
          s.trim();
          if ( sMac == s  ) {
            pos=i;
          }
          i++;
        } 
        f.close(); 
      }
    }
  }
  return pos;
}

boolean macRegistered(const unsigned char* mac){
  String sMac = macToString(mac);
  return lineNumInMacs(sMac) != -1; 
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

/*
 * First MAC in txt file is admin's
 */
boolean isAdmin(String sIp){ 
  wifi_sta_list_t wifi_sta_list;
  tcpip_adapter_sta_list_t adapter_sta_list;
  boolean bAdmin=false;
  memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
  memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
  Serial.println(" testing for admin IP "+ sIp);
  esp_wifi_ap_get_sta_list(&wifi_sta_list);
  tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
 
  for (int i = 0; i < adapter_sta_list.num; i++) {
    tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
    Serial.printf("station nr %d", i);
    
    String stationIp =  ip4addr_ntoa(&(station.ip));
    Serial.println("conn station IP: "+ stationIp);  
    
    if ( sIp == stationIp ) {
      String stationMac = macToString(station.mac);
      Serial.println("conn MAC: " + stationMac );
      if ( lineNumInMacs(stationMac) == 0 ){ //first one
        bAdmin=true;
        break;
      }
    }
  }
  return bAdmin;
}

void handle_OnConnect() {
  Serial.print("OnConnect IP ");
  Serial.println(server.client().remoteIP().toString());
  server.send(200, "text/html", SendHTML()); 
}

void handle_gate1on() {
  digitalWrite(pin, HIGH);
  delay(1000);
  // open transistor (button release)
  digitalWrite(pin, LOW);
  Serial.println("gate 1: toogle");
  server.send(200, "text/html", SendHTML()); 
}


void handle_gate2on() {
  digitalWrite(pin2, HIGH);
  delay(1000);
  // open transistor (button release)
  digitalWrite(pin2, LOW);
  
  Serial.println("gate 2: toogle");
  server.send(200, "text/html", SendHTML()); 
}


void handle_NotFound(){
  server.send(404, "text/plain", "<H1>Not found</h1>");
}

String SendHTML(){
  String ptr = "<!DOCTYPE html>";
  ptr += "<html>";
  ptr += "<head>";
  ptr += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">";
  ptr += "<title>Wifi Remote Control</title>";
  ptr += "<style>body {overflow-y: hidden;font-family: Arial, Helvetica, sans-serif;margin: 0;padding: 0;padding-top: 0px;max-height: 100vh;background: conic-gradient(from -32.42deg at 57.47% 51.91%,#fefefe 0deg, #e9edd8 360deg);}a {text-decoration: none;}#header {text-align: center;}#main-title {padding-top: 130px;text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.25);font-weight: 700;margin: 0;font-size: 60px;}#version-tag {font-weight: bold;color: #d41b1b;margin: 0;padding: 0;font-size: 20px;padding-left: 300px;}#main {padding-top: 150px;display: flex;flex-direction: column;align-items: center;}#authors {color: gray;line-height: 20px;font-size: 15px;font-style: italic;margin-left: auto;margin-right: auto;text-align: center;}.button {color: white;padding: 20px 20px;font-size: 30px;width: 280px;background-color: #3d5074;border-radius: 8px;text-align: center;margin-bottom: 60px;box-shadow: 3px 3px 5px rgba(0, 0, 0, 0.25);}.gate-interna {background-color: #fb9963;}#alert-message {color: white;background-color: #A33232;height: 25px;text-align: center;padding-top: 6px;box-shadow: 0px 4px 10px rgba(0, 0, 0, 0.25);}</style>";
  ptr += "</head>";
  ptr += "<body>";
  ptr += "<div id=\"header\">";
  ptr += "<div id=\"alert-message\">Cuidado al cerrar</div>";
  ptr += "<h1 id=\"main-title\">Abre Garaje</h1>";
  ptr += "<span id=\"version-tag\">V 2.0</span>";
  ptr += "</div>";
  ptr += "<div id=\"main\">";
  ptr += "<a class=\"button gate-principal\" href=\"/gate1on\">Principal</a>";
  ptr += "<a class=\"button gate-interna\" href=\"/gate2on\">Interna</a>";
  ptr += "</div>";
  ptr += "<footer id=\"authors\">";
  ptr += "@aurzolar <br/>";
  ptr += "@aaurzola";
  ptr += "</footer>";
  ptr += "</body>";
  ptr += "</html>";
  return ptr;
}

void handle_OnListMacs() {
  Serial.print("OnConnect IP ");
  Serial.println(server.client().remoteIP().toString());
  server.send(200, "text/html", SendHTMLMacs()); 
}

String SendHTMLMacs(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>LED Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; margin:0px auto; }\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr += APPHEADING;
  ptr +="<h3>Trusted Macs</h3>\n<ul>";
  boolean bAdmin = isAdmin(server.client().remoteIP().toString());
  if (spiffsActive) {
    if (SPIFFS.exists(TESTFILE)) {
      File f = SPIFFS.open(TESTFILE, "r");
      if (!f) {
        Serial.printf("Unable To Open '%s' for Reading\n",TESTFILE);
      }
      else {
        String s;
        while (f.position()<f.size())
        {
          s=f.readStringUntil('\n');
          s.trim();
          if ( bAdmin ) 
            ptr += "<li>"+s+" &nbsp;&nbsp; <a href=/delmac?mac="+s+">Elimina</a></li>";
          else 
            ptr += "<li>"+s+"</li>";
        } 
        f.close(); 
      }
    }
    else 
      ptr += String(TESTFILE) + " not exists ";
  } 
  else 
    ptr +=" spiffs not Active";
     
  ptr +="</ul></br></body>\n";
  ptr +="</html>\n";
  return ptr;
}

void handle_OnDelMac() { 

    String message = "";
    String mac2del =  server.arg("mac");
    if ( mac2del == ""){     //Parameter not found
       message = "<h1>mac argument missing</h1>";
    }
    else{     //Parameter found
      if ( delMac(mac2del) ) {
        message = mac2del + " eliminada";
      }
      else{
        message =  "No se pudo eliminar la mac";
      }
    }
    message += "<br><br><a href=/listmacs>Regresar</a>";
    server.send(200, "text/html", message);          //Returns the HTTP response
}
