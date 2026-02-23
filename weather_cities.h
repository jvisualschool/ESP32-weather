#ifndef WEATHER_CITIES_H
#define WEATHER_CITIES_H

struct City {
    const char* name;
    float lat;
    float lon;
};

// 실제 정의는 weather_cities.cpp 에 있음
extern City k_cities[];
extern const int city_count;

#endif
