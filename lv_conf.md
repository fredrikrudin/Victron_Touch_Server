# 🔧 Ändringar i `lv_conf.h` för CABBY v0.8 (VenusOS v2-stil)

Öppna den `lv_conf.h` som följde med ditt Waveshare-bibliotek (ligger i mappen `libraries/lvgl/` i din Arduino-skissmapp) och gör följande sex ändringar. Använd sökfunktionen (Ctrl+F) i din textredigerare för att hitta raderna.

---

## 🚀 1. Aktivera PSRAM (Viktigast - förhindrar boot-loops)
Leta upp sektionen `MEMORY SETTINGS` (runt rad 45). Ändra `LV_MEM_CUSTOM` från `0` till `1` så att LVGL använder ESP32-S3:s externa 8MB PSRAM via standard `malloc` istället för att krascha det interna minnet.

**Sök efter:**
```c
#define LV_MEM_CUSTOM 0
```
**Ändra till:**
```c
#define LV_MEM_CUSTOM 1
```

---

## 🎨 2. Slå på Mörkt Tema (VenusOS-bakgrund)
Leta upp sektionen `THEMES` (runt rad 470). Ändra `LV_THEME_DEFAULT_DARK` från `0` till `1` för att tvinga LVGL till mörkt läge, vilket ger den solida svarta VenusOS-bakgrunden.

**Sök efter:**
```c
#define LV_THEME_DEFAULT_DARK 0
```
**Ändra till:**
```c
#define LV_THEME_DEFAULT_DARK 1
```

---

## 🌀 3. Aktivera Kantutjämning (För runda bågar)
Leta upp sektionen `COLOR SETTINGS` eller `Drawing` (runt rad 130). Se till att `LV_COLOR_ANTIALIAS` är satt till `1` så att bågarna ritas mjuka och skarpa utan pixliga kanter.

**Sök efter:**
```c
#define LV_COLOR_ANTIALIAS 0
```
*(Om den står på 0, ändra till 1. Om den redan är 1, låt den vara).*
```c
#define LV_COLOR_ANTIALIAS 1
```

---

## 🔤 4. Aktivera Stora och Feta Typsnitt
Leta upp sektionen `FONT USAGE` (runt rad 350). LVGL inaktiverar stora typsnitt som standard för att spara minne. Aktivera storlek 16, 18, 28 och 48 (startskärmen) genom att ändra dem till `1`.

**Ändra följande rader till `1`:**
```c
#define LV_FONT_MONTSERRAT_16 1  // För etiketter och sekundärtext
#define LV_FONT_MONTSERRAT_18 1  // För sidtitlar
#define LV_FONT_MONTSERRAT_28 1  // För de stora VenusOS-mätvärdena (W, V)
#define LV_FONT_MONTSERRAT_48 1  // För den stora "CABBY"-loggan vid boot
```

---

## ⚙️ 5. Lås upp nödvändiga Gränssnittskomponenter
Leta upp sektionen `WIDGET USAGE` (runt rad 400). Kontrollera att de komponenter vi använder i källkoden är aktiverade (`1`).

**Kontrollera/ändra till `1` på dessa rader:**
```c
#define LV_USE_ARC      1  // Krävs för runda VenusOS-bågarna
#define LV_USE_LIST     1  // Krävs för hamburgermeny-listan
#define LV_USE_KEYBOARD 1  // Krävs för tangentbordet vid parning
#define LV_USE_TEXTAREA 1  // Krävs för textrutorna vid parning
```

---

## ⏱️ 6. Optimera Uppdateringsintervallen
Leta upp sektionen `HAL SETTINGS` (runt rad 75). För att få en blixtsnabb touch-respons på Waveshare-skärmen, sänk avläsningsperioden från standard (ofta 30ms) till `10` ms.

**Sök efter:**
```c
#define LV_INDEV_DEF_READ_PERIOD 30
```
**Ändra till:**
```c
#define LV_INDEV_DEF_READ_PERIOD 10
```
