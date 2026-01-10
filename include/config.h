// config.h - Stałe konfiguracyjne dla projektu ESP32 BT
#ifndef CONFIG_H
#define CONFIG_H

// Wersja projektu
#define VERSION 0.1

// Piny I2S dla audio
#define I2S_BCK   26  // Bit Clock
#define I2S_LRCK  25  // Word Select (Left/Right Clock)
#define I2S_DIN   22  // Data In

// Parametry buforów i opóźnień
#define METADATA_BUFFER_SIZE 256  // Rozmiar bufora dla metadanych (tytuł/artysta)
#define NAME_BUFFER_SIZE 64       // Rozmiar bufora dla nazwy urządzenia
#define MAC_BUFFER_SIZE 18        // Rozmiar bufora dla adresu MAC (17 znaków + null)
#define BT_DEVICE_NAME "yoRadio PMW"  // Nazwa widoczna w BT
#define BT_PIN_CODE "9876"            // PIN do parowania
#define VOLUME_DELAY_MS 2000          // Opóźnienie ustawienia głośności po połączeniu
#define LOOP_DELAY_MS 5               // Opóźnienie w głównej pętli (ms)

// Debugowanie - włącz/wyłącz komunikaty na UART0 (Serial)
#define ENABLE_SERIAL_DEBUG 1  // 1 - włącz debug (komunikaty na PC), 0 - wyłącz (oszczędność zasobów)

#endif