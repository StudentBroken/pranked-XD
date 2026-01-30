#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include <ArduinoJson.h>

USBHIDKeyboard Keyboard;
Preferences preferences;

const int PIN_BUTTON = 0;
const int PIN_LED = 8;

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
    JsonDocument doc;
    doc["type"] = "status";
    doc["state"] = state;
    doc["msg"] = msg;
    if (progress >= 0) doc["progress"] = progress;
    
    String output;
    serializeJson(doc, output);
    Serial.println(output);
}

// Check for serial commands constantly
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
            else if (cmd == "start") {
                // Trigger typing from web
                // We handle this in loop, just set a flag or call function?
                // Easier to handle via main loop logic, but for now:
                // We can't call blocking typeHuman here.
                // We will rely on the main loop button check or simple flag.
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

// A delay that listens to Serial and Buttons
void smartDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        handleSerial(); // Allow commands during wait
        
        // Check hardware button for abort
        if (digitalRead(PIN_BUTTON) == LOW) {
            abortRequested = true;
        }
        
        if (abortRequested) break;
        delay(1); // Small yield
    }
}

char getRandomChar() {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz";
    return chars[random(0, 26)];
}

void typeHuman() {
    isTyping = true;
    abortRequested = false;
    digitalWrite(PIN_LED, LOW); // On
    
    sendStatus("COUNTDOWN", "Waiting start delay...", 0);
    
    // Countdown
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
            
            // Send periodic progress updates (every 5 chars or so to reduce traffic)
            if (i % 5 == 0) {
                sendStatus("TYPING", "Typing...", (i * 100) / totalLen);
            }

            // 1. Error Simulation
            if (config.errorRate > 0 && random(0, config.errorRate) == 0 && c != '\n' && c != ' ') {
                Keyboard.print(getRandomChar());
                smartDelay(random(100, 300));
                Keyboard.write(KEY_BACKSPACE);
                smartDelay(random(50, 150));
            }

            // 2. Type Char
            Keyboard.print(c);

            // 3. Calc Delay
            int d = config.baseDelay + random(-config.delayVariance, config.delayVariance);
            if (d < 10) d = 10;
            
            if (c == ' ' || c == '\n') {
                d += random(50, 150); 
                if (config.longPauseChance > 0 && random(0, config.longPauseChance) == 0) {
                    d += random(500, 2000); 
                    sendStatus("TYPING", "Thinking (Long Pause)...", (i * 100) / totalLen);
                }
            }
            smartDelay(d);
        }
    }

    digitalWrite(PIN_LED, HIGH); // Off
    isTyping = false;
    
    if(!abortRequested) {
        sendStatus("FINISHED", "Typing complete", 100);
    }
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    Serial.begin(115200);
    Keyboard.begin();
    USB.begin();

    loadSettings();
    randomSeed(analogRead(1));
}

// Debounce vars
unsigned long lastDebounceTime = 0;
bool buttonState;
bool lastButtonState = HIGH;

void loop() {
    handleSerial(); // Listen for commands

    // Button Handling
    int reading = digitalRead(PIN_BUTTON);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                // Button Pressed
                if (isTyping) {
                    abortRequested = true; // Stop if running
                } else {
                    typeHuman(); // Start if idle
                }
            }
        }
    }
    lastButtonState = reading;
}