#include <Arduino.h>

// Network stack
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#define OTA_PASS "hugo"

char host[22];
WiFiMulti wifiMulti;

// Config
#define PIN_A_G 27
#define PIN_A_R 14
#define PIN_A_B 12

#define PIN_B_G 4
#define PIN_B_R 16
#define PIN_B_B 17

#define PIN_C_G 5
#define PIN_C_R 18
#define PIN_C_B 19

#define NUM_CHANNEL 9 

#define LED_BUILTIN 2
#define PIN_BTN 0

#define BAUDRATE 115200

// App
#include <ArtnetWifi.h>
#define FAST_BLINK (millis() % 200 < 50)
#define HEARTBEAT ((millis() + 1000 )% 2000 < 50)

#define PWM_FREQ 1000
#define PWM_RES_BITS 16
#define INPUT_RES 255
#define LOOP_INTERVAL 1  // global loop interval in ticks

#define START_UNIV 1
#define START_ADDR 1

bool running = true;

const uint16_t PROGMEM gamma28_8b_16b[] = {
      0,    0,    0,    0,    1,    1,    2,    3,    4,    6,    8,   10,   13,   16,   19,   24,
     28,   33,   39,   46,   53,   60,   69,   78,   88,   98,  110,  122,  135,  149,  164,  179,
    196,  214,  232,  252,  273,  295,  317,  341,  366,  393,  420,  449,  478,  510,  542,  575,
    610,  647,  684,  723,  764,  806,  849,  894,  940,  988, 1037, 1088, 1140, 1194, 1250, 1307,
   1366, 1427, 1489, 1553, 1619, 1686, 1756, 1827, 1900, 1975, 2051, 2130, 2210, 2293, 2377, 2463,
   2552, 2642, 2734, 2829, 2925, 3024, 3124, 3227, 3332, 3439, 3548, 3660, 3774, 3890, 4008, 4128,
   4251, 4376, 4504, 4634, 4766, 4901, 5038, 5177, 5319, 5464, 5611, 5760, 5912, 6067, 6224, 6384,
   6546, 6711, 6879, 7049, 7222, 7397, 7576, 7757, 7941, 8128, 8317, 8509, 8704, 8902, 9103, 9307,
   9514, 9723, 9936,10151,10370,10591,10816,11043,11274,11507,11744,11984,12227,12473,12722,12975,
  13230,13489,13751,14017,14285,14557,14833,15111,15393,15678,15967,16259,16554,16853,17155,17461,
  17770,18083,18399,18719,19042,19369,19700,20034,20372,20713,21058,21407,21759,22115,22475,22838,
  23206,23577,23952,24330,24713,25099,25489,25884,26282,26683,27089,27499,27913,28330,28752,29178,
  29608,30041,30479,30921,31367,31818,32272,32730,33193,33660,34131,34606,35085,35569,36057,36549,
  37046,37547,38052,38561,39075,39593,40116,40643,41175,41711,42251,42796,43346,43899,44458,45021,
  45588,46161,46737,47319,47905,48495,49091,49691,50295,50905,51519,52138,52761,53390,54023,54661,
  55303,55951,56604,57261,57923,58590,59262,59939,60621,61308,62000,62697,63399,64106,64818,65535
};

void setup() {

	pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
	Serial.begin(BAUDRATE);

	// build and set hostname
	uint64_t mac = ESP.getEfuseMac();
    uint64_t reversed_mac = 0;

    for (int i = 0; i < 6; i++) {
        reversed_mac |= ((mac >> (8 * i)) & 0xFF) << (8 * (5 - i));
    }

    snprintf(host, 22, "METAPIX-%llX", reversed_mac);
    Serial.println(host);

    // Wi-Fi
    WiFi.begin("domo", "th1Sp4((!");

    // OTA
    setupOTA();

	ledcAttachPin(PIN_A_R, 0);
	ledcAttachPin(PIN_A_G, 1);
	ledcAttachPin(PIN_A_B, 2);
	ledcAttachPin(PIN_B_R, 3);
	ledcAttachPin(PIN_B_G, 4);
	ledcAttachPin(PIN_B_B, 5);
	ledcAttachPin(PIN_C_R, 6);
	ledcAttachPin(PIN_C_G, 7);
	ledcAttachPin(PIN_C_B, 8);
	//ledcAttachPin(LED_BUILTIN, 9);
	pinMode(PIN_BTN, INPUT);

	ledcSetup(0, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(1, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(2, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(3, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(4, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(5, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(6, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(7, PWM_FREQ, PWM_RES_BITS);
	ledcSetup(8, PWM_FREQ, PWM_RES_BITS);
	//ledcSetup(9, PWM_FREQ, PWM_RES_BITS);

    xTaskCreatePinnedToCore(
        loop_metapixel,     // Function that should be called
        "Metapixel",    // Name of the task (for debugging)
        10000,           // Stack size (bytes)
        NULL,            // Parameter to pass
        3,               // Task priority
        NULL,            // Task handle
        !xPortGetCoreID()
    );
    delay(10);
}

void loop_metapixel(void * _){
	ArtnetWifi artnet;
	artnet.begin();
	artnet.setArtDmxCallback(onDmxFrame);

	delay(10);

	while(running){
		if(digitalRead(PIN_BTN)){
			artnet.read();
		}else{
			testMode();
		}
		vTaskDelay(LOOP_INTERVAL);
	}
    vTaskDelete(NULL);
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
	for (int i = 0; i < NUM_CHANNEL; ++i)
	{
		ledcWrite(i, gamma28_8b_16b[data[i]]);
	}
}

void testMode(){
	for (int i = 0; i < NUM_CHANNEL; ++i)
	{
		ledcWrite(i, gamma28_8b_16b[(millis()/10)%200]);
	}
}

void loop(){

    digitalWrite(LED_BUILTIN, HEARTBEAT);
	// wifi
    if(WiFi.status() != WL_CONNECTED){
    	digitalWrite(LED_BUILTIN, HIGH);
   	}

    // OTA
    ArduinoOTA.handle();
    vTaskDelay(LOOP_INTERVAL);
}


void setupOTA(){
    ArduinoOTA.setHostname(host);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA
    .onStart([]() {
    })
    .onEnd([]() {
    })
    .onProgress([](unsigned int progress, unsigned int total) {
        digitalWrite(LED_BUILTIN, FAST_BLINK);
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