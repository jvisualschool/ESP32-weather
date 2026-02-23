# ESP32 날씨 디스플레이

ESP32-S3 + QSPI LCD(AXS15231B)를 사용한 국내 도시 날씨 앱.
LVGL 기반 UI로 도시 목록과 날씨 상세 정보를 표시합니다.

## 화면 구성

| 도시 목록 | 날씨 상세 |
|---|---|
| 국내 31개 도시 리스트 | 온도 / 날씨 상태 / 습도 / 풍속 |
| 도시 탭 → 상세 페이지로 전환 | 온도에 따라 배경색 변경 |

## 하드웨어

| 항목 | 내용 |
|---|---|
| MCU | ESP32-S3 (240MHz, 8MB OPI PSRAM) |
| 디스플레이 | AXS15231B QSPI LCD 320×480 |
| 인터페이스 | QSPI (SPI2_HOST) |
| 터치 | AXS15231B I2C 터치 |

### 핀 배열

| 신호 | GPIO |
|---|---|
| QSPI CS | 45 |
| QSPI CLK | 47 |
| QSPI D0–D3 | 21, 48, 40, 39 |
| DC | 8 |
| TE (Tear) | 38 |
| 백라이트 | 1 |
| Touch SDA | 4 |
| Touch SCL | 8 |

## 소프트웨어 의존성

- **Arduino IDE** (ESP32 보드 패키지 3.x)
- **lvgl** 8.3.x
- **ArduinoJson** 7.x

## 설치 및 빌드

### 1. 설정 파일 생성

```bash
cp config.example.h config.h
```

`config.h`를 열어 Wi-Fi 정보와 API 키를 입력합니다:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* weather_api_key = "YOUR_OPENWEATHERMAP_KEY";
```

> OpenWeatherMap API 키는 [https://openweathermap.org](https://openweathermap.org) 에서 무료로 발급받을 수 있습니다.

### 2. 빌드 & 업로드 (arduino-cli)

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,UploadSpeed=921600" \
  .

arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,UploadSpeed=921600" \
  --port /dev/cu.usbmodem101 \
  .
```

### 3. 시리얼 모니터

```bash
arduino-cli monitor --port /dev/cu.usbmodem101 --config baudrate=115200
```

## 파일 구조

```
ESP32_STOCK/
├── ESP32_STOCK.ino       # 메인 스케치 (UI, WiFi, 날씨 API)
├── config.h              # 자격증명 (gitignore, 직접 생성 필요)
├── config.example.h      # 설정 템플릿
├── weather_cities.h      # 도시 구조체 선언
├── weather_cities.cpp    # 국내 31개 도시 좌표 데이터
├── esp_bsp.h / .c        # Board Support Package (디스플레이 초기화)
├── lv_port.h / .c        # LVGL 포팅 레이어
├── display.h             # LCD 해상도 상수
├── lv_conf.h             # LVGL 설정
├── font_kr.h             # 한글 폰트 선언
├── font_kr_14/20/32.c    # NotoSansKR 서브셋 폰트 (lv_font_conv 생성)
├── esp_lcd_axs15231b.*   # LCD 패널 드라이버
└── esp_lcd_touch.*       # 터치 드라이버
```

## 주요 설계

- **LVGL full_refresh 모드**: `lv_port.c`가 `full_refresh=1`을 강제하므로 디스플레이 버퍼는 전체 화면 크기(320×480) 이상이어야 함. 버퍼는 PSRAM에 할당.
- **비동기 날씨 갱신**: 도시 선택 이벤트(LVGL 태스크)에서 직접 HTTP 호출하면 렌더링이 동결되므로, `fetch_requested` 플래그를 세워 `loop()`에서 처리.
- **한글 폰트**: Montserrat는 한글 미지원. NotoSansKR로 도시명·에러 메시지에 사용되는 문자만 서브셋으로 생성 (14/20/32px).
- **날씨 자동 갱신**: 10분 주기 (`weather_interval = 600000ms`).

## 라이선스

- 소스 코드: MIT
- NotoSansKR 폰트: [SIL Open Font License 1.1](https://scripts.sil.org/OFL)
