#pragma once
#include "lvgl.h"

// NotoSansKR 기반 한글+라틴 서브셋 폰트 (lv_font_conv 생성)
// 포함 문자: ASCII 0x20-0x7E + 도시명/에러메시지 한글 52자
LV_FONT_DECLARE(font_kr_14);
LV_FONT_DECLARE(font_kr_20);
LV_FONT_DECLARE(font_kr_32);
