#define WIFI_TIMEOUT_FOULE 10000
#define WIFI_TIMEOUT_ENSAD 30000

#define HEARTBEAT_LOW 1000
#define HEARTBEAT_HIGH 5000

#define SSID_FOULE "FOULE_LED"
#define PSK_FOULE  "Bijahouix"

#define SSID_ENSAD "ENSAD-LED"
#define PSK_ENSAD  "Lmdpdeensadled"

#define DEFAULT_UNIVERSE 1
#define START_ADDR 1

#include <Arduino.h>
#include <Preferences.h>
Preferences preferences;

#define BAUDRATE 115200
#define LOOP_INTERVAL 10  // global loop interval in ticks

// Network stack
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#define OTA_PASS "Bijahouix"

char host[22];

// App
#include <ArtnetWifi.h>
ArtnetWiFiReceiver artnet;

#define FAST_BLINK (millis() % 200 < 50)
#define HEARTBEAT ((millis() + 1000 )% 2000 < 50)

uint16_t frame_count = 0;

uint16_t univ_a = 1;
uint16_t univ_b = 1;
uint16_t univ_c = 1;

uint16_t addr_a = 0;
uint16_t addr_b = 0;
uint16_t addr_c = 0;

uint32_t colormult_r = 65535;
uint32_t colormult_g = 65535;
uint32_t colormult_b = 65535;

uint32_t brightness = 32768;

#include "backend.h"
#include "led_helper.h"

bool running = true;

void testMode(){

	for (int i = 0; i < NUM_CHANNEL; ++i)
	{
		ledcWrite(i, gamma28_8b_16b[(millis()/10)%200]);
	}
}

// front end loop
void loop_metapixel(void * _){


    ledcAttachPin(LED_BUILTIN, 9);
    ledcSetup(9, PWM_FREQ, PWM_RES_BITS);

	delay(10);

	while(running){
		if(digitalRead(PIN_BTN)){
			artnet.parse();
		}else{
			testMode();
		}
    vTaskDelay(1);
	}
    vTaskDelete(NULL);
}


void on_artnet(uint8_t pixel, const uint8_t* data, const uint16_t size){
    uint16_t starts[3] =  {addr_a, addr_b, addr_c};
    for (int i = 0; i < 3; ++i)
    {
        ledcWrite((pixel*3)+i, gamma28_8b_16b[data[starts[pixel]+i]]);
    }

    ledcWrite(9, gamma28_8b_16b[frame_count*10]);
    frame_count ++;
    frame_count = frame_count % 25;
}


void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
	pinMode(PIN_BTN, INPUT);

	// build and set hostname
	uint64_t mac = ESP.getEfuseMac();
    uint64_t reversed_mac = 0;

    for (int i = 0; i < 6; i++) {
        reversed_mac |= ((mac >> (8 * i)) & 0xFF) << (8 * (5 - i));
    }

    snprintf(host, 22, "MPX-%llX", reversed_mac);

    setupWiFi();
    setupOTA();
    setupLED();

    univ_a = preferences.getUInt("univ_a", univ_a);
    univ_b = preferences.getUInt("univ_b", univ_b);
    univ_c = preferences.getUInt("univ_c", univ_c);

    addr_a = preferences.getUInt("addr_a", addr_a);
    addr_b = preferences.getUInt("addr_b", addr_b);
    addr_c = preferences.getUInt("addr_c", addr_c);

    colormult_r = (uint32_t)preferences.getUInt("colormult_r", colormult_r);
    colormult_g = (uint32_t)preferences.getUInt("colormult_g", colormult_g);
    colormult_b = (uint32_t)preferences.getUInt("colormult_b", colormult_b);

    brightness = (uint32_t)preferences.getUInt("brightness", brightness);

    artnet.begin();

    artnet.subscribe(univ_a,
        [&](const uint8_t* data, const uint16_t size) {on_artnet(0, data, size);});
    artnet.subscribe(univ_b,
        [&](const uint8_t* data, const uint16_t size) {on_artnet(1, data, size);});
    artnet.subscribe(univ_c,
        [&](const uint8_t* data, const uint16_t size) {on_artnet(2, data, size);});

    OscWiFi.subscribe(OSC_IN_PORT, "/addresses", addr_a, addr_b, addr_c);
    OscWiFi.subscribe(OSC_IN_PORT, "/calibration", colormult_r, colormult_g, colormult_b);
    OscWiFi.subscribe(OSC_IN_PORT, "/brightness", brightness);
    
    OscWiFi.subscribe(OSC_IN_PORT, "/universes",
        [&](
            const int& _univ_a,
            const int& _univ_b,
            const int& _univ_c
            ) {

            univ_a = _univ_a;
            univ_b = _univ_b;
            univ_c = _univ_c;

            artnet.unsubscribe();

            artnet.subscribe(univ_a,
                [&](const uint8_t* data, const uint16_t size) {on_artnet(0, data, size);});
            artnet.subscribe(univ_b,
                [&](const uint8_t* data, const uint16_t size) {on_artnet(1, data, size);});
            artnet.subscribe(univ_c,
                [&](const uint8_t* data, const uint16_t size) {on_artnet(2, data, size);});

        });

    OscWiFi.subscribe(OSC_IN_PORT, "/save", [](){
        preferences.putUInt("univ_a", univ_a);
        preferences.putUInt("univ_b", univ_b);
        preferences.putUInt("univ_c", univ_c);

        preferences.putUInt("addr_a", addr_a);
        preferences.putUInt("addr_b", addr_b);
        preferences.putUInt("addr_c", addr_c);

        preferences.putUInt("colormult_r", (uint16_t)colormult_r);
        preferences.putUInt("colormult_g", (uint16_t)colormult_g);
        preferences.putUInt("colormult_b", (uint16_t)colormult_b);

        preferences.putUInt("brightness", (uint16_t)brightness);
    });

    xTaskCreatePinnedToCore(
        loop_metapixel,     // Function that should be called
        "Metapixel",    // Name of the task (for debugging)
        20000,           // Stack size (bytes)
        NULL,            // Parameter to pass
        1,               // Task priority
        NULL,            // Task handle
        !xPortGetCoreID()// pin to not the one running this
    );

    delay(10);

    digitalWrite(LED_BUILTIN, LOW);

    delay(100);
    Serial.begin(BAUDRATE);
    delay(400);
    Serial.println(host);
    Serial.println(broadcastAddress);
}

// backend loop
void loop(){
  // wifi
  if(WiFi.status() != WL_CONNECTED){
    ledcWrite(9, gamma28_8b_16b[255]);
    setupWiFi();
    broadcastAddress = (WiFi.localIP() | ~WiFi.subnetMask());
  }

  // OSC
  handle_osc();

  // OTA
  ArduinoOTA.handle();
  vTaskDelay(LOOP_INTERVAL);
}
