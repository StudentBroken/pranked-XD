#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include <ArduinoJson.h>

USBHIDKeyboard Keyboard;
Preferences preferences;

// Pins
const int PIN_BUTTON = 0; // BOOT button on SuperMini
const int PIN_LED = 8;    // Built-in LED (Blue)

// Application States
enum AppMode {
    MODE_HID_WAIT,      // Waiting to type
    MODE_HID_TYPING,    // Currently typing
    MODE_SERIAL_CONFIG  // Accepting commands/text via Serial
};

AppMode currentMode = MODE_HID_WAIT;

// Configuration Structure
struct Config {
    int baseDelay;       // Average ms between keys
    int delayVariance;   // Randomness in ms
    int errorRate;       // 1 in X chance of error (lower is more errors)
    int longPauseChance; // 1 in X chance of long pause on space
    int startDelay;      // Seconds to wait before typing
};

Config config;
String textPayload = "";

// Default Settings
void loadSettings() {
    preferences.begin("human_hid", false);
    config.baseDelay = preferences.getInt("base", 70);
    config.delayVariance = preferences.getInt("var", 30);
    config.errorRate = preferences.getInt("err", 50); // 1 in 50 chars
    config.longPauseChance = preferences.getInt("pause", 15); // 1 in 15 words
    config.startDelay = preferences.getInt("start", 5);
    
    // Check if text exists
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

// --- Logic ---

void blink(int times, int speed) {
    for(int i=0; i<times; i++) {
        digitalWrite(PIN_LED, LOW); // LED is active LOW usually
        delay(speed);
        digitalWrite(PIN_LED, HIGH);
        delay(speed);
    }
}

// Function to generate random char for "fat finger" error
char getRandomChar() {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz";
    return chars[random(0, 26)];
}

void typeHuman(String text) {
    currentMode = MODE_HID_TYPING;
    
    for (int i = 0; i < text.length(); i++) {
        // Check if user wants to abort by holding BOOT
        if (digitalRead(PIN_BUTTON) == LOW) {
            currentMode = MODE_SERIAL_CONFIG;
            blink(5, 50);
            return;
        }

        char c = text[i];
        
        // 1. Error Simulation
        if (config.errorRate > 0 && random(0, config.errorRate) == 0 && c != '\n' && c != ' ') {
            Keyboard.print(getRandomChar()); // Type wrong char
            delay(random(100, 300));         // "Oh no" realization pause
            Keyboard.write(KEY_BACKSPACE);   // Delete it
            delay(random(50, 150));          // Correction pause
        }

        // 2. Type the actual character
        Keyboard.print(c);

        // 3. Calculation delays
        int d = config.baseDelay + random(-config.delayVariance, config.delayVariance);
        if (d < 10) d = 10;
        
        // 4. Word Interval / Long Pause Logic
        if (c == ' ' || c == '\n') {
            // Natural word break is slightly longer
            d += random(50, 150); 
            
            // Random "Thinking" Pause
            if (config.longPauseChance > 0 && random(0, config.longPauseChance) == 0) {
                d += random(500, 2000); 
            }
        }

        delay(d);
    }
    currentMode = MODE_HID_WAIT;
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP); // BOOT Button
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); // Off

    Serial.begin(115200);
    Keyboard.begin();
    USB.begin();

    loadSettings();
    randomSeed(analogRead(1));
}

// Button Debounce vars
unsigned long lastDebounceTime = 0;
bool buttonState;
bool lastButtonState = HIGH;

void loop() {
    // --- Button Handling for Mode Switching ---
    int reading = digitalRead(PIN_BUTTON);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == LOW) {
                // Button Pressed
                if (currentMode == MODE_SERIAL_CONFIG) {
                    // Switch to HID Mode
                    currentMode = MODE_HID_WAIT;
                    Serial.println("Mode: HID WAIT");
                    blink(2, 200);
                    
                    // Countdown and Type
                    delay(config.startDelay * 1000);
                    typeHuman(textPayload);
                    
                } else {
                    // Switch to Serial Mode
                    currentMode = MODE_SERIAL_CONFIG;
                    Serial.println("Mode: SERIAL CONFIG");
                    blink(3, 100);
                }
            }
        }
    }
    lastButtonState = reading;

    // --- Serial Command Handling ---
    if (currentMode == MODE_SERIAL_CONFIG && Serial.available()) {
        String input = Serial.readStringUntil('\n');
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, input);

        if (!error) {
            String cmd = doc["cmd"];
            
            if (cmd == "config") {
                config.baseDelay = doc["baseDelay"];
                config.delayVariance = doc["delayVariance"];
                config.errorRate = doc["errorRate"];
                config.longPauseChance = doc["longPauseChance"];
                config.startDelay = doc["startDelay"];
                saveSettings();
                Serial.println("{\"status\":\"config_saved\"}");
            }
            else if (cmd == "text") {
                String newText = doc["data"];
                textPayload = newText;
                saveSettings();
                Serial.println("{\"status\":\"text_saved\", \"len\":" + String(textPayload.length()) + "}");
            }
            else if (cmd == "get") {
                // Send current config back to web
                JsonDocument resp;
                resp["baseDelay"] = config.baseDelay;
                resp["delayVariance"] = config.delayVariance;
                resp["errorRate"] = config.errorRate;
                resp["longPauseChance"] = config.longPauseChance;
                resp["startDelay"] = config.startDelay;
                resp["payload"] = textPayload;
                serializeJson(resp, Serial);
                Serial.println();
            }
        } else {
            // Raw text fallback if JSON fails (simple paste)
             if (input.length() > 0) {
                 textPayload = input;
                 saveSettings();
                 Serial.println("Raw text saved");
             }
        }
    }
}