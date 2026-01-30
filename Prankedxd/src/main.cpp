#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include <ArduinoJson.h>

USBHIDKeyboard Keyboard;
Preferences preferences;

const int PIN_BUTTON = 0;
const int PIN_LED = 8; // Adjust if your board uses a different pin (Some S3 use 15 or 21)

// --- Config & State ---
struct Config {
    int baseDelay;
    int delayVariance;
    int errorRate;
    int longPauseChance;
    int startDelay;
};

Config config;
String textPayload = "";

// State tracking
bool isTyping = false;
bool abortRequested = false;

// --- NVS Storage ---
void loadSettings() {
    preferences.begin("human_hid", false);
    config.baseDelay = preferences.getInt("base", 70);
    config.delayVariance = preferences.getInt("var", 30);
    config.errorRate = preferences.getInt("err", 50);
    config.longPauseChance = preferences.getInt("pause", 15);
    config.startDelay = preferences.getInt("start", 5);
    if (preferences.isKey("payload")) {
        textPayload = preferences.getString("payload", "Hello World");
    } else {
        textPayload = "Hello World";
    }
    preferences.end();
}

void saveSettings() {
    preferences.begin("human_hid", false);
    preferences.putInt("base", config.baseDelay);
    preferences.putInt("var", config.delayVariance);
    preferences.putInt("err", config.errorRate);
    preferences.putInt("pause", config.longPauseChance);
    preferences.putInt("start", config.startDelay);
    preferences.putString("payload", textPayload);
    preferences.end();
}

// --- Helper Functions ---

void sendStatus(String state, String msg, int progress = -1) {
    // Only send if Serial is actually connected to avoid buffer filling
    if(!Serial) return;

    JsonDocument doc;
    doc["type"] = "status";
    doc["state"] = state;
    doc["msg"] = msg;
    if (progress >= 0) doc["progress"] = progress;
    
    String output;
    serializeJson(doc, output);
    Serial.println(output);
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, input);

        if (!error) {
            String cmd = doc["cmd"];
            
            if (cmd == "stop") {
                abortRequested = true;
                sendStatus("ABORT", "Stop command received");
            }
            else if (cmd == "config") {
                config.baseDelay = doc["baseDelay"];
                config.delayVariance = doc["delayVariance"];
                config.errorRate = doc["errorRate"];
                config.longPauseChance = doc["longPauseChance"];
                config.startDelay = doc["startDelay"];
                saveSettings();
                sendStatus("CONFIG", "Configuration saved");
            }
            else if (cmd == "text") {
                String newText = doc["data"];
                textPayload = newText;
                saveSettings();
                sendStatus("TEXT", "Text payload updated", textPayload.length());
            }
            else if (cmd == "get") {
                JsonDocument resp;
                resp["type"] = "settings";
                resp["baseDelay"] = config.baseDelay;
                resp["delayVariance"] = config.delayVariance;
                resp["errorRate"] = config.errorRate;
                resp["longPauseChance"] = config.longPauseChance;
                resp["startDelay"] = config.startDelay;
                resp["payload"] = textPayload;
                serializeJson(resp, Serial);
                Serial.println();
            }
        }
    }
}

void smartDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        handleSerial(); 
        
        if (digitalRead(PIN_BUTTON) == LOW) {
            abortRequested = true;
        }
        
        if (abortRequested) break;
        delay(1); 
    }
}

char getRandomChar() {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz";
    return chars[random(0, 26)];
}

void typeHuman() {
    isTyping = true;
    abortRequested = false;
    digitalWrite(PIN_LED, LOW); // LED ON
    
    sendStatus("COUNTDOWN", "Waiting start delay...", 0);
    
    for(int i=0; i<config.startDelay; i++) {
        smartDelay(1000);
        sendStatus("COUNTDOWN", String(config.startDelay - i) + "s remaining", (i*100)/config.startDelay);
        if(abortRequested) break;
    }

    if (!abortRequested) {
        sendStatus("TYPING", "Started typing...", 0);
        int totalLen = textPayload.length();

        for (int i = 0; i < totalLen; i++) {
            if (abortRequested) {
                sendStatus("ABORTED", "Typing aborted by user");
                break;
            }

            char c = textPayload[i];
            
            if (i % 5 == 0) sendStatus("TYPING", "Typing...", (i * 100) / totalLen);

            // Error Logic
            if (config.errorRate > 0 && random(0, config.errorRate) == 0 && c != '\n' && c != ' ') {
                Keyboard.print(getRandomChar());
                smartDelay(random(100, 300));
                Keyboard.write(KEY_BACKSPACE);
                smartDelay(random(50, 150));
            }

            Keyboard.print(c);

            int d = config.baseDelay + random(-config.delayVariance, config.delayVariance);
            if (d < 10) d = 10;
            
            if (c == ' ' || c == '\n') {
                d += random(50, 150); 
                if (config.longPauseChance > 0 && random(0, config.longPauseChance) == 0) {
                    d += random(500, 2000); 
                    sendStatus("TYPING", "Thinking...", (i * 100) / totalLen);
                }
            }
            smartDelay(d);
        }
    }

    digitalWrite(PIN_LED, HIGH); // LED OFF
    isTyping = false;
    
    if(!abortRequested) {
        sendStatus("FINISHED", "Typing complete", 100);
    }
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); 

    // 1. Initialize USB Stack
    USB.begin();
    
    // 2. Initialize Serial (CDC)
    Serial.begin(115200);
    
    // IMPORTANT: Don't block if no serial monitor is connected
    Serial.setTxTimeoutMs(0); 

    // 3. Initialize Keyboard
    Keyboard.begin();

    loadSettings();
    randomSeed(analogRead(1));
    
    // Blink to indicate boot success (even if Serial is dead)
    for(int i=0; i<3; i++) {
        digitalWrite(PIN_LED, LOW); delay(100);
        digitalWrite(PIN_LED, HIGH); delay(100);
    }
    
    Serial.println("ESP32-S3 HID Ready.");
}

unsigned long lastDebounceTime = 0;
bool buttonState;
bool lastButtonState = HIGH;

void loop() {
    handleSerial(); 

    int reading = digitalRead(PIN_BUTTON);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                if (isTyping) {
                    abortRequested = true;
                } else {
                    typeHuman();
                }
            }
        }
    }
    lastButtonState = reading;
}