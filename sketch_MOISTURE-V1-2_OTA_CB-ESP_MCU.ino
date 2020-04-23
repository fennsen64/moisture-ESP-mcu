/*********************************************************************
ESP8266/ESP-NodeMCO Moisture-Sensor-Module to MQTT - V1.2 OTA Version 

NodeMCU-Based IoT Project: Connecting YL-69 & YL-38 Moisture

- Additional functions:
* the intervall value for the sensor read can be adjusted via MQTT publish a new Millisecond value to this device: 
  ** see INTERVALL_SUB_FEED
  ** Default: 60000 ms = 1 min. interval at which to read sensor: can be updated by publish ../set_sensor_intervall
 
* the sensor can be resetted by MQTT publish a "1" to this device: see RESET_SUB_FEED
  ** see: RESET_SUB_FEED[] PROGMEM = "yourname/home/IoT/" IOT_HOSTNAME "/reset_sensor";

What you have to adjust:

- sensor_pin: if you use an other chip or pin
- IOT_HOSTNAME: unique name of the ESP chip (normaly the last 4 elements from MAC address
WIFI:
- WLAN_SSID and WLAN_PASS
MQTT:
- AIO_SERVER: the IP address of the MQTT broker host
- AIO_USERNAME & AIO_KEY (if you use authorisation on the broker)
- MQTT feeds: all PROGMEM variables which holds the publish topic structure: adjust to your existing hyrarchi
OTA (Over The Air Updates)
- ArduinoOTA.setPassword .... : put your password here if you use authorisation for OTA

Flash with Arduino IDE (ESP / Nodem MCU libraries have to been installed 
*********************************************************************/

#include <ESP8266WiFi.h>
// new for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

//********************************************
//MOISTURE SENSOR on Node-MCU (ESP-12)
//********************************************
// https://www.hackster.io/HARGOVIND/nodemcu-based-iot-project-connecting-yl-69-yl-38-moisture-7cf84a
// NodeMCU Pin A0 > Analog input
int sensor_pin = A0;

int output_value_raw ;
int output_value_percent ;

int previousReading3 = 0;

const char* moisture_char_percent = ""; /* For translating digital value to publish-able char */
const char* moisture_char_raw = ""; /* For translating digital value to publish-able char */

float diff = 1.0;

/* WIFI SETUP */

#define WLAN_SSID       "YOUR-SSID"  //Put your SSID here
#define WLAN_PASS       "YOUR-PASSWORD"  //Put you wifi password here

/* ADAFRUIT MQTT SETUP */

#define AIO_SERVER      "MQTT-BROKER-IP-ADRESS" // e.g. "192.168.0.100"
#define AIO_SERVERPORT  1883         // Standard MQTT Port is 1883; use 8883 for SSL
#define AIO_USERNAME    "YOUR-MQTT-USERNAME"      //Put your MQTT userid here
#define AIO_KEY         "YOUR-MQTT-PASSWORD"    //Put your MQTT key here

// ESP-Modul: ESP-12E (Node-MCU)
#define IOT_HOSTNAME    "ESP_TEST"  // replace this after successful test with real ESP ID, e.g. "ESP_258A4F"

// Generally, you should use "unsigned long" for variables that hold time
unsigned long previousMillis = 0;        // will store last temp was read
// Variable darf nicht konstant sein, da ueber MQTT publish ein update erfolgen kann
unsigned long interval = 60000;              // 1 min. interval at which to read sensor: can be updated by publish ../set_sensor_intervall

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Store the MQTT server, username, and password in flash memory.

const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, AIO_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);

/************************* Feeds ***************************************/

// Setup feeds called for publishing.
//** NEW FOR Feuchtigkeit ********************************
const char MOISTURE_FEED1[] PROGMEM = "yourname/home/IoT/" IOT_HOSTNAME "/moisture_level_raw";
Adafruit_MQTT_Publish moisture_publish_1 = Adafruit_MQTT_Publish(&mqtt, MOISTURE_FEED1);

const char MOISTURE_FEED2[] PROGMEM = "yourname/home/IoT/" IOT_HOSTNAME "/moisture_level_percent";
Adafruit_MQTT_Publish moisture_publish_2 = Adafruit_MQTT_Publish(&mqtt, MOISTURE_FEED2);


// Setup feeds called for subscribe.
const char INTERVALL_SUB_FEED[] PROGMEM = "yourname/home/IoT/" IOT_HOSTNAME "/set_sensor_intervall";
Adafruit_MQTT_Subscribe intervall_set = Adafruit_MQTT_Subscribe(&mqtt, INTERVALL_SUB_FEED);

const char RESET_SUB_FEED[] PROGMEM = "yourname/home/IoT/" IOT_HOSTNAME "/reset_sensor";
Adafruit_MQTT_Subscribe reset_set = Adafruit_MQTT_Subscribe(&mqtt, RESET_SUB_FEED);

/********************** Sketch Code ************************************/

void MQTT_connect();

//

void setup() {

  Serial.begin(115200); 
  Serial.println(F("ESP8266/ESP-NodeMCO Moisture-Sensor-Module to MQTT - V1.2 "));
  Serial.print(F("IOT-Hostname: "));
  Serial.println(IOT_HOSTNAME);
  delay(10);

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  // disable wifi-ap
  // modes: WiFi.mode(m): set mode to WIFI_AP, WIFI_STA, or WIFI_AP_STA - See more at: http://www.esp8266.com/viewtopic.php?p=16816#sthash.TBDvly2h.dpuf
  WiFi.mode(WIFI_STA);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // wait 2 seconds that Sensor synchronize itself
  delay(2000);


  // Setup MQTT subscriptions for feeds.
  bool subscription_ret = mqtt.subscribe(&intervall_set);
  Serial.println("Subscription : ");
  Serial.println(subscription_ret);
  
  bool reset_ret = mqtt.subscribe(&reset_set);
  Serial.println("Subscription : ");
  Serial.println(reset_ret);

  // Callback enablen
  intervall_set.setCallback(callback_intervall);
  reset_set.setCallback(callback_reset);
     
  // OTA Block

  // ** Port defaults to 8266, , uncomment to adjust
  // ArduinoOTA.setPort(8266);

  // ** Hostname defaults to esp8266-[ChipID], , uncomment to adjust
  // ArduinoOTA.setHostname("myesp8266");

  // ** No authentication by default, uncomment to enable and adjust password
  // ArduinoOTA.setPassword((const char *)"YOUR-OTA-PASSWORD");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");

  // END OTA Block
}

//********************** END Setup ************************************/

int delayTime = 5000;  //initial Wait 5 seconds before sending data to web
int startDelay = 0;

//********************** START Loop **********************************/  
void loop() {
    
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  
  // wait 1 seconds for subscription messages
  // This tells the library to wait xx seconds (xxxxxms) for incoming packets before moving on to the next task
  // and it will block all other code execution. 
  // If your sketch is only waiting for subscription messages, then 10 seconds might be a good timeout,
  // but if your sketch handles other tasks, you may want to reduce the timeout to 1 second (1000ms).
  mqtt.processPackets(1000);

  int delayTime = interval;  //set delayTime to actual intervall value (if overwritten by publish, otherwise it's the default from initial definition)
    
  ArduinoOTA.handle();
    
  // Now we can publish stuff!
  if (millis() - startDelay < delayTime) {
    Serial.print (".");
  } else {

    // Sensor data will be read
    Serial.println();
    Serial.print("Requesting MOISTURE level...");
    Serial.println();

  // C. BEGIN SENSOR SECTION
  // read Sensor data
  output_value_raw = analogRead(sensor_pin);
  Serial.print("Moisture RAW-value: "); 
  Serial.print(output_value_raw);

  // kalibrierte Sensorwerte in einem Glas Wasser:
  // voll im Wasser: 320-350 (->max value = 320)
  // 1/4 im Wasser: 750-780 (-> ca. 50% value)
  
  // voll in voll nasser Erde: 232 (gerade gegossen), geht dann nach 10 Sek. runter auf 250 (->max value = 250)
  // minimal feuchte Erde: 437
  // komplett trockene Erde: 1024
  
  // komplett trocken: 1024 (->min value = 1024)
  output_value_percent = map(output_value_raw,1024,250,0,100); 
  Serial.print("Moisture : "); 
  Serial.print(output_value_percent);
  Serial.print("%\n");  
  
  unsigned int moisture_val_1 = float(output_value_percent);
  //* Wert merken um nur bei Delta zu publishen
  int reading3 = moisture_val_1;

  unsigned int moisture_val_2 = float(output_value_raw);
  
  // Hat sich der Messwert generell geändert? Differenz ausrechnen
  if (previousReading3 != 0 && reading3 != 0) {
    diff = (float)reading3 / (float)previousReading3;
    Serial.print("Difference Faktor to last read: ");
    Serial.println(diff);
  }
  
  // Schleife #1: Ist der Messwert wirklich gross anders?
  // Messungenauigkeit nivellieren: Range +- 10% als gleichen Wert betrachten
  // if ((previousReading3 != reading3) && (diff >= 1.1) || (diff <= 0.9)) {
  // Messungenauigkeit nivellieren: Range +- 2% als gleichen Wert betrachten
  //if ((previousReading3 != reading3) && (diff >= 1.02) || (diff <= 0.98)) {
    
  char buf2[6];
  moisture_char_percent = itoa (moisture_val_1, buf2, 10);

  char buf3[6];
  moisture_char_raw = itoa (moisture_val_2, buf3, 10);

    Serial.println();
    // Konsolen Ausgabe MQTT string
    Serial.print("Publish message: ");
    Serial.print("vondefenn/home/IoT/" IOT_HOSTNAME "/moisture_level_percent");
    Serial.print(" [");
    Serial.print(moisture_val_1);
    Serial.print("] %");
    Serial.println();

    Serial.println();
    Serial.print("Publish message: ");
    Serial.print("vondefenn/home/IoT/" IOT_HOSTNAME "/moisture_level_raw");
    Serial.print(" [");
    Serial.print(moisture_val_2);
    Serial.print("] raw value");
    Serial.println();


    if (! moisture_publish_1.publish(moisture_char_raw)) {               //Publish moisture (raw value) to Adafruit
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("Sent!"));
    }
    
    if (! moisture_publish_2.publish(moisture_char_percent)) {               //Publish moisture (in %) to Adafruit
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("Sent!"));
    }

 
  //* Ende Schleife #1: hiernach
  // Abweichungs-Schleife disabled
  //}

  previousReading3 = reading3;

  startDelay = millis();
  // ENDE startDelay Schleife: hiernach
  }
  
  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds

  if (! mqtt.ping()) {
    mqtt.disconnect();
  }
  
   //* END SENSOR SECTION
}
/********************** END Loop ************************************/

/********************** Additional Functions*************************/

void callback_intervall(char *payload, uint16_t length) {
  Serial.print("Message arrived !");
  Serial.print(" Length : ");
  Serial.print(length);
  Serial.print(" Data : ");
  Serial.println(payload);
  
  //String intervall_temp = "";
  String intervall_payload = "";

  intervall_payload = (String)payload;
  
  //intervall_temp = intervall_payload.toInt();

  // jetzt den intervall Wert ersetzen für loop
  interval = intervall_payload.toInt();
  Serial.println("Intervall aktualisiert!"); 
}

void callback_reset(char *payload, uint16_t length) {
  Serial.print("Message arrived !");
  Serial.print(" Length : ");
  Serial.print(length);
  Serial.print(" Data : ");
  Serial.println(payload);
  
  //String intervall_temp = "";
  String reset_payload = "";

  reset_payload = (String)payload;

  if (reset_payload == "1") {
      // restart after 5 seconds
      Serial.println("Resetting Sensor! Rebooting...");
      delay(5000);
      ESP.restart();
    }
  Serial.println("Reset Payload end!"); 
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 50;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      //while (1);

      // new: restart after X retries (don't halt)
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
  }
  Serial.println("MQTT Connected!");
}
