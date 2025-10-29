# MAX98357A Audio Troubleshooting Guide

## Hardware Checklist

### 1. Verify Connections (CRITICAL!)

```
ESP32-S3          MAX98357A
────────          ─────────
GPIO 4     ───►   BCLK
GPIO 5     ───►   LRC (also called LRCLK or WS)
GPIO 6     ───►   DIN
GPIO 7     ───►   SD (Shutdown)
GND        ───►   GND
5V or 3.3V ───►   Vin
GND or Vin ───►   GAIN
```

### 2. Power Requirements
- **Vin**: 2.5V to 5.5V (5V recommended for louder output)
- **Current**: At least 500mA available for loud volume
- **Speaker**: 4Ω or 8Ω (3W MAX)

### 3. GAIN Pin Settings
| GAIN Connection | Amplification | Use Case |
|----------------|---------------|----------|
| To GND         | 9dB          | Quiet/normal volume |
| Floating       | 12dB         | Medium volume |
| To Vin         | 15dB         | Maximum volume |

**Start with GAIN to GND!**

## Software Diagnostics

### Build and Monitor:
```bash
idf.py build flash monitor
```

### Expected Boot Logs:
```
I (xxx) audio_service: MAX98357A I2S audio initialized (from BSP config)
I (xxx) audio_service: I2S Pins - BCLK:4, WS/LRC:5, DOUT/DIN:6, SD:7
I (xxx) audio_service: Configuring I2S: Sample Rate=44100, 16-bit Stereo, Philips mode
I (xxx) audio_service: I2S preloaded with XX bytes of silence
I (xxx) audio_service: Test tone: 440 Hz
I (xxx) audio_service: Audio playback task started
```

### When You Press Play:
```
I (xxx) audio_service: SD pin (GPIO 7) set HIGH
I (xxx) audio_service: Audio playback started (volume=50%)
I (xxx) audio_service: Audio playing: 100 buffers written, volume=50%
I (xxx) audio_service: Audio playing: 200 buffers written, volume=50%
```

**If you DON'T see "buffers written" messages, I2S is not working!**

## Common Issues and Solutions

### Issue 1: No sound, but logs show "buffers written"

**Cause:** Wiring problem or hardware issue

**Check:**
1. **DIN connection** - This carries the audio data!
   - Verify GPIO 6 → MAX98357A DIN pin
   - Check for loose connection
   - Try different wire

2. **SD pin** - Must be HIGH for audio
   - Should see "SD pin (GPIO 7) set HIGH" in logs
   - Measure with multimeter: Should be 3.3V when playing
   - If LOW (0V), amplifier is muted!

3. **Power to MAX98357A**
   - Measure Vin pin: Should be 3.3V or 5V
   - If no voltage, check power connection

4. **Speaker connection**
   - Speaker must be connected to + and - on MAX98357A
   - Try different speaker (4Ω or 8Ω)
   - Check speaker isn't broken (test with another device)

### Issue 2: No "buffers written" messages

**Cause:** I2S not working

**Check:**
1. **GPIO pins available?**
   - GPIOs 4, 5, 6, 7 must not be used elsewhere
   - Check no pin conflicts in BSP

2. **I2S initialization failed?**
   - Look for error messages in boot logs
   - "Failed to create I2S channel"?
   - "Failed to enable I2S channel"?

### Issue 3: Clicking/popping sounds only

**Cause:** I2S clock running but no clean audio

**Try:**
1. Different sample rate (try 22050 Hz)
2. Check BCLK and LRC connections
3. Verify ground connection is solid

### Issue 4: Very quiet or distorted sound

**Solutions:**
1. **Increase GAIN**: Move GAIN pin from GND to Vin (15dB)
2. **Increase software volume**: Use LEFT/RIGHT buttons
3. **Use 5V power**: Not 3.3V
4. **Check speaker impedance**: 4Ω is louder than 8Ω

## Hardware Test with Multimeter

### When Playing Audio:

| Pin  | Expected Voltage | What It Means |
|------|-----------------|---------------|
| SD   | 3.3V (HIGH)     | Amplifier enabled |
| Vin  | 3.3V or 5.0V    | Power OK |
| GND  | 0V              | Ground OK |
| BCLK | Oscillating     | Clock signal present |
| LRC  | Oscillating     | Word clock present |
| DIN  | Varying 0-3.3V  | Audio data present |

**Important:** BCLK should oscillate at ~1.4 MHz (44100 Hz × 32 bits)

## Test Procedure

1. **Connect multimeter to SD pin**
   - Should be ~0V when not playing
   - Should jump to ~3.3V when you press Play

2. **Listen for clicking**
   - When you press Play/Pause, you should hear a small "pop"
   - This confirms SD pin is working

3. **Measure Vin and GND**
   - Vin should be stable 3.3V or 5V
   - If voltage drops when playing, power supply issue

4. **Check BCLK with oscilloscope (if available)**
   - Should see 1.4 MHz square wave
   - If present, I2S is working

## Still No Sound?

### Verify MAX98357A Module is Good:

**Test 1: Connect SD pin directly to 3.3V**
- Remove SD wire from GPIO 7
- Connect SD to 3.3V
- Flash code and amplifier stays on
- Press Play
- Should hear tone continuously

**Test 2: Try different GPIO pins**
Edit `/components/bsp/bsp.c`:
```c
.pin_bclk = 10,
.pin_lrclk = 11,
.pin_dout = 12,
.pin_sd = 13,
```

**Test 3: Different MAX98357A module**
- Module might be defective
- Try another board if available

## Contact for Help

If still no sound after all checks, provide:
1. Full boot log
2. Log when pressing Play
3. Multimeter readings from Vin, GND, SD pins
4. Photo of your wiring
5. MAX98357A module brand/version
