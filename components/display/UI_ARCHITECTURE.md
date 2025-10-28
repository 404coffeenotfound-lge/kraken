# Kraken UI System Documentation

## Architecture Overview

The UI system is built with a modular architecture using LVGL (Light and Versatile Graphics Library) and integrates with the Kraken kernel event system.

```
┌─────────────────────────────────────────────────────────────┐
│                     Display Service                          │
│  - LCD/SPI initialization                                    │
│  - LVGL port setup                                           │
│  - Periodic timer for UI updates                             │
└───────────────┬─────────────────────────────────────────────┘
                │
                ▼
┌─────────────────────────────────────────────────────────────┐
│                      UI Manager                              │
│  - Event subscription and handling                           │
│  - Coordinates topbar and menu                               │
│  - Status state management                                   │
└─────────┬───────────────────────────────────┬───────────────┘
          │                                   │
          ▼                                   ▼
┌──────────────────────┐           ┌──────────────────────────┐
│     UI Top Bar       │           │       UI Menu            │
│ ┌──────────────────┐ │           │ ┌──────────────────────┐ │
│ │ WiFi│BT│Battery │ │           │ │ ▶ Audio              │ │
│ │   12:34:56 PM    │ │           │ │   Network            │ │
│ └──────────────────┘ │           │ │   Bluetooth          │ │
│                      │           │ │   Apps               │ │
│ - Clock display      │           │ │   Settings           │ │
│ - Status icons       │           │ │   About              │ │
│ - Event listeners    │           │ └──────────────────────┘ │
└──────────────────────┘           │                          │
                                   │ - 5-way navigation       │
                                   │ - Menu selection         │
                                   │ - Callbacks              │
                                   └──────────────────────────┘
```

## File Structure

```
components/display/
├── display_service.c          # Core display/LCD initialization
├── include/
│   └── kraken/
│       └── display_service.h  # Public display API
└── ui/
    ├── ui_internal.h          # UI internal types and APIs
    ├── ui_manager.c           # Main UI orchestrator
    ├── ui_topbar.c            # Status bar component
    ├── ui_menu.c              # Main menu component
    └── ui_widgets.c           # Reusable UI widgets
```

## Components

### 1. Display Service (`display_service.c`)

**Responsibility**: Low-level display hardware setup

```c
esp_err_t display_service_init(void);
esp_err_t display_service_deinit(void);
```

**Features**:
- SPI bus initialization
- LCD panel setup (ST7789)
- LVGL port configuration
- Creates periodic timer for UI updates (1Hz)

---

### 2. UI Manager (`ui_manager.c`)

**Responsibility**: High-level UI orchestration and event handling

```c
esp_err_t ui_manager_init(lv_obj_t *screen);
esp_err_t ui_manager_deinit(void);
void ui_manager_periodic_update(void);  // Called every second
```

**Event Subscriptions**:
- `KRAKEN_EVENT_WIFI_CONNECTED` → Update WiFi icon
- `KRAKEN_EVENT_WIFI_DISCONNECTED` → Update WiFi icon
- `KRAKEN_EVENT_BT_CONNECTED` → Update BT icon
- `KRAKEN_EVENT_BT_DISCONNECTED` → Update BT icon
- `KRAKEN_EVENT_SYSTEM_TIME_SYNC` → Sync and display clock
- `KRAKEN_EVENT_INPUT_UP/DOWN/LEFT/RIGHT/CENTER` → Menu navigation

**State Management**:
```c
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;           // Signal strength
    bool bt_enabled;
    bool bt_connected;
    uint8_t battery_percent;
    bool battery_charging;
    struct tm current_time;
    bool time_synced;
} ui_status_t;
```

---

### 3. UI Top Bar (`ui_topbar.c`)

**Responsibility**: Status bar with clock and icons

**Layout**:
```
┌────────────────────────────────────────────────┐
│ WiFi BT     12:34:56 PM          Battery 75%  │
└────────────────────────────────────────────────┘
```

**APIs**:
```c
esp_err_t ui_topbar_init(lv_obj_t *parent);
void ui_topbar_update_time(struct tm *time);
void ui_topbar_update_wifi(bool connected, int8_t rssi);
void ui_topbar_update_bluetooth(bool enabled, bool connected);
void ui_topbar_update_battery(uint8_t percent, bool charging);
```

**Icon States**:
- **WiFi**: 
  - Gray = Disconnected
  - Green = Strong signal (> -50 dBm)
  - Yellow = Medium signal (> -70 dBm)
  - Orange = Weak signal
  
- **Bluetooth**:
  - Dark Gray = Off
  - Gray = On but not connected
  - Blue = Connected
  
- **Battery**:
  - Green = > 50%
  - Yellow = 20-50%
  - Red = < 20%
  - Cyan = Charging

---

### 4. UI Menu (`ui_menu.c`)

**Responsibility**: Main menu with 5-way navigation

**Menu Items**:
1. Audio (🔊)
2. Network (📶)
3. Bluetooth (Bluetooth symbol)
4. Apps (📋)
5. Settings (⚙️)
6. About (ℹ️)

**APIs**:
```c
esp_err_t ui_menu_init(lv_obj_t *parent);
void ui_menu_navigate(kraken_event_type_t direction);
void ui_menu_select_current(void);
void ui_menu_set_callback(ui_menu_callback_t callback);
```

**Navigation**:
- `INPUT_UP` → Previous menu item
- `INPUT_DOWN` → Next menu item
- `INPUT_CENTER` → Select current item
- `INPUT_LEFT/RIGHT` → Reserved for sub-menus

**Visual Feedback**:
- Selected item: Blue background (#0080FF)
- Normal item: Gray background (#222222)

---

### 5. UI Widgets (`ui_widgets.c`)

**Responsibility**: Reusable UI components

```c
lv_obj_t *ui_create_menu_item(lv_obj_t *parent, const char *title, const char *icon);
lv_obj_t *ui_create_icon_label(lv_obj_t *parent, const char *symbol, const char *text);
void ui_set_menu_item_selected(lv_obj_t *item, bool selected);
```

---

## Event Flow Examples

### Example 1: WiFi Connection

```
1. WiFi service connects to AP
   ↓
2. WiFi service posts: KRAKEN_EVENT_WIFI_CONNECTED
   ↓
3. UI Manager receives event
   ↓
4. UI Manager updates internal state:
   - status.wifi_connected = true
   - status.wifi_rssi = -50
   ↓
5. UI Manager calls: ui_topbar_update_wifi(true, -50)
   ↓
6. Top bar changes WiFi icon to GREEN
```

### Example 2: Time Synchronization

```
1. System service detects WiFi connected
   ↓
2. System service syncs time via SNTP
   ↓
3. System service posts: KRAKEN_EVENT_SYSTEM_TIME_SYNC
   ↓
4. UI Manager receives event
   ↓
5. UI Manager gets current time:
   - time(&now)
   - localtime_r(&now, &status.current_time)
   ↓
6. UI Manager calls: ui_topbar_update_time(&status.current_time)
   ↓
7. Top bar displays: "12:34:56"
   ↓
8. Periodic timer updates clock every second
```

### Example 3: Menu Navigation

```
1. User presses "Down" button
   ↓
2. System service posts: KRAKEN_EVENT_INPUT_DOWN
   ↓
3. UI Manager receives event
   ↓
4. UI Manager calls: ui_menu_navigate(KRAKEN_EVENT_INPUT_DOWN)
   ↓
5. Menu system:
   - Deselects current item (gray background)
   - Increments selected_index
   - Selects new item (blue background)
   ↓
6. Screen updates visually
```

---

## Integration with System Service

The **System Service** is responsible for:

1. **Time Management**:
```c
// In system_service.c
static void wifi_connected_handler(const kraken_event_t *event, void *user_data)
{
    // Start SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Wait for time sync...
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < 10) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    
    // Broadcast time sync event
    kraken_event_post(KRAKEN_EVENT_SYSTEM_TIME_SYNC, NULL, 0);
}
```

2. **Input Event Generation**:
```c
// In system_service.c - GPIO interrupt handler
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int gpio_num = (int)arg;
    kraken_event_type_t event;
    
    switch (gpio_num) {
        case BUTTON_UP_PIN:
            event = KRAKEN_EVENT_INPUT_UP;
            break;
        case BUTTON_DOWN_PIN:
            event = KRAKEN_EVENT_INPUT_DOWN;
            break;
        case BUTTON_CENTER_PIN:
            event = KRAKEN_EVENT_INPUT_CENTER;
            break;
        // ...
    }
    
    kraken_event_post_from_isr(event, NULL, 0);
}
```

---

## Best Practices

1. **Thread Safety**: All LVGL calls should be protected with `display_lock()` / `display_unlock()` if called from non-LVGL tasks

2. **Event-Driven**: UI updates are triggered by kernel events, not polling

3. **Modularity**: Each UI component is independent and can be reused

4. **Performance**: Periodic updates limited to 1Hz to reduce CPU usage

5. **Scalability**: Easy to add new menu items or status icons

---

## Future Enhancements

- **Sub-menus**: Implement screens for each menu item
- **Touch support**: Add touch panel driver integration
- **Animations**: Add smooth transitions between screens
- **Themes**: Support light/dark themes
- **Localization**: Multi-language support
- **Custom widgets**: Dialog boxes, progress bars, etc.
