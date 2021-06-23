#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <RCSwitch.h>
#include <WiFiManager.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include "DubRtttl.h"

#define RECEIVER_PIN    D5
#define BUZZER_PIN      D6
#define LED_PIN         D7

#define CODE_DING_DONG  123467

#define MQTT_HOST       "xxx.xxx.xxx.xxx"       // MQTT host (eg m21.cloudmqtt.com)
#define MQTT_PORT       1883                    // MQTT port (18076)
#define MQTT_USER       "KJsdfyUYADSFhjscxv678" // Ingored if brocker allows guest connection
#define MQTT_PASS       "d6823645823746dcfgwed" // Ingored if brocker allows guest connection

#if __has_include("local-constants.h")
#include "local-constants.h"                    // Override some constants if local file exists
#endif

String gDeviceName  = "433receiver";
String gTopic       = "wifi2mqtt/433receiver";

WiFiManager         wifiManager;
WiFiClient          wifiClient;
RCSwitch            mySwitch;
MQTTClient          mqttClient(10000);
DubRtttl            rtttl(BUZZER_PIN);
ESP8266WebServer    webServer(80);

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

        if (doc.containsKey("tone"))
        {
            // play tone HZ   
            tone(BUZZER_PIN, doc["tone"].as<float>(), 2000);
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
    //mySwitch.setReceiveTolerance(200);
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

    // setup webserver 

    webServer.begin();

    String menu;
        menu += "<div>";
        menu += "<a href='/'>index</a> ";
        menu += "<a href='/play'>play</a> ";
        menu += "<a href='/restart'>restart</a> ";
        menu += "<a href='/logout'>logout</a> ";
        menu += "</div><hr>";

    webServer.on("/", [menu](){
        String str = ""; 
        str += menu;
        str += "<pre>";
        str += String() + "           Uptime: " + (millis() / 1000) + " \n";
        str += String() + "      FullVersion: " + ESP.getFullVersion() + " \n";
        str += String() + "      ESP Chip ID: " + ESP.getChipId() + " \n";
        str += String() + "         Hostname: " + WiFi.hostname() + " \n";
        str += String() + "       CpuFreqMHz: " + ESP.getCpuFreqMHz() + " \n";
        str += String() + "      WiFi status: " + wifiClient.status() + " \n";
        str += String() + "         FreeHeap: " + ESP.getFreeHeap() + " \n";
        str += String() + "       SketchSize: " + ESP.getSketchSize() + " \n";
        str += String() + "  FreeSketchSpace: " + ESP.getFreeSketchSpace() + " \n";
        str += String() + "    FlashChipSize: " + ESP.getFlashChipSize() + " \n";
        str += String() + "FlashChipRealSize: " + ESP.getFlashChipRealSize() + " \n";
        str += "</pre>";

        webServer.send(200, "text/html; charset=utf-8", str);     
    });


    // Play melody 
    webServer.on("/play", [menu](){
        if(webServer.method() == HTTP_POST){
            String melody = webServer.arg("melody");

            if(melody.length() > 0){
                rtttl.play(melody);
                webServer.send(200, "text/html", String("Playing melody: ") + melody);        
            }
            else
                webServer.send(400, "text/html", "'melody' GET parameter is required");
        }
        else{
            webServer.send(400, "text/html", menu + "<form method='POST'><textarea name='melody'></textarea><button>play</button></form>");
        }
    });


    // Restart ESP
    webServer.on("/restart", [menu](){
        if(webServer.method() == HTTP_POST){
            webServer.send(200, "text/html", "OK");
            ESP.reset();
        }
        else{
            String output = "";
            output += menu;
            output += String() + "<pre>";
            output += String() + "Uptime: " + (millis() / 1000) + " \n";
            output += String() + "</pre>";
            output += "<form method='post'><button>Restart ESP now!</button></form>";
            webServer.send(400, "text/html", output);
        }
    });


    // Logout (reset wifi settings)
    webServer.on("/logout", [menu](){
        if(webServer.method() == HTTP_POST){
            webServer.send(200, "text/html", "OK");
            wifiManager.resetSettings();
            ESP.reset();
        }
        else{
            String output = "";
            output += menu;
            output += String() + "<pre>";
            output += String() + "Wifi network: " + WiFi.SSID() + " \n";
            output += String() + "        RSSI: " + WiFi.RSSI() + " \n";
            output += String() + "    hostname: " + WiFi.hostname() + " \n";
            output += String() + "</pre>";
            output += "<form method='post'><button>Forget</button></form>";
            webServer.send(200, "text/html", output);
        }
    });


    // Initialize OTA (firmware updates via WiFi)
    ArduinoOTA.begin();

    // Play 'Setup completed' melody
    myTone(1000, 100);
    myTone(500, 100);
    myTone(1500, 100);
}

void handleRadio()
{
    if (mySwitch.available())
    {
        tone(BUZZER_PIN, 800, 50); // beep sound
        tone(LED_PIN, 800, 50);    // flash led
        long receivedValue = mySwitch.getReceivedValue();
        int delay = mySwitch.getReceivedDelay();

        DynamicJsonDocument doc(1024);
        doc["id"] = receivedValue;
        doc["delay"] = delay;

        String json;
        serializeJson(doc, json);

        mqttClient.publish(gTopic, json);

        if(receivedValue == CODE_DING_DONG){
            rtttl.play("Mario:d=4,o=5,b=150:32p,16e6,16e6,16p,16e6,16p,16c6,16e6,16p,16g6,8p,16p,16g");
        }
        
        mySwitch.resetAvailable();
    }

}

void loop()
{
    handleRadio();
    ArduinoOTA.handle();
    rtttl.loop();
    webServer.handleClient();
    mqttClient.loop();

    if (!mqttClient.connected())
    {
        mqtt_connect();
    }
}