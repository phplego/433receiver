#include <Arduino.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <RCSwitch.h>
#include <WiFiManager.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "DubRtttl.h"
#include "Logger.h"

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

String gDeviceName  = String() + "433receiver-" + ESP.getChipId();
String gTopic       = "wifi2mqtt/433receiver";

WiFiManager         wifiManager;
WiFiClient          wifiClient;
RCSwitch            mySwitch;
MQTTClient          mqttClient(2048);
ESP8266WebServer    webServer(80);
DubRtttl            rtttl(BUZZER_PIN);
Logger              logger;

void myTone(int freq, int duration)
{
    tone(BUZZER_PIN, freq, duration);
    delay(duration);
}

void messageReceived(String &topic, String &payload)
{
    logger.log("MQTT msg: " + topic + " " + payload);

    if (topic == gTopic + "/set")
    {
        // parse json
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);
        if (error){
            logger.log("failed to parse json");
            return;
        }

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
    static unsigned long stLastConnectTryTime = 0;

    // retring to connect not too often (every minute)
    if(stLastConnectTryTime && millis() - stLastConnectTryTime < 60000)
        return;

    stLastConnectTryTime = millis();

    logger.log_no_ln("Connecting to MQTT...");
    
    if(mqttClient.connect(gDeviceName.c_str(), MQTT_USER, MQTT_PASS))
    {
        logger.println(" connected!");
        mqttClient.subscribe(gTopic + "/set");
        mqttClient.subscribe(gTopic + "/play");
    }
    else
        logger.println(" failed. Retry in 60 sec..");
}


void setup()
{
    logger.log("Setup begin");

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

    wifi_set_sleep_type(NONE_SLEEP_T); // prevent wifi sleep (stronger connection)

    // On Access Point started (not called if wifi is configured)
    wifiManager.setAPCallback([](WiFiManager *mgr)
                              {
                                  logger.log(String("Please connect to Wi-Fi"));
                                  logger.log(String("Network: ") + mgr->getConfigPortalSSID());
                                  logger.log(String("Password: 12341234"));
                                  logger.log(String("Then go to ip: 10.0.1.1"));
                              });

    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.autoConnect(gDeviceName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Restart if not connected
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP.restart();
    }

    String hostname = String() + "esp-" + gDeviceName;
    hostname.replace('.', '_'); 
    WiFi.hostname(hostname);

    mqttClient.begin(MQTT_HOST, MQTT_PORT, wifiClient);
    mqttClient.onMessage(messageReceived);

    mqtt_connect();

    bool ok = mqttClient.publish(gTopic, "started");
    logger.log(ok ? "Status successfully published to MQTT" : "Cannot publish status to MQTT");

    if(!LittleFS.begin()){
        logger.log("format LittleFS...");
        LittleFS.format();
        LittleFS.begin();
    }

    // setup webserver 
    webServer.begin();

    //webServer.serveStatic("/logs.js", LittleFS, "/logs.js");
    
    
    String menu;
        menu += "<div>";
        menu += "<a href='/'>index</a> ";
        menu += "<a href='/stop-list'>stop-list</a> ";
        menu += "<a href='/play'>play</a> ";
        menu += "<a href='/restart'>restart</a> ";
        menu += "<a href='/logs'>logs</a> ";
        menu += "<a href='/fs'>FS</a> ";
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

    // Stop list
    webServer.on("/stop-list", [menu](){
        String output = menu;
        if(webServer.method() == HTTP_POST){
            String list = webServer.arg("list");
            File f = LittleFS.open("stop-list.txt", "w");

            if(f && f.print(list) == list.length()){
                output += "Saved OK.<br>";
                f.close();
            }
            else{
                output += "Failed to save.<br>";
            }
        }
        output += "New line separated values to be ignored:<br>";
        output += "<form method='POST'><textarea name='list' rows=10>";
        File f = LittleFS.open("stop-list.txt", "r");
        output += f.readString();
        f.close();
        output += "</textarea><br><button>save</button></form>";
        webServer.send(200, "text/html", output);
    });


    // Play melody 
    webServer.on("/play", [menu](){
        String output = "";
        output += menu;
        String melody = "Intel:d=16,o=5,b=320:d,p,d,p,d,p,g,p,g,p,g,p,d,p,d,p,d,p,a,p,a,p,a,2p";
        if(webServer.method() == HTTP_POST){
            melody = webServer.arg("melody");
            rtttl.play(melody);
            output += "Playing melody: " + melody;
        }
        output += "<form method='POST'><textarea name='melody' rows=10>"+melody+"</textarea><br><button>play</button></form>";
        webServer.send(200, "text/html", output);
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

    // Show logs page
    webServer.on("/logs", [menu](){
        String output = "";
        String output2 = "";
        output += menu;
        output += String() + "millis: <span id='millis'>"+millis()+"</span>";
        output += String() + "<br>size="+strlen(logger.buffer);
        output += String() + "<pre id='text'>";
        //output += String() + logger.buffer;
        output2 += String() + "</pre>\n";
        output2 += String() + "<script>\n";
        output2 += String() + "const millis = " + millis()+"\n";
        output2 += String() + "const replaceFunc = x => {\n";
        output2 += String() +    "const deltaMillis = millis - parseInt(x.replace('.', ''))\n";        
        output2 += String() +    "return new Date(Date.now() - deltaMillis).toLocaleString('RU')\n";
        output2 += String() + "}\n";
        output2 += String() + "document.getElementById('text').innerHTML = document.getElementById('text').innerHTML.replaceAll(/^\\d{3,}\\.\\d{3}/mg, replaceFunc)\n";
        output2 += String() + "</script>";
        webServer.setContentLength(output.length() + strlen(logger.buffer) + output2.length());
        webServer.send(200, "text/html", output);
        webServer.sendContent(String() + logger.buffer);
        webServer.sendContent(output2);
    });
    
    // Show filesystem list of files
    webServer.on("/fs", [menu](){
        String output = "";
        output += menu;
        output += String() + "<pre>";
        Dir dir = LittleFS.openDir("");
        while (dir.next()) {
            output += String() + dir.fileSize() + "B " + dir.fileName() + "\n";
        }

        output += String() + "</pre>";
        webServer.send(400, "text/html", output);
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


const char* wlStatusToString(wl_status_t status) 
{
    switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_WRONG_PASSWORD: return "WL_WRONG_PASSWORD";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
        default: return "unknown wl code";
    }
}


bool isInStopList(long receivedValue)
{
    File f = LittleFS.open("stop-list.txt", "r");
    if(!f) return false;
    f.setTimeout(0); // to prevent delays
    while(f.available()){
        String line = f.readStringUntil('\n');
        long code = line.toInt();
        //logger.log(String() + "code: " + code);

        if(receivedValue == code) {
            f.close();
            return true;
        }
    }
    f.close();
    return false;
}


void handleRadio()
{
    static int lastReceivedValue = 0;
    static unsigned long lastMessageTime = 0;
    const unsigned long MIN_INTERVAL = 1000; // ms; messages with the same value received with less than this interval will be ignored

    if (mySwitch.available())
    {
        long receivedValue = mySwitch.getReceivedValue();
        int delay = mySwitch.getReceivedDelay();

        logger.log_no_ln(String()+"Value " + receivedValue + 
            " p: "+mySwitch.getReceivedProtocol() + 
            " d: " + mySwitch.getReceivedDelay() + 
            " l: " + mySwitch.getReceivedBitlength());


        if(isInStopList(receivedValue))
        {
            logger.println(String() + " is in the stop list! Skiping it");
            mySwitch.resetAvailable();
            return;
        }
        else if (receivedValue < 1000)
        {
            logger.println(String() + " is too small! Skiping it");
            mySwitch.resetAvailable();
            return;
        }
        logger.ln();

        // messages with the same value received with less than MIN_INTERVAL will be ignored
        if(receivedValue == lastReceivedValue && millis() < lastMessageTime + MIN_INTERVAL){
            logger.println(String() + "Skip duplicate");
            mySwitch.resetAvailable();
            return; 
        }

        lastReceivedValue = receivedValue;
        lastMessageTime = millis();



        tone(BUZZER_PIN, 800, 50); // beep sound
        tone(LED_PIN, 800, 50);    // flash led


        DynamicJsonDocument doc(1024);
        doc["id"] = receivedValue;
        doc["delay"] = delay;
        doc["proto"] = mySwitch.getReceivedProtocol();
        doc["len"] = mySwitch.getReceivedBitlength();


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

    static int stLastWifiStatus = 0;
    if(stLastWifiStatus != WiFi.status()){
        logger.log(String() + "WiFi goes to status " + wlStatusToString(WiFi.status()));
        stLastWifiStatus = WiFi.status();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!mqttClient.connected())
        {
            mqtt_connect();
        }    
    }
}