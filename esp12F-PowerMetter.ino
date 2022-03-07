#include <Filters.h> //Easy library to do the calculations
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

const char* ssid = "SSID";
const char* password = "WIFIPass";

char* SendStatus;
/************************* MQTT Setup *********************************/

#define MQTT_SERVER      "MQTTSERVER"
#define CLINET_ID       "esp8266" 
// Using port 8883 for MQTTS
#define MQTT_SERVERPORT  8883

//Connect to OpenRemote MQTT server
#define MQTT_USERNAME    "mqtt-user"
#define MQTT_KEY         "pass"

/************************ End of MQTT Setup ****************************/


// WiFiFlientSecure for SSL/TLS support
WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, CLINET_ID, MQTT_USERNAME, MQTT_KEY);

// your MQTT SHA1 fingerprint
static const char *fingerprint PROGMEM = "FA 35 D8 62 65 7D 04 95 72 37 00 D2 CB 01 8A F5 D6 66 D0 03";

/****************************** Feeds ***************************************/

// Setup feeds for publish
Adafruit_MQTT_Publish KiloWatt = Adafruit_MQTT_Publish(&mqtt, "master/esp8266/writeattributevalue/power/3qZSlO8Dn1Aw4e7W49uyhV");
Adafruit_MQTT_Publish KWh = Adafruit_MQTT_Publish(&mqtt, "master/esp8266/writeattributevalue/kwh/3qZSlO8Dn1Aw4e7W49uyhV");

/*************************** Sketch Code ************************************/

// MQTT sendig interval values
unsigned long Previous_Time=0;
unsigned long Previous_Hour=0;
int Period=60000;  //We send a value every 60s

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

float RMSPower;
double kilos = 0;
float kwhour = 0;
float peakPower = 0;

Adafruit_ADS1015 ads;

const int FACTOR = 100; //100A/1V from the CT

const float multiplier = 0.002;

void(* resetFunc) (void) = 0;//declare reset function at address 0

// Voltage Sensor
float testFrequency = 50;                     // test signal frequency (Hz)
float windowLength = 40.0/testFrequency;     // how long to average the signal, for statistist

int Sensor = 0; //Sensor analog input, here it's A0

float intercept = -0.04; // to be adjusted based on calibration testing
float slope = 0.0405; // to be adjusted based on calibration testing
float current_Volts; // Voltage

// End of voltage sensor

void setup_wifi() {
  int8_t ret;

  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  uint8_t retries = 3;
  while (ret = (WiFi.status()) != 0) {
  delay(500);
  retries--;
  if (retries == 0) {
      break;
      }
  }
}


// Function to connect and reconnect as necessary to the MQTT server.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         break;
       }
  }

//  Serial.println("MQTT Connected!");
}


void setup() {
  Serial.begin(9200);
  setup_wifi();

  // check the fingerprint of s5.itnso.com's SSL cert
  client.setFingerprint(fingerprint);
  

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Running Power Metter");
  display.display();
  
  // ADS Configuration
  ads.setDataRate(0x00C0);
  ads.setGain(GAIN_ONE);
  ads.begin();

}

void loop() {
  unsigned long Current_Time=millis();
  float currentRMS = getcurrent();
  float current_volt = getvoltage();
  RMSPower = current_volt*currentRMS;
  
  if (RMSPower > peakPower)
  {
    peakPower = RMSPower;
  }

  kilos += RMSPower*3; // 3 seconds long to calculate the RMSPower.
  kwhour += (RMSPower*3)/3600000; // 3 seconds long to calculate the RMSPower and devide by 360.. for KWh
    
  if(Current_Time - Previous_Time >= Period){  //Every 1 minute we send the value to the MQTT provider
//  Serial.print(F("\nSending KW only "));
//  Serial.println(kilos);
  MQTT_connect();
  if (! KiloWatt.publish(kilos/1000)) { // K-watt per min
//  Serial.println(F("Failed"));
    SendStatus = "Failed";
    resetFunc();
    
  } else {
    SendStatus = "Ok";
    kilos = 0;
  Serial.println(F("OK"));
    
  }
  Previous_Time = Current_Time;
  
  }


  if(Current_Time - Previous_Hour >= 3600000){  //Every 1 hour we send the KWh to the MQTT provider
//  Serial.print(F("\nSending kwhour "));
//  Serial.println(kwhour);
  MQTT_connect();
  if (! KWh.publish(kwhour)) {
//    Serial.println(F("Failed!"));
    SendStatus = "Failed!";
    resetFunc();

  } else {
    SendStatus = "OK!";
//    Serial.println(F("OK!"));
    kwhour = 0;
    
  }
  Previous_Hour = Current_Time;
  
  }

//  delay (1000);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(currentRMS, 3);
  display.print(" A");
  display.setCursor(50,0);
  display.print("==>");
  display.setCursor(70,0);
  display.print(RMSPower, 2);
  display.print(" W");
  display.setCursor(0,12);
  display.print("Peak");
  display.setCursor(50,12);
  display.print("==>");
  display.setCursor(70,12);
  display.print(peakPower, 2);
  display.print(" W");
  display.setCursor(0,25);
  display.print(current_volt);
  display.print("V");
  display.setCursor(50,25);
  display.print("ST:");
  display.setCursor(70,25);
  display.print(SendStatus);
//  display.print(" kWh");
  display.display();

}

float getvoltage() {
  long time_check = millis();
  int count = 0;
    RunningStatistics inputStats;                //RMS Calculation
    inputStats.setWindowSecs( windowLength );
    
     while(millis() - time_check < 2500) {

     Sensor = ads.readADC_SingleEnded(2);  // read the analog in value:
     inputStats.input(Sensor);  // log to Stats function
        delay(1);
        }

       current_Volts = intercept + slope * inputStats.sigma(); //Calibartions for offset and amplitude
       current_Volts= current_Volts*(40.3231);                //Further calibrations for the amplitude
      
     return(current_Volts);
  }      



float getcurrent()
{
  float voltage;
  float current;
  float sum = 0;
  long time_check = millis();
  int counter = 0;

  while (millis() - time_check < 1000)
  {
    voltage = ads.readADC_Differential_0_1() * multiplier;
    current = voltage * FACTOR;
  
    sum += sq(current);
    counter = counter + 1;

  }

  current = sqrt(sum / counter);
  return (current);
}
