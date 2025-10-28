# Network Submenu - Complete WiFi Management UI

## Overview

The Network submenu provides a complete WiFi management interface with:
- WiFi ON/OFF toggle
- Auto-scan and display of nearby networks
- Signal strength-based sorting
- Password input with virtual keyboard
- Connection status notifications
- 5-way navigation support

## User Flow

```
Main Menu
    │
    ├─> User selects "Network" (CENTER or RIGHT button)
    │
    ▼
┌─────────────────────────────────────────┐
│ Network Settings                        │
│                                         │
│ ┌────────────────────────────────────┐  │
│ │ 📶 WiFi: OFF                       │  │ ◄─ Toggle button
│ └────────────────────────────────────┘  │
│                                         │
│ (Network list - empty when OFF)         │
└─────────────────────────────────────────┘
    │
    ├─> User clicks WiFi toggle
    │
    ▼
WiFi turns ON → Auto scan → Display networks
    │
    ▼
┌─────────────────────────────────────────┐
│ Network Settings                        │
│                                         │
│ ┌────────────────────────────────────┐  │
│ │ 📶 WiFi: ON                        │  │ (Green)
│ └────────────────────────────────────┘  │
│                                         │
│ ┌────────────────────────────────────┐  │
│ │ MyHomeWiFi    📶 Strong (-45dBm)  │  │ ◄─ Sorted by signal
│ └────────────────────────────────────┘  │
│ ┌────────────────────────────────────┐  │
│ │ CoffeeShop    📶 Medium (-65dBm)  │  │
│ └────────────────────────────────────┘  │
│ ┌────────────────────────────────────┐  │
│ │ Neighbor      📶 Weak (-80dBm)    │  │
│ └────────────────────────────────────┘  │
│                                         │
│ (Scrollable if many networks)           │
└─────────────────────────────────────────┘
    │
    ├─> User clicks on a network
    │
    ▼
┌─────────────────────────────────────────┐
│ Connect to: MyHomeWiFi                  │
│                                         │
│ ┌────────────────────────────────────┐  │
│ │ ••••••••                           │  │ ◄─ Password input
│ └────────────────────────────────────┘  │
│                                         │
│ ┌───┬───┬───┬───┬───┬───┬───┬───┬───┐  │
│ │ Q │ W │ E │ R │ T │ Y │ U │ I │ O │  │
│ ├───┼───┼───┼───┼───┼───┼───┼───┼───┤  │
│ │ A │ S │ D │ F │ G │ H │ J │ K │ L │  │
│ ├───┼───┼───┼───┼───┼───┼───┼───┼───┤  │ ◄─ Virtual keyboard
│ │ Z │ X │ C │ V │ B │ N │ M │ ⌫ │   │  │
│ ├───┴───┴───┴───┴───┴───┴───┼───┼───┤  │
│ │     Space    │ ABC │ OK  │ ✕  │   │  │
│ └──────────────┴─────┴─────┴────┴───┘  │
└─────────────────────────────────────────┘
    │
    ├─> User types password and presses "OK"
    │
    ▼
┌─────────────────────────────────────────┐
│        ┌──────────────────────┐         │
│        │   Connecting...      │         │ ◄─ Notification
│        └──────────────────────┘         │
└─────────────────────────────────────────┘
    │
    ├─> WiFi Service attempts connection
    │
    ▼ (Success)
┌─────────────────────────────────────────┐
│        ┌──────────────────────┐         │
│        │ WiFi Connected! ✓    │         │ ◄─ Auto-dismiss in 5s
│        └──────────────────────┘         │
└─────────────────────────────────────────┘
    │
    ▼ (Failure)
┌─────────────────────────────────────────┐
│        ┌──────────────────────┐         │
│        │ Connection Failed! ✗ │         │ ◄─ Auto-dismiss in 5s
│        └──────────────────────┘         │
└─────────────────────────────────────────┘
```

## Features

### 1. WiFi Toggle Button

**Appearance**:
- OFF state: Gray background, text "WiFi: OFF"
- ON state: Green background, text "WiFi: ON"

**Behavior**:
- Click to toggle WiFi on/off
- When turned ON:
  - Enables WiFi service
  - Automatically starts scan
  - Shows "Scanning WiFi networks..." notification
  - Populates network list when scan completes

### 2. Network List

**Sorting**:
- Networks sorted by signal strength (RSSI) - strongest first
- Uses bubble sort algorithm

**Display Format**:
```
SSID              Signal Strength
────────────────  ─────────────────────
MyHomeWiFi        📶 Strong (-45dBm)   [Green]
CoffeeShop        📶 Medium (-65dBm)   [Yellow]
Neighbor          📶 Weak (-80dBm)     [Orange]
```

**Signal Strength Categories**:
- **Strong**: RSSI > -50 dBm (Green)
- **Medium**: RSSI > -70 dBm (Yellow)
- **Weak**: RSSI ≤ -70 dBm (Orange)

**Scrolling**:
- Automatically enabled by LVGL when list exceeds container height
- User can scroll with touch (if touch screen available)

### 3. Password Input Screen

**Components**:
1. **Title**: "Connect to: [SSID]"
2. **Password Textarea**:
   - Password mode (shows bullets ••••)
   - One-line input
   - Placeholder: "Enter password"
3. **Virtual Keyboard**:
   - Full QWERTY layout
   - Built-in LVGL keyboard component
   - Modes: lowercase, uppercase, numbers, symbols
   - Special keys: Space, Backspace, OK, Cancel

**Keyboard Controls**:
- **OK button**: Trigger connection
- **Cancel button** (✕): Go back to network list

### 4. Notifications

**Types**:

1. **"Scanning WiFi networks..."** (2s)
   - Shown when WiFi is turned ON
   - Auto-dismiss after 2 seconds

2. **"Connecting..."** (No auto-dismiss)
   - Shown when connection attempt starts
   - Stays until connection result

3. **"WiFi Connected!"** (5s)
   - Success notification
   - Green border
   - Auto-dismiss after 5 seconds

4. **"Connection Failed!"** (5s)
   - Failure notification
   - Can occur if:
     - Wrong password
     - Network out of range
     - Other connection errors
   - Auto-dismiss after 5 seconds

**Dismissal**:
- Auto-dismiss after specified duration
- Manual dismiss: Press any button

## Event Flow

### WiFi Scan Flow

```
User toggles WiFi ON
    │
    ▼
ui_network.c: wifi_toggle_event_cb()
    │
    ├─> wifi_service_enable()
    ├─> wifi_service_scan()
    └─> ui_network_show_notification("Scanning...")
         │
         ▼
    WiFi Service scans networks
         │
         ▼
    Posts: KRAKEN_EVENT_WIFI_SCAN_DONE
         │
         ▼
    ui_manager.c: ui_manager_handle_event()
         │
         ├─> Checks: if (g_ui.in_submenu)
         └─> Calls: ui_network_update_scan_results()
              │
              ▼
         ui_network.c: ui_network_update_scan_results()
              │
              ├─> wifi_service_get_scan_results()
              ├─> Sort networks by RSSI
              └─> create_network_list()
                   │
                   └─> Creates buttons for each network
```

### Connection Flow

```
User clicks network item
    │
    ▼
network_item_clicked()
    │
    ├─> Saves selected SSID
    └─> show_password_screen()
         │
         ▼
    User types password on keyboard
         │
         ▼
    User presses "OK" on keyboard
         │
         ▼
    keyboard_event_cb(LV_EVENT_READY)
         │
         ├─> Gets password from textarea
         ├─> hide_password_screen()
         ├─> ui_network_show_notification("Connecting...")
         └─> connect_to_wifi(ssid, password)
              │
              └─> wifi_service_connect(ssid, password)
                   │
                   ▼
              WiFi Service attempts connection
                   │
                   ├─> Success: KRAKEN_EVENT_WIFI_CONNECTED
                   │            │
                   │            └─> ui_network_on_wifi_connected()
                   │                 │
                   │                 └─> "WiFi Connected!" notification
                   │
                   └─> Failure: KRAKEN_EVENT_WIFI_DISCONNECTED
                                │
                                └─> ui_network_on_wifi_disconnected(true)
                                     │
                                     └─> "Connection Failed!" notification
```

## Input Handling

### Main Menu → Network Screen

- **CENTER** or **RIGHT** on "Network" menu item → Open network screen

### Network Screen

- **LEFT button**: Go back to main menu
- **Click WiFi toggle**: Turn WiFi on/off
- **Click network item**: Show password screen

### Password Screen

- **Keyboard interaction**: Type password using virtual keyboard
- **OK button**: Attempt connection
- **Cancel button**: Return to network list

### Notification Active

- **Any button press**: Dismiss notification (except auto-dismissing ones)

## API Reference

### Functions

```c
// Create network screen (call once during init)
lv_obj_t *ui_network_screen_create(lv_obj_t *parent);

// Show/hide network screen
void ui_network_screen_show(void);
void ui_network_screen_hide(void);

// Update network list after scan
void ui_network_update_scan_results(void);

// Handle input events (UP/DOWN navigation, etc.)
void ui_network_handle_input(kraken_event_type_t input);

// Connection result handlers
void ui_network_on_wifi_connected(void);
void ui_network_on_wifi_disconnected(bool was_connecting);

// Show notification popup
void ui_network_show_notification(const char *message, uint32_t duration_ms);
```

### Integration Points

**UI Manager must call**:
1. `ui_network_screen_create()` during initialization
2. `ui_network_screen_show()` when user selects Network menu
3. `ui_network_update_scan_results()` on WIFI_SCAN_DONE event
4. `ui_network_on_wifi_connected()` on WIFI_CONNECTED event
5. `ui_network_on_wifi_disconnected()` on WIFI_DISCONNECTED event

**WiFi Service must post**:
1. `KRAKEN_EVENT_WIFI_SCAN_DONE` when scan completes
2. `KRAKEN_EVENT_WIFI_CONNECTED` when connection succeeds
3. `KRAKEN_EVENT_WIFI_DISCONNECTED` when disconnected

## File Structure

```
ui_network.c (450 lines)
├── WiFi toggle button
├── Network list with scrolling
├── Password input screen
├── Virtual keyboard integration
├── Notification system
└── Event handling
```

## Future Enhancements

- **Saved networks**: Remember passwords for known networks
- **Forget network**: Option to forget saved network
- **Auto-connect**: Automatically connect to known networks
- **Connection details**: Show IP address, gateway, DNS after connecting
- **Hidden networks**: Manual SSID entry for hidden networks
- **WPS support**: WPS button connection method
- **Network info**: Show security type (WPA2, WPA3, etc.)
