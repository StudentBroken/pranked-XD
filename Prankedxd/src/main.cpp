#include <Arduino.h>

/* ==========================================
   ESP32-S3 SuperMini Configuration
   ========================================== */
// Common pins on ESP32-S3 SuperMini
#define LED_PIN 13           // Built-in LED (GPIO 13)
#define BUTTON_PIN 0         // Boot button (GPIO 0)
#define BUZZER_PIN 11        // Buzzer output (GPIO 11)

// Timings
#define BLINK_INTERVAL_MS 500  // LED blink interval
#define TONE_FREQUENCY 1000    // Hz - default buzzer frequency

/* ==========================================
   STATE VARIABLES
   ========================================== */
unsigned long lastBlinkTime = 0;
bool ledState = false;
int bootCount = 0;

/* ==========================================
   FUNCTION PROTOTYPES
   ========================================== */
void blinkLED(int times = 1, int delayMs = 200);
void beep(int frequency = 1000, int duration = 100);
void printSystemInfo();

/* ==========================================
   HELPER FUNCTIONS
   ========================================== */

/**
 * Blink the LED a specified number of times
 */
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

/**
 * Generate a beep sound on the buzzer
 */
void beep(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
  delay(duration + 10);
  noTone(BUZZER_PIN);
}

/**
 * Print system information to serial
 */
void printSystemInfo() {
  Serial.println("System Information:");
  Serial.printf("  CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("  Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("  Free SRAM: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  Flash Size: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("  Chip ID: %016llX\n", ESP.getEfuseMac());
  Serial.println();
}

/* ==========================================
   SETUP
   ========================================== */
void setup() {
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);  // Wait for serial to stabilize
  
  Serial.println("\n\n========================================");
  Serial.println("   ESP32-S3 SuperMini Initialized");
  Serial.println("========================================\n");
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Turn off LED and buzzer initially
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Print system information
  printSystemInfo();
  
  // Startup sequence: blink LED and beep buzzer
  Serial.println("Starting up...");
  blinkLED(3, 150);
  beep(1000, 100);
  delay(100);
  beep(1500, 100);
  
  Serial.printf("Boot count: %d\n\n", bootCount++);
}

/* ==========================================
   MAIN LOOP
   ========================================== */
void loop() {
  unsigned long currentTime = millis();
  
  // Blink LED at regular intervals
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL_MS) {
    lastBlinkTime = currentTime;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    Serial.printf("LED: %s\n", ledState ? "ON" : "OFF");
  }
  
  // Check button press
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed!");
    delay(20);  // Debounce
    
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(10);
    }
    delay(20);  // Debounce on release
    
    // Play a tone when button is pressed
    beep(2000, 200);
    blinkLED(2, 100);
    
    Serial.println("Button released!");
  }
  
  delay(50);  // Small delay to prevent overwhelming the serial monitor
}
