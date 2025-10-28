# Kraken OS Boot Animation

## Overview

The boot animation displays a dancing "Kraken" pixel logo on startup, creating a professional boot experience.

## Animation Sequence

```
Boot Start
    │
    ▼
┌────────────────────────────────────┐
│                                    │
│         K      K                   │  Phase 1: Rainbow Fade In (0-1s)
│          K    K                    │  - Pixels fade in with cycling colors
│           K  K                     │  - Rainbow effect (blue→green→cyan→red→magenta→yellow)
│            KK                      │
│            KK                      │
│           K  K                     │
│          K    K                    │
│         K      K                   │
│                                    │
│         Kraken OS                  │
└────────────────────────────────────┘
    │
    ▼
┌────────────────────────────────────┐
│                                    │
│         K      K                   │  Phase 2: Dancing Pixels (1-2.5s)
│          K    K                    │  - Pixels pulse in size
│           K  K                     │  - Wave animation effect
│            KK                      │  - Continuous color cycling
│            KK                      │
│           K  K                     │
│          K    K                    │
│         K      K                   │
│                                    │
│         Kraken OS                  │
└────────────────────────────────────┘
    │
    ▼
┌────────────────────────────────────┐
│                                    │
│         K      K                   │  Phase 3: Fade to Cyan (2.5-3s)
│          K    K                    │  - Smooth transition to final color
│           K  K                     │  - Pixels stabilize to normal size
│            KK                      │  - Add glow border effect
│            KK                      │  - Bright cyan (Kraken theme color)
│           K  K                     │
│          K    K                    │
│         K      K                   │
│                                    │
│         Kraken OS                  │
└────────────────────────────────────┘
    │
    ▼
Animation Complete → Show Main UI
```

## Technical Details

### Pixel Pattern (8x8)
```
K . . . . . . K
. K . . . . K .
. . K . . K . .
. . . K K . . .
. . . K K . . .
. . K . . K . .
. K . . . . K .
K . . . . . . K
```

Forms a stylized "K" pattern representing Kraken.

### Animation Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Duration | 3000ms | Total animation time |
| Update Rate | 50ms | 20 FPS smooth animation |
| Pixel Size | 8x8 px | Each pixel block |
| Grid Size | 8x8 | 64 total pixels (24 active) |
| Colors | 6 | Rainbow palette |

### Phase Breakdown

#### Phase 1: Rainbow Fade In (0-1000ms)
- **Effect**: Pixels appear with rainbow colors
- **Logic**: Each pixel cycles through color palette
- **Formula**: `color_idx = ((x + y) + (time / 100)) % 6`
- **Opacity**: Fades from 0% to 100%

#### Phase 2: Dancing Effect (1000-2500ms)
- **Effect**: Pixels pulse and change colors
- **Logic**: Sine wave controls size
- **Formula**: `size = 8 + sin(phase + (x+y)*0.5) * 2`
- **Wave**: Creates ripple effect across logo
- **Colors**: Continuous cycling

#### Phase 3: Final Transition (2500-3000ms)
- **Effect**: Stabilize to cyan color
- **Final Color**: `#00FFFF` (Cyan - Kraken theme)
- **Border**: Add 2px glow effect
- **Size**: Reset to 8x8px

## API Usage

### Basic Usage
```c
#include "kraken/ui_boot_animation.h"

void boot_complete_callback(void) {
    ESP_LOGI("BOOT", "Animation done, show main UI");
    // Initialize main UI here
}

void app_main(void) {
    // ... initialize display ...
    
    // Start boot animation
    ui_boot_animation_start(lv_screen_active(), boot_complete_callback);
    
    // Animation runs asynchronously
    // callback will be invoked when done
}
```

### Integration with UI Manager
```c
// ui_manager.c
esp_err_t ui_manager_init(lv_obj_t *screen) {
    // Start with boot animation
    ui_boot_animation_start(screen, boot_animation_complete);
    
    // Main UI initialized in callback after animation
    return ESP_OK;
}

static void boot_animation_complete(void) {
    // Now create topbar, menu, etc.
    ui_topbar_init(screen);
    ui_menu_init(screen);
    // Subscribe to events
    kraken_event_subscribe(...);
}
```

## Customization

### Change Colors
```c
// In ui_boot_animation.c
static const lv_color_t colors[] = {
    {.full = 0xF800},  // Red
    {.full = 0xFFE0},  // Yellow
    {.full = 0x07E0},  // Green
    // Add your colors here
};
```

### Adjust Duration
```c
#define ANIMATION_DURATION_MS 3000  // Change to 2000 for faster boot
```

### Different Pattern
```c
// Create your own 8x8 pattern
static const char my_pattern[8][9] = {
    "########",
    "#......#",
    "#.####.#",
    "#.#..#.#",
    "#.#..#.#",
    "#.####.#",
    "#......#",
    "########"
};
```

## Performance

- **CPU Usage**: Low (~5% during animation)
- **Memory**: ~2KB for pixel objects
- **Frame Rate**: 20 FPS (smooth on ESP32)
- **Auto Cleanup**: All objects deleted after animation

## Event Blocking

- User input is ignored during boot animation
- `ui_manager_handle_event()` returns early if `!boot_animation_done`
- Prevents accidental menu navigation during boot

## Files

```
components/display/
├── include/kraken/
│   └── ui_boot_animation.h     (Public API)
└── ui/
    └── ui_boot_animation.c     (Implementation)
```

## Future Enhancements

- **Sound effects**: Add boot sound
- **Progress bar**: Show loading progress
- **Custom logos**: Support user-defined patterns
- **Smooth transitions**: Better handoff to main UI
- **Skip option**: Allow user to skip animation

---

**The boot animation adds a professional, polished feel to Kraken OS!** 🎉
