#define ASO_VERSION "0.2.4"
// 0001, 0011, 0014

#include <stdbool.h>

// For the AP
#include <wdt_samd21.h>
#include <FlashStorage.h>
#include "HttpRequest.h"
#include "StaticContent.h"
#include "StringUtil.h"
#include "IOTConfig.h"


#define WIFI_MODE_AP 1
#define WIFI_MODE_CLIENT 2
// TODO move param handling to callback to save memory in HTTPRequest class
void callback(int what, char *key, char *value)
{
}

// defines stuff for persistent config storage
FlashStorage(flash_store, ConfigData);
HttpRequest httpReq;
Config conf = Config();


// Including local stuff
#include "SLM.h"
#include "SerialDummy.h"
#include "PString.h"

// Wifi thingies needed
#include <SPI.h>
#include <WiFiNINA.h>
#include "WiFiUtil.h"           // needs to be here because of dependancies
static int wifiConnectTry = 0;  // Count connection attempts

// Udp and NTP stuff
#include <NTPClient.h>
const long utcOffsetWinter = 3600;      // Offset from UTC in seconds (3600 seconds = 1h) -- UTC+1 (Central European Winter Time)
const long utcOffsetSummer = 7200;      // Offset from UTC in seconds (7200 seconds = 2h) -- UTC+2 (Central European Summer Time)
unsigned long lastupdate = 0UL;
WiFiUDP udpSocket;
NTPClient ntpClient(udpSocket, "pool.ntp.org", utcOffsetWinter);


// MQTT
#include <ArduinoMqttClient.h>
static int mqttConnectTry = 0;  // Count connection attempts

#define JUMPER_OUT_PIN 12
#define JUMPER_IN_PIN 11

#define AP_NAME "ASOKit"

#ifdef DISABLE_SERIAL
#define Serial dummy
#endif

uint32_t send_interval_secs = 1;        // send interval
static bool in_setup = false;   // flag to check if in setup


// create objects
// Sound level meter
SLM slm(Serial);
WiFiServer server(80);
//WiFiClient client;
WiFiSSLClient client;           // moved to TLS, so traffic over internet is ok


/*
void callback(int what, char *key, char *value) {
}
*/


void blinkTimes(int t)
{
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    for (int i = 0; i < t; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
    }
}

MqttClient mqttClient(client);  // depends on wificlient and nicely moves to TLS

int status = WL_IDLE_STATUS;
int statusWiFi = WL_IDLE_STATUS;

byte WIFI_MODE = WIFI_MODE_CLIENT;


void setupwifi()
{

    wifiConnectTry = 0;

    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_MODULE) {
//    Serial.println(F("WiFi shield not present"));
        // don't continue:
        while (true);
    }

    // attempt to connect to WiFi network:

    while (status != WL_CONNECTED) {
//    Serial.print(F("Attempting to connect to WPA SSID: "));
//    Serial.println(conf.data.SSID);

        blinkTimes(10);
        status = WiFi.begin(conf.data.SSID, conf.data.PASS);
        wdt_reset();
        wifiConnectTry++;

        if (wifiConnectTry > 3) {
            if (in_setup) {
//        Serial.println(F("Wifi connect in setup mode taking to long, moving to AP mode"));
                WIFI_MODE = WIFI_MODE_AP;
                break;
            } else {
//        Serial.println(F("Wifi connect in run mode taking to long, restarting controller"));
                NVIC_SystemReset();
            }
        }

        // wait 3 seconds for connection:
        delay(5000);
    }

    if (WL_CONNECTED == status) {
        // you're connected now, so print out the data:
        WIFI_MODE = WIFI_MODE_CLIENT;
//    Serial.println(F("You're connected to the network"));
        //printCurrentNet();
        //printWiFiData();
//    printWiFiStatus();
        WiFi.noLowPowerMode();
        delay(5000);            //Needed for wifi to finish starting
    }
}

void setuptime()
{

    // Setup NTP

//  Serial.println(F("Starting NTP"));
    ntpClient.begin();
    delay(3000);
    ntpClient.update();
    ntpClient.setUpdateInterval(600000);        // update every 10 min, default is every minute


    /*
       Serial.print(ntpClient.getHours());
       Serial.print(":");
       Serial.print(ntpClient.getMinutes());
       Serial.print(":");
       Serial.println(ntpClient.getSeconds());
     */
//  Serial.println(ntpClient.getFormattedTime());
}

void setupMQTT()
{
/*
    -4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
    -3 : MQTT_CONNECTION_LOST - the network connection was broken
    -2 : MQTT_CONNECT_FAILED - the network connection failed
    -1 : MQTT_DISCONNECTED - the client is disconnected cleanly
    0 : MQTT_CONNECTED - the client is connected
    1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
    2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
    3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
    4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
    5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
*/


    mqttConnectTry = 0;
    mqttClient.stop();
    char buffer10[7 + 5];
    char buffer11[50];

    PString mqttclientid(buffer10, sizeof(buffer10));
    PString restartinfo(buffer11, sizeof(buffer11));

    mqttclientid = "sensor-";
    mqttclientid += conf.data.DEVID;
    mqttclientid += "\n";

//  Serial.print(F("Attempting MQTT re-connect to broker: "));
//  Serial.println(conf.data.MQTT_IP);
    mqttClient.setUsernamePassword(conf.data.MQTT_USER, conf.data.MQTT_PASS);
    mqttClient.setId(mqttclientid);

    // Loop until we're reconnected
    wdt_reset();
    while (!mqttClient.connected()) {
        //Serial.print(F("step 0"));
        // Attempt to connect
        if (mqttClient.connect(conf.data.MQTT_IP, atoi(conf.data.MQTT_PORT))) {
            //Serial.print(F("step 10"));
//      Serial.println(F("connected"));
            if (in_setup) {
//        Serial.println(F("Connection established in setup, so restarted, sending out restart info."));
                // assemble message
                restartinfo = "sensor-";
                restartinfo += conf.data.DEVID;
                restartinfo += " version: ";
                restartinfo += ASO_VERSION;
                restartinfo += " restarted at: ";
                restartinfo += ntpClient.getFormattedTime();
                restartinfo += "\n";
                // send message
                mqttClient.poll();
                mqttClient.beginMessage("sensor-mgmt");
                mqttClient.print(buffer11);
                mqttClient.endMessage();
            }

        } else {
//      Serial.print(F("step 20"));

            mqttConnectTry++;
//      Serial.print(ntpClient.getFormattedTime());
//      Serial.print(F(": failed, rc="));
//      Serial.print(mqttClient.connectError());
//      Serial.print(F(": attempt="));
//      Serial.print(mqttConnectTry);
//      Serial.println(F(", will try again in 5 seconds"));
            // Wait 5 seconds before retrying
            delay(5000);
        }
        if (mqttConnectTry > 5) {
//      Serial.println(F("Taking to long, restarting"));
            NVIC_SystemReset();
        }
    }
}

void startAP()
{
//    Serial.print(F("Creating access point named: "));
//    Serial.println(AP_NAME);

    statusWiFi = WiFi.beginAP(AP_NAME, 11);
    if (statusWiFi != WL_AP_LISTENING) {
//      Serial.println(F("Creating access point failed"));
        while (true);
    }
    // LED on
    digitalWrite(LED_BUILTIN, HIGH);
//    Serial.println(F("Access Point Web Server starting"));

    // Wait 10 seconds for connection:
    delay(5000);

    // Start the web server on port 80
    server.begin();

    // Print status
//    printWiFiStatus();
}

void setup()
{

    // Serial
#ifndef DISABLE_SERIAL
    //Initialize serial and wait for port to open:
    Serial.begin(115200);
    while (!Serial && millis() < 5000);

    Serial.println(F("Starting"));
#endif

    httpReq.setCallback(callback);

    in_setup = true;            // we are in setup

    // LED off
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    //httpReq.setCallback(callback);

    // Read or init config
    conf.data = flash_store.read();
    if (!conf.inited()) {
        Serial.println(F("No config!"));
        conf.init();
        for (int i = 0; i < conf.num(); i++) {
            Serial.print(conf.names[i]);
            Serial.print(" : ");
            Serial.println(conf.values(i));
        }
        flash_store.write(conf.data);
        WIFI_MODE = WIFI_MODE_AP;
    } else {
        Serial.println(F("Current config"));
        for (int i = 0; i < conf.num(); i++) {
            Serial.print(conf.names[i]);
            Serial.print(" : ");
            Serial.println(conf.values(i));
        }
    }

    send_interval_secs = atoi(conf.data.SEND_SECS);

    blinkTimes(5);

    // Create open network WPA
    // Only happens when 1) in setup and no config was found or 2) in setup stage, config does exist and wifi connect takes to long
    if (WIFI_MODE == WIFI_MODE_AP) {
        startAP();
    } else {

        // Start wifi, ntp, mqtt and finally sound measurement

        setupwifi();
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            WiFi.end();
            WIFI_MODE = WIFI_MODE_AP;
            startAP();
        } else {
            WIFI_MODE = WIFI_MODE_CLIENT;
            setuptime();
            setupMQTT();
        }

//    Serial.println(F("Setup SLM"));
        slm.setup();

//    Serial.println(F("Starting SLM"));
        slm.start();
    }

    in_setup = false;           // setup is done (succesfully)
}

static long secs = 0;
void loop()
{

    // In AP mode we do nothing else
    if (WIFI_MODE == WIFI_MODE_AP) {
        //Serial.println(F("in loopAP"));
        // Only happens when 1) in setup and no config was found or 2) in setup stage, config does exist and wifi connect takes to long

        // Now if AP modes takes to long (> 3 min), restart and try setup again
        if (millis() < 180000) {
            loopAP();
            return;
        } else {
            //Serial.println(setup stage in AP takes longer the 5 min trying restart);
            NVIC_SystemReset();
        }
    }

    //Serial.println(F("in slm.update"));

    slm.update();

    //Every second
    long n_secs = millis() / 1000;

    if (secs != n_secs) {
        secs = n_secs;
        everySecond(secs);
    }
}


void everySecond(long current)
{

    //Serial.println(F("in everySecond"));
    //Serial.println(send_interval_secs);

    if ((current % send_interval_secs) == 0) {
        //Serial.println(F("pause "));
        slm.pause();
        //Serial.println(F("send "));
        sendDataMQTT();
        //Serial.println(F("reset "));
        slm.reset();
        //Serial.println(F("resume "));
        slm.resume();
    }
}

void sendDataMQTT()
{

    double leq = 120 + db_full_scale_pow(slm.statsRMS.Mean());

    char buffer1[200 + 1];
    char buffer2[8 + 1];
    char buffer3[8 + 1];
    char buffer4[8 + 1];
    char buffer5[8 + 1];
    char buffer6[8 + 1];
    char buffer7[12 + 1];
    char buffer8[12 + 1];

    // print all metrics into fixed buffers using pstring
    PString Pmetrics(buffer1, sizeof(buffer1));
    PString dbmin(buffer2, sizeof(buffer2), slm.statsDB.Min());
    PString dbmean(buffer3, sizeof(buffer3), slm.statsDB.Mean());
    PString dbmax(buffer4, sizeof(buffer4), slm.statsDB.Max());
    PString dbdev(buffer5, sizeof(buffer5), slm.statsDB.StdDev(true));
    PString dbleq(buffer6, sizeof(buffer6), leq);
    PString lat(buffer7, sizeof(buffer7), "52.1316566");
    PString lon(buffer8, sizeof(buffer8), "4.7602249");


    // compose POST body by concatenating all pieces
    // example
    /*
       min,id=0013 value=51.41
       mean,id=0013 value=52.32
       leq,id=0013 value=52.28
       max,id=0013 value=53.09
       stddev,id=0013 value=0.45
     */
    // should be
    // 1) sound,id=0013,lat=52.1316566,lon=4.7602249 min=51.41,mean=52.32,leq=52.28,max=53.09,stddev=0.45
    // 2) 0013,id=0013,lat=52.1316566,lon=4.7602249 min=51.41,mean=52.32,leq=52.28,max=53.09,stddev=0.45
    // 3) 0013,id=0013 lat=52.1316566,lon=4.7602249,min=51.41,mean=52.32,leq=52.28,max=53.09,stddev=0.45

// modus=1
//  Pmetrics = conf.data.DEVID;
    Pmetrics = "veldtest";
    Pmetrics += ",id=";
    Pmetrics += conf.data.DEVID;
//  Pmetrics += ",lat=";
//  Pmetrics += lat;
//  Pmetrics += ",lon=";
//  Pmetrics += lon;
    Pmetrics += "  min=";
    Pmetrics += dbmin;
    Pmetrics += ",mean=";
    Pmetrics += dbmean;
    Pmetrics += ",leq=";
    Pmetrics += dbleq;
    Pmetrics += ",max=";
    Pmetrics += dbmax;
    Pmetrics += ",stddev=";
    Pmetrics += dbdev;
    Pmetrics += "\n";

    //Serial.println(Pmetrics);

    wdt_init(WDT_CONFIG_PER_8K);
    digitalWrite(LED_BUILTIN, HIGH);

    if (!mqttClient.connected()) {
//    Serial.println(F("MQTT connection broken, Restarting wifi and MQTT"));
        WiFi.disconnect();
        WiFi.end();
        status = WL_IDLE_STATUS;
        setupwifi();
        wdt_reset();
        setupMQTT();
    } else {
        mqttClient.poll();
        mqttClient.beginMessage(conf.data.MQTT_TOPIC);
        mqttClient.print(buffer1);
        mqttClient.endMessage();
    }
    digitalWrite(LED_BUILTIN, LOW);
    wdt_disable();
}

// Handle incoming requests
void loopAP()
{

    //Serial.println(F("in 0"));
    if (statusWiFi != WiFi.status()) {
        //Serial.println(F("in 0a"));
        statusWiFi = WiFi.status();
        if (statusWiFi == WL_AP_CONNECTED) {
            //Serial.println(F("in 0b"));
            byte remoteMac[6];
//      Serial.print(F("Device connected to AP, MAC address: "));
//      WiFi.macAddress(remoteMac);
//      printMacAddress(remoteMac);
//    } else {
//      Serial.println(F("Device disconnected from AP"));
        }
    }
    //digitalWrite(LED_BUILTIN, HIGH);
    //delay(50);
    // Handle incoming data from clients
    WiFiClient client = server.available();
    char name[HTTP_REQ_PARAM_NAME_LENGTH], value[HTTP_REQ_PARAM_VALUE_LENGTH];
    if (client) {
//    Serial.println(F("new client"));
        // stream incoming chars to parser
        while (client.connected()) {
            //delayMicroseconds(10);
            //delay(50);
            //Serial.println(F("in client.connected"));
            if (client.available()) {
                //Serial.println(F("in client.available"));
                char c = client.read();
//        Serial.write(c); // echo everything coming in to serial (debug)
                httpReq.parseRequest(c);
                if (httpReq.endOfRequest()) {
                    if (String(httpReq.method) == "GET" && String(httpReq.uri) == "/") {
                        //Serial.println(F("in 1"));
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:text/html; charset=iso-8859-1"));
                        client.println(F("Connection: close"));
                        client.println(PAGE_CONFIG);
                    } else if (String(httpReq.method) == "GET"
                               && String(httpReq.uri) == "/asokit.svg") {
                        //Serial.println(F("in 2"));
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:image/svg+xml"));
                        client.println(F("Connection: close"));
                        client.println(IMAGE_SVG);
                    } else if (String(httpReq.method) == "GET"
                               && String(httpReq.uri) == "/script.js") {
                        //Serial.println(F("in 3"));
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:text/javascript; charset=iso-8859-1"));
                        client.println(F("Connection: close"));
                        client.println(SCRIPT_JS);
                    } else if (String(httpReq.method) == "GET"
                               && String(httpReq.uri) == "/icon.gif") {
                        //Serial.println(F("in 4"));
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:image/gif"));
                        client.println(F("Connection: close"));
                        client.println();
                        client.write(FAVICON, sizeof(FAVICON));
                    } else if (String(httpReq.method) == "GET" && String(httpReq.uri) == "/values") {
                        //Serial.println(F("in 5"));
                        client.println(F("HTTP/1.1 200 OK"));
                        client.println(F("Content-type:text/html"));
                        client.println(F("Connection: close"));
                        client.println();
                        // build JSON
                        client.print(F("{"));
                        for (int i = 0; i < conf.num(); i++) {
                            if (i > 0)
                                client.print(F(","));
                            client.print(F("\""));
                            client.print(escapeJsonString(conf.names[i]));
                            client.print(F("\""));
                            client.print(" : ");
                            client.print(F("\""));
                            client.print(escapeJsonString(conf.values(i)));
                            client.print(F("\""));
                        }
                        client.print(F("}"));
                    } else if (String(httpReq.method) == "POST"
                               && String(httpReq.uri) == F("/values")) {
                        //Serial.println(F("in 6"));
                        for (int i = 1; i <= httpReq.paramCount; i++) {
                            httpReq.getParam(i, name, value);
                            String nameDecoded = urldecode(String(name));
                            String valueDecoded = urldecode(String(value));
                            int index = conf.indexOf(nameDecoded.c_str());
                            if (index != -1) {
                                conf.set(index, valueDecoded.c_str());
                            }
                        }
                        flash_store.write(conf.data);
                        client.println(F("HTTP/1.1 204 OK"));
                        client.println(F("Connnection: close"));
                        //             Serial.println(F("Restarting sensor with new config"));
                        NVIC_SystemReset();
                    }
                    httpReq.resetRequest();
                    break;
                }
            }
        }
        // Close the connection:
        client.stop();
//    Serial.println(F("client disconnected"));
    }
}
