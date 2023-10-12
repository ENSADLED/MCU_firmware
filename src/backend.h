// OSC
#include <WiFiUdp.h>
#include <ArduinoOSCWiFi.h>

#define MODE_ARTNET 0
#define MODE_WHITE 1
#define MODE_BLACK 2
#define MODE_IDLE 3

WiFiUDP udp;

IPAddress broadcastAddress;
#define OSC_OUT_PORT 2107
#define OSC_IN_PORT  2108
unsigned long next_heartbeat = 0;

void setupOTA(){
    ArduinoOTA.setHostname(host);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA
    .onStart([]() {
    })
    .onEnd([]() {
    })
    .onProgress([](unsigned int progress, unsigned int total) {
    })
    .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
}

void setupWiFi(){

  long long now = millis();
  WiFi.begin(SSID_FOULE, PSK_FOULE);

  while(WiFi.status() != WL_CONNECTED && millis() < WIFI_TIMEOUT_FOULE + now){
    vTaskDelay(LOOP_INTERVAL);
  }

  if(WiFi.status() == WL_CONNECTED){
    delay(10);
    broadcastAddress = (WiFi.localIP() | ~WiFi.subnetMask());
    return;
  }

  WiFi.disconnect();
  WiFi.begin(SSID_ENSAD, PSK_ENSAD);
  now = millis();

  while(WiFi.status() != WL_CONNECTED && millis() < WIFI_TIMEOUT_ENSAD + now){
    vTaskDelay(LOOP_INTERVAL);
  }

  if(WiFi.status() == WL_CONNECTED){
    delay(10);
    broadcastAddress = (WiFi.localIP() | ~WiFi.subnetMask());
    return;
  }

  ESP.restart();
}

void send_heartbeat(){
    String status = String(WiFi.RSSI());
   /* StaticJsonDocument<192> doc;
    doc["ua"] = univ_a;
    doc["ub"] = univ_b;
    doc["uc"] = univ_c;
    doc["aa"] = addr_a;
    doc["ab"] = addr_b;
    doc["ac"] = addr_c;
    doc["cmr"] = colormult_r;
    doc["cmg"] = colormult_g;
    doc["cmb"] = colormult_b;
    doc["bn"] = brightness;
    doc["md"] = mode;
    doc["wf"] = WiFi.RSSI();

    serializeJson(doc, output);*/
    OscWiFi.send(broadcastAddress.toString(), OSC_OUT_PORT, "/heartbeat", String(host), WiFi.localIP().toString(), String(VERSION), status);
}

void handle_osc(){

  if(millis() > next_heartbeat){
    next_heartbeat = millis() + random(HEARTBEAT_LOW, HEARTBEAT_HIGH);
    send_heartbeat();
  }

  OscWiFi.update();
}
