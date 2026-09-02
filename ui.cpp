#include "ui.h"
#include "clock_manager.h"
#include "industrial_busses.h"

// Definitioner av globala skärmobjekt
lv_obj_t *scr_dashboard = NULL;
lv_obj_t *menu_list = NULL;
lv_obj_t *content_area = NULL;
lv_obj_t *lbl_time = NULL;

lv_obj_t *lbl_victron_data = NULL;
lv_obj_t *lbl_ruuvi_data = NULL;
lv_obj_t *lbl_busses_data = NULL;
lv_obj_t *lbl_events_data = NULL;
lv_obj_t *lbl_hardware_data = NULL;

lv_obj_t *slider_victron = NULL;
lv_obj_t *slider_ruuvi = NULL;
lv_obj_t *lbl_slider_v_txt = NULL;
lv_obj_t *lbl_slider_r_txt = NULL;

lv_obj_t *btn_rs485_test = NULL;
lv_obj_t *lbl_btn_test_txt = NULL;
lv_obj_t *btn_bus_reset = NULL;
lv_obj_t *btn_ble_reset = NULL;

static int active_page = 0;
static bool test_relay_state = false;

// 1. BAKGRUNDSBELYSNING & PWM CONTROL LOGIK
void setBacklight(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    // Konvertera 0-100% till 8-bitars PWM (0-255)
    uint32_t duty = (percent * 255) / 100;
    ledcWrite(0, duty); // Kanal 0 används
}

void updateBacklight() {
    time_t now; 
    struct tm timeinfo; 
    time(&now); 
    localtime_r(&now, &timeinfo);
    unsigned long currentMillis = millis();
    bool isNight = false;

    // Hantera nattsänkningstider (inklusive över midnatt)
    if (sysSettings.nightStartHour > sysSettings.nightEndHour) {
        if (timeinfo.tm_hour >= sysSettings.nightStartHour || timeinfo.tm_hour < sysSettings.nightEndHour) {
            isNight = true;
        }
    } else {
        if (timeinfo.tm_hour >= sysSettings.nightStartHour && timeinfo.tm_hour < sysSettings.nightEndHour) {
            isNight = true;
        }
    }

    // Kontrollera om skärmen nyligen berörts (Vilotimer)
    if (currentMillis - lastTouchTime < (unsigned long)(sysSettings.screenTimeoutSec * 1000)) {
        setBacklight(100); // Alltid full ljusstyrka vid aktiv touch
    } else {
        if (isNight) {
            setBacklight(sysSettings.brightnessNight); // Nattläge
        } else {
            setBacklight(sysSettings.brightnessDimmed); // Dag-vila
        }
    }
}

// 2. TOUCH OCH CONTROL-CALLBACKS
static void toggle_menu_cb(lv_event_t * e) {
    if(lv_obj_has_flag(menu_list, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(menu_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_foreground(menu_list);
    } else {
        lv_obj_add_flag(menu_list, LV_OBJ_FLAG_HIDDEN);
    }
}

static void menu_btn_event_cb(lv_event_t * e) {
    long page_id = (long)lv_event_get_user_data(e);
    lv_obj_add_flag(menu_list, LV_OBJ_FLAG_HIDDEN);
    active_page = (int)page_id;
    updateScreenContent(active_page);
}

static void victron_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    sysSettings.victronScanInterval = value;
    
    char buf[32]; 
    snprintf(buf, sizeof(buf), "Victron sokning: %d sek", value);
    lv_label_set_text(lbl_slider_v_txt, buf);
    
    if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        saveAllSettings();
    }
}

static void ruuvi_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    sysSettings.ruuviScanInterval = value;
    
    char buf[32]; 
    snprintf(buf, sizeof(buf), "Ruuvi sokning: %d sek", value);
    lv_label_set_text(lbl_slider_r_txt, buf);
    
    if(lv_event_get_code(e) == LV_EVENT_RELEASED) {
        saveAllSettings();
    }
}
static void rs485_test_btn_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        test_relay_state = !test_relay_state;
        uint16_t val = test_relay_state ? 0xFF00 : 0x0000;
        
        // Skicka direkt via hårdvarubussen på Core 1
        sendModbusCommand(1, 0x05, 0, val);
        
        if (test_relay_state) {
            lv_label_set_text(lbl_btn_test_txt, "Skickade: PA (0xFF00)");
            lv_obj_set_style_bg_color(btn_rs485_test, lv_color_make(39, 174, 96), 0);
        } else {
            lv_label_set_text(lbl_btn_test_txt, "Skickade: AV (0x0000)");
            lv_obj_set_style_bg_color(btn_rs485_test, lv_color_make(192, 41, 43), 0);
        }
    }
}

static void bus_reset_btn_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        factoryResetBusSettings();
        refreshHardwarePageData();
    }
}

static void ble_reset_btn_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        factoryResetBLESettings();
        if(slider_victron) {
            lv_slider_set_value(slider_victron, 5, LV_ANIM_ON);
            char buf; snprintf(buf, sizeof(buf), "Victron sokning: 5 sek");
            lv_label_set_text(lbl_slider_v_txt, buf);
        }
        if(slider_ruuvi) {
            lv_slider_set_value(slider_ruuvi, 15, LV_ANIM_ON);
            char buf; snprintf(buf, sizeof(buf), "Ruuvi sokning: 15 sek");
            lv_label_set_text(lbl_slider_r_txt, buf);
        }
        refreshHardwarePageData();
    }
}

// 3. DYNAMISK STRUKTURERING AV UNDERSIDOR (LVGL)
void updateScreenContent(int page_id) {
    // Frigör minne genom att rensa gamla widgets på huvudyrtan [09_LVGL_Widgets]
    lv_obj_clean(content_area);
    
    // Nollställ aktiva pekare så trådsäkringen inte skriver till raderade objekt
    lbl_victron_data = NULL; lbl_ruuvi_data = NULL; lbl_busses_data = NULL; 
    lbl_events_data = NULL; lbl_hardware_data = NULL; slider_victron = NULL; slider_ruuvi = NULL;

    lv_obj_t *title = lv_label_create(content_area);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 10);

    if (page_id == 0) {
        lv_label_set_text(title, "Victron Solar System");
        lbl_victron_data = lv_label_create(content_area);
        lv_obj_align(lbl_victron_data, LV_ALIGN_TOP_LEFT, 15, 45);
        lv_label_set_text(lbl_victron_data, "Skannar BLE...");
    } 
    else if (page_id == 1) {
        lv_label_set_text(title, "RuuviTag Miljodata");
        lbl_ruuvi_data = lv_label_create(content_area);
        lv_obj_align(lbl_ruuvi_data, LV_ALIGN_TOP_LEFT, 15, 45);
        lv_label_set_text(lbl_ruuvi_data, "Vantar pa paket...");
    }
    else if (page_id == 4) {
        lv_label_set_text(title, "Waveshare Hardware Config");
        
        lbl_hardware_data = lv_label_create(content_area);
        lv_obj_align(lbl_hardware_data, LV_ALIGN_TOP_LEFT, 15, 45);

        // Inställnings-sliders (Höger sida) [09_LVGL_Widgets]
        lbl_slider_v_txt = lv_label_create(content_area);
        lv_obj_align(lbl_slider_v_txt, LV_ALIGN_TOP_RIGHT, -20, 45);
        
        slider_victron = lv_slider_create(content_area);
        lv_obj_set_size(slider_victron, 160, 20); // Tjock touchyta
        lv_obj_align_to(slider_victron, lbl_slider_v_txt, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        lv_slider_set_range(slider_victron, 2, 120);
        lv_slider_set_value(slider_victron, sysSettings.victronScanInterval, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider_victron, victron_slider_event_cb, LV_EVENT_ALL, NULL);

        lbl_slider_r_txt = lv_label_create(content_area);
        lv_obj_align(lbl_slider_r_txt, LV_ALIGN_TOP_RIGHT, -20, 120);
        
        slider_ruuvi = lv_slider_create(content_area);
        lv_obj_set_size(slider_ruuvi, 160, 20);
        lv_obj_align_to(slider_ruuvi, lbl_slider_r_txt, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        lv_slider_set_range(slider_ruuvi, 2, 120);
        lv_slider_set_value(slider_ruuvi, sysSettings.ruuviScanInterval, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider_ruuvi, ruuvi_slider_event_cb, LV_EVENT_ALL, NULL);

        // Skapa den stora testknappen för RS485-reläet i underkant [09_LVGL_Widgets]
        btn_rs485_test = lv_btn_create(content_area);
        lv_obj_set_size(btn_rs485_test, 200, 45);
        lv_obj_align(btn_rs485_test, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_add_event_cb(btn_rs485_test, rs485_test_btn_event_cb, LV_EVENT_ALL, NULL);
        
        lbl_btn_test_txt = lv_label_create(btn_rs485_test);
        lv_label_set_text(lbl_btn_test_txt, "Testa RS485-Rela \xEF\x83\xA7"); // Blixtsymbol
        lv_obj_center(lbl_btn_test_txt);

        // Fabriksåterställningsknappar (Nollställ) [09_LVGL_Widgets]
        btn_bus_reset = lv_btn_create(content_area);
        lv_obj_set_size(btn_bus_reset, 140, 35);
        lv_obj_align(btn_bus_reset, LV_ALIGN_BOTTOM_LEFT, 10, -65);
        lv_obj_set_style_bg_color(btn_bus_reset, lv_color_make(231, 76, 60), 0);
        lv_obj_add_event_cb(btn_bus_reset, bus_reset_btn_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_t *l1 = lv_label_create(btn_bus_reset); lv_label_set_text(l1, "Reset Buss"); lv_obj_center(l1);

        btn_ble_reset = lv_btn_create(content_area);
        lv_obj_set_size(btn_ble_reset, 140, 35);
        lv_obj_align(btn_ble_reset, LV_ALIGN_BOTTOM_RIGHT, -10, -65);
        lv_obj_set_style_bg_color(btn_ble_reset, lv_color_make(231, 76, 60), 0);
        lv_obj_add_event_cb(btn_ble_reset, ble_reset_btn_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_t *l2 = lv_label_create(btn_ble_reset); lv_label_set_text(l2, "Rensa BLE"); lv_obj_center(l2);

        // Skriv ut telemetrin en första gång vid sidöppning
        refreshHardwarePageData();
    }
}
void refreshHardwarePageData() {
    if (lbl_hardware_data != NULL) {
        float vBat = readBatteryVoltage();
        bool rtcOK = checkRTCStatus();
        char hw_buffer[256];
        
        snprintf(hw_buffer, sizeof(hw_buffer),
                 "📊 MODEL: Touch-LCD-4\n"
                 "🔋 LiPo: %.2f V\n"
                 "   Status: %s\n\n"
                 "⏰ RTC: %s\n\n"
                 "🔌 RS485: G43/44\n"
                 "📡 CAN: G41/42\n\n"
                 "🛡️ Rela-vakt: %s",
                 vBat, 
                 (vBat > 4.1 ? "Fullt/Laddar" : (vBat < 3.5 ? "LAG STR0M!" : "Drift")),
                 (rtcOK ? "OK (I2C)" : "FEL / SAKNAS"),
                 (sysSettings.autoRelayCheck ? (isRelayPhysicallyConnected ? "OK" : "SAKNAS") : "AV")
        );
        lv_label_set_text(lbl_hardware_data, hw_buffer);
    }
}

// 4. ANIMATION OCH GRUNDUPPBYGGNAD (SPLASH SCREEN)
static void splash_timeout_cb(lv_timer_t * timer) {
    // Växla över till dashboarden med en mjuk cross-fade animation [09_LVGL_Widgets]
    lv_scr_load_anim(scr_dashboard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, true);
    lv_timer_del(timer);
}

void showSplashScreen() {
    // Skapa en tillfällig startskärm
    lv_obj_t *scr_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_splash, lv_color_white(), 0);

    // Stora röda bokstäver för "CABBY" [09_LVGL_Widgets]
    lv_obj_t *lbl_cabby = lv_label_create(scr_splash);
    lv_label_set_text(lbl_cabby, "CABBY");
    lv_obj_set_style_text_font(lbl_cabby, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_cabby, lv_color_make(231, 76, 60), 0); // Solid röd
    lv_obj_align(lbl_cabby, LV_ALIGN_CENTER, 0, -20);

    // Diskret versionsnummer under logotypen
    lv_obj_t *lbl_v = lv_label_create(scr_splash);
    lv_label_set_text(lbl_v, "Version 0.8");
    lv_obj_set_style_text_font(lbl_v, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_v, lv_color_make(127, 140, 141), 0); // Grå
    lv_obj_align_to(lbl_v, lbl_cabby, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Ladda startskärmen direkt vid boot
    lv_scr_load(scr_splash);
    
    // Skapa en timer som väntar 3 sekunder (3000ms) innan den byter vy
    lv_timer_create(splash_timeout_cb, 3000, NULL);
}

void initDisplayAndTouch() {
    // Sätt upp LEDC PWM-kanalen för belysningen [09_LVGL_Widgets]
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BACKLIGHT_PIN, 0);
    lastTouchTime = millis();
    setBacklight(sysSettings.brightnessDay);

    // Skapa dashboardskärmen som ska ligga redo i bakgrunden
    scr_dashboard = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_dashboard, lv_color_white(), 0);
    
    // Rita Toppbar (Header)
    lv_obj_t *header = lv_obj_create(scr_dashboard);
    lv_obj_set_size(header, 480, 55);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(3, 62, 22), 0); // Victron-grön
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 5, 0);

    // Skapa Hamburgarknappen (☰)
    lv_obj_t *btn_menu = lv_btn_create(header);
    lv_obj_set_size(btn_menu, 45, 45);
    lv_obj_align(btn_menu, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(btn_menu, toggle_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *m_txt = lv_label_create(btn_menu); 
    lv_label_set_text(m_txt, "X"); 
    lv_obj_center(m_txt);

    // Klocktext i Headern
    lbl_time = lv_label_create(header);
    lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);

    // Skapa ytan för huvudinnehållet
    content_area = lv_obj_create(scr_dashboard);
    lv_obj_set_size(content_area, 480, 425);
    lv_obj_align(content_area, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_radius(content_area, 0, 0);
    lv_obj_set_style_border_width(content_area, 0, 0);

    // Bygg upp själva listan för Hamburgermenyn [09_LVGL_Widgets]
    menu_list = lv_list_create(scr_dashboard);
    lv_obj_set_size(menu_list, 240, 425);
    lv_obj_align(menu_list, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_add_flag(menu_list, LV_OBJ_FLAG_HIDDEN); // Göm menyn som standard

    // Lägg till de touch-vänliga valen i menyn
    lv_obj_t *btn;
    btn = lv_list_add_btn(menu_list, LV_SYMBOL_CHARGE, "Victron Solar"); 
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void*)0);
    
    btn = lv_list_add_btn(menu_list, LV_SYMBOL_SETTINGS, "RuuviTags"); 
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void*)1);
    
    btn = lv_list_add_btn(menu_list, LV_SYMBOL_HARDWARE, "WS Hardware"); 
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void*)4);

    // Förbered vy 0 och starta sedan startanimationen
    updateScreenContent(0);
    showSplashScreen();
}
