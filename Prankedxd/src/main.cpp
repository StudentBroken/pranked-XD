#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include <ArduinoJson.h>

USBHIDKeyboard Keyboard;
Preferences preferences;

const int PIN_BUTTON = 0;
const int PIN_LED = 8; 

struct Config {
    int baseDelay;
    int delayVariance;
    int errorRate;
    int longPauseChance;
    int startDelay;
};

Config config;
String textPayload = "";

bool isTyping = false;
bool abortRequested = false;

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

// --- FIXED SEND STATUS ---
void sendStatus(String state, String msg, int progress = -1) {
    JsonDocument doc;
    doc["type"] = "status";
    doc["state"] = state;
    doc["msg"] = msg;
    if (progress >= 0) doc["progress"] = progress;
    
    String output;
    serializeJson(doc, output);
    // Send regardless of DTR state, but rely on timeout to prevent blocking
    Serial.println(output); 
}

void handleSerial() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        
        // Filter out empty lines
        input.trim();
        if(input.length() == 0) return;

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
                sendStatus("CONFIG_SAVED", "Configuration saved to memory");
            }
            else if (cmd == "text") {
                String newText = doc["data"];
                textPayload = newText;
                saveSettings();
                sendStatus("TEXT_SAVED", "Text payload updated", textPayload.length());
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
    digitalWrite(PIN_LED, LOW); 
    
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

    digitalWrite(PIN_LED, HIGH); 
    isTyping = false;
    
    if(!abortRequested) {
        sendStatus("FINISHED", "Typing complete", 100);
    }
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); 

    USB.begin();
    Serial.begin(115200);
    
    // CRITICAL: Prevent blocking if web is closed
    Serial.setTxTimeoutMs(0); 

    Keyboard.begin();
    loadSettings();
    randomSeed(analogRead(1));
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