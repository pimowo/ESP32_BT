# ESP32 Bluetooth A2DP Sink z Metadanymi

Projekt ESP32 działający jako Bluetooth A2DP Sink dla streamingu audio, z obsługą metadanych (tytuł, artysta) wysyłanych przez dodatkowy UART.

## Wersja
v1.0

## Funkcjonalności
- **Bluetooth A2DP Sink**: Odbiór audio z urządzeń BT (np. telefonów).
- **Obsługa metadanych**: Tytuły utworów i artyści wysyłane przez UART1.
- **Optymalizacja energii**: WiFi i BLE wyłączone dla oszczędności.
- **Komunikacja**: Status przez UART0 (Serial), metadane przez UART1 (Serial1).

## Wymagania sprzętowe
- ESP32 (np. ESP-WROOM-32)
- DAC audio (np. PCM5102)
- Zasilanie: 5V (sieciowe dla maksymalnej wydajności)

### Połączenia
```
ESP32          PCM5102
-----          -------
GPIO 26 (BCK)  BCK
GPIO 25 (WS)   LCK
GPIO 22 (DIN)  DIN
GND            GND
5V             VCC

UART1 Metadane:
ESP32 TX (GPIO 17) -> RX drugiego urządzenia
```

## Instalacja i budowa
1. Zainstaluj PlatformIO w VS Code.
2. Sklonuj repo: `git clone https://github.com/pimowo/ESP32_BT.git`
3. Otwórz w VS Code, przejdź do projektu.
4. Build: `pio run`
5. Upload: `pio run --target upload`

## Konfiguracja
- **Nazwa BT**: "yoRadio PMW"
- **PIN**: "9876"
- **UART Status**: Serial (115200 baud) - GPIO 1 TX, GPIO 3 RX
- **UART Metadane**: Serial1 (115200 baud) - GPIO 17 TX

## Użycie
1. Wgraj firmware na ESP32.
2. Podłącz DAC i zasilanie.
3. Sparuj urządzenie BT z ESP32.
4. Streamuj audio - status w Serial, metadane w Serial1.

### Komunikaty
- **Status (UART0)**: `BT:INIT:OK`, `BT:CONNECTED`, `BT:PLAYING`, `BT:DISCONNECTED`
- **Metadane (UART1)**: `BT:ARTIST:Nazwa`, `BT:TITLE:Tytuł`

## Struktura projektu
```
include/
  config.h      - Stałe konfiguracyjne
  utils.h       - Funkcje pomocnicze
  callbacks.h   - Callbacki BT
src/
  main.cpp      - Główny kod
  utils.cpp     - Implementacje utils
  callbacks.cpp - Implementacje callbacków
platformio.ini  - Konfiguracja build
README.md       - Dokumentacja
```

## Optymalizacje
- CPU: 240 MHz dla płynnego audio BT.
- Partycje: huge_app.csv (3MB app space).
- Wyłączone: WiFi, BLE, debug.
- Zużycie: ~50-100 mA (BT active).

## Licencja
MIT

## Autor
Piotr
