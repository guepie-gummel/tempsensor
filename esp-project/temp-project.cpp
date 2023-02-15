/**************************************************************
Teichtemp mit Webserver ohne Blynk
 **************************************************************/

#include <ESP8266WiFi.h>

#include <WiFiUdp.h>
#include <TimeLib.h>


#include <ESP8266HTTPClient.h> //neu
#include <ArduinoOTA.h> //neu


#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
MDNSResponder mdns;              //webserver

time_t getNtpTime();
time_t prevDisplay = 0; // when the digital clock was displayed
void sendNTPpacket(IPAddress &address);
#define NTP_SERVER "europe.pool.ntp.org"  //#define NTP_SERVER "pool.ntp.org"
IPAddress timeServerIP;   // IP aus POOL
IPAddress timeServerIPBraunschweig(192,53,103,104); //Braunschweig - GP: quasi als backup wenn man aus dem POOL nichts zu Stande bringt
int timeZone = 1;     // UTC +1 = Winterzeit, UTC + 2 = Sommerzeit
int countntpnio = 0;  //zählt fehlerhafte Versuche ntp

const char compile_date[] = __DATE__ " " __TIME__;
int start_date;   //Neustart uC für Debugausgabe

WiFiUDP Udp;  // create an object for UDP connection over WiFi
unsigned int localPort = 8888;  // local port to listen for UDP packets

// temporary variables for time and date values => extraction of NTP time
byte hourval, minuteval, secondval, monthval, dayval;
int  yearval;

int tempspeicher[2] [1000];          // Speicher für Temp (GP: Zeitstempel + Temperatur mit einer Queue-Size von 1000 Einträgen)
int laufvar_tempspeicher=0;

#include <OneWire.h> 
#include <DallasTemperature.h>

#define ONE_WIRE_BUS D4   // PIN Temperatursensor

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

DeviceAddress sensorDeviceAddress;

// settings for WLAN connection for further processing
char ssid[] = "GummelNMel";
char pass[] = "0391292846245736";

int numberOfDevices; // Anzahl der gefundenen Sensoren


void setup()
{ 
  // ArduinoOTA.begin(); // GP: relevant wenn OTA-Update des ESP gewünscht wäre

  // Open up a serial connection in order to see the connection status in Serial Monitor plus any further alive / debug messages
  Serial.begin(115200);

  // Open up a WiFi connection to a router etc.
  WiFi.begin(ssid, pass);

  // Query the connection status and "wait" until the connection could be established
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 1");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 2");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 3");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 4");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 5");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 6");
    delay(2000);}
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("kein WIFI - step 7");
    Serial.println("Oops, thats bad - no WiFi connection could be established");}
  else {
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());  
    delay(3000);
  }
  
  // ToDo: Assumption, that everything went right => we can continue => generally this is no robust implementation at the very first!
  Serial.println("ESP Alive");
  Serial.println("Starting UDP");
  
  // Establish a UDP connection / port as a prerequesite for NTP querries
  Udp.begin(localPort);

  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  // Resolve the given hostname of a NTP_SERVER to an IP address
  WiFi.hostByName(NTP_SERVER, timeServerIP);

  setSyncProvider(getNtpTime);
  setSyncInterval(86400); // 1 Tag    in sek 7200=2STd nach dieser zeit wird versucht, die Zeit neu vom Server zu beziehen

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
  Serial.print(hour());Serial.print(":");Serial.print(minute()); Serial.print("  ");Serial.print(day());Serial.print("-");Serial.print(month());Serial.print("-");Serial.println(year());
  if (summertime_EU()){adjustTime(3600); setvals();}    
  Serial.print("Es ist "); Serial.print(hourval); Serial.print(" "); Serial.print(minuteval); Serial.print(" ");Serial.print(secondval); Serial.print(" weekday: "); Serial.println(weekday()); 
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
{ mdns.update();
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
    Serial.println("kein WlanSync");
    return 0;
  }

  // debugmessage to show that a NTP request is sent
  Serial.print("NTP Request to IP: ");

  if (countntpnio>9999){
    Serial.println(timeServerIPBraunschweig);
    sendNTPpacket(timeServerIPBraunschweig);  //Rückfall auf Braunschweig wenn 2x nio in Setup
  }
  else if (countntpnio%2 == 1){
    WiFi.hostByName(NTP_SERVER, timeServerIP);
    Serial.print("new hostByName ");
    Serial.println(timeServerIP);
    sendNTPpacket(timeServerIP);
    countntpnio++;}  //neue IP bei jedem n.i.o.
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
void sendNTPpacket(IPAddress &address){
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
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
