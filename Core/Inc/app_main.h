/**
 * @file app_main.h
 * @brief Мост между сгенерированным CubeMX main.c (язык C) и приложением на C++.
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);

void app_step(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
