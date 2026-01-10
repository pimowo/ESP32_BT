/*
   PCM | ESP
  -----|-------------
   SCK | NC/GND  
   BCK | GPIO 26
   DIN | GPIO 22
  LRCK | GPIO 25
   GND | GND
   VCC | +5V
*/

#include <Arduino.h>
#include <WiFi.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_gap_bt_api.h"
#include "config.h"
#include "utils.h"
#include "callbacks.h"

// ============================================================================
// ZMIENNE GLOBALNE
// ============================================================================

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

// Bufory metadanych - volatile z mutex protection
volatile char currentTitle[METADATA_BUFFER_SIZE] = "";
volatile char currentArtist[METADATA_BUFFER_SIZE] = "";
portMUX_TYPE metadataMux = portMUX_INITIALIZER_UNLOCKED;

// Flagi atomiczne
volatile bool titleChanged = false;
volatile bool artistChanged = false;
volatile bool shouldPrintDisconnection = false;
volatile bool audioStateChanged = false;
volatile uint8_t currentAudioState = 0;
volatile bool connectionPending = false;

// Cache (nie volatile - tylko w loop)
char printedTitle[METADATA_BUFFER_SIZE] = "";
char printedArtist[METADATA_BUFFER_SIZE] = "";

// Dane połączenia
char pendingDeviceName[NAME_BUFFER_SIZE] = "";
char pendingDeviceMAC[MAC_BUFFER_SIZE] = "";
char connectedDeviceName[NAME_BUFFER_SIZE] = "";
char connectedDeviceMAC[MAC_BUFFER_SIZE] = "";

// Status
bool isConnected = false;
bool volumeSetPending = false;
uint32_t volumeSetTime = 0;

// ============================================================================
// SETUP
// ============================================================================

// ============================================================================
// SETUP - Inicjalizacja systemu
// ============================================================================

void setup() {
  // Inicjalizacja UART dla PC (debug/status)
  Serial.begin(115200);
  if (!Serial) {
    // Jeśli Serial nie działa, zatrzymaj (rzadkie)
    while (true) delay(1000);
  }

  // Inicjalizacja UART1 dla metadanych (GPIO 17 TX)
  Serial1.begin(115200);
  if (!Serial1) {
    Serial.println("BT:ERROR:UART1_INIT_FAILED");
    while (true) delay(1000);
  }

  // Wyłącz WiFi całkowicie dla oszczędności energii
  WiFi.mode(WIFI_OFF);

  Serial.flush();
  delay(100);

  Serial.println("BT:INIT:START");
  Serial.print("BT:VERSION:");
  Serial.println(VERSION);

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
    Serial.println("BT:ERROR:I2S_INIT_FAILED");
    while (true) delay(1000);  // Zatrzymaj jeśli I2S nie działa
  }
  
  // Rejestracja callbacków dla BT
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);

  // Start Bluetooth A2DP Sink
  if (!a2dp_sink.start(BT_DEVICE_NAME)) {
    Serial.println("BT:ERROR:BT_START_FAILED");
    while (true) delay(1000);
  }

  // Konfiguracja GAP dla parowania
  esp_bt_gap_register_callback(esp_bt_gap_cb);
  if (esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) != ESP_OK) {
    Serial.println("BT:ERROR:GAP_CONFIG_FAILED");
  }

  // Ustawienia bezpieczeństwa PIN
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT;
  if (esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t)) != ESP_OK) {
    Serial.println("BT:ERROR:SECURITY_CONFIG_FAILED");
  }
  
  Serial.println("BT:INIT:OK");
  Serial.println("BT:AUDIO:PCM5102");
  Serial.println("BT:I2S:BCK26_WS25_DATA22");
  Serial.print("BT:PIN:");
  Serial.println(BT_PIN_CODE);
  Serial.flush();
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

    Serial.println("BT:CONNECTED");

    if (connectedDeviceMAC[0] != '\0') {
      Serial.print("BT:MAC:");
      Serial.println(connectedDeviceMAC);
    }
    
    if (connectedDeviceName[0] != '\0') {
      Serial.print("BT:NAME:");
      Serial.println(connectedDeviceName);
    }

    Serial.flush();
  }

  // 2. Ustawienie głośności z opóźnieniem (ochrona przed overflow millis)
  if (volumeSetPending) {
    if ((now - volumeSetTime) < 0x80000000UL) {
      volumeSetPending = false;
      a2dp_sink.set_volume(127);
      Serial.println("BT:VOLUME:MAX");
      Serial.flush();
    }
  }

  // 3. Obsługa rozłączenia BT
  if (shouldPrintDisconnection) {
    shouldPrintDisconnection = false;
    Serial.println("BT:DISCONNECTED");
    Serial.flush();
  }

  // 4. Zmiany stanu audio (play/stop)
  if (audioStateChanged) {
    audioStateChanged = false;

    switch (currentAudioState) {
      case ESP_A2D_AUDIO_STATE_STARTED:
        Serial.println("BT:PLAYING");
        break;
      case ESP_A2D_AUDIO_STATE_STOPPED:
        Serial.println("BT:STOPPED");
        break;
    }

    Serial.flush();
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