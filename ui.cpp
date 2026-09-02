#include "ui.h"
#include "clock_manager.h"
#include "industrial_busses.h"
#include "event_scheduler.h"
#include "victron_ble.h"

// Allokering av grundläggande LVGL-objekt
Screen currentScreen = SCREEN_DASHBOARD;
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

// Allokering av de nya parningsobjekten [09_LVGL_Widgets]
lv_obj_t *list_discovered = NULL;
lv_obj_t *ta_name = NULL;
lv_obj_t *ta_key = NULL;
lv_obj_t *kb = NULL;
lv_obj_t *btn_save_pair = NULL;
char selected_mac = "";

static int active_page = 0;
static bool test_relay_state = false;

// 1. HARDWARUSTYRNING AV BAKGRUNDSBELYSNING (PWM Channel 0)
void setBacklight(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * 255) / 100;
    ledcWrite(0, duty);
}

void updateBacklight() {
    time_t now; struct tm timeinfo; time(&now); localtime_r(&now, &timeinfo);
    unsigned long currentMillis = millis();
    bool isNight = false;

    if (sysSettings.nightStartHour > sysSettings.nightEndHour) {
        if (timeinfo.tm_hour >= sysSettings.nightStartHour || timeinfo.tm_hour < sysSettings.nightEndHour) isNight = true;
    } else {
        if (timeinfo.tm_hour >= sysSettings.nightStartHour && timeinfo.tm_hour < sysSettings.nightEndHour) isNight = true;
    }

    if (currentMillis - lastTouchTime < (unsigned long)(sysSettings.screenTimeoutSec * 1000)) {
        setBacklight(100); // 100% ljusstyrka vid aktiv touch
    } else {
        if (isNight) setBacklight(sysSettings.brightnessNight); // Nattsänkning
        else         setBacklight(sysSettings.brightnessDimmed); // Dagvila
    }
}

// 2. NAVIGERING- OCH INTERAKTIONS-CALLBACKS
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
    char buf; snprintf(buf, sizeof(buf), "Victron: %d sek", value);
    lv_label_set_text(lbl_slider_v_txt, buf);
    if(lv_event_get_code(e) == LV_EVENT_RELEASED) saveAllSettings();
}

// Skannar och uppdaterar listan över okända enheter live på LCD-skärmen
static void btn_scan_ble_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        runVictronDiscoveryScan(); // Kör den råa BLE-sökningen på Core 0
        
        if(list_discovered != NULL) {
            lv_obj_clean(list_discovered); // Töm gammal lista
            for(int i = 0; i < discVictronCount; i++) {
                lv_list_add_btn(list_discovered, LV_SYMBOL_PLUS, discVictrons[i].mac);
            }
        }
    }
}
// Aktiverar textfält och tangentbord när en MAC-adress väljs i listan [09_LVGL_Widgets]
static void list_select_mac_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    const char * mac = lv_list_get_btn_text(list_discovered, btn);
    strncpy(selected_mac, mac, sizeof(selected_mac) - 1);
    
    // Gör inmatningskomponenterna synliga på skärmen
    lv_obj_clear_flag(ta_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ta_key, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_save_pair, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    
    lv_keyboard_set_textarea(kb, ta_name); // Fokusera tangentbordet på namnfältet först
}

// Sparar den nya parade enheten trådsäkert till NVS Flash
static void btn_save_pair_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if(victronCount < MAX_DEVICES && strlen(selected_mac) > 0) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                strncpy(savedVictrons[victronCount].mac, selected_mac, sizeof(savedVictrons[victronCount].mac) - 1);
                strncpy(savedVictrons[victronCount].name, lv_textarea_get_text(ta_name), sizeof(savedVictrons[victronCount].name) - 1);
                strncpy(savedVictrons[victronCount].encryptionKey, lv_textarea_get_text(ta_key), sizeof(savedVictrons[victronCount].encryptionKey) - 1);
                savedVictrons[victronCount].connected = false;
                
                victronCount++;
                xSemaphoreGive(dataMutex);
                
                saveAllSettings(); // Permanent sparning till flash
                updateScreenContent(0); // Återgå till dashboardvyn direkt
            }
        }
    }
}

// 3. GENERERING AV GRÄNSSNITTSSIDOR
void updateScreenContent(int page_id) {
    lv_obj_clean(content_area);
    lbl_victron_data = NULL; lbl_ruuvi_data = NULL; lbl_hardware_data = NULL;
    list_discovered = NULL; ta_name = NULL; ta_key = NULL; kb = NULL; btn_save_pair = NULL;

    lv_obj_t *title = lv_label_create(content_area);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 10);

    if (page_id == 0) {
        lv_label_set_text(title, "Victron Solar System");
        lbl_victron_data = lv_label_create(content_area);
        lv_obj_align(lbl_victron_data, LV_ALIGN_TOP_LEFT, 15, 45);
        lv_label_set_text(lbl_victron_data, "Skannar enheter...");
    } 
    else if (page_id == 4) {
        lv_label_set_text(title, "Hardware & Parning");
        
        // Parningsknapp (Sök BLE)
        lv_obj_t *btn_scan = lv_btn_create(content_area);
        lv_obj_set_size(btn_scan, 150, 35);
        lv_obj_align(btn_scan, LV_ALIGN_TOP_LEFT, 10, 40);
        lv_obj_add_event_cb(btn_scan, btn_scan_ble_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_s = lv_label_create(btn_scan); lv_label_set_text(lbl_s, "Sok BLE Enheter"); lv_obj_center(lbl_s);

        // Resultatlista (Funna enheter)
        list_discovered = lv_list_create(content_area);
        lv_obj_set_size(list_discovered, 200, 150);
        lv_obj_align(list_discovered, LV_ALIGN_TOP_LEFT, 10, 85);
        lv_obj_add_event_cb(list_discovered, list_select_mac_cb, LV_EVENT_CLICKED, NULL);

        // Text Area: Enhetsnamn
        ta_name = lv_textarea_create(content_area);
        lv_obj_set_size(ta_name, 230, 40);
        lv_obj_align(ta_name, LV_ALIGN_TOP_RIGHT, -10, 40);
        lv_textarea_set_placeholder_text(ta_name, "Enhetsnamn (ex. Shunt)");
        lv_obj_add_flag(ta_name, LV_OBJ_FLAG_HIDDEN);

        // Text Area: Bindkey
        ta_key = lv_textarea_create(content_area);
        lv_obj_set_size(ta_key, 230, 40);
        lv_obj_align_to(ta_key, ta_name, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        lv_textarea_set_placeholder_text(ta_key, "32-tecken Bindkey (Hex)");
        lv_obj_add_flag(ta_key, LV_OBJ_FLAG_HIDDEN);

        // Spara parning-knapp [09_LVGL_Widgets]
        btn_save_pair = lv_btn_create(content_area);
        lv_obj_set_size(btn_save_pair, 230, 35);
        lv_obj_align_to(btn_save_pair, ta_key, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
        lv_obj_set_style_bg_color(btn_save_pair, lv_color_make(46, 204, 113), 0); // Grön
        lv_obj_add_event_cb(btn_save_pair, btn_save_pair_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl_sv = lv_label_create(btn_save_pair); lv_label_set_text(lbl_sv, "Spara Parning 💾"); lv_obj_center(lbl_sv);
        lv_obj_add_flag(btn_save_pair, LV_OBJ_FLAG_HIDDEN);

        // Fullskärmstangentbord i nederkant [09_LVGL_Widgets]
        kb = lv_keyboard_create(content_area);
        lv_obj_set_size(kb, 460, 140);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

        // Länka fälten till tangentbordet vid fokusering (klick) [09_LVGL_Widgets]
        lv_obj_add_event_cb(ta_name, [](lv_event_t* e){ lv_keyboard_set_textarea(kb, ta_name); }, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(ta_key,  [](lv_event_t* e){ lv_keyboard_set_textarea(kb, ta_key);  }, LV_EVENT_FOCUSED, NULL);
    }
}

// 4. ANIMATION OCH GRUNDUPPBYGGNAD
static void splash_timeout_cb(lv_timer_t * timer) {
    lv_scr_load_anim(scr_dashboard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, true);
    lv_timer_del(timer);
}

void showSplashScreen() {
    lv_obj_t *scr_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_splash, lv_color_white(), 0);

    lv_obj_t *lbl_cabby = lv_label_create(scr_splash);
    lv_label_set_text(lbl_cabby, "CABBY");
    lv_obj_set_style_text_font(lbl_cabby, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_cabby, lv_color_make(231, 76, 60), 0); // Röd
    lv_obj_align(lbl_cabby, LV_ALIGN_CENTER, 0, -20);

    lv_scr_load(scr_splash);
    lv_timer_create(splash_timeout_cb, 3000, NULL); // 3 sekunder startanimation
}

void initDisplayAndTouch() {
    // Starta PWM-belysning
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BACKLIGHT_PIN, 0);
    lastTouchTime = millis();
    setBacklight(sysSettings.brightnessDay);

    scr_dashboard = lv_obj_create(NULL);
    
    // Toppbar
    lv_obj_t *header = lv_obj_create(scr_dashboard);
    lv_obj_set_size(header, 480, 55);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(3, 62, 22), 0); // Victron-grön
    lv_obj_set_style_radius(header, 0, 0);

    // Hamburgarknapp (☰) [09_LVGL_Widgets]
    lv_obj_t *btn_menu = lv_btn_create(header);
    lv_obj_set_size(btn_menu, 45, 45);
    lv_obj_align(btn_menu, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_event_cb(btn_menu, toggle_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *m_txt = lv_label_create(btn_menu); lv_label_set_text(m_txt, "X"); lv_obj_center(m_txt);

    lbl_time = lv_label_create(header);
    lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);

    content_area = lv_obj_create(scr_dashboard);
    lv_obj_set_size(content_area, 480, 425);
    lv_obj_align(content_area, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_radius(content_area, 0, 0);

    // Hamburgermeny rullgardin [09_LVGL_Widgets]
    menu_list = lv_list_create(scr_dashboard);
    lv_obj_set_size(menu_list, 240, 425);
    lv_obj_align(menu_list, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_add_flag(menu_list, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *btn;
    btn = lv_list_add_btn(menu_list, LV_SYMBOL_CHARGE, "Victron Solar"); lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void*)0);
    btn = lv_list_add_btn(menu_list, LV_SYMBOL_HARDWARE, "WS Hardware"); lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void*)4);

    updateScreenContent(0);
    showSplashScreen();
}

void updateUI() {
    lv_timer_handler(); // Tickar LVGL grafikmotorn
    
    // Avläs hårdvaru-touch händelser
    lv_indev_t * indev = lv_indev_get_next(NULL);
    if(indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_indev_data_t data;
        _lv_indev_read(indev, &data);
        if(data.state == LV_INDEV_STATE_PRESSED) lastTouchTime = millis(); // Nollställ vilotimer
    }
    updateBacklight();
}
