#include <Arduino.h>
#include <Wire.h>
#include <Trill.h>
#include <Audio.h>
#include <AudioStream.h>

// ── Audio objects ─────────────────────────────────────────────
AudioSynthWaveform    waveform;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  sgtl5000;

AudioConnection patchCord1(waveform, 0, audioOut, 0);
AudioConnection patchCord2(waveform, 0, audioOut, 1);

// ── Trill ─────────────────────────────────────────────────────
Trill trill;

// ── Config ────────────────────────────────────────────────────
const int   TOUCH_THRESHOLD = 100;
const float TONE_FREQ       = 440.0;
const float TONE_AMP        = 0.5;

bool toneActive = false;

// ── Helpers ───────────────────────────────────────────────────
bool anyChannelTouched() {
  while (trill.rawDataAvailable() > 0) {
    if (trill.rawDataRead() > TOUCH_THRESHOLD) return true;
  }
  return false;
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  AudioMemory(12);
  sgtl5000.enable();
  sgtl5000.volume(0.6);

  waveform.begin(WAVEFORM_SINE);
  waveform.frequency(TONE_FREQ);
  waveform.amplitude(0);

  Wire1.begin();
  if (trill.setup(Trill::TRILL_CRAFT, 0x30, &Wire1) != 0) {
    Serial.println("Trill init failed — check wiring");
    while (true) {}
  }
  trill.setMode(Trill::RAW);

  trill.setPrescaler(7); // lowest sensitivity, try 1-8

  Serial.println("Ready");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  trill.requestRawData();

  bool touched = anyChannelTouched();

  if (touched && !toneActive) {
    waveform.amplitude(TONE_AMP);
    toneActive = true;
    Serial.println("Touch — tone ON");
  } else if (!touched && toneActive) {
    waveform.amplitude(0);
    toneActive = false;
    Serial.println("Release — tone OFF");
  }

  delay(20);
}