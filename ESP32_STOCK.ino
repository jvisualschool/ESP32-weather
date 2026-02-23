#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "esp_bsp.h"
#include "display.h"
#include "lv_port.h"
#include "config.h"
#include "weather_cities.h"
#include "font_kr.h"

// -------------------------------------------------------------------
// 전역 변수 및 상태
// -------------------------------------------------------------------
int current_city_idx = 0;
unsigned long last_weather_update = 0;
const unsigned long weather_interval = 600000; // 10분마다 갱신

// LVGL 콜백에서 메인 루프로 요청을 전달하는 플래그
// (LVGL 태스크에서 블로킹 HTTP 호출 방지)
volatile bool fetch_requested = false;

lv_obj_t *tv; // tileview
lv_obj_t *city_list_page;
lv_obj_t *detail_page;

// UI 요소
lv_obj_t *label_city_name;
lv_obj_t *label_temp;
lv_obj_t *label_condition;
lv_obj_t *label_details;
lv_obj_t *label_status;   // 도시 목록 페이지 상태 표시
lv_obj_t *bg_overlay;

// -------------------------------------------------------------------
// 에러 표시 헬퍼
// -------------------------------------------------------------------
void show_error(const char* msg) {
    bsp_display_lock(0);
    lv_label_set_text(label_city_name, k_cities[current_city_idx].name);
    lv_label_set_text(label_temp, "--");
    lv_label_set_text(label_condition, msg);
    lv_label_set_text(label_details, "");
    lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x333333), 0);
    bsp_display_unlock();
}

// -------------------------------------------------------------------
// 데이터 수신 및 파싱
// -------------------------------------------------------------------
void update_weather_ui(String json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.printf("JSON Parse Error: %s\n", error.c_str());
        show_error("데이터 파싱 실패");
        return;
    }

    float temp        = doc["main"]["temp"];
    int   humidity    = doc["main"]["humidity"];
    float wind_speed  = doc["wind"]["speed"];
    const char* description = doc["weather"][0]["description"];

    bsp_display_lock(0);

    lv_label_set_text(label_city_name, k_cities[current_city_idx].name);

    // [수정] buf[32] → buf[64], sprintf → snprintf (버퍼 오버플로우 방지)
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f°", temp);
    lv_label_set_text(label_temp, buf);

    lv_label_set_text(label_condition, description);

    // 최대 길이 예: "Humidity: 100%  |  Wind: 100.0m/s" = 34자
    snprintf(buf, sizeof(buf), "Humidity: %d%%  |  Wind: %.1fm/s", humidity, wind_speed);
    lv_label_set_text(label_details, buf);

    if (temp > 25) {
        lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0xFF4500), 0); // Hot
    } else if (temp > 10) {
        lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x00BFFF), 0); // Mild
    } else {
        lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x4169E1), 0); // Cold
    }

    bsp_display_unlock();
}

void fetch_weather(int idx) {
    // [수정] WiFi 연결 상태 확인 후 에러 표시
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        show_error("WiFi 연결 없음");
        return;
    }

    // TODO: 보안 강화 시 client->setCACert(root_ca) 로 교체.
    //       현재 setInsecure()는 TLS 인증서를 검증하지 않아
    //       동일 네트워크 내 MITM 공격에 취약할 수 있음.
    WiFiClientSecure *client = new WiFiClientSecure;
    if (!client) {
        show_error("메모리 부족");
        return;
    }
    client->setInsecure();

    HTTPClient http;

    String url = "https://api.openweathermap.org/data/2.5/weather?";
    url += "lat=" + String(k_cities[idx].lat, 4);
    url += "&lon=" + String(k_cities[idx].lon, 4);
    url += "&appid=" + String(weather_api_key);
    url += "&units=metric";  // lang=en (기본값) - 한글 서브셋 폰트이므로 영문 날씨 설명 사용

    if (http.begin(*client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            update_weather_ui(http.getString());
        } else {
            // [수정] HTTP 오류 코드 UI에 표시
            Serial.printf("HTTP Error: %d\n", httpCode);
            char msg[32];
            snprintf(msg, sizeof(msg), "HTTP 오류 %d", httpCode);
            show_error(msg);
        }
        http.end();
    } else {
        Serial.println("HTTP begin failed");
        show_error("서버 연결 실패");
    }

    delete client;
}

// -------------------------------------------------------------------
// UI 빌드 (Liquid Glass 스타일)
// -------------------------------------------------------------------
static void city_event_cb(lv_event_t * e) {
    int idx = (int)lv_event_get_user_data(e);

    current_city_idx = idx;

    // [수정] LVGL 태스크에서 직접 fetch_weather() 호출 금지.
    //        블로킹 HTTP 요청으로 인해 렌더링 태스크가 수 초간 동결되는 문제 방지.
    //        대신 플래그를 세우고 메인 루프(loop())에서 처리.
    fetch_requested = true;

    lv_obj_set_tile_id(tv, 1, 0, LV_ANIM_ON);
}

void setup_ui() {
    // 배경
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_opa(tv, 0, 0);

    // --- Page 0: City List ---
    city_list_page = lv_tileview_add_tile(tv, 0, 0, LV_DIR_ALL);

    lv_obj_t *title = lv_label_create(city_list_page);
    lv_label_set_text(title, "KOREA CITIES");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // [추가] WiFi/네트워크 상태 표시 레이블
    label_status = lv_label_create(city_list_page);
    lv_label_set_text(label_status, "");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF4444), 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_RIGHT, -10, 15);

    lv_obj_t *list = lv_list_create(city_list_page);
    lv_obj_set_size(list, 440, 240);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // city_count는 weather_cities.cpp에서 sizeof로 자동 계산됨
    for (int i = 0; i < city_count; i++) {
        lv_obj_t * btn = lv_list_add_btn(list, NULL, k_cities[i].name);
        lv_obj_add_event_cb(btn, city_event_cb, LV_EVENT_CLICKED, (void*)i);
        lv_obj_set_style_bg_opa(btn, 0, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(btn, &font_kr_14, 0);  // 한글 폰트
    }

    // --- Page 1: Detail Weather ---
    detail_page = lv_tileview_add_tile(tv, 1, 0, LV_DIR_ALL);

    bg_overlay = lv_obj_create(detail_page);
    lv_obj_set_size(bg_overlay, 480, 320);
    lv_obj_set_style_bg_opa(bg_overlay, 150, 0);
    lv_obj_set_style_border_width(bg_overlay, 0, 0);
    lv_obj_set_style_radius(bg_overlay, 0, 0);

    lv_obj_t *card = lv_obj_create(detail_page);
    lv_obj_set_size(card, 400, 240);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, 30, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    label_city_name = lv_label_create(card);
    lv_obj_set_style_text_font(label_city_name, &font_kr_32, 0);  // 한글 폰트
    lv_obj_align(label_city_name, LV_ALIGN_TOP_MID, 0, 10);

    label_temp = lv_label_create(card);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -10);

    label_condition = lv_label_create(card);
    lv_obj_set_style_text_font(label_condition, &font_kr_20, 0);  // 한글 에러메시지 대응
    lv_obj_align(label_condition, LV_ALIGN_CENTER, 0, 40);

    label_details = lv_label_create(card);
    lv_obj_set_style_text_font(label_details, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_details, lv_color_hex(0xDDDDDD), 0);
    lv_obj_align(label_details, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_label_set_text(label_city_name, "Loading...");
}

// -------------------------------------------------------------------
// 메인
// -------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("System Starting...");

    // LVGL 스택 증설 (4KB -> 16KB)
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16384;

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = port_cfg,
        // full_refresh=1 이므로 버퍼는 전체 화면 크기 이상 필수
        // 부족하면 버퍼 오버플로우 → 노이즈 화면 (lv_port.c line 242)
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90,
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    setup_ui();
    bsp_display_unlock();

    WiFi.begin(ssid, password);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi Connected!");
        fetch_weather(current_city_idx);
        // [수정] 초기 fetch 후 타이머 기준 설정
        last_weather_update = millis();
    } else {
        Serial.println("WiFi Failed!");
        // [수정] WiFi 실패 시 도시 목록 페이지에 에러 표시
        bsp_display_lock(0);
        lv_label_set_text(label_status, "WiFi FAIL");
        bsp_display_unlock();
    }
}

void loop() {
    unsigned long now = millis();

    // [수정] fetch_requested 플래그를 메인 루프에서 처리.
    //        도시 선택 후 타이머도 함께 리셋해 직후 중복 갱신 방지.
    if (fetch_requested) {
        fetch_requested = false;
        last_weather_update = now;
        fetch_weather(current_city_idx);
    } else if (now - last_weather_update > weather_interval) {
        last_weather_update = now;
        fetch_weather(current_city_idx);
    }

    delay(10);
}
