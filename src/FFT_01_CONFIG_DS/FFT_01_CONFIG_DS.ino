/*

	Example of use of the FFT libray
        Copyright (C) 2014 Enrique Condes

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#define MQTT_MAX_PACKET_SIZE 2048
#include <ESP8266WiFi.h>
#include <PubSubClient.h> 
#include "arduinoFFT.h"
#include <Wire.h>
#include "MPU6050.h"
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>   

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
void gotoSleep(int flashCount);
ADC_MODE(ADC_VCC);

//define your default values here, if there are different values in config.json, they are overwritten.

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/*
// RTC Support
#define RTCMEMORYSTART 64
#define RTCMEMORYLEN 127

typedef struct {
long magic;
long toggleFlag;
IPAddress ip, gw;
} rtcStore;  // The size of this structure must be a multiple of 4 bytes

rtcStore rtcMem;

int buckets = (sizeof(rtcMem) / 4);

extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}
*/

//void wifiConnect();
void mqttConnect();

const char publishTopic[] = "iot-2/evt/status/fmt/json";

#define ORG "" 
#define DEVICE_TYPE "" 
#define DEVICE_ID ""
#define TOKEN ""
char org[10] = ORG;
char deviceType[32] = DEVICE_TYPE;
char deviceId[32] = DEVICE_ID;
char token[50] = TOKEN;

const char IBMserver[] = ".messaging.internetofthings.ibmcloud.com";
char server[200];
char authMethod[] = "use-token-auth";
char clientId[250]; // = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;
void callback(char* topic, byte* payload, unsigned int payloadLength);

WiFiClientSecure wifiClient;
PubSubClient client(server, 443, callback, wifiClient);


MPU6050 accelgyro;

int16_t ax, ay, az;
double tempC;
float volts;

int publishInterval = 2000;
long lastPublishMillis;
char perHour[10] = "";

arduinoFFT FFT = arduinoFFT(); /* Create FFT object */

// FFT Parameters
const uint16_t samples = 128; //This value MUST ALWAYS be a power of 2
const double signalFrequency = 100;
const double samplingFrequency = 300;
const double delayFactor = 1000000 / samplingFrequency;

/*
These are the input and output vectors
Input vectors receive computed results from FFT
*/
double vReal[samples];
double vImag[samples];
double meanArray(double *array, uint8_t size);
char MAC_char[48] = {0};

void readSensorData() {
  volts = ESP.getVcc() / 1000.0;  
  accelgyro.getAcceleration(&ax, &ay, &az);
}

#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02

#define Theta 6.2831 //2*Pi

#define TRIGGER_PIN D5

#ifndef D1
#define D1 5
#define D2 4
#define D4 2
#define D5 14
#define D8 15
#endif

void readSpiffs(), writeSpiffs(), localParams(), initGyro();
WiFiManager wifiManager;

void setup() { 
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Ground this pin to force config portal
  int trigger = digitalRead(TRIGGER_PIN);
  pinMode(D4, OUTPUT);  // We flash this on successful publish
  digitalWrite(D4, LOW);
  Serial.begin(115200);
  delay(100);
  readSpiffs();
  
  pinMode(D8, OUTPUT);
  digitalWrite(D8, HIGH); //Power up the sensor
// Declare custom config parameters
  WiFiManagerParameter custom_org("org", "WIoT org", org, 10);
  WiFiManagerParameter custom_device_type("deviceType", "device type", deviceType, 32);
  WiFiManagerParameter custom_device_id("deviceId", "device id", deviceId, 32);
  WiFiManagerParameter custom_token("token", "token", token, 32);
  WiFiManagerParameter custom_per_hour("perHour", "per hour", perHour, 10);
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
/*
  //set static ip
  system_rtc_mem_read(RTCMEMORYSTART, &rtcMem, buckets * 4);
  if (rtcMem.magic == 19111955) {
    wifiManager.setSTAStaticIPConfig(rtcMem.ip, rtcMem.gw, IPAddress(255,255,255,0));    
    Serial.println("Set Static IP - " + rtcMem.ip.toString());
  }
  else {
    Serial.println("No static IP");
  }
  */
  Serial.println("Setting custom params");
  
  //add all your parameters here
  wifiManager.addParameter(&custom_org);
  wifiManager.addParameter(&custom_device_type);
  wifiManager.addParameter(&custom_device_id);
  wifiManager.addParameter(&custom_token);
  wifiManager.addParameter(&custom_per_hour);

  //reset settings - for testing
  //wifiManager.resetSettings(); // ***********************************************************
  uint8_t MAC_array[6];
  // WiFi.persistent(false);
  WiFi.macAddress(MAC_array);
  for (int i = sizeof(MAC_array)-3; i < sizeof(MAC_array); ++i){
      sprintf(MAC_char,"%s%02x",MAC_char,MAC_array[i]);
  }
  Serial.println(MAC_char);
  sprintf(MAC_char, "%s%s", MAC_char, " Sensor");
  Serial.println(MAC_char);

  Serial.println("Ready");
    if ( trigger == LOW ) {  // Never leaves this if block
      WiFi.mode(WIFI_STA);  
      wifiManager.resetSettings(); // Do we want this here?
      
      if (!wifiManager.startConfigPortal(MAC_char,"IBMSensor")) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(5000);
    }
  }
  wifiManager.setConfigPortalTimeout(30);
  bool connectSuccess = wifiManager.autoConnect(MAC_char,"IBMSensor"); // This will enter the config portal if it fails to connect to WiFi
  digitalWrite(D4, HIGH);
  //read updated parameters
  strcpy(org, custom_org.getValue());
  strcpy(deviceType, custom_device_type.getValue());
  strcpy(deviceId, custom_device_id.getValue());
  strcpy(token, custom_token.getValue());
  strcpy(perHour, custom_per_hour.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    writeSpiffs();
  }

  if (!connectSuccess) {
    gotoSleep(5);
  }
  
  /*
  // Save IP address in RTC memory
  rtcMem.ip = WiFi.localIP();
  rtcMem.gw = WiFi.localIP();
  rtcMem.gw[3] = 1;
  rtcMem.magic = 19111955;
  system_rtc_mem_write(RTCMEMORYSTART, &rtcMem, buckets * 4);
  */
  
  // Assemble full WIoTP server name & Client ID
  localParams();

  Wire.begin(D1, D2); // CLK on D2, Data on D1
  initGyro();
}

void loop() {
//  if (digitalRead(D5) == 0)
//    digitalWrite(D4, LOW);
//  else
//    digitalWrite(D4, HIGH);
 if (!client.loop()) { // Invoke the MQTT client loop to check for published messages
   mqttConnect();
 } 

 // Collect raw accel data
  memset((void *)vImag, 0, sizeof(vImag)); // Clear the array
  long n1 = micros();
  for (uint8_t i = 0; i < samples; i++)
  {
    readSensorData();
    vReal[i] = ax;
    long n2 = micros();
    long d = n2 - n1;
    n1 += delayFactor;
    delayMicroseconds(delayFactor - d);
  }

  // Collect Temperature
  int16_t temp = accelgyro.getTemperature();
  delay(10);
  int16_t temp2 = accelgyro.getTemperature();

  for (int i=0; i < 100 && temp2 != temp; i++) {  // Introduced this to avoid some occasional odd temperature readings
    delay(10);
    temp = temp2;
    temp2 = accelgyro.getTemperature();    
  }

  if (temp != temp2)
    temp = 0; // This should not happen!
    
  tempC = ((float) temp) / 340.0 + 36.53;
//  Serial.println(tempC);

  //Remove DC Component 0 Hz
  double meanReal = meanArray(vReal, samples);
  for (uint8_t i = 0; i < samples; i++) {
    vReal[i] = vReal[i] - meanReal;
  }

  // FFT processing
  FFT.Windowing(vReal, samples, FFT_WIN_TYP_HAMMING, FFT_FORWARD);	/* Weigh data */
  FFT.Compute(vReal, vImag, samples, FFT_FORWARD); /* Compute FFT */
  FFT.ComplexToMagnitude(vReal, vImag, samples); /* Compute magnitudes */
  double x = FFT.MajorPeak(vReal, samples, samplingFrequency);
  double y = FFT.MajorPeakAmplitude(vReal, samples, samplingFrequency);

  // Spectrum makes little sense when there is (almost) no motion detected
  if (isnan(x) || y < 1000)  // Super small reading, so ignore all the frequency measurements
    x = 0;
  y = int(y / 100);
  
  // Assemble payload
  String payload = "{\"d\":{\"MAC\":\"" + String(MAC_char) + 
           "\",\"Freq\":" + String(x) + 
           ",\"Magnitude\": " + String(y) + ",\"Temperature\":" + String(tempC) + ", ";
  
  payload += "\"FreqArray\" : [";
  for (uint16_t i = 0; i < samples >> 1; i++)
  {
    double abscissa;
    abscissa = ((i * 1.0 * samplingFrequency) / samples);
    if (i > 0)
      payload += ",";
    payload += "{\"f\":" + String(int(abscissa)) + ",\"v\":" + String(int(vReal[i]/100)) + "}";
  }
  payload += "], ";
  
  payload += "\"Volts\":" + String(int(100*volts) / 100.0) +" }}";  
  
  Serial.print("Sending payload: "); Serial.println(payload);
  if (client.publish(publishTopic, (char*) payload.c_str())) {
       Serial.println("Publish OK");
       digitalWrite(D4, LOW);
       delay(50);
       digitalWrite(D4, HIGH);
     } else {
       digitalWrite(D4, LOW);
       delay(50);
       digitalWrite(D4, HIGH);
       delay(50);
       digitalWrite(D4, LOW);
       delay(50);
       digitalWrite(D4, HIGH);
       Serial.println("Publish FAILED");
     }
  lastPublishMillis = millis();

  gotoSleep(0);
  }

// Used to remove DC component
double meanArray(double *array, uint8_t size)
{
  double sum = 0.0;
  for(int i=0; i < size; i++)
    sum += array[i];
  return sum / size;
}

void initGyro() {
  accelgyro.initialize();
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  if (accelgyro.testConnection() == false && digitalRead(TRIGGER_PIN) == HIGH)
    gotoSleep(10);
  accelgyro.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  accelgyro.setTempSensorEnabled(true);
}

void gotoSleep(int flashCount) { // Never returns!
  digitalWrite(D4, HIGH);
  if (flashCount > 0)
    delay(500);
      for (int i = 0; i < flashCount; i++) { // FOUR flashes - WiFi connect failure
       digitalWrite(D4, LOW);
       delay(50);
       digitalWrite(D4, HIGH);
       delay(300);
    }
  if (atoi(perHour) != 0) {
    client.disconnect();
    digitalWrite(D8, LOW); // Turn off the sensor
    int sleepTime = 3600 / atoi(perHour);
    ESP.deepSleep(sleepTime*1000000, WAKE_RF_DEFAULT);    
  } else 
    delay(3000);
}


