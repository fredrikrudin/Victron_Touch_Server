#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include "config.h"

enum Screen { SCREEN_DASHBOARD, SCREEN_RUUVI, SCREEN_BUSSES, SCREEN_EVENTS, SCREEN_HARDWARE };
extern Screen currentScreen;

// Externa globala LVGL-objekt för rendering
extern lv_obj_t *scr_dashboard;
extern lv_obj_t *menu_list;
extern lv_obj_t *content_area;

// Dynamiska textfält för realtidsdata
extern lv_obj_t *lbl_victron_data;
extern lv_obj_t *lbl_ruuvi_data;
extern lv_obj_t *lbl_busses_data;
extern lv_obj_t *lbl_events_data;
extern lv_obj_t *lbl_hardware_data;

// Sliders för inställningar
extern lv_obj_t *slider_victron;
extern lv_obj_t *slider_ruuvi;
extern lv_obj_t *lbl_slider_v_txt;
extern lv_obj_t *lbl_slider_r_txt;

void initDisplayAndTouch();
void showSplashScreen();
void updateUI();
void updateScreenContent(int page_id);
void refreshHardwarePageData();

#endif
