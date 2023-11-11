/* To be implemented*/
/* - Error Handling*/
/* - Pull Stored tag to local machine when boot up*/
/* Maybe replace HTTP with TCP socket*/

#define LOGGING true
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <ArduinoJson.h>
#include <FastLED.h>

#define RELAY_PIN 14
#define GREEN_LED_PIN 27
#define RED_LED_PIN 26
#define BLUE_LED_PIN 14

#define RFID_RESET_PIN 25

#define STATE_IDLE 0
#define STATE_RFID_PASS 1
#define STATE_RFID_NO_PASS 2
#define STATE_ERROR 3

int currentState = STATE_IDLE;

#define ADDRESS_LED_PIN 13
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
int brightness;
int fadeDirection;
#define maxBrightness 127

int openDuration = 5000;
unsigned long openTimeNow; 

QueueHandle_t rfidQueue;
TaskHandle_t rdifTaskHandler;

PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);
int lastPassState = 2;

const char kHostname[] = "192.168.1.101";
const char kPath[] = "/test/rfid";

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

typedef struct {
        uint8_t *uid; // Buffer to store the returned UID
        uint8_t uidLength;
}rfidData;

class functionInterval
{
public:
    functionInterval(int interval, void (*func)())
    {
        _interval = interval;
        _func = func;
    }
    void loop()
    {
        if (millis() - timeNow > _interval)
        {
            (*_func)();
            timeNow = millis();
        }
    }

private:
    void (*_func)();
    unsigned long _interval;
    unsigned long timeNow;
};

void stateHandler(void *pvParameter){
    for(;;){
        if (currentState == STATE_ERROR){
            brightness = brightness < maxBrightness ? maxBrightness : 0;
            leds[0].setRGB(brightness,0,0);
            vTaskDelay(500);
        }
        else if (currentState == STATE_IDLE){
            if (brightness >= maxBrightness - fadeDirection){
                fadeDirection = -1;
            }
            else if (brightness <= 0){
                fadeDirection = +1;
            }
            brightness = constrain(brightness+fadeDirection,0,maxBrightness);
            leds[0].setRGB(brightness,brightness,brightness);
            vTaskDelay(10);
        }
        else if (currentState == STATE_RFID_PASS){
            if (brightness >= maxBrightness){
                fadeDirection = -1;
            }
            else if (brightness <= 0){
                fadeDirection = +1;
            }
            brightness = constrain(brightness+fadeDirection,0,maxBrightness);
            leds[0].setRGB(0,brightness,brightness * 0.4);
            vTaskDelay(10);
        }
        else if (currentState == STATE_RFID_NO_PASS){
            if (brightness >= maxBrightness){
                fadeDirection = -1;
            }
            else if (brightness <= 0){
                fadeDirection = +1;
            }
            brightness = constrain(brightness+fadeDirection,0,maxBrightness);
            leds[0].setRGB(brightness,brightness * 0.2,brightness * 0.2);
            vTaskDelay(10);
        }
        FastLED.show();
    }
}

void setupIO(){
    FastLED.addLeds<WS2812B,ADDRESS_LED_PIN,GRB>(leds,NUM_LEDS);
    pinMode(RELAY_PIN,OUTPUT);
    pinMode(GREEN_LED_PIN,OUTPUT);
    pinMode(RED_LED_PIN,OUTPUT);
    pinMode(BLUE_LED_PIN,OUTPUT);
    digitalWrite(RELAY_PIN,LOW);
    digitalWrite(GREEN_LED_PIN,LOW);
    digitalWrite(RED_LED_PIN,LOW);
    digitalWrite(BLUE_LED_PIN,LOW);
}

void httpRequest(void *pvParameter)
{
    rfidData rfidIn;
    rfidData lastRfidIn;
    int cmpResult;
    char returnResult[128];
    for(;;){

        // char tempdata[50]; 
        // strcpy(tempdata,kPath);
        // strcat(postData,tempdata);
        if( xQueueReceive( rfidQueue,&(rfidIn),10) == pdPASS ){
            vTaskSuspend(rdifTaskHandler);
            EthernetClient c;
            HttpClient http(c);
            int bodyLen = http.contentLength();
            char hex[] = "0123456789ABCDEF";
            char postData[50];
            sprintf(postData,"%s:",kPath);
            Serial.print("RFID Input length : ");
            Serial.println(rfidIn.uidLength);
            int len = strlen(postData);
            cmpResult = 0;
            for(int i = 0;i < rfidIn.uidLength;i++){
                // if (rfidIn.uid[i] != lastRfidIn.uid[i]) {cmpResult = 1;}
                postData[len + 2*i] = hex[(rfidIn.uid[i] >> 4) & 15];
                postData[len + 2*i + 1] = hex[rfidIn.uid[i] & 15];
                postData[len + 2*i + 2] = '\0';
                // Serial.print((rfidIn.uid[i] >> 4)&15);
                // Serial.print(" ");
                // Serial.println(rfidIn.uid[i] & 15);
            }
            Serial.print("Post data : ");
            Serial.println(postData);

            int err = 0;
            try{
                err = http.post(kHostname,1880,postData);
            }
            catch(int err){
                Serial.println("Post Error");
                Serial.println("Suspending task");
                vTaskSuspend(NULL);
            }
            Serial.print("Posting : ");
            Serial.print(postData);
            Serial.print(" len : ");
            Serial.println(strlen(postData));
            int resCode = http.responseStatusCode();
            Serial.print("Post Status : ");
            Serial.println(err);
            if (resCode == 200){
                err = http.skipResponseHeaders();
                int bodyLen = http.contentLength();
                Serial.print("Content length is: ");
                Serial.println(bodyLen);
                Serial.println("Body returned follows:");
                int i = 0;
                while(http.available()){
                    char c = http.read();
                    // returnResult = strcat(returnResult,&c);
                    returnResult[i] = c;
                    returnResult[i+1] = '\0';
                    i++;
                    // Serial.print(c);
                }
                Serial.print("-->   ");
                Serial.print(returnResult);
                Serial.println("   <---");
                StaticJsonDocument<128> returnJson;
                DeserializationError error = deserializeJson(returnJson,returnResult);
                if (error){
                    Serial.print("Deserialize JSON failed : ");
                    Serial.println(error.f_str());
                }
                bool pass =  returnJson["pass"];
                if (pass != lastPassState){
                    if (pass){
                        currentState = STATE_RFID_PASS;
                        digitalWrite(RELAY_PIN,HIGH);
                        digitalWrite(GREEN_LED_PIN,HIGH);
                    }
                    else{
                        currentState = STATE_RFID_NO_PASS;
                        // digitalWrite(RELAY_PIN,LOW);
                        digitalWrite(GREEN_LED_PIN,LOW);
                    }
                    openTimeNow = millis();
                    brightness = 0;
                }
                lastPassState = pass;


                // Serial.println("");
            }
            else if (err == -1){
                currentState = STATE_ERROR;
                Serial.println("Reconnect...");
                while (Ethernet.begin(mac) != 1)
                {
                    Serial.println("Error getting IP address via DHCP, trying again...");
                    vTaskDelay(1000);
                }
                Serial.println("Reconnected");
                currentState = STATE_IDLE;
            }
            http.stop();
            vTaskResume(rdifTaskHandler);
        }

        // err = http.get(kHostname, 1880, kPath);
        // if (err == 0)
        // {
        //     Serial.println("startedRequest ok");

        //     err = http.responseStatusCode();
        //     if (err >= 0)
        //     {
        //         Serial.print("Got status code: ");
        //         Serial.println(err);
        //         // err = http.skipResponseHeaders();
        //     }
        //     else
        //     {
        //         Serial.print("Getting response failed: ");
        //         Serial.println(err);
        //     }
        // }
        // else
        // {
        //     Serial.print("Connect failed: ");
        //     Serial.println(err);
        // }
        // vTaskDelay(1000);
    }
}

void readRFID(void *pvParameter)
{
    rfidData rfidIn;
    for(;;){
        uint8_t success;
        uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
        uint8_t uidLength;
        success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
        if (success)
        {
            // Display some basic information about the card
            Serial.println("Found an ISO14443A card");
            Serial.print("  UID Length: ");
            Serial.print(uidLength, DEC);
            Serial.println(" bytes");
            Serial.print("  UID Value: ");
            nfc.PrintHex(uid, uidLength);
            Serial.println("");
            rfidIn.uid = uid;
            rfidIn.uidLength = uidLength;
            xQueueSend(rfidQueue,(void*)&rfidIn,1000);
        }
        vTaskDelay(1000);
    }
}

void setupRFID()
{
    // nfc.
    pinMode(RFID_RESET_PIN,OUTPUT);
    digitalWrite(RFID_RESET_PIN,LOW);
    delay(100);
    digitalWrite(RFID_RESET_PIN,HIGH);

    nfc.begin();
    Wire.setClock(200000);
    Serial.print("I2C Clock freq : ");
    Serial.println(Wire.getClock());
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata)
    {
        Serial.print("Didn't find PN53x board");
        Serial.println("Restart in 5 seconds");
        delay(5000);
        ESP.restart();
    }
    // Got ok data, print it out!
    Serial.print("Found chip PN5");
    Serial.println((versiondata >> 24) & 0xFF, HEX);
    Serial.print("Firmware ver. ");
    Serial.print((versiondata >> 16) & 0xFF, DEC);
    Serial.print('.');
    Serial.println((versiondata >> 8) & 0xFF, DEC);

    // configure board to read RFID tags
    nfc.SAMConfig();
    Serial.println("Waiting for an ISO14443A Card ...");
}

// functionInterval scanRFID(1000, &readRFID);
// functionInterval httpRequestFunction(1000, &httpRequest);

void setup()
{
    Serial.begin(115200);
    Ethernet.init(5);
    setupRFID();
    setupIO();
    while (Ethernet.begin(mac) != 1)
    {
        // currentState = STATE_ERROR;
        Serial.println("Error getting IP address via DHCP, trying again...");
        vTaskDelay(15000);
    }
    Serial.print("Local IP : ");
    Serial.println(Ethernet.localIP());
    // Serial.print()
    xTaskCreate(readRFID,"Read RFID Task",4096,NULL,0,&rdifTaskHandler);
    xTaskCreate(httpRequest,"httpRequest",4096,NULL,0,NULL);
    xTaskCreate(stateHandler,"State Handler",2048,NULL,0,NULL);
    rfidQueue = xQueueCreate(10,1024);
}

void loop()
{
    vTaskDelay(100);
    if (millis() - openTimeNow > openDuration && currentState != STATE_ERROR){
        // Serial.println(brightness);
        if (brightness < 5){
            currentState = STATE_IDLE;
            digitalWrite(RELAY_PIN,LOW);
            lastPassState = 2;
        }
    }
    // And just stop, now that we've tried a download
    // httpRequestFunction.loop();
    // scanRFID.loop();
}
