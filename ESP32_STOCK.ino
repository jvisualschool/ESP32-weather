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
// 전역 변수
// -------------------------------------------------------------------
int current_city_idx = 0;
unsigned long last_weather_update = 0;
const unsigned long weather_interval = 600000;
volatile bool fetch_requested   = false;
volatile bool forecast_requested = false;

lv_obj_t *tv;
lv_obj_t *city_list_page;
lv_obj_t *detail_page;
lv_obj_t *forecast_page;

lv_obj_t *label_status;
lv_obj_t *bg_overlay;
lv_obj_t *label_city_name;
lv_obj_t *label_temp;
lv_obj_t *label_datetime;
lv_obj_t *label_condition;
lv_obj_t *label_humidity_val;
lv_obj_t *label_wind_val;
lv_obj_t *label_wind_dir_val;
lv_obj_t *label_clouds_val;
lv_obj_t *label_feels_val;
lv_obj_t *label_minmax_val;
lv_obj_t *label_pressure_val;
lv_obj_t *label_vis_val;

lv_obj_t *fc_time[8];
lv_obj_t *fc_cond[8];
lv_obj_t *fc_temp[8];

// -------------------------------------------------------------------
// 날씨 설명 번역
// -------------------------------------------------------------------
static const char* translate_weather(const char* d) {
    if (!d || !d[0]) return "";
    if (strstr(d, "thunderstorm"))          return "뇌우";
    if (strstr(d, "drizzle"))               return "이슬비";
    if (strstr(d, "freezing rain"))         return "강한 비";
    if (strstr(d, "very heavy rain"))       return "폭우";
    if (strstr(d, "extreme rain"))          return "폭우";
    if (strstr(d, "heavy intensity rain"))  return "강한 비";
    if (strstr(d, "moderate rain"))         return "보통 비";
    if (strstr(d, "heavy intensity shower"))return "강한 소나기";
    if (strstr(d, "shower rain"))           return "소나기";
    if (strstr(d, "light rain"))            return "약한 비";
    if (strstr(d, "rain"))                  return "비";
    if (strstr(d, "heavy snow"))            return "폭설";
    if (strstr(d, "sleet"))                 return "진눈깨비";
    if (strstr(d, "rain and snow"))         return "비와 눈";
    if (strstr(d, "shower snow"))           return "눈 소나기";
    if (strstr(d, "light snow"))            return "약한 눈";
    if (strstr(d, "snow"))                  return "눈";
    if (strstr(d, "tornado"))               return "토네이도";
    if (strstr(d, "squall"))                return "돌풍";
    if (strstr(d, "volcanic ash"))          return "화산재";
    if (strstr(d, "sand") || strstr(d, "dust")) return "황사";
    if (strstr(d, "fog") || strstr(d, "mist"))  return "안개";
    if (strstr(d, "haze"))                  return "연무";
    if (strstr(d, "smoke"))                 return "연기";
    if (strstr(d, "overcast"))              return "흐림";
    if (strstr(d, "broken clouds"))         return "흐림";
    if (strstr(d, "scattered clouds"))      return "구름";
    if (strstr(d, "few clouds"))            return "구름 조금";
    if (strstr(d, "clear"))                 return "맑음";
    return d;
}

// -------------------------------------------------------------------
// 풍향 변환
// -------------------------------------------------------------------
static const char* get_wind_dir(int deg) {
    if (deg >= 338 || deg < 23)  return "북";
    if (deg < 68)                return "북동";
    if (deg < 113)               return "동";
    if (deg < 158)               return "남동";
    if (deg < 203)               return "남";
    if (deg < 248)               return "남서";
    if (deg < 293)               return "서";
    return "북서";
}

// -------------------------------------------------------------------
// 에러 표시
// -------------------------------------------------------------------
void show_error(const char* msg) {
    bsp_display_lock(0);
    lv_label_set_text(label_city_name,    k_cities[current_city_idx].name);
    lv_label_set_text(label_temp,         "--");
    lv_label_set_text(label_datetime,     "");
    lv_label_set_text(label_condition,    msg);
    lv_label_set_text(label_humidity_val, "--");
    lv_label_set_text(label_wind_val,     "--");
    lv_label_set_text(label_wind_dir_val, "--");
    lv_label_set_text(label_clouds_val,   "--");
    lv_label_set_text(label_feels_val,    "--");
    lv_label_set_text(label_minmax_val,   "--");
    lv_label_set_text(label_pressure_val, "--");
    lv_label_set_text(label_vis_val,      "--");
    lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x333333), 0);
    bsp_display_unlock();
}

// -------------------------------------------------------------------
// 날씨 데이터 업데이트
// -------------------------------------------------------------------
void update_weather_ui(String json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) { show_error("파싱 실패"); return; }

    float temp       = doc["main"]["temp"];
    float feels_like = doc["main"]["feels_like"];
    float temp_min   = doc["main"]["temp_min"];
    float temp_max   = doc["main"]["temp_max"];
    int   humidity   = doc["main"]["humidity"];
    int   pressure   = doc["main"]["pressure"];
    float wind_speed = doc["wind"]["speed"];
    int   wind_deg   = doc["wind"]["deg"] | 0;
    int   clouds     = doc["clouds"]["all"] | 0;
    int   visibility = doc["visibility"] | 10000;
    const char* desc = doc["weather"][0]["description"];
    long  dt         = doc["dt"];

    // KST 날짜/시간 (UTC+9)
    time_t kst = (time_t)dt + 9 * 3600;
    struct tm *t = gmtime(&kst);
    char dt_buf[20];
    strftime(dt_buf, sizeof(dt_buf), "%m/%d %H:%M", t);

    bsp_display_lock(0);

    char buf[64];
    lv_label_set_text(label_city_name, k_cities[current_city_idx].name);

    snprintf(buf, sizeof(buf), "%.1f°", temp);
    lv_label_set_text(label_temp, buf);

    lv_label_set_text(label_datetime, dt_buf);
    lv_label_set_text(label_condition, translate_weather(desc));

    snprintf(buf, sizeof(buf), "%d%%",         humidity);   lv_label_set_text(label_humidity_val, buf);
    snprintf(buf, sizeof(buf), "%.1fm/s",      wind_speed); lv_label_set_text(label_wind_val, buf);
    lv_label_set_text(label_wind_dir_val, get_wind_dir(wind_deg));
    snprintf(buf, sizeof(buf), "%d%%",         clouds);     lv_label_set_text(label_clouds_val, buf);
    snprintf(buf, sizeof(buf), "%.1f°",        feels_like); lv_label_set_text(label_feels_val, buf);
    snprintf(buf, sizeof(buf), "%.0f/%.0f°",   temp_min, temp_max); lv_label_set_text(label_minmax_val, buf);
    snprintf(buf, sizeof(buf), "%dhPa",        pressure);   lv_label_set_text(label_pressure_val, buf);
    snprintf(buf, sizeof(buf), "%.0fkm",       visibility / 1000.0f); lv_label_set_text(label_vis_val, buf);

    if (temp > 25)      lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0xB71C1C), 0);
    else if (temp > 10) lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x0D47A1), 0);
    else                lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x1A237E), 0);

    bsp_display_unlock();
}

// -------------------------------------------------------------------
// 시간별 예보 업데이트
// -------------------------------------------------------------------
void update_forecast_ui(String json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    JsonArray list = doc["list"];
    int count = min((int)list.size(), 8);

    bsp_display_lock(0);
    for (int i = 0; i < count; i++) {
        // 시간 (KST)
        const char* dt_txt = list[i]["dt_txt"];
        char buf[32];
        if (dt_txt && strlen(dt_txt) >= 16) {
            int h = ((dt_txt[11]-'0')*10 + (dt_txt[12]-'0') + 9) % 24;
            int m = (dt_txt[14]-'0')*10 + (dt_txt[15]-'0');
            snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        } else {
            strcpy(buf, "--:--");
        }
        lv_label_set_text(fc_time[i], buf);

        // 날씨
        const char* desc = list[i]["weather"][0]["description"];
        lv_label_set_text(fc_cond[i], translate_weather(desc));

        // 온도
        float t = list[i]["main"]["temp"];
        snprintf(buf, sizeof(buf), "%.0f°", t);
        lv_label_set_text(fc_temp[i], buf);
    }
    bsp_display_unlock();
}

// -------------------------------------------------------------------
// HTTP 요청
// -------------------------------------------------------------------
void fetch_weather(int idx) {
    if (WiFi.status() != WL_CONNECTED) { show_error("WiFi 연결 없음"); return; }
    WiFiClientSecure *c = new WiFiClientSecure;
    if (!c) { show_error("메모리 부족"); return; }
    c->setInsecure();
    HTTPClient http;
    String url = "https://api.openweathermap.org/data/2.5/weather?";
    url += "lat=" + String(k_cities[idx].lat, 4);
    url += "&lon=" + String(k_cities[idx].lon, 4);
    url += "&appid=" + String(weather_api_key) + "&units=metric";
    if (http.begin(*c, url)) {
        int code = http.GET();
        if (code == HTTP_CODE_OK) update_weather_ui(http.getString());
        else { char m[32]; snprintf(m,sizeof(m),"HTTP 오류 %d",code); show_error(m); }
        http.end();
    } else show_error("연결 실패");
    delete c;
}

void fetch_forecast(int idx) {
    if (WiFi.status() != WL_CONNECTED) return;
    WiFiClientSecure *c = new WiFiClientSecure;
    if (!c) return;
    c->setInsecure();
    HTTPClient http;
    String url = "https://api.openweathermap.org/data/2.5/forecast?";
    url += "lat=" + String(k_cities[idx].lat, 4);
    url += "&lon=" + String(k_cities[idx].lon, 4);
    url += "&appid=" + String(weather_api_key) + "&units=metric&cnt=8";
    if (http.begin(*c, url)) {
        if (http.GET() == HTTP_CODE_OK) update_forecast_ui(http.getString());
        http.end();
    }
    delete c;
}

// -------------------------------------------------------------------
// UI 콜백
// -------------------------------------------------------------------
static void back_to_list_cb   (lv_event_t *e) { lv_obj_set_tile_id(tv, 0, 0, LV_ANIM_ON); }
static void to_forecast_cb    (lv_event_t *e) { lv_obj_set_tile_id(tv, 2, 0, LV_ANIM_ON); }
static void back_to_detail_cb (lv_event_t *e) { lv_obj_set_tile_id(tv, 1, 0, LV_ANIM_ON); }

static void city_event_cb(lv_event_t *e) {
    current_city_idx = (int)lv_event_get_user_data(e);
    fetch_requested  = true;
    // LVGL 태스크 내부이므로 lock 불필요 — 즉시 도시명 표시
    lv_label_set_text(label_city_name, k_cities[current_city_idx].name);
    lv_label_set_text(label_condition, "...");
    lv_obj_set_tile_id(tv, 1, 0, LV_ANIM_ON);
}

// 헤더 내비 버튼 생성 헬퍼
static void make_nav_btn(lv_obj_t *parent, int x, int y,
                         const char *text, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 80, 30);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn, 25, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 15, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
}

// 예보 카드 생성 헬퍼 (시간 + 온도 + 날씨)
static void make_fc_card(lv_obj_t *parent, int x, int y,
                          lv_obj_t **time_out, lv_obj_t **cond_out, lv_obj_t **temp_out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, 112, 129);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, 22, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(card, 45, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    *time_out = lv_label_create(card);
    lv_label_set_text(*time_out, "--:--");
    lv_obj_set_style_text_font(*time_out, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*time_out, lv_color_hex(0x7C8FA8), 0);
    lv_obj_align(*time_out, LV_ALIGN_TOP_MID, 0, 0);

    *temp_out = lv_label_create(card);
    lv_label_set_text(*temp_out, "--°");
    lv_obj_set_style_text_font(*temp_out, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(*temp_out, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(*temp_out, LV_ALIGN_CENTER, 0, -4);

    *cond_out = lv_label_create(card);
    lv_label_set_text(*cond_out, "");
    lv_obj_set_style_text_font(*cond_out, &font_kr_14, 0);
    lv_obj_set_style_text_color(*cond_out, lv_color_hex(0xAABBDD), 0);
    lv_obj_set_style_max_width(*cond_out, 96, 0);
    lv_label_set_long_mode(*cond_out, LV_LABEL_LONG_CLIP);
    lv_obj_align(*cond_out, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// 벤토 카드 생성 헬퍼
static void make_card(lv_obj_t *parent, int x, int y, int w, int h,
                      const char *title, lv_obj_t **val_out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(card, 35, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(card, 55, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lt = lv_label_create(card);
    lv_label_set_text(lt, title);
    lv_obj_set_style_text_font(lt, &font_kr_14, 0);
    lv_obj_set_style_text_color(lt, lv_color_hex(0x90B4E0), 0);
    lv_obj_align(lt, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lv = lv_label_create(card);
    lv_label_set_text(lv, "--");
    lv_obj_set_style_text_font(lv, &font_kr_20, 0);
    lv_obj_set_style_text_color(lv, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lv, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    *val_out = lv;
}

// -------------------------------------------------------------------
// UI 빌드
// -------------------------------------------------------------------
void setup_ui() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0A0E1A), 0);
    tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_opa(tv, 0, 0);

    // =============================================
    // Tile 0: 도시 목록
    // =============================================
    city_list_page = lv_tileview_add_tile(tv, 0, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(city_list_page, lv_color_hex(0x0A0E1A), 0);
    lv_obj_set_style_pad_all(city_list_page, 0, 0);

    lv_obj_t *hdr0 = lv_obj_create(city_list_page);
    lv_obj_set_size(hdr0, 480, 48);  lv_obj_set_pos(hdr0, 0, 0);
    lv_obj_set_style_bg_color(hdr0, lv_color_hex(0x111827), 0);
    lv_obj_set_style_border_width(hdr0, 0, 0);
    lv_obj_set_style_radius(hdr0, 0, 0);
    lv_obj_clear_flag(hdr0, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t0 = lv_label_create(hdr0);
    lv_label_set_text(t0, "KOREA WEATHER");
    lv_obj_set_style_text_font(t0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t0, lv_color_hex(0x7C8FA8), 0);
    lv_obj_align(t0, LV_ALIGN_LEFT_MID, 16, 0);

    label_status = lv_label_create(hdr0);
    lv_label_set_text(label_status, "");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF5252), 0);
    lv_obj_align(label_status, LV_ALIGN_RIGHT_MID, -16, 0);

    lv_obj_t *grid = lv_obj_create(city_list_page);
    lv_obj_set_size(grid, 480, 272);  lv_obj_set_pos(grid, 0, 48);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x0A0E1A), 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);

    for (int i = 0; i < city_count; i++) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 150, 48);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2035), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A3A60), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x2A3A55), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, city_event_cb, LV_EVENT_CLICKED, (void*)i);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, k_cities[i].name);
        lv_obj_set_style_text_font(lbl, &font_kr_20, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xCDD8F0), 0);
        lv_obj_center(lbl);
    }

    // =============================================
    // Tile 1: 날씨 디테일
    // =============================================
    // y=0   h=44 : 헤더 (LIST< / 도시명 / >FCST)
    // y=44  h=90 : 메인 (온도 + 날짜시간 + 날씨설명)
    // y=134 h=87 : 벤토행1 (습도/풍속/풍향/운량)
    // y=225 h=89 : 벤토행2 (체감/최저최고/기압/가시거리)  ← 하단 6px 여백
    detail_page = lv_tileview_add_tile(tv, 1, 0, LV_DIR_HOR);
    lv_obj_set_style_pad_all(detail_page, 0, 0);

    bg_overlay = lv_obj_create(detail_page);
    lv_obj_set_size(bg_overlay, 480, 320);  lv_obj_set_pos(bg_overlay, 0, 0);
    lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x0D47A1), 0);
    lv_obj_set_style_bg_opa(bg_overlay, 255, 0);
    lv_obj_set_style_border_width(bg_overlay, 0, 0);
    lv_obj_set_style_radius(bg_overlay, 0, 0);
    lv_obj_clear_flag(bg_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr1 = lv_obj_create(detail_page);
    lv_obj_set_size(hdr1, 480, 44);  lv_obj_set_pos(hdr1, 0, 0);
    lv_obj_set_style_bg_color(hdr1, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hdr1, 70, 0);
    lv_obj_set_style_border_width(hdr1, 0, 0);
    lv_obj_set_style_radius(hdr1, 0, 0);
    lv_obj_clear_flag(hdr1, LV_OBJ_FLAG_SCROLLABLE);

    make_nav_btn(hdr1,   6, 7, "< LIST", back_to_list_cb);
    make_nav_btn(hdr1, 394, 7, "FCST >", to_forecast_cb);

    label_city_name = lv_label_create(hdr1);
    lv_obj_set_style_text_font(label_city_name, &font_kr_32, 0);
    lv_obj_set_style_text_color(label_city_name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_city_name, LV_ALIGN_CENTER, 0, 0);

    // 온도 (좌측)
    label_temp = lv_label_create(detail_page);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(label_temp, 16, 50);

    // 날짜/시간 (온도 아래, 작게)
    label_datetime = lv_label_create(detail_page);
    lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_datetime, lv_color_hex(0x8899BB), 0);
    lv_obj_set_pos(label_datetime, 18, 104);

    // 날씨 설명 (우측)
    label_condition = lv_label_create(detail_page);
    lv_obj_set_style_text_font(label_condition, &font_kr_20, 0);
    lv_obj_set_style_text_color(label_condition, lv_color_hex(0xBBD4FF), 0);
    lv_obj_set_style_max_width(label_condition, 210, 0);
    lv_label_set_long_mode(label_condition, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(label_condition, 248, 62);

    // 벤토 행 1 (y=134, h=87)
    make_card(detail_page,   6, 134, 112, 87, "습도",  &label_humidity_val);
    make_card(detail_page, 124, 134, 112, 87, "풍속",  &label_wind_val);
    make_card(detail_page, 242, 134, 112, 87, "풍향",  &label_wind_dir_val);
    make_card(detail_page, 360, 134, 112, 87, "운량",  &label_clouds_val);

    // 벤토 행 2 (y=225, h=89) — 하단 y+h=314, 화면 6px 여백
    make_card(detail_page,   6, 225, 112, 89, "체감온도",  &label_feels_val);
    make_card(detail_page, 124, 225, 112, 89, "최저/최고", &label_minmax_val);
    make_card(detail_page, 242, 225, 112, 89, "기압",      &label_pressure_val);
    make_card(detail_page, 360, 225, 112, 89, "가시거리",  &label_vis_val);

    lv_label_set_text(label_city_name, k_cities[0].name);
    lv_label_set_text(label_temp,      "--");
    lv_label_set_text(label_datetime,  "");
    lv_label_set_text(label_condition, "...");

    // =============================================
    // Tile 2: 시간별 예보 (3시간 간격 × 8)
    // =============================================
    // y=0  h=44 : 헤더
    // y=44 h=34 × 8 = 272px (하단 4px 여백)
    forecast_page = lv_tileview_add_tile(tv, 2, 0, LV_DIR_HOR);
    lv_obj_set_style_pad_all(forecast_page, 0, 0);

    lv_obj_t *fc_bg = lv_obj_create(forecast_page);
    lv_obj_set_size(fc_bg, 480, 320);  lv_obj_set_pos(fc_bg, 0, 0);
    lv_obj_set_style_bg_color(fc_bg, lv_color_hex(0x0A1628), 0);
    lv_obj_set_style_bg_opa(fc_bg, 255, 0);
    lv_obj_set_style_border_width(fc_bg, 0, 0);
    lv_obj_set_style_radius(fc_bg, 0, 0);
    lv_obj_clear_flag(fc_bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr2 = lv_obj_create(forecast_page);
    lv_obj_set_size(hdr2, 480, 44);  lv_obj_set_pos(hdr2, 0, 0);
    lv_obj_set_style_bg_color(hdr2, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hdr2, 70, 0);
    lv_obj_set_style_border_width(hdr2, 0, 0);
    lv_obj_set_style_radius(hdr2, 0, 0);
    lv_obj_clear_flag(hdr2, LV_OBJ_FLAG_SCROLLABLE);

    make_nav_btn(hdr2, 6, 7, "< DETAIL", back_to_detail_cb);

    lv_obj_t *fc_title = lv_label_create(hdr2);
    lv_label_set_text(fc_title, "3-HOUR FORECAST");
    lv_obj_set_style_text_font(fc_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(fc_title, lv_color_hex(0x7C8FA8), 0);
    lv_obj_align(fc_title, LV_ALIGN_CENTER, 0, 0);

    // 벤토 그리드 4×2 (가로 시간순)
    // x: 6, 124, 242, 360  /  Row1 y=50, Row2 y=185
    static const int xs[4] = {6, 124, 242, 360};
    for (int i = 0; i < 8; i++) {
        int x = xs[i % 4];
        int y = (i < 4) ? 50 : 185;
        make_fc_card(forecast_page, x, y, &fc_time[i], &fc_cond[i], &fc_temp[i]);
    }
}

// -------------------------------------------------------------------
// 메인
// -------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16384;
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = port_cfg,
        .buffer_size   = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate        = LV_DISP_ROT_90,
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    setup_ui();
    bsp_display_unlock();

    WiFi.begin(ssid, password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(500); retry++; }

    if (WiFi.status() == WL_CONNECTED) {
        fetch_weather(current_city_idx);
        fetch_forecast(current_city_idx);
        last_weather_update = millis();
    } else {
        bsp_display_lock(0);
        lv_label_set_text(label_status, "WiFi FAIL");
        bsp_display_unlock();
    }
}

void loop() {
    unsigned long now = millis();
    if (fetch_requested) {
        fetch_requested  = false;
        last_weather_update = now;
        fetch_weather(current_city_idx);
        forecast_requested = true;
    } else if (forecast_requested) {
        forecast_requested = false;
        fetch_forecast(current_city_idx);
    } else if (now - last_weather_update > weather_interval) {
        last_weather_update = now;
        fetch_weather(current_city_idx);
        fetch_forecast(current_city_idx);
    }
    delay(10);
}
