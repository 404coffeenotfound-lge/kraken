# MAX98357A I2S Audio Configuration

## Pin Configuration

All hardware pin assignments are centralized in the **BSP (Board Support Package)**.

To configure your MAX98357A pins, edit:
```
/components/bsp/bsp.c
```

Look for the `s_audio_config` structure:

```c
static const board_audio_config_t s_audio_config = {
    .port = I2S_NUM_0,
    .pin_bclk = 4,       // MAX98357A BCLK
    .pin_lrclk = 5,      // MAX98357A LRC (Word Select)
    .pin_dout = 6,       // MAX98357A DIN (Data Input)
    .pin_din = -1,       // Not used (MAX98357A is output only)
    .pin_sd = 7,         // MAX98357A SD (Shutdown control)
};
```

## Current Default Wiring

| MAX98357A Pin | ESP32-S3 GPIO | Description |
|---------------|---------------|-------------|
| LRC (LRCLK)   | GPIO 5        | Left/Right Clock (Word Select) |
| BCLK          | GPIO 4        | Bit Clock |
| DIN           | GPIO 6        | Data Input |
| SD (Shutdown) | GPIO 7        | Shutdown Control (HIGH=enabled, LOW=shutdown) |
| GND           | GND           | Ground |
| Vin           | 3.3V or 5V    | Power Supply (check your module specs) |
| GAIN          | GND or VIN    | Volume Gain Control |

## GAIN Pin Configuration

The GAIN pin sets the output amplification level:

- **GAIN to GND**: 9dB gain
- **GAIN floating**: 12dB gain  
- **GAIN to VIN**: 15dB gain

Start with GAIN to GND for moderate volume. Increase if needed.

## Important Notes

1. **Speaker Connection**: Connect an 4-8Ω speaker to the speaker terminals on the MAX98357A
2. **Power Supply**: Make sure Vin provides enough current (at least 500mA for loud volume)
3. **SD Pin**: The SD (shutdown) pin is used for mute/unmute control
4. **Volume Control**: Software volume is implemented by scaling audio data. Hardware gain is set via the GAIN pin.

## Pin Configuration in Code

**All pin assignments are managed by the BSP (Board Support Package).**

To change GPIO pins, edit `/components/bsp/bsp.c` and modify the `s_audio_config` structure.

This centralizes all hardware configuration in one place for easy board customization.

## Testing

After wiring:
1. Build and flash: `idf.py build flash monitor`
2. Navigate to Audio menu
3. Press play/pause to test audio output
4. You should hear audio through the speaker

## Troubleshooting

- **No sound**: Check SD pin is HIGH (enabled), verify wiring
- **Distorted sound**: Reduce GAIN pin setting or lower software volume
- **Clicking/popping**: Normal when enabling/disabling, can add capacitor to SD pin
