idf_component_register(
    SRCS "display_service.c"
         "ui/ui_topbar.c"
         "ui/ui_menu.c"
         "ui/ui_widgets.c"
         "ui/ui_manager.c"
         "ui/ui_network.c"
         "ui/ui_boot_animation.c"
         "ui/ui_keyboard.c"
         "ui/ui_bluetooth.c"
         "ui/ui_audio.c"
    INCLUDE_DIRS "include" "ui"
    REQUIRES esp_lcd esp_lvgl_port lvgl bsp kernel esp_timer wifi bluetooth esp_http_client
)
