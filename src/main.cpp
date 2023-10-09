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
#define BAUDRATE 115200


// Network stack
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#define OTA_PASS "hugo"

char host[22];

// OSC
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>
WiFiUDP udp;

IPAddress broadcastAddress;
#define OSC_OUT_PORT 2107
#define OSC_IN_PORT  2108
unsigned long next_heartbeat = 0;


// App
#include <ArtnetWifi.h>
#define FAST_BLINK (millis() % 200 < 50)
#define HEARTBEAT ((millis() + 1000 )% 2000 < 50)

#define PWM_FREQ 1000
#define PWM_RES_BITS 16
#define INPUT_RES 255
#define LOOP_INTERVAL 10  // global loop interval in ticks
#define NUM_CHANNEL 9 


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

void testMode(){
	for (int i = 0; i < NUM_CHANNEL; ++i)
	{
		ledcWrite(i, gamma28_8b_16b[(millis()/10)%200]);
	}
}

void setupWiFi(){

  long long now = millis();
  WiFi.begin(SSID_FOULE, PSK_FOULE);

  while(WiFi.status() != WL_CONNECTED or millis() < WIFI_TIMEOUT_FOULE + now){
    vTaskDelay(LOOP_INTERVAL);
  }

  if(WiFi.status() == WL_CONNECTED){
    return;
    broadcastAddress = (WiFi.localIP() | ~WiFi.subnetMask());
  }

  WiFi.disconnect();
  WiFi.begin(SSID_ENSAD, PSK_ENSAD);
  now = millis();

  while(WiFi.status() != WL_CONNECTED or millis() < WIFI_TIMEOUT_ENSAD + now){
    vTaskDelay(LOOP_INTERVAL);
  }

  if(WiFi.status() == WL_CONNECTED){
    return;
    broadcastAddress = (WiFi.localIP() | ~WiFi.subnetMask());
  }

  ESP.restart();
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

// front end loop
void loop_metapixel(void * _){

  preferences.begin("MPX", false);
  int universe =   preferences.getUInt("UNIV", DEFAULT_UNIVERSE);
  int start_addr = preferences.getUInt("SADD", DEFAULT_UNIVERSE);

	ArtnetWiFiReceiver artnet;
	artnet.begin();

	delay(10);

	while(running){
		if(digitalRead(PIN_BTN)){
			artnet.parse();
		}else{
			testMode();
		}
    vTaskDelay(0);
	}
    vTaskDelete(NULL);
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  ledcWrite(9, gamma28_8b_16b[frame_count]);
  frame_count ++;
  frame_count %= 256;
	for (int i = 0; i < NUM_CHANNEL; ++i)
	{
		ledcWrite(i, gamma28_8b_16b[data[i]]);
	}
}

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

  snprintf(host, 22, "MPX-%llX", reversed_mac);
  Serial.println(host);

  setupWiFi();
  setupOTA();
  udp.begin(OSC_IN_PORT);

	ledcAttachPin(PIN_A_R, 0);
	ledcAttachPin(PIN_A_G, 1);
	ledcAttachPin(PIN_A_B, 2);
	ledcAttachPin(PIN_B_R, 3);
	ledcAttachPin(PIN_B_G, 4);
	ledcAttachPin(PIN_B_B, 5);
	ledcAttachPin(PIN_C_R, 6);
	ledcAttachPin(PIN_C_G, 7);
	ledcAttachPin(PIN_C_B, 8);
	ledcAttachPin(LED_BUILTIN, 9);
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
	ledcSetup(9, PWM_FREQ, PWM_RES_BITS);

  xTaskCreatePinnedToCore(
    loop_metapixel,     // Function that should be called
    "Metapixel",    // Name of the task (for debugging)
    20000,           // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL,            // Task handle
    1                // pin to core 1
  );
  delay(10);
  
  digitalWrite(LED_BUILTIN, LOW);
}

int frame_count;

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

void send_heartbeat(){
  OSCMessage msg("/heartbeat");
  msg.add(host);
  udp.beginPacket(broadcastAddress, OSC_OUT_PORT);
  msg.send(osc_out_socket);
  udp.endPacket();
  msg.empty();
}

void handle_osc(){

  if(millis() > next_heartbeat){
    next_heartbeat = millis() + random(HEARTBEAT_LOW, HEARTBEAT_HIGH);
    send_heartbeat();
  }


  OSCMessage message;
  int size = udp.parsePacket();

  if (size > 0) {
    while (size--) {
      message.fill(udp.read());
    }
    if (!message.hasError()) {
      message.dispatch("/update_addr", update_addr);
      message.dispatch("/update_univ", update_univ);
      /*message.dispatch("/color_r", update_color_red);
      message.dispatch("/color_g", update_color_green);
      message.dispatch("/color_b", update_color_blue);
      message.dispatch("/brightness",  update_bright);
      message.dispatch("/restart",     osc_restart);
      message.dispatch("/blackout",    osc_blackout);
      message.dispatch("/whiteout",    osc_whiteout);
      message.dispatch("/artnetout",   osc_arnetout);*/
    }
  }
}
