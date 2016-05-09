#include <FS.h>                   //this needs to be first, or it all crashes and burns...
                      //https://github.com/esp8266/Arduino
//#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//#include <ESP8266mDNS.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

boolean debug=1;                                       //Set to 1 to few debug serial output, turning debug off increases battery life
// Time to sleep (in seconds):
const int sleepTimeS = 60;            //versión deepSleep, conectar D0 con RESET //no hace caso del server
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


WiFiManager wifiManager;

WiFiClient client_emon;
IPAddress server_emon(163,117,157,189); //ordenador jaen-espectros2
unsigned long lastConnectionTime_emon = 0;             // last time you connected to the server, in milliseconds
boolean lastConnected_emon = false;                    // state of the connection last time through the main loop
const unsigned long postingInterval_emon = 28000; // delay between updates, in milliseconds

 

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
  Serial.println("INICIO emoncms:");
  if (client_emon.available()) {
    char c_emon = client_emon.read();
    //Serial.print(c_emon);
    }
  if (!client_emon.connected()) {
    Serial.println();
    Serial.println("Disconnecting emoncms...");
    client_emon.stop();
    }
     //if((millis() - lastConnectionTime_emon > postingInterval_emon) ) {
      // delay(5);
        sendData_emon();
    //}
  lastConnected_emon = client_emon.connected();
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
  
   irradiancia=(float)broadband*1000/Ccalib;
  
}

void imprimirdatos(void){
  if ((millis()-t_ant_print)>t_print){
         t_ant_print=millis();
  Serial.print("Lux: ");
  Serial.println(lux);
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
  
  
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  
  displaySensorDetails();
  configureSensor();
}


void loop() {
  // put your main code here, to run repeatedly:
  medir();
  envio_emon();
  if (debug==1)  {   imprimirdatos();  }
  ESP.deepSleep(sleepTimeS * 1000000, WAKE_RF_DEFAULT);
  delay(500);
  
}

