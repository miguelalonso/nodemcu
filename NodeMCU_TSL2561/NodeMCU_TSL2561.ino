#include <FS.h>                   //this needs to be first, or it all crashes and burns...
                      //https://github.com/esp8266/Arduino
//#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <TimeLib.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

boolean debug=1;                                       //Set to 1 to few debug serial output, turning debug off increases battery life
// Time to sleep (in seconds):



unsigned long t_ant_print=0;
int t_print=3000;

uint16_t broadband = 0;
uint16_t infrared = 0; 
float lux;
float luxCS;
float luxCL;
float Ccalib=1000; //cuentas a 1000 W/m2
float irradiancia;
  
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);




WiFiServer server ( 80 );
int ledPin = 12; // GPIO2
 WiFiManager wifiManager;

WiFiClient client_emon;
IPAddress server_emon(163,117,157,189); //ordenador jaen-espectros2
unsigned long lastConnectionTime_emon = 0;             // last time you connected to the server, in milliseconds
boolean lastConnected_emon = false;                    // state of the connection last time through the main loop
const unsigned long postingInterval_emon = 28000; // delay between updates, in milliseconds

 
//NTP
const int timeZone = 0;     // Central European Time
unsigned int localPort = 8888;      // local port to listen for UDP packets
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServer; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
//IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
time_t prevDisplay = 0; // when the digital clock was displayed
//NTP


void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year()); 
  Serial.println(); 
}


void checkinternet(){
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
   
  // Wait until the client sends some data
  Serial.println("new client");
  digitalClockDisplay();
  while(!client.available()){
    delay(1);
  }
   
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();
   
  // Match the request
  int value = LOW;
  if (request.indexOf("/LED=ON") != -1) {
    digitalWrite(ledPin, HIGH);
    value = HIGH;
  } 
  if (request.indexOf("LED=OFF") != -1){
    digitalWrite(ledPin, LOW);
    value = LOW;
  }

 if (request.indexOf("RESET") != -1){
    //reset settings - for testing
    wifiManager.resetSettings();
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
// Set ledPin according to the request
//digitalWrite(ledPin, value);
   
 //client.flush();
  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  String message = "<head>";
  message += "<meta http-equiv='refresh' content='120'/>";
  message += "<title>Sensor G Nodemcu</title>";
  message += "<style>";
  message += "body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }";
  message += "</style></head><body>"; 
 client.print(message);
  
   
  client.print("Led pin is now: ");
   
  if(value == HIGH) {
    client.print("On");  
  } else {
    client.print("Off");
  }
  client.println("<br><br>");
  client.println("Click <a href=\"/LED=ON\">here</a> turn the LED on pin 2 ON<br>");
  client.println("Click <a href=\"/LED=OFF\">here</a> turn the LED on pin 2 OFF<br>");
  client.println("<button type=\"button\" onClick=\"location.href='/LED=ON'\">LED ON</button><br>");
  client.println("<button type=\"button\" onClick=\"location.href='/LED=OFF'\">LED OFF</button><br>");
  client.println("<button type=\"button\" onClick=\"location.href='/RESET'\">RESET WIFI</button><br>");
  
  client.println("<p> UTC Time: ");
  client.print(hour());
  client.print(":");
  client.print(minute());
  client.print(":");
  client.print(second());
  client.print("</p><br>broadband: ");
  client.print(broadband);
  client.print("<br>infrared: ");
  client.print(infrared);
  client.print("<br>lux: ");
  client.print(lux);
  
  client.println("</html>");
  delay(1);
  Serial.println("Client disonnected");
  Serial.println("");
 
}



/*-------- NTP code ----------*/

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
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


time_t getNtpTime()
{
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServer); 
  
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

//================EMONCMS====================

//envio de datos a emoncms
void sendData_emon(void){
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  //client_emon.stop();
   // if there's a successful connection:
   
  Serial.println(F("comienzo de envio a emoncms"));
  if (client_emon.connect(server_emon, 80)) {
    Serial.println(F("Connecting emoncms..."));
    // send the HTTP PUT request:
    client_emon.print(F("GET /emoncms_spectrum/input/post.json?apikey=7953d32569acd2a5384adf18f0a837f3&node=108&json={broadband"));
    client_emon.print(F(":"));
     float valor=broadband;
    client_emon.print(valor);
    client_emon.print(F(",infrared:")); 
    valor=infrared;
    client_emon.print(valor);  
    client_emon.print(F(",Lux:")); 
    valor=lux;
    client_emon.print(valor);  
  
  
    client_emon.println(F("} HTTP/1.1"));
    //client_emon.println(" HTTP/1.1");
    client_emon.println(F("Host: 163.117.157.189"));
    client_emon.println(F("User-Agent: Arduino-ethernet"));
    client_emon.println(F("Connection: close"));
    client_emon.println();

    // note the time that the connection was made:
    lastConnectionTime_emon = millis();
    Serial.println(F("Datos enviados a emoncms_spectrum"));
  } 
  else {
    // if you couldn't make a connection:
    Serial.println(F("Fallo"));
//    Serial.println("Disconnecting emoncms...");
    lastConnectionTime_emon = millis();
    client_emon.stop();
    delay(100);
  }
  
}


void envio_emon(void){
  if (client_emon.available()) {
    char c_emon = client_emon.read();
    //Serial.print(c_emon);
    }
  if (!client_emon.connected() && lastConnected_emon) {
    Serial.println();
    Serial.println("Disconnecting emoncms...");
    client_emon.stop();
    }
     if((millis() - lastConnectionTime_emon > postingInterval_emon) ) {
       delay(5);
        sendData_emon();
    }
  lastConnected_emon = client_emon.connected();
}

void calularLuxCS(){
  //datos obtenidos de hoja de características técnicas del TSL2561
  float Lux=0;
  float CH0=broadband;
  float CH1=infrared;
  float ratio=CH1/CH0;
  float pot=pow(ratio,1.4);


  if(ratio>0 && ratio<= 0.52){ Lux=0.0315*CH0-0.0593*CH0*pot;}
  if( ratio>0.52 && ratio<= 0.65){ Lux = 0.0229*CH0-0.0291*CH1;}
  if( ratio>0.65 &&ratio<= 0.80){ Lux = 0.0157 * CH0-0.0180*CH1;}
  if( ratio>0.80 && ratio<= 1.30){ Lux = 0.00338*CH0-0.00260*CH1;}
  if( ratio>1.30 ){ Lux = 0;}

  luxCS=Lux;
  
}

void calularLuxCL(){
  float Lux=0;
  float CH0=broadband;
  float CH1=infrared;
  float ratio=CH1/CH0;
  float pot=pow(ratio,1.4);


  if(ratio>0 && ratio<= 0.50){ Lux=0.0304*CH0-0.062*CH0*pot;}
  if( ratio>0.50 && ratio<= 0.61){ Lux = 0.0224*CH0-0.031*CH1;}
  if( ratio>0.61 &&ratio<= 0.80){ Lux = 0.0128 * CH0-0.0153*CH1;}
  if( ratio>0.80 && ratio<= 1.30){ Lux = 0.00146*CH0-0.00112*CH1;}
  if( ratio>1.30 ){ Lux = 0;}
 
  luxCL=Lux;
  
}

void medir(){
       if (debug==1){ Serial.println ("Midiendo....");}
  sensors_event_t event;
  tsl.getEvent(&event);
  tsl.getLuminosity (&broadband, &infrared);
  if (event.light)
  {
    lux =event.light;
         if (debug==1)  { 
             Serial.print(event.light); Serial.println(" lux");
             Serial.print(broadband); Serial.println(" broadband;");
             Serial.print(infrared); Serial.println(" infrared;");
         }
  }  else  {    if (debug==1)  {Serial.println("Sensor overload"); } }
    delay(1000);
   calularLuxCS();
   calularLuxCL(); //el bueno en este caso es el CL
   irradiancia=(float)broadband*1000/Ccalib;
  
}

void imprimirdatos(void){
  if ((millis()-t_ant_print)>t_print){
         t_ant_print=millis();
  Serial.print("Lux: ");
  Serial.println(lux);
  Serial.print("LuxCL: ");
  Serial.println(luxCL);
  Serial.print("LuxCS: ");
  Serial.println(luxCS);
  Serial.print("broadband: ");
  Serial.println(broadband);
  Serial.print("infrared: ");
  Serial.println(infrared);
  Serial.print("irradiancia: ");
  Serial.println(irradiancia);
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
 }
}

 void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}



/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2561
*/
/**************************************************************************/
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
   tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  //tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
   //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("402 ms");
  Serial.println("------------------------------------");
}




void setup() {
  Serial.begin(115200);
  Serial.println();
 
  wifiManager.setBreakAfterConfig(true);
  //reset settings - for testing
  //wifiManager.resetSettings();
  if (!wifiManager.autoConnect("NodeMCU", "A12345678")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  Serial.println("connected...yeey :)");
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  /////////////////////////////////////////server
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  server.begin();
  Serial.println("Server started");
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  displaySensorDetails();
  configureSensor();
}


void loop() {
  // put your main code here, to run repeatedly:
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
     // digitalClockDisplay();  
    }
  }
  checkinternet();
  envio_emon();
  medir();
  if (debug==1)  {   imprimirdatos();  }
 
  
}

