// config.h - Stałe konfiguracyjne dla projektu ESP32 BT
#ifndef CONFIG_H
#define CONFIG_H

// Wersja projektu
#define VERSION 1

// Piny I2S
#define I2S_BCK   26
#define I2S_LRCK  25
#define I2S_DIN   22

// Parametry
#define METADATA_BUFFER_SIZE 256
#define NAME_BUFFER_SIZE 64
#define MAC_BUFFER_SIZE 18
#define BT_DEVICE_NAME "yoRadio PMW"
#define BT_PIN_CODE "9876"
#define VOLUME_DELAY_MS 2000
#define LOOP_DELAY_MS 5

#endif