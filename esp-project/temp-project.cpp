/**************************************************************
Temperature sensor with Webserver w/o Blynk framework (https://blynk.io/getting-started) -> "bare metal"
 **************************************************************/

// FEATURE-SECTION
#undef FEATURE_OVER_THE_AIR_UPDATE_ENABLED
#define FEATURE_USE_DALLAS_TEMPSENSOR_ENABLED
#define DEBUG_OUTPUT

#include <ESP8266WiFi.h>

#include <WiFiUdp.h>
#include <TimeLib.h>


#include <ESP8266HTTPClient.h> //neu
#ifdef FEATURE_OVER_THE_AIR_UPDATE_ENABLED
  #include <ArduinoOTA.h> //neu
#endif

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


// META-information for the SW / uC
const char compile_date[] = __DATE__ " " __TIME__;
int start_date;   //Neustart-Datum uC für Debugausgabe


// WIFI section
// settings for WLAN connection for further processing
#include "WiFi_settings.h"
char ssid[] = WLAN_SSID;  //"GummelNMel";
char pass[] = WLAN_PWD;   // "0391292846245736";


// Webserver section (would be incorporated by Blynk framework)
#define HTTP_WEBSRV_PORT   80
ESP8266WebServer server(HTTP_WEBSRV_PORT);    // Create a webserver object that listens for HTTP request on given port
MDNSResponder mdns;              // -webserver- => Multicast Domain Name Server => able to resolve a name instead of using IP-adresses


// UDP protocol handler
// used for internet communication like NTP etc.
// ressources:
//  https://www.rfc-editor.org/rfc/rfc768.html -> User Datagram Protocol
//  https://www.rfc-editor.org/rfc/rfc791 -> INTERNET PROTOCOL
WiFiUDP Udp;  // create an object for UDP connection over WiFi
unsigned int localPort = 8888;  // local port to listen for UDP packets


// NTP Network Time Protocol
// used to get the current time for the ESP-device
// ressources:
//  https://www.rfc-editor.org/rfc/rfc1305 -> (outdated) Network Time Protocol (Version 3) Specification, Implementation and Analysis
//  https://www.rfc-editor.org/rfc/rfc5905.html -> (used here) Network Time Protocol Version 4: Protocol and Algorithms Specification
// requires:
//  UDP protocol
// improve: use of NTP class e.g. https://github.com/arduino-libraries/NTPClient / 
#define NTP_SERVER      "europe.pool.ntp.org"  //#define NTP_SERVER "pool.ntp.org"
#define NTP_UDP_PORT    123
#define TIME_ONEDAY_IN_SECONDS 86400 // TEST: 1U*24U*60U*60U -> [d]*[h]*[min]*[sec]
IPAddress timeServerIP;   // IP aus POOL
IPAddress timeServerIPBraunschweig(192,53,103,104); //Braunschweig - GP: quasi als backup wenn man aus dem POOL nichts zu Stande bringt
int timeZone = 1;     // UTC +1 = Winterzeit, UTC + 2 = Sommerzeit
int countntpnio = 0;  //zählt fehlerhafte Versuche ntp
time_t prevDisplay = 0; // when the digital clock was displayed
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);



// temporary variables for time and date values => extraction of NTP time
byte hourval, minuteval, secondval, monthval, dayval;
int  yearval;

// Temperature storage space
// ToDo: Implement ring-buffer like storage
int tempspeicher[2] [1000];          // Speicher für Temp (GP: Zeitstempel + Temperatur mit einer Queue-Size von 1000 Einträgen)
int laufvar_tempspeicher=0;

// section for temperature sensor
// OneWire connection
// Abstraction for temperature sensor via Dallas Semiconductor C++ class
#include <OneWire.h>
#ifdef FEATURE_USE_DALLAS_TEMPSENSOR_ENABLED
  #include <DallasTemperature.h>
#endif

#define ONE_WIRE_BUS D4   // PIN Temperatursensor
OneWire oneWire(ONE_WIRE_BUS); 
#ifdef FEATURE_USE_DALLAS_TEMPSENSOR_ENABLED
  DallasTemperature sensors(&oneWire);
#endif

DeviceAddress sensorDeviceAddress;
int numberOfDevices; // Anzahl der gefundenen Sensoren


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// let the execution begin!
// The Arduino standard "1st executed service"
void setup()
{ 
  
#ifdef FEATURE_OVER_THE_AIR_UPDATE_ENABLED
  ArduinoOTA.begin(); // GP: relevant wenn OTA-Update des ESP gewünscht wäre
#endif

// Open up a serial connection in order to see the connection status in Serial Monitor plus any further alive / debug messages
  Serial.begin(115200);

// Open up a WiFi connection to a router etc.
  WiFi.begin(ssid, pass);

// Query the connection status and "wait" until the connection could be established
  // ToDo -> integrate this within a loop instead of multiple if-statements or even better outsource this as a waitForConnection-service
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 1");
    // ToDo -> Alternative: Use of the "F()-macro" in order to safe SRAM but take use of Flash memory e.g. https://arduinogetstarted.com/faq/what-is-serial.println-f 
    //Serial.println(F("kein WIFI - step 1"));
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 2");
    // ToDo - F()-macro
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 3");
    // ToDo - F()-macro
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 4");
    // ToDo - F()-macro
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 5");
    // ToDo - F()-macro
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 6");
    // ToDo - F()-macro
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 7");
    Serial.println("Oops, thats bad - no WiFi connection could be established");
    // ToDo - F()-macro
  }
  else {
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    // ToDo - F()-macro
    // ToDo -> combine in 1 command like:
    // Serial.println("\n"
    //              "WiFi connected\n"
    //              "IP address: \n")
    // Alternative:
    // Serial.println( F("\n WiFi connected\n IP address: \n") )
    Serial.println(WiFi.localIP());  
    delay(3000);
  }
  
  // ToDo: Assumption, that everything went right => we can continue => generally this is no robust implementation at the very first!
  // ToDo - F()-macro
  Serial.println("ESP Alive");
  Serial.println("Starting UDP protocol handler");
  
// Establish a UDP connection / port as a prerequesite for NTP querries
  Udp.begin(localPort);

  // debugging output
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

// Resolve the given hostname of a NTP_SERVER to an IP address
  WiFi.hostByName(NTP_SERVER, timeServerIP);

// "time settings" for the ESP-device or precise: declare a time providing service (via setSyncProvider()) to a time-library (time.cpp) and set a re-sync cycle via setSyncInterval()
  setSyncProvider(getNtpTime);
  setSyncInterval(TIME_ONEDAY_IN_SECONDS); // 1 Tag    in sek 7200=2STd nach dieser zeit wird versucht, die Zeit neu vom Server zu beziehen

  // ToDo: Hier an der Stelle als auch innerhalb von getNtpTime scheint mir ein Gewurschtel zu sein.
  // Da das in der Setup passiert sollte da meiner Ansicht nach keinesfalls die 3 rauskommen
  if (countntpnio == 1){
    Serial.println("ntp1 x nio");
    setSyncProvider(getNtpTime); //erste Wiederholung mit neuer IP
    if (countntpnio == 3){
      Serial.println("ntp2 x nio, verbinde mit Braunschweig");
      countntpnio=10000;  // ToDo: Ach pfui! Hier kennt dieser Code das Verhalten von getNtpTime!!!
      setSyncProvider(getNtpTime);
      }
  } //zweite Wiederholung Sync mit Braunschweig    
  
  //Serial.print("M. n. NTP: "); Serial.println(millis());
  setvals();
  Serial.print(hour());  Serial.print(":");   Serial.print(minute()); Serial.print("  ");Serial.print(day());Serial.print("-");Serial.print(month());Serial.print("-");Serial.println(year());
  if (summertime_EU()){
    adjustTime(3600);
    setvals();
  }    
  // Print the current time
  Serial.print("Es ist ");
  Serial.print(hourval);
  Serial.print(" ");
  Serial.print(minuteval);
  Serial.print(" ");
  Serial.print(secondval);
  Serial.print(" weekday: ");
  Serial.println(weekday()); 
  start_date= hourval*10000+minuteval*100+secondval;  

  Serial.println("Suche Temperatur Sensoren...");
  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  Serial.print("Habe ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" Sensoren gefunden.");

  WiFi.hostname("Temperaturlogger");
  
  //sensors.begin();
  sensors.getAddress(sensorDeviceAddress, 0);
  sensors.setResolution(sensorDeviceAddress, 11);
  /* sensors.getAddress(sensorDeviceAddress, 1);
  sensors.setResolution(sensorDeviceAddress, 11);
  sensors.getAddress(sensorDeviceAddress, 2);
  sensors.setResolution(sensorDeviceAddress, 11);
  sensors.getAddress(sensorDeviceAddress, 3);
  sensors.setResolution(sensorDeviceAddress, 11);
  sensors.getAddress(sensorDeviceAddress, 4);
  sensors.setResolution(sensorDeviceAddress, 11);
  sensors.getAddress(sensorDeviceAddress, 5);
  sensors.setResolution(sensorDeviceAddress, 11);
  */

  
  pinMode(D4, INPUT);  
  
  
  
  /* GP: Web-Server initialisieren */
  mdns.begin ( "esp8266", WiFi.localIP() );
  server.on ( "/", []() {handleRoot();} );
  server.onNotFound ( handleNotFound );
  server.begin();
  
  for ( int i = 0; i < 1000; i++ ) {
    tempspeicher[0][i]=0;
    tempspeicher[1][i]=0;
  }
  messen();
}



void loop()
{ 
  mdns.update();
  server.handleClient();
  ArduinoOTA.handle();
  
      if (now() != prevDisplay) { //update every sec      
        RTCauslesen(); //Secondval etc belegen
        Serial.print(second());
       if (second()%10==0){  //jede 10 sek   
          messen();
        }
         
      }//end update every sec  
}


void messen()
{
	//digitalWrite(D2, HIGH);
  //pinMode(D4, INPUT);
  Serial.print(" Requesting temperatures..."); 
  sensors.requestTemperatures(); // Send the command to get temperature readings 
  delay(1000);
  Serial.println("DONE"); 
  Serial.print("Kollektor: "); 
  Serial.println(sensors.getTempCByIndex(0));
	
  Serial.print("laufvar_tempspeicher: "); 
  Serial.println(laufvar_tempspeicher);
  tempspeicher[0][laufvar_tempspeicher]=hourval*10000+minuteval*100+secondval;
  tempspeicher[1][laufvar_tempspeicher]=sensors.getTempCByIndex(0)*100;
  laufvar_tempspeicher++;
  if (laufvar_tempspeicher==999){laufvar_tempspeicher=999;}     
}






/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
long lastntp;

/*
 * getNtpTime ()
 * Param in:  void
 * Param out: time_t time including variable as one object (e.g. long)
 * 
 * Retrieve NTP-protocol based time of a NTP server
 * 
 * 
*/

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets

  if(WiFi.status() != WL_CONNECTED){
    Serial.println("kein WlanSync"); // ToDo - F()-macro
    return 0;
  }

  // debugmessage to show that a NTP request is sent
  Serial.print("NTP Request to IP: "); // ToDo - F()-macro
 
  if (countntpnio>9999){ //ToDo: hässlicher "Fix" weil der NiO von aussen modifiziert wird!
    Serial.println(timeServerIPBraunschweig);
    sendNTPpacket(timeServerIPBraunschweig);  //Rückfall auf Braunschweig wenn 2x nio in Setup
  }
  else if (countntpnio%2 == 1){
    WiFi.hostByName(NTP_SERVER, timeServerIP);
    Serial.print("new hostByName "); // ToDo - F()-macro
    Serial.println(timeServerIP);
    sendNTPpacket(timeServerIP);
    countntpnio++;
  }  //neue IP bei jedem n.i.o.
  else {
    Serial.println(timeServerIP);
    sendNTPpacket(timeServerIP);
  }  //normaler Abgleich mit alter IP
  
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;      
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24; // convert four bytes starting at location 40 to a long integer
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];      
      lastntp=0;    
      return secsSince1900 - 2208988800UL + (timeZone+summertime_EU()) * SECS_PER_HOUR;  //in Timezone =1 
     }
  }
   Serial.println("No NTP Response :-(");
   countntpnio++;
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
// source: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/NTPClient/NTPClient.ino
void sendNTPpacket(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see "..URL above.." or https://www.ietf.org/rfc/rfc5905.txt [Page 18] for details on the packets)
  packetBuffer[0] = 0b11 100 011;   // LI (2-bit) = 3 aka. unknown, Version (3-bit) = 4, Mode (3-bit) = 3 aka. client
  packetBuffer[1] = 0;     // Stratum, or type of clock = 0 aka. "unspecified or invalid"
  packetBuffer[2] = 6;     // Polling Interval in log2 seconds => 6 == 2,584 sec
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;   // reference ID - clarification of these contents see: https://forum.arduino.cc/t/udp-ntp-clients/95868
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, NTP_UDP_PORT); // NTP requests are to port 123 as by RFC "..When operating in symmetric modes (1 and 2), this field must contain the NTP port number PORT (123) assigned by the IANA.  In other modes, it can contain any number consistent with local policy.."
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

boolean summertime_EU(){ // return value: returns true during Daylight Saving Time, false otherwise
   if (monthval<3 || monthval>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
   if (monthval>3 && monthval<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
   //if (monthval==3 && (hourval + 24 * dayval)>=(2 + 24*(31 - (5 * yearval /4 + 4) % 7)) || monthval==10 && (hourval + 24 * dayval)<(2 + 24*(31 - (5 * yearval /4 + 1) % 7)))
   if (monthval==3 && (24 * dayval)>=(24*(31 - (5 * yearval /4 + 4) % 7)) || monthval==10 && (24 * dayval)<(24*(31 - (5 * yearval /4 + 1) % 7)))  
     return true;
   else
     return false;
}

 void RTCauslesen(){  //Secondval etc belegen, LDRvalue auslesen, brightness_time brightness_back brightness_c3 belegen
  prevDisplay = now();  
  secondval = second();
  minuteval = minute();
  hourval = hour(); 
 } 

 void setvals(){
  secondval = second();
  minuteval = minute();
  hourval = hour();
  dayval=day();
  monthval=month();
  yearval=year();
}



void handleRoot() {    
  String out = "<html><head><title>Teichtemperatur</title><meta name=\"viewport\" content=\"width=260\" /><meta charset=\"UTF-8\"></head>";
  out += "<body background='http://www.koeckkeller.de/esp/paper.jpg' style='background-color:gray'>";
  out += "<span style='display:block; width:100%; font-size:1em; font-family:Verdana; text-align:left'>";
  long rssi = WiFi.RSSI(); out += "<br>WLAN Empfang rssi "; out += rssi; 
  out += F("<br>compiliert "); out += compile_date;
  out += F("<br>start hhmmss "); out += start_date;
    
  out += "<br>Anzahl gefundener Sensoren: "; out += numberOfDevices;
  //out += "<br>auth "; out += auth;
  out += "<br>Kollektor "; out += sensors.getTempCByIndex(0);
  out += "<br>Rücklauf "; out += sensors.getTempCByIndex(1);
  out += "<br>Einlauf 1 "; out += sensors.getTempCByIndex(2);
  out += "<br>Einlauf 2 "; out += sensors.getTempCByIndex(3);
  out += "<br>Einlauf 3 "; out += sensors.getTempCByIndex(4);
  out += "<br>Sensor 6 "; out += sensors.getTempCByIndex(5);
  for (int i=0;i<laufvar_tempspeicher;i++){
    out += "<br>Zeit "; out += tempspeicher[0][i];
    out += "  Temp "; out += tempspeicher[1][i]/100; out += ","; out += tempspeicher[1][i]%100;
  }
  out += "</body></html>";
  server.send ( 200, "text/html", out );
  }



void handleNotFound() {    
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for ( uint8_t i = 0; i < server.args(); i++ ) {
        message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }
    server.send ( 404, "text/plain", message );    
}
