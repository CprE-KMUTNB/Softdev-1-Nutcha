#define ESP32_RTOS

#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <string.h>
#include <ArduinoJson.h>

#include <ESPmDNS.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include "OTA.h"
#include "credentials.h"

#define USE_SERIAL Serial

#define SCK_PIN 18
#define MOSI_PIN 23
#define SS_PIN 27
#define LATCH_PIN 2

#define LED_PIN 26

const char* server = "192.168.1.101";

WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

WStype_t wsState = WStype_DISCONNECTED;

String msgInput;
#define INPUT_MSG_BUFFERSIZE (100)
char msg_in[INPUT_MSG_BUFFERSIZE];
bool dataReadyFlag;

unsigned long reportTimeNow;
int reportInterval = 1000;

void setup_wifi() {

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFiMulti.addAP(ssid, password);
	while(WiFiMulti.run() != WL_CONNECTED) {
		delay(100);
	}
  setupOTA("espRelayOTA");

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  wsState = type;
	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			webSocket.sendTXT("Connected");
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      for (int i = 0; i < length; i++) {
        msg_in[i] = (char)payload[i];
        msg_in[i + 1] = '\0';
      }
      dataReadyFlag = true;
			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}

}

class LED_Register {
  public:
    LED_Register(int STORAGE_CLOCK_PIN, int SERIAL_DATA_PIN, int SERIAL_CLOCK_PIN);
    void setLED(int pin, int state);
    int getLED(int pin);
  private:
    uint8_t _ledState;
    int _STORAGE_CLOCK_PIN;
    int _SERIAL_DATA_PIN;
    int _SERIAL_CLOCK_PIN;
};
void LED_Register::setLED(int pin, int state) {
  _ledState &= ~(1 << pin);
  _ledState |= (state << pin);
  digitalWrite(_STORAGE_CLOCK_PIN, LOW);
  SPI.transfer(_ledState);
  digitalWrite(_STORAGE_CLOCK_PIN, HIGH);
}
int LED_Register::getLED(int pin) {
  return (_ledState >> pin) & 1;
}

LED_Register::LED_Register(int STORAGE_CLOCK_PIN, int SERIAL_DATA_PIN = -1, int SERIAL_CLOCK_PIN = -1) {
  _STORAGE_CLOCK_PIN = STORAGE_CLOCK_PIN;
  _SERIAL_DATA_PIN = SERIAL_DATA_PIN;
  _SERIAL_CLOCK_PIN = SERIAL_CLOCK_PIN;
  pinMode(_STORAGE_CLOCK_PIN, OUTPUT);
  pinMode(_SERIAL_DATA_PIN, OUTPUT);
  pinMode(_SERIAL_CLOCK_PIN, OUTPUT);
}

void ledStateDisplay(void *pvPatameter){

  for(;;){
    digitalWrite(LED_PIN,!digitalRead(LED_PIN));
    if (wsState == WStype_DISCONNECTED){
      vTaskDelay(250);
    } 
    else if (wsState == WStype_CONNECTED){
      vTaskDelay(2000);
    }
    else if (wsState == WStype_TEXT){
      vTaskDelay(50);
    }
  }
}

LED_Register ledRegister(SS_PIN);

void setup() {
  SPI.begin(SCK_PIN, -1, MOSI_PIN, SS_PIN);
  pinMode(LED_PIN,OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  digitalWrite(LATCH_PIN, HIGH);
  for (int i = 0; i < 6; i++) {
    ledRegister.setLED(i, LOW);
  }

  Serial.begin(115200);
  setup_wifi();



  if (!MDNS.begin("espRelay")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
        delay(10000);
        ESP.restart();
    }
  }
  Serial.println("mDNS responder started");

  webSocket.begin(server, 1880, "/ws/espRelay");
	webSocket.onEvent(webSocketEvent);
	webSocket.setAuthorization("user", "Password");
	webSocket.setReconnectInterval(5000);
  xTaskCreate(ledStateDisplay, "LED State", 2048, NULL, 0, NULL);
}

void loop() {

  // digitalWrite(LED_PIN,WiFi.status() == WL_CONNECTED);
  webSocket.loop();

  if (strlen(msg_in) != 0) {
    StaticJsonDocument<200> doc;
    char msg_out[50];
    DeserializationError error = deserializeJson(doc, msg_in);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    int index = doc["index"];
    int On = doc["On"];
    TelnetStream.println("Relay Switched");
    ledRegister.setLED(index - 1,On);
    serializeJson(doc,msg_out);
    webSocket.sendTXT(msg_out,strlen(msg_out));
    msg_in[0] = '\0';
    wsState = WStype_CONNECTED;
    dataReadyFlag = false;
  }
  
  if (millis() - reportTimeNow > reportInterval){
    TelnetStream.print("RSSI : ");
    TelnetStream.println(WiFi.RSSI());
    reportTimeNow = millis();
  }
}
