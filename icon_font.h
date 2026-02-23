#pragma once
#include "lvgl.h"

// Phosphor Thin 아이콘 폰트 (lv_font_conv 생성)
LV_FONT_DECLARE(font_icon_sm);   // 20px — 카드 아이콘용
LV_FONT_DECLARE(font_icon_lg);   // 52px — 날씨 큰 아이콘용

// UTF-8 인코딩 (Phosphor Thin, PUA 영역 U+E000~)
#define ICON_CLOUD           "\xEE\x86\xAA"  // U+E1AA cloud-thin
#define ICON_CLOUD_LIGHTNING "\xEE\x86\xB2"  // U+E1B2 cloud-lightning-thin
#define ICON_CLOUD_RAIN      "\xEE\x86\xB4"  // U+E1B4 cloud-rain-thin
#define ICON_CLOUD_SNOW      "\xEE\x86\xB8"  // U+E1B8 cloud-snow-thin
#define ICON_COMPASS         "\xEE\x87\x88"  // U+E1C8 compass-thin
#define ICON_DROP            "\xEE\x88\x90"  // U+E210 drop-thin
#define ICON_EYE             "\xEE\x88\xA0"  // U+E220 eye-thin
#define ICON_MAP_PIN         "\xEE\x8C\x96"  // U+E316 map-pin-thin
#define ICON_MOON            "\xEE\x8C\xB0"  // U+E330 moon-thin
#define ICON_SUN             "\xEE\x91\xB2"  // U+E472 sun-thin
#define ICON_CLOUD_FOG       "\xEE\x94\xBC"  // U+E53C cloud-fog-thin
#define ICON_CLOUD_SUN       "\xEE\x95\x80"  // U+E540 cloud-sun-thin
#define ICON_SNOWFLAKE       "\xEE\x96\xAA"  // U+E5AA snowflake-thin
#define ICON_SUN_HORIZON     "\xEE\x96\xB6"  // U+E5B6 sun-horizon-thin
#define ICON_THERMOMETER     "\xEE\x97\x86"  // U+E5C6 thermometer-thin
#define ICON_WIND            "\xEE\x97\x92"  // U+E5D2 wind-thin
#define ICON_GAUGE           "\xEE\x98\xA8"  // U+E628 gauge-thin
