# Bold Tendencies — Bird Sound Installation

A sound installation created for **Animate Artists X Bold Tendencies**, a collaboration between [Trinity Laban Conservatoire of Music and Dance](https://www.trinitylaban.ac.uk/whats-on/animate-x-bold-tendencies/) and [Bold Tendencies](https://boldtendencies.com/), Peckham.

13 laser-cut plywood birds hang in the rooftop car park at Bold Tendencies, 95a Rye Lane, SE15. Each bird has a conductive foil surface — touching one triggers a unique audio track. The piece was developed collaboratively with young musicians during the spring half-term 2026 as part of Trinity Laban's Animate programme, inspired by Bold Tendencies' 2026 artistic theme *Euphoria* and the music of Olivier Messiaen.

**Sound design & electronics: [Gawain Hewitt](https://gawainhewitt.co.uk)**

---

## How it works

- Touch any bird → plays its associated WAV file
- Touching a second bird (or the same one again) cuts the current track and starts the new one immediately
- After 10 minutes of no interaction, the installation enters ambient mode, cycling through the 13 tracks in random order continuously
- Any touch interrupts ambient mode and returns to triggered playback

---

## Hardware

| Component | Detail |
|---|---|
| Microcontroller | Teensy 4.0 |
| Audio | Teensy Audio Shield Rev D |
| Touch sensing | Trill Craft (I2C address 0x30, Wire1) |
| Calibration button | Pin 14 (INPUT_PULLUP, active low) |
| Status LED | Pin 15 (220Ω resistor) |
| Storage | SD card (Audio Shield slot) |

Trill Craft connects on Wire1 (pins 16/17). Audio Shield on Wire0 (pins 18/19). Do not share I2C buses.

---

## SD Card

WAV files go in the **root** of the SD card, named:

```
ONE.WAV
TWO.WAV
THREE.WAV
FOUR.WAV
FIVE.WAV
SIX.WAV
SEVEN.WAV
EIGHT.WAV
NINE.WAV
TEN.WAV
ELEVEN.WAV
TWELVE.WAV
THIRTEEN.WAV
```

16-bit, 44.1kHz stereo WAV format recommended.

A calibration file `BASELINE.CAL` is written automatically — do not delete it mid-install.

---

## Calibration

On boot the system loads saved baselines from SD. If none exist it performs a live calibration automatically.

To recalibrate (e.g. after wiring changes or environmental shifts):
1. Press and hold the button on pin 14
2. LED flashes during the 1-second capture window — keep hands off the birds
3. LED returns to solid when done

Baselines are saved to SD and persist across power cycles.

---

## LED status

| State | LED |
|---|---|
| Ready | Solid on |
| Calibrating | Flashing |

---

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
pio run --target upload
```

**Important:** Audio library must come from the Teensy framework bundle, not the GitHub version — there is an I2S compatibility issue with PlatformIO Teensy 1.59. The `platformio.ini` is already configured correctly for this.

---

## License

MIT — see [LICENSE](LICENSE)
