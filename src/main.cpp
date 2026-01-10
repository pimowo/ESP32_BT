/*
   PCM | ESP32
  -----|-------------
   SCK | NC/GND
   BCK | GPIO 26
   DIN | GPIO 22
  LRCK | GPIO 25
   GND | GND
   VCC | +5V
*/

// Główny plik projektu ESP32 BT A2DP
// Obsługuje Bluetooth audio, metadane przez UART1, status przez UART0/1

#include <Arduino.h>
#include <WiFi.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_gap_bt_api.h"
#include "esp_task_wdt.h"
#include "config.h"
#include "utils.h"
#include "callbacks.h"

// ============================================================================
// ZMIENNE GLOBALNE - współdzielone między plikami
// ============================================================================

// Bufory metadanych - volatile z ochroną mutex
volatile char currentTitle[METADATA_BUFFER_SIZE] = "";
volatile char currentArtist[METADATA_BUFFER_SIZE] = "";
portMUX_TYPE metadataMux = portMUX_INITIALIZER_UNLOCKED;

// Flagi atomowe dla callbacków
volatile bool titleChanged = false;
volatile bool artistChanged = false;
volatile bool shouldPrintDisconnection = false;
volatile bool audioStateChanged = false;
volatile uint8_t currentAudioState = 0;
volatile bool connectionPending = false;

// Cache dla metadanych (nie volatile - tylko w loop)
char printedTitle[METADATA_BUFFER_SIZE] = "";
char printedArtist[METADATA_BUFFER_SIZE] = "";

// Dane połączonego urządzenia
char pendingDeviceName[NAME_BUFFER_SIZE] = "";
char pendingDeviceMAC[MAC_BUFFER_SIZE] = "";
char connectedDeviceName[NAME_BUFFER_SIZE] = "";
char connectedDeviceMAC[MAC_BUFFER_SIZE] = "";

// Status połączenia i głośności
bool isConnected = false;
bool volumeSetPending = false;
uint32_t volumeSetTime = 0;

// Obiekty główne: I2S dla audio, BT A2DP Sink
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

// ============================================================================
// SETUP - Inicjalizacja systemu przy starcie
// ============================================================================

void setup() {
  // Inicjalizacja watchdog (10s timeout) dla stabilności systemu
  esp_task_wdt_init(10, true);

  // Inicjalizacja UART dla PC (debug/status) - jeśli włączony
  Serial.begin(115200);
  if (!Serial) {
    // Jeśli Serial nie działa, zatrzymaj (rzadkie)
    while (true) delay(1000);
  }

  // Inicjalizacja UART1 dla metadanych i statusu (GPIO 17 TX)
  Serial1.begin(115200);
  if (!Serial1) {
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:ERROR:UART1_INIT_FAILED");
    while (true) delay(1000);
  }

  // Wyłącz WiFi całkowicie dla oszczędności energii
  WiFi.mode(WIFI_OFF);

  if (ENABLE_SERIAL_DEBUG) Serial.flush();
  delay(100);

  if (ENABLE_SERIAL_DEBUG) Serial.println("BT:INIT:START");
  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("BT:VERSION:");
    Serial.println(VERSION);
  }

  // Konfiguracja I2S - optymalna dla PCM5102
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = I2S_BCK;
  cfg.pin_ws = I2S_LRCK;
  cfg.pin_data = I2S_DIN;
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = 32;  // Kluczowe dla PCM5102
  cfg.channels = 2;
  cfg.buffer_count = 8;      // Optymalne dla płynności
  cfg.buffer_size = 512;

  if (!i2s.begin(cfg)) {
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:ERROR:I2S_INIT_FAILED");
    while (true) delay(1000);  // Zatrzymaj jeśli I2S nie działa
  }

  // Rejestracja callbacków dla BT
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);

  // Start Bluetooth A2DP Sink
  a2dp_sink.start(BT_DEVICE_NAME);

  // Konfiguracja GAP dla parowania
  esp_bt_gap_register_callback(esp_bt_gap_cb);
  if (esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) != ESP_OK) {
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:ERROR:GAP_CONFIG_FAILED");
  }

  // Ustawienia bezpieczeństwa PIN
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT;
  if (esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t)) != ESP_OK) {
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:ERROR:SECURITY_CONFIG_FAILED");
  }

  if (ENABLE_SERIAL_DEBUG) Serial.println("BT:INIT:OK");
  if (ENABLE_SERIAL_DEBUG) Serial.println("BT:AUDIO:PCM5102");
  if (ENABLE_SERIAL_DEBUG) Serial.println("BT:I2S:BCK26_WS25_DATA22");
  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("BT:PIN:");
    Serial.println(BT_PIN_CODE);
  }
  if (ENABLE_SERIAL_DEBUG) Serial.flush();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

// ============================================================================
// MAIN LOOP - Główna pętla programu
// ============================================================================

void loop() {
  // Jawny feed watchdog dla stabilności
  yield();

  uint32_t now = millis();

  // 1. Obsługa nowego połączenia BT - wykonaj tylko raz
  if (connectionPending && !isConnected) {
    connectionPending = false;
    isConnected = true;

    strcpy(connectedDeviceMAC, pendingDeviceMAC);
    strcpy(connectedDeviceName, pendingDeviceName);

    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:CONNECTED");
    Serial1.println("BT:CONNECTED");

    if (connectedDeviceMAC[0] != '\0') {
      if (ENABLE_SERIAL_DEBUG) {
        Serial.print("BT:MAC:");
        Serial.println(connectedDeviceMAC);
      }
      Serial1.print("BT:MAC:");
      Serial1.println(connectedDeviceMAC);
    }

    if (connectedDeviceName[0] != '\0') {
      if (ENABLE_SERIAL_DEBUG) {
        Serial.print("BT:NAME:");
        Serial.println(connectedDeviceName);
      }
      Serial1.print("BT:NAME:");
      Serial1.println(connectedDeviceName);
    }

    if (ENABLE_SERIAL_DEBUG) Serial.flush();
    Serial1.flush();
  }

  // 2. Ustawienie głośności z opóźnieniem
  if (volumeSetPending && (millis() - volumeSetTime >= VOLUME_DELAY_MS)) {
    volumeSetPending = false;
    a2dp_sink.set_volume(127);
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:VOLUME:MAX");
    if (ENABLE_SERIAL_DEBUG) Serial.flush();
  }

  // 3. Obsługa rozłączenia BT
  if (shouldPrintDisconnection) {
    shouldPrintDisconnection = false;
    if (ENABLE_SERIAL_DEBUG) Serial.println("BT:DISCONNECTED");
    Serial1.println("BT:DISCONNECTED");
    if (ENABLE_SERIAL_DEBUG) Serial.flush();
    Serial1.flush();
  }

  // 4. Zmiany stanu audio (play/stop)
  if (audioStateChanged) {
    audioStateChanged = false;

    switch (currentAudioState) {
      case ESP_A2D_AUDIO_STATE_STARTED:
        if (ENABLE_SERIAL_DEBUG) Serial.println("BT:PLAYING");
        Serial1.println("BT:PLAYING");
        break;
      case ESP_A2D_AUDIO_STATE_STOPPED:
        if (ENABLE_SERIAL_DEBUG) Serial.println("BT:STOPPED");
        Serial1.println("BT:STOPPED");
        break;
    }

    if (ENABLE_SERIAL_DEBUG) Serial.flush();
    Serial1.flush();
  }
  
  // 5. Wysyłanie metadanych - ARTIST (przez UART1)
  if (artistChanged) {
    artistChanged = false;

    char localArtist[METADATA_BUFFER_SIZE];
    copyVolatileString(currentArtist, localArtist, METADATA_BUFFER_SIZE);

    if (localArtist[0] != '\0' && strcmp(localArtist, printedArtist) != 0) {
      Serial1.print("BT:ARTIST:");
      Serial1.println(localArtist);
      strcpy(printedArtist, localArtist);
      Serial1.flush();
    }
  }

  // 6. Wysyłanie metadanych - TITLE (przez UART1)
  if (titleChanged) {
    titleChanged = false;

    char localTitle[METADATA_BUFFER_SIZE];
    copyVolatileString(currentTitle, localTitle, METADATA_BUFFER_SIZE);

    if (localTitle[0] != '\0' && strcmp(localTitle, printedTitle) != 0) {
      Serial1.print("BT:TITLE:");
      Serial1.println(localTitle);
      strcpy(printedTitle, localTitle);
      Serial1.flush();
    }
  }

  delay(LOOP_DELAY_MS);
}