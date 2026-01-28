#include <Arduino.h>

/* ==========================================
   CONFIGURATION
   ========================================== */
#define BUZZER_PIN 4  // Pin 4 (A4) on ESP32-C3 Supermini

// Conversion factor for microseconds to seconds
#define uS_TO_S_FACTOR 1000000ULL  

// DEBUG MODE: Uncomment to speed up time for testing
// #define DEBUG_MODE 

#ifdef DEBUG_MODE
  #define INITIAL_WAIT_MIN 0.1 // 6 seconds
  #define START_SLEEP_MIN  0.1
#else
  #define INITIAL_WAIT_MIN 15
  #define START_SLEEP_MIN  10
#endif

/* ==========================================
   RTC MEMORY VARIABLES
   ========================================== */
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int currentSleepDurationMinutes = START_SLEEP_MIN; 
RTC_DATA_ATTR int songIndex = 0;

/* ==========================================
   NOTE DEFINITIONS
   ========================================== */
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978
#define REST      0

/* ==========================================
   FUNCTION PROTOTYPES
   ========================================== */
void playHappyBirthday();
void playTetris();
void playNokia();
void stopBuzzer(); // Helper to safely turn off sink-buzzer

/* ==========================================
   HELPER
   ========================================== */
void stopBuzzer() {
  noTone(BUZZER_PIN);
  // CRITICAL: Since Buzzer is on +5V, we must go HIGH to stop current flow.
  // Or better, set to INPUT to be High Impedance (Disconnect).
  // Writing HIGH on 3.3V logic against 5V source is still 1.7V diff, 
  // but better than LOW.
  digitalWrite(BUZZER_PIN, HIGH); 
}

/* ==========================================
   MAIN LOGIC
   ========================================== */
void setup() {
  Serial.begin(115200);
  delay(1000); 

  // Initialize Pin: OUTPUT and HIGH (Off state for Sink config)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); 

  esp_reset_reason_t reason = esp_reset_reason();

  if (reason == ESP_RST_POWERON) {
    // --- FRESH START ---
    Serial.println(">>> Power On Detected. Arming device...");
    
    // --- Beep 5 times (Active Low Logic handled by tone, silence handled by stopBuzzer) ---
    for(int i=0; i<5; i++){
      tone(BUZZER_PIN, 2000); // Start Sound
      delay(100);             // Beep duration
      stopBuzzer();           // Stop Sound (Force High)
      delay(150);             // Silence duration
    }
    // ------------------------------------------
    
    bootCount = 0;
    currentSleepDurationMinutes = START_SLEEP_MIN;
    songIndex = 0;

    Serial.printf(">>> Waiting silently for %.1f minutes...\n", (float)INITIAL_WAIT_MIN);
    
    // Ensure buzzer is floating/off before sleep
    pinMode(BUZZER_PIN, INPUT); 

    uint64_t sleepSeconds = INITIAL_WAIT_MIN * 60;
    esp_sleep_enable_timer_wakeup(sleepSeconds * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  } 
  else {
    // --- WAKE UP ---
    Serial.printf(">>> Wake Up #%d\n", bootCount++);
    
    // Set output again after sleep
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);

    // Play Song
    switch (songIndex) {
      case 0: playHappyBirthday(); break;
      case 1: playTetris(); break;
      case 2: playNokia(); break;
    }

    // Increment
    songIndex++;
    if (songIndex > 2) songIndex = 0;

    // Calculate Sleep
    int sleepTime = currentSleepDurationMinutes;
    if (currentSleepDurationMinutes > 2) currentSleepDurationMinutes -= 2;
    if (currentSleepDurationMinutes < 2) currentSleepDurationMinutes = 2;

    Serial.printf(">>> Sleep for %d minutes.\n", sleepTime);
    Serial.flush(); 

    // Safety: Set pin to Input before sleep to prevent 5V leakage
    pinMode(BUZZER_PIN, INPUT);

    esp_sleep_enable_timer_wakeup(sleepTime * 60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}

void loop() {
  // Unreachable
}

/* ==========================================
   SONG IMPLEMENTATIONS
   ========================================== */
// NOTE: I added stopBuzzer() after every note to ensure the 
// pin goes HIGH (Off) rather than defaulting to LOW (On/Short).

void playHappyBirthday() {
  int tempo = 140;
  int melody[] = {
    NOTE_C4,4, NOTE_C4,8, 
    NOTE_D4,-4, NOTE_C4,-4, NOTE_F4,-4,
    NOTE_E4,-2, NOTE_C4,4, NOTE_C4,8, 
    NOTE_D4,-4, NOTE_C4,-4, NOTE_G4,-4,
    NOTE_F4,-2, NOTE_C4,4, NOTE_C4,8,
    NOTE_C5,-4, NOTE_A4,-4, NOTE_F4,-4, 
    NOTE_E4,-4, NOTE_D4,-4, NOTE_AS4,4, NOTE_AS4,8,
    NOTE_A4,-4, NOTE_F4,-4, NOTE_G4,-4,
    NOTE_F4,-2,
  };
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    divider = melody[thisNote + 1];
    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; 
    }
    tone(BUZZER_PIN, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration * 0.9);
    stopBuzzer(); // Force HIGH
    delay(noteDuration * 0.1);
  }
}

void playTetris() {
  int tempo = 144; 
  int melody[] = {
    NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,
    NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
    NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
    NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST,4,
    REST,8, NOTE_D5, 4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,
    NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
    NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
    NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,
  };
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    divider = melody[thisNote + 1];
    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; 
    }
    tone(BUZZER_PIN, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration * 0.9);
    stopBuzzer(); // Force HIGH
    delay(noteDuration * 0.1);
  }
}

void playNokia() {
  int tempo = 180;
  int melody[] = {
    NOTE_E5, 8, NOTE_D5, 8, NOTE_FS4, 4, NOTE_GS4, 4, 
    NOTE_CS5, 8, NOTE_B4, 8, NOTE_D4, 4, NOTE_E4, 4, 
    NOTE_B4, 8, NOTE_A4, 8, NOTE_CS4, 4, NOTE_E4, 4,
    NOTE_A4, 2, 
  };
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    divider = melody[thisNote + 1];
    if (divider > 0) {
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; 
    }
    tone(BUZZER_PIN, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration * 0.9);
    stopBuzzer(); // Force HIGH
    delay(noteDuration * 0.1);
  }
}