# HTTP Audio Streaming Feature

## Overview

The audio service now supports two modes:
1. **Test Tone** - 440 Hz sine wave (when WiFi not connected)
2. **HTTP Stream** - Stream MP3/AAC audio from internet radio (when WiFi connected)

## How It Works

### Automatic Mode Selection

When you press Play:
- **WiFi Connected** → Streams music from HTTP URL
- **WiFi Not Connected** → Plays test tone

### Supported Audio Formats

The HTTP streaming supports:
- **MP3** - Most common format
- **AAC** - High quality, used by many internet radios
- **WAV** - Uncompressed (large bandwidth)

**Note:** The ESP32-S3 streams raw audio data to I2S. For compressed formats like MP3, you'll need a decoder (see Advanced section).

## Usage

### Default Stream URL

Currently set to: `http://stream.radioparadise.com/aac-320`
- This is a free internet radio station
- 320 kbps AAC quality
- No decoder needed (streams raw AAC)

### Change Stream URL

To use a different station, edit `/components/display/ui/ui_audio.c`:

```c
audio_set_url("http://your-stream-url-here");
```

### Free Internet Radio URLs

Here are some test URLs:

| Station | URL | Format |
|---------|-----|--------|
| Radio Paradise | `http://stream.radioparadise.com/aac-320` | AAC 320k |
| SomaFM Groove Salad | `http://ice1.somafm.com/groovesalad-128-mp3` | MP3 128k |
| Classical KING FM | `http://classicalking.streamguys1.com/king-fm-mp3` | MP3 |

## Implementation Details

### New API Functions

```c
// Set playback mode
esp_err_t audio_set_mode(audio_mode_t mode);
// Options: AUDIO_MODE_TEST_TONE, AUDIO_MODE_HTTP_STREAM

// Set HTTP stream URL  
esp_err_t audio_set_url(const char *url);
```

### Audio Task Behavior

```c
if (mode == AUDIO_MODE_HTTP_STREAM) {
    // HTTP Client connects to URL
    // Reads data in 4KB chunks
    // Writes directly to I2S
    // Streams until:
    //   - End of stream
    //   - User presses pause
    //   - Connection error
}
```

### Buffer Management

- **HTTP Buffer:** 4096 bytes
- **I2S Buffer:** 1024 samples (2048 bytes stereo)
- **Streaming:** Continuous, minimal latency

## Limitations & Notes

### 1. **No MP3 Decoder (Yet)**

The current implementation streams RAW data to I2S. This works for:
- ✅ Uncompressed WAV streams
- ✅ Raw PCM streams
- ❌ MP3 (needs decoder)
- ❌ AAC (needs decoder)

**Most internet radios use MP3/AAC**, so you'll hear noise unless the stream is raw PCM.

### 2. **Sample Rate**

Fixed at 44.1 kHz. If the stream is a different rate (e.g., 48 kHz), it will play at wrong speed.

### 3. **Network Dependency**

- Requires stable WiFi connection
- No buffering - if connection drops, audio stops
- Restart playback to reconnect

## Advanced: Adding MP3 Decoder

To properly decode MP3/AAC streams, you need to integrate a decoder library.

### Option A: ESP-ADF (ESP Audio Development Framework)

```bash
cd ~/esp
git clone https://github.com/espressif/esp-adf.git
```

Then in your project:
```c
#include "audio_element.h"
#include "mp3_decoder.h"

// Create MP3 decoder element
audio_element_handle_t mp3_decoder = mp3_decoder_init();

// Connect: HTTP → MP3 Decoder → I2S
```

### Option B: libmad (Lightweight MP3 decoder)

Add `libmad` component and decode in chunks:

```c
#include "mad.h"

// Decode MP3 frame by frame
// Output PCM to I2S
```

## Testing

### 1. Connect to WiFi

Use the Network menu to connect to your WiFi.

### 2. Navigate to Audio Menu

Press CENTER on "Audio" in main menu.

### 3. Press Play

- If WiFi connected: "Status: Streaming"
- If WiFi not connected: "Status: Playing" (test tone)

### 4. Expected Logs

```
I (xxx) ui_audio: Playing HTTP stream (WiFi connected)
I (xxx) audio_service: Audio mode set to: HTTP_STREAM
I (xxx) audio_service: Audio URL set to: http://stream.radioparadise.com/aac-320
I (xxx) audio_service: Starting HTTP stream from: http://stream.radioparadise.com/aac-320
I (xxx) audio_service: HTTP stream opened, content_length=-1
I (xxx) audio_service: Streamed 40960 bytes
I (xxx) audio_service: Streamed 81920 bytes
```

## Troubleshooting

### "HTTP stream ended or error"

**Causes:**
- URL is wrong
- Server is down
- WiFi disconnected
- Firewall blocking

**Fix:** Check URL, verify WiFi, try different station

### Noise/Static When Streaming

**Cause:** Stream is MP3/AAC but no decoder

**Fix:** Either:
- Find a raw PCM/WAV stream
- Implement MP3 decoder (see Advanced)

### Stream Stops After Few Seconds

**Cause:** Server timeout or connection issue

**Fix:**
- Increase timeout in `esp_http_client_config_t`
- Check network stability

## Future Enhancements

- [ ] Add MP3/AAC decoder
- [ ] Multiple station presets
- [ ] Display song metadata (if available in stream)
- [ ] Buffer management for stable playback
- [ ] Support for different sample rates
- [ ] Playlist support (M3U/PLS)
- [ ] Volume normalization

## Configuration

### Change Default URL

Edit `/components/display/ui/ui_audio.c`:

```c
audio_set_url("http://your-stream-here");
```

### Change Buffer Size

Edit `/components/audio/audio_service.c`:

```c
#define HTTP_BUFFER_SIZE 4096  // Increase for more buffering
```

### Add Station Selector UI

Create a station list in UI and call `audio_set_url()` based on selection.

---

**Enjoy streaming music on your ESP32-S3!** 🎵
