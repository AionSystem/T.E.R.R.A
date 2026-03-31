// TERRA-HOME v1.3 Firmware
// ESP32, Arduino framework

#include <ESP32TimerInterrupt.h>
#include <WiFi.h>
#include <WebServer.h>

// Pin definitions
#define CRYSTAL1_PIN 25
#define CRYSTAL2_PIN 26
#define CRYSTAL3_PIN 27
#define TEMP_SENSOR_PIN 34
#define LED_STATUS 13
#define LED_ALERT 2
#define BUZZER_PIN 14
#define BUTTON_START 15
#define BUTTON_STOP 16
#define BUTTON_STATUS 17

// Protocol parameters
const int BURST_PULSE_COUNT = 10;      // 10 pulses per burst
const int PULSE_ON_MS = 10;            // 10 ms on per pulse
const int PULSE_OFF_MS = 10;           // 10 ms off per pulse (100 Hz)
const int BURST_OFF_MS = 900;          // 900 ms off between bursts
const int BURSTS_PER_TREATMENT = 600;   // 600 bursts = 60 seconds

// Crystal impedance monitoring
float baseline_impedance[3] = {0, 0, 0};
float current_impedance[3] = {0, 0, 0};

// Timer interrupt for burst generation
hw_timer_t *timer = NULL;
volatile int burst_counter = 0;
volatile int pulse_counter = 0;
volatile bool burst_active = false;
volatile bool treatment_active = false;

void IRAM_ATTR onTimer() {
  if (!treatment_active) return;
  
  if (!burst_active) {
    // Start a new burst
    burst_active = true;
    pulse_counter = 0;
    digitalWrite(CRYSTAL1_PIN, HIGH);
    digitalWrite(CRYSTAL2_PIN, HIGH);
    digitalWrite(CRYSTAL3_PIN, HIGH);
    timerAlarmWrite(timer, PULSE_ON_MS * 1000, false);
  } else {
    pulse_counter++;
    if (pulse_counter < BURST_PULSE_COUNT) {
      // Continue burst: toggle crystal off/on
      digitalWrite(CRYSTAL1_PIN, LOW);
      digitalWrite(CRYSTAL2_PIN, LOW);
      digitalWrite(CRYSTAL3_PIN, LOW);
      delayMicroseconds(100);
      digitalWrite(CRYSTAL1_PIN, HIGH);
      digitalWrite(CRYSTAL2_PIN, HIGH);
      digitalWrite(CRYSTAL3_PIN, HIGH);
      timerAlarmWrite(timer, PULSE_ON_MS * 1000, false);
    } else {
      // End of burst
      digitalWrite(CRYSTAL1_PIN, LOW);
      digitalWrite(CRYSTAL2_PIN, LOW);
      digitalWrite(CRYSTAL3_PIN, LOW);
      burst_active = false;
      burst_counter++;
      
      if (burst_counter >= BURSTS_PER_TREATMENT) {
        // End of treatment
        treatment_active = false;
        digitalWrite(LED_STATUS, LOW);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
        timerAlarmDisable(timer);
      } else {
        timerAlarmWrite(timer, BURST_OFF_MS * 1000, false);
      }
    }
  }
  timerAlarmEnable(timer);
}

void startTreatment() {
  if (treatment_active) return;
  
  // Check temperature
  float temp = readTemperature();
  if (temp > 10.0) {
    displayMessage("Water too warm. Chill to 4°C.");
    return;
  }
  
  // Check impedance
  for (int i = 0; i < 3; i++) {
    current_impedance[i] = readImpedance(i);
    if (current_impedance[i] > baseline_impedance[i] * 1.1) {
      displayMessage("Crystal " + String(i) + " degraded. Replace.");
      digitalWrite(LED_ALERT, HIGH);
      return;
    }
  }
  
  burst_counter = 0;
  treatment_active = true;
  digitalWrite(LED_STATUS, HIGH);
  timerAlarmWrite(timer, 0, false); // Start immediately
  timerAlarmEnable(timer);
}

void stopTreatment() {
  treatment_active = false;
  digitalWrite(CRYSTAL1_PIN, LOW);
  digitalWrite(CRYSTAL2_PIN, LOW);
  digitalWrite(CRYSTAL3_PIN, LOW);
  digitalWrite(LED_STATUS, LOW);
  timerAlarmDisable(timer);
}

void setup() {
  // Initialize pins
  pinMode(CRYSTAL1_PIN, OUTPUT);
  pinMode(CRYSTAL2_PIN, OUTPUT);
  pinMode(CRYSTAL3_PIN, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LED_ALERT, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  pinMode(BUTTON_STATUS, INPUT_PULLUP);
  
  // Initialize timer
  timer = timerBegin(0, 80, true); // 1 MHz clock
  timerAttachInterrupt(timer, &onTimer, true);
  
  // Read baseline impedance
  for (int i = 0; i < 3; i++) {
    baseline_impedance[i] = readImpedance(i);
  }
  
  // Display ready
  displayMessage("TERRA-HOME v1.3 Ready");
  digitalWrite(LED_STATUS, HIGH);
  delay(500);
  digitalWrite(LED_STATUS, LOW);
}

void loop() {
  // Check buttons
  if (digitalRead(BUTTON_START) == LOW) {
    startTreatment();
    delay(500); // debounce
  }
  if (digitalRead(BUTTON_STOP) == LOW) {
    stopTreatment();
    delay(500);
  }
  if (digitalRead(BUTTON_STATUS) == LOW) {
    displayStatus();
    delay(500);
  }
  
  delay(100);
}
