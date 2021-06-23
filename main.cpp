#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <RCSwitch.h>
#include <WiFiManager.h>
#include <MQTT.h>
#include <ArduinoJson.h>

#include "DubRtttl.h"

#define RECEIVER_PIN D5
#define BUZZER_PIN D6
#define LED_PIN D7

#define MQTT_HOST "192.168.1.157"         // MQTT host (m21.cloudmqtt.com)
#define MQTT_PORT 11883                   // MQTT port (18076)
#define MQTT_USER "KJsdfyUYADSFhjscxv678" // Ingored if brocker allows guest connection
#define MQTT_PASS "d6823645823746dcfgwed" // Ingored if brocker allows guest connection

String gDeviceName  = "433receiver";
String gTopic       = "wifi2mqtt/433receiver";

WiFiManager wifiManager;
WiFiClient wifiClient;
RCSwitch mySwitch = RCSwitch();
MQTTClient mqttClient(10000);
DubRtttl rtttl(BUZZER_PIN);

void myTone(int freq, int duration)
{
    tone(BUZZER_PIN, freq, duration);
    delay(duration);
}

void messageReceived(String &topic, String &payload)
{
    Serial.println("incoming: " + topic + " - " + payload);

    if (topic == gTopic + "/set")
    {
        // parse json
        DynamicJsonDocument doc(10000);
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
            return;

        if (doc.containsKey("melody"))
        {
            // play 'melody' json key 
            rtttl.play(doc["melody"]);
        }
    }

    if (topic == gTopic + "/play")
    {
        // play plain text
        rtttl.play(payload);
    }
}

void mqtt_connect()
{
    Serial.print("checking wifi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }

    Serial.print("\nconnecting...");
    while (!mqttClient.connect(gDeviceName.c_str(), MQTT_USER, MQTT_PASS))
    {
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nconnected!");

    mqttClient.subscribe(gTopic + "/set");
    mqttClient.subscribe(gTopic + "/play");
}

void setup()
{
    Serial.begin(74880);
    //pinMode(D3, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    mySwitch.enableReceive(RECEIVER_PIN); // Receiver on interrupt 0 => that is pin #2

    // Play 'start' melody
    myTone(800, 100);
    myTone(400, 100);
    myTone(1200, 100);

    WiFi.hostname(gDeviceName);
    WiFi.mode(WIFI_STA); // no access point after connect

    wifi_set_sleep_type(NONE_SLEEP_T); // prevent wifi sleep (stronger connection)

    // On Access Point started (not called if wifi is configured)
    wifiManager.setAPCallback([](WiFiManager *mgr)
                              {
                                  Serial.println(String("Please connect to Wi-Fi"));
                                  Serial.println(String("Network: ") + mgr->getConfigPortalSSID());
                                  Serial.println(String("Password: 12341234"));
                                  Serial.println(String("Then go to ip: 10.0.1.1"));
                              });

    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.autoConnect(gDeviceName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Restart if not connected
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP.restart();
    }

    mqttClient.begin(MQTT_HOST, MQTT_PORT, wifiClient);
    mqttClient.onMessage(messageReceived);

    mqtt_connect();

    bool ok = mqttClient.publish(gTopic, "started");
    Serial.println(ok ? "Published: OK" : "Published: ERR");

    // Initialize OTA (firmware updates via WiFi)
    ArduinoOTA.begin();

    // Play 'Setup completed' melody
    myTone(1000, 100);
    myTone(500, 100);
    myTone(1500, 100);
}

void loop()
{
    if (mySwitch.available())
    {
        tone(BUZZER_PIN, 800, 50); // beep sound
        tone(LED_PIN, 800, 50);    // flash led
        long receivedValue = mySwitch.getReceivedValue();
        int delay = mySwitch.getReceivedDelay();

        //Serial.print("Value: ");
        // Serial.print(receivedValue);
        // Serial.print(" bit length: ");
        // Serial.print(mySwitch.getReceivedBitlength());
        // Serial.print(" Delay: ");
        // Serial.print(mySwitch.getReceivedDelay());
        // Serial.print("RawData: ");
        // Serial.println(mySwitch.getReceivedRawdata());
        // Serial.print(" Protocol: ");
        // Serial.println(mySwitch.getReceivedProtocol());

        mySwitch.resetAvailable();

        DynamicJsonDocument doc(1024);

        doc["id"] = receivedValue;
        doc["delay"] = delay;

        String json;
        serializeJson(doc, json);

        mqttClient.publish(gTopic, json);
    }

    ArduinoOTA.handle();
    mqttClient.loop();
    rtttl.loop();

    if (!mqttClient.connected())
    {
        mqtt_connect();
    }
}