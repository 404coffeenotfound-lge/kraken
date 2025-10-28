# Modular UI API Architecture

## Problem Solved

**Before**: Monolithic `ui_internal.h` with all UI functions
- Gets bigger as more UI components are added
- User apps must include everything
- Hard to maintain
- Violates separation of concerns

**After**: Modular API with individual headers
- Each UI component has its own header
- User apps include only what they need
- Clean, scalable architecture
- Future-proof design

## New Public API Structure

```
components/display/include/kraken/
├── ui_api.h              ← Main header (includes all below)
├── ui_manager.h          ← UI lifecycle & events
├── ui_topbar.h           ← Status bar API
├── ui_menu.h             ← Main menu API
├── ui_widgets.h          ← Reusable widgets
└── ui_network.h          ← Network settings UI
```

## For User App Developers

### Option 1: Include Everything
```c
#include "kraken/ui_api.h"

// Now have access to:
// - ui_manager_*()
// - ui_topbar_*()
// - ui_menu_*()
// - ui_create_*()
// - ui_network_*()
// - All LVGL functions
```

### Option 2: Include Only What You Need
```c
// Minimal includes
#include "kraken/ui_widgets.h"    // Just need widgets
#include "kraken/kernel.h"         // For events

void my_simple_app(lv_obj_t *parent) {
    lv_obj_t *item = ui_create_menu_item(parent, "Option", LV_SYMBOL_OK);
}
```

## Benefits

### ✅ Scalability
Adding new UI components is easy:

1. Create `ui/ui_audio.c` (implementation)
2. Create `include/kraken/ui_audio.h` (public API)
3. Add `#include "kraken/ui_audio.h"` to `ui_api.h`
4. Done! Apps can now use audio UI

No changes to existing code needed!

### ✅ Maintainability
Each header is focused:
- `ui_topbar.h` - Only 5 functions (topbar API)
- `ui_menu.h` - Only 6 functions (menu API)
- `ui_widgets.h` - Only 3 functions (widgets)
- `ui_network.h` - Only 8 functions (network UI)

Easy to read, understand, and modify.

### ✅ Flexibility
User apps can:
- Use high-level Kraken widgets
- Use low-level LVGL directly
- Mix and match as needed

### ✅ Documentation
Each header is self-documenting with Doxygen comments:

```c
/**
 * @brief Update WiFi icon state
 * @param connected True if WiFi is connected
 * @param rssi Signal strength in dBm (-50 = strong, -70 = weak)
 */
void ui_topbar_update_wifi(bool connected, int8_t rssi);
```

## Internal Organization

Internal implementation still uses simplified internal header:

```c
// ui/ui_internal.h - Just includes public headers
#include "kraken/ui_manager.h"
#include "kraken/ui_topbar.h"
#include "kraken/ui_menu.h"
#include "kraken/ui_widgets.h"
#include "kraken/ui_network.h"

// No duplicate declarations needed!
```

## Example: Adding Audio UI (Future)

### Step 1: Create Implementation
```c
// ui/ui_audio.c
#include "kraken/ui_audio.h"

lv_obj_t *ui_audio_screen_create(lv_obj_t *parent) {
    // Create audio settings screen
}

void ui_audio_set_volume(uint8_t volume) {
    // Update volume
}
```

### Step 2: Create Public Header
```c
// include/kraken/ui_audio.h
#pragma once
#include "lvgl.h"

lv_obj_t *ui_audio_screen_create(lv_obj_t *parent);
void ui_audio_set_volume(uint8_t volume);
```

### Step 3: Add to Main API
```c
// include/kraken/ui_api.h
#include "kraken/ui_audio.h"  // Just add this line
```

### Step 4: Update CMakeLists.txt
```cmake
SRCS "ui/ui_audio.c"  # Add to build
```

**Done!** Apps can now use audio UI without touching existing code.

## Migration Guide

### For Existing Code

If you had:
```c
#include "ui/ui_internal.h"
```

Change to:
```c
#include "kraken/ui_api.h"
```

Everything else stays the same - APIs are identical!

### For New Apps

Use modular includes:
```c
// Only need widgets?
#include "kraken/ui_widgets.h"

// Need network UI?
#include "kraken/ui_network.h"

// Need everything?
#include "kraken/ui_api.h"
```

## File Size Comparison

### Before (Monolithic)
```
ui_internal.h: 90 lines (will grow to 200+ as features added)
```

### After (Modular)
```
ui_api.h:      80 lines  (includes + docs, doesn't grow)
ui_manager.h:  40 lines  (focused, stable)
ui_topbar.h:   35 lines  (focused, stable)
ui_menu.h:     40 lines  (focused, stable)
ui_widgets.h:  30 lines  (focused, stable)
ui_network.h:  45 lines  (focused, stable)
ui_audio.h:    40 lines  (future - doesn't affect existing headers)
ui_bluetooth.h 40 lines  (future - doesn't affect existing headers)
```

Total: 270 lines, but **distributed across focused files**.

## Summary

✅ **Modular** - Each UI component in its own header
✅ **Scalable** - Easy to add new components
✅ **Maintainable** - Small, focused files
✅ **Flexible** - Apps include only what they need
✅ **Future-proof** - New features don't break existing code
✅ **Well-documented** - Doxygen comments in headers
✅ **Clean API** - Single `ui_api.h` for convenience

**Perfect for a growing OS with multiple apps!** 🚀
