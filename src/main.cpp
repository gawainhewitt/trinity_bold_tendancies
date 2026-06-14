#include <Arduino.h>
#include <Wire.h>
#include <Trill.h>
#include <Audio.h>
#include <AudioStream.h>
#include <SD.h>
#include <SPI.h>

// ── Audio objects ─────────────────────────────────────────────
AudioPlaySdWav        player;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  sgtl5000;

AudioConnection patchCord1(player, 0, audioOut, 0);
AudioConnection patchCord2(player, 1, audioOut, 1);

// ── Trill ─────────────────────────────────────────────────────
Trill trill;

// ── Constants ─────────────────────────────────────────────────
const int   NUM_BIRDS          = 13;
const int   CAL_THRESHOLD      = 100;   // added on top of baseline
const int   CAL_DURATION_MS    = 1000;  // calibration sample window
const int   CAL_SAMPLES        = 50;    // reads during calibration
const int   DEBOUNCE_MS        = 150;   // per-channel re-trigger lockout
const long  IDLE_TIMEOUT_MS    = 10UL * 60UL * 1000UL; // 10 minutes
const int   PIN_CAL_BUTTON     = 14;
const int   PIN_CAL_LED        = 15;
const int   SD_CS_PIN          = 10;    // Audio Shield SD chip select
const int   CAL_FLASH_INTERVAL = 150;   // LED flash speed during calibration

const char* WAV_NAMES[NUM_BIRDS] = {
  "ONE.WAV", "TWO.WAV", "THREE.WAV", "FOUR.WAV", "FIVE.WAV",
  "SIX.WAV", "SEVEN.WAV", "EIGHT.WAV", "NINE.WAV", "TEN.WAV",
  "ELEVEN.WAV", "TWELVE.WAV", "THIRTEEN.WAV"
};

const char* CAL_FILE = "BASELINE.CAL";

// ── State ─────────────────────────────────────────────────────
uint16_t baseline[NUM_BIRDS]      = {0};
uint16_t touchThreshold[NUM_BIRDS] = {0};
uint32_t lastTriggerTime[NUM_BIRDS] = {0}; // for per-channel debounce

bool     wasAboveThreshold[NUM_BIRDS] = {false}; // edge detection

uint32_t lastTouchTime = 0;
bool     ambientMode   = false;
int      currentBird   = -1; // -1 = nothing playing / ambient

// ── Forward declarations ───────────────────────────────────────
void calibrate();
bool loadCalibration();
void saveCalibration();
void playBird(int index);
void playRandomAmbient();
void ledSolid();
void ledOff();
void flashLED(int times, int intervalMs);

// ── LED helpers ───────────────────────────────────────────────
void ledSolid() { digitalWrite(PIN_CAL_LED, HIGH); }
void ledOff()   { digitalWrite(PIN_CAL_LED, LOW);  }

void flashLED(int times, int intervalMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_CAL_LED, HIGH);
    delay(intervalMs);
    digitalWrite(PIN_CAL_LED, LOW);
    delay(intervalMs);
  }
}

// ── Calibration ───────────────────────────────────────────────
void calibrate() {
  Serial.println("Calibrating...");

  // Flash LED during calibration
  uint32_t calStart = millis();
  uint32_t accumulator[NUM_BIRDS] = {0};
  int      counts[NUM_BIRDS]      = {0};
  bool     ledState               = false;
  uint32_t lastFlash              = millis();

  while (millis() - calStart < CAL_DURATION_MS) {
    // Flash LED
    if (millis() - lastFlash >= CAL_FLASH_INTERVAL) {
      ledState = !ledState;
      digitalWrite(PIN_CAL_LED, ledState ? HIGH : LOW);
      lastFlash = millis();
    }

    trill.requestRawData();
    int ch = 0;
    while (trill.rawDataAvailable() > 0 && ch < NUM_BIRDS) {
      accumulator[ch] += trill.rawDataRead();
      counts[ch]++;
      ch++;
    }
    delay(CAL_DURATION_MS / CAL_SAMPLES);
  }

  for (int i = 0; i < NUM_BIRDS; i++) {
    baseline[i]       = (counts[i] > 0) ? (accumulator[i] / counts[i]) : 0;
    touchThreshold[i] = baseline[i] + CAL_THRESHOLD;
    Serial.printf("  Ch %2d  baseline=%d  threshold=%d\n", i, baseline[i], touchThreshold[i]);
  }

  saveCalibration();
  ledSolid();
  lastTouchTime = millis();
  Serial.println("Calibration done.");
}

bool loadCalibration() {
  File f = SD.open(CAL_FILE, FILE_READ);
  if (!f) return false;

  for (int i = 0; i < NUM_BIRDS; i++) {
    if (f.available() < 4) { f.close(); return false; }
    uint16_t b, t;
    f.read((uint8_t*)&b, 2);
    f.read((uint8_t*)&t, 2);
    baseline[i]       = b;
    touchThreshold[i] = t;
  }
  f.close();
  Serial.println("Calibration loaded from SD.");
  return true;
}

void saveCalibration() {
  SD.remove(CAL_FILE);
  File f = SD.open(CAL_FILE, FILE_WRITE);
  if (!f) { Serial.println("Could not save calibration."); return; }
  for (int i = 0; i < NUM_BIRDS; i++) {
    f.write((uint8_t*)&baseline[i],       2);
    f.write((uint8_t*)&touchThreshold[i], 2);
  }
  f.close();
  Serial.println("Calibration saved to SD.");
}

// ── Playback ──────────────────────────────────────────────────
void playBird(int index, uint32_t now) {
  if (index < 0 || index >= NUM_BIRDS) return;
  player.stop();
  player.play(WAV_NAMES[index]);
  currentBird = index;
  ambientMode = false;
  lastTouchTime = now;
  Serial.printf("Playing: %s\n", WAV_NAMES[index]);
}

void playRandomAmbient() {
  int next;
  // avoid repeating the same track immediately if possible
  do {
    next = random(NUM_BIRDS);
  } while (next == currentBird && NUM_BIRDS > 1);

  player.stop();
  player.play(WAV_NAMES[next]);
  currentBird = next;
  Serial.printf("Ambient: %s\n", WAV_NAMES[next]);
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_CAL_BUTTON, INPUT_PULLUP);
  pinMode(PIN_CAL_LED,    OUTPUT);
  ledOff();

  // Audio
  AudioMemory(24);
  sgtl5000.enable();
  sgtl5000.volume(0.7);

  // SD
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD init failed — check card");
    // Don't halt — continue without SD persistence
  }

  // Trill on Wire1
  Wire1.begin();
  if (trill.setup(Trill::TRILL_CRAFT, 0x30, &Wire1) != 0) {
    Serial.println("Trill init failed — check wiring");
    while (true) {}
  }
  trill.setMode(Trill::RAW);
  trill.setPrescaler(7);

  // Calibration: load from SD or do live capture
  if (!loadCalibration()) {
    calibrate();
  }

  randomSeed(analogRead(A0));
  lastTouchTime = millis();
  ledSolid();
  Serial.println("Ready.");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── Calibration button ──────────────────────────────────────
  if (digitalRead(PIN_CAL_BUTTON) == LOW) {
    delay(50); // debounce the button itself
    if (digitalRead(PIN_CAL_BUTTON) == LOW) {
      player.stop();
      calibrate();
      // Wait for button release
      while (digitalRead(PIN_CAL_BUTTON) == LOW) {}
      ambientMode = false;
      lastTouchTime = millis();
      now = millis(); // refresh so idle check doesn't fire this iteration
    }
  }

  // ── Read Trill ──────────────────────────────────────────────
  trill.requestRawData();

  uint16_t raw[NUM_BIRDS] = {0};
  int ch = 0;
  while (trill.rawDataAvailable() > 0 && ch < NUM_BIRDS) {
    raw[ch++] = trill.rawDataRead();
  }

  // ── Touch detection — edge on crossing threshold ─────────────
  int triggeredBird = -1;
  for (int i = 0; i < NUM_BIRDS; i++) {
    bool above = (raw[i] > touchThreshold[i]);

    if (above && !wasAboveThreshold[i]) {
      // Rising edge — check debounce
      if (now - lastTriggerTime[i] >= DEBOUNCE_MS) {
        triggeredBird       = i;        // last one wins if multiple
        lastTriggerTime[i]  = now;
      }
    }
    wasAboveThreshold[i] = above;
  }

  if (triggeredBird >= 0) {
    playBird(triggeredBird, now);
  }

  // ── Ambient mode ────────────────────────────────────────────
  bool justStartedTrack = false;

  if (!ambientMode && (now - lastTouchTime >= IDLE_TIMEOUT_MS)) {
    ambientMode = true;
    justStartedTrack = true;
    Serial.println("Entering ambient mode.");
    playRandomAmbient();
  }

  if (ambientMode && !justStartedTrack && !player.isPlaying()) {
    playRandomAmbient();
  }

  // ── Triggered track finished ─────────────────────────────────
  // Nothing to do — WAV plays to completion unless interrupted.

  delay(10);
}
