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
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_gap_bt_api.h"

// ============================================================================
// KONFIGURACJA
// ============================================================================

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
// FUNKCJE POMOCNICZE
// ============================================================================

void macToString(const uint8_t *mac, char *output) {
  if (!mac || !output) return;
  snprintf(output, MAC_BUFFER_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

inline void trySetVolume() {
  a2dp_sink.set_volume(127);
  Serial.println("BT:VOLUME:MAX");
  Serial.flush();
}

// Bezpieczna kopia volatile string z mutex
void copyVolatileString(volatile char *src, char *dst, size_t size) {
  portENTER_CRITICAL(&metadataMux);
  strncpy(dst, (char*)src, size - 1);
  dst[size - 1] = '\0';
  portEXIT_CRITICAL(&metadataMux);
}

// Bezpieczny zapis do volatile string z mutex
void writeVolatileString(volatile char *dst, const char *src, size_t size) {
  portENTER_CRITICAL(&metadataMux);
  strncpy((char*)dst, src, size - 1);
  dst[size - 1] = '\0';
  portEXIT_CRITICAL(&metadataMux);
}

// ============================================================================
// CALLBACKS
// ============================================================================

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  if (!text) return;
  
  switch (id) {
    case ESP_AVRC_MD_ATTR_ARTIST: 
      writeVolatileString(currentArtist, (char*)text, METADATA_BUFFER_SIZE);
      artistChanged = true;
      break;
      
    case ESP_AVRC_MD_ATTR_TITLE: 
      writeVolatileString(currentTitle, (char*)text, METADATA_BUFFER_SIZE);
      titleChanged = true;
      break;
  }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (!param) return;
  
  switch (event) {
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:  {
      if (param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS) {
        macToString(param->acl_conn_cmpl_stat.bda, pendingDeviceMAC);
        esp_bt_gap_read_remote_name(param->acl_conn_cmpl_stat.bda);
      }
      break;
    }
    
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
      if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
        strncpy(pendingDeviceName, (char*)param->read_rmt_name.rmt_name, NAME_BUFFER_SIZE - 1);
        pendingDeviceName[NAME_BUFFER_SIZE - 1] = '\0';
      } else {
        strcpy(pendingDeviceName, "Unknown");
      }
      break;
    }
    
    case ESP_BT_GAP_PIN_REQ_EVT:  {
      esp_bt_pin_code_t pin_code;
      const char *pin = BT_PIN_CODE;
      for (int i = 0; i < 4 && pin[i]; i++) {
        pin_code[i] = pin[i];
      }
      esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(BT_PIN_CODE), pin_code);
      break;
    }
    
    default:
      break;
  }
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    connectionPending = true;
    volumeSetPending = true;
    volumeSetTime = millis() + VOLUME_DELAY_MS;
    
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    if (isConnected) {
      isConnected = false;
      shouldPrintDisconnection = true;
      volumeSetPending = false;
      connectionPending = false;
      
      // Reset wszystkich buforów
      portENTER_CRITICAL(&metadataMux);
      currentTitle[0] = '\0';
      currentArtist[0] = '\0';
      portEXIT_CRITICAL(&metadataMux);
      
      printedTitle[0] = '\0';
      printedArtist[0] = '\0';
      connectedDeviceName[0] = '\0';
      connectedDeviceMAC[0] = '\0';
      pendingDeviceName[0] = '\0';
      pendingDeviceMAC[0] = '\0';
      
      titleChanged = false;
      artistChanged = false;
      audioStateChanged = false;
    }
  }
}

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  currentAudioState = (uint8_t)state;
  audioStateChanged = true;
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.flush();
  delay(100);
  
  Serial.println("BT:INIT:START");
  
  // Konfiguracja I2S - optymalna dla PCM5102
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = I2S_BCK;
  cfg.pin_ws = I2S_LRCK;
  cfg. pin_data = I2S_DIN;
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = 32;  // Kluczowe dla PCM5102
  cfg.channels = 2;
  cfg.buffer_count = 8;      // Optymalne dla płynności
  cfg.buffer_size = 512;
  
  if (! i2s.begin(cfg)) {
    Serial.println("BT:ERROR:I2S_INIT_FAILED");
    while(1) delay(1000);  // Zatrzymaj jeśli I2S nie działa
  }
  
  // Rejestracja callbacków
  a2dp_sink. set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink. set_on_connection_state_changed(connection_state_changed);
  a2dp_sink. set_on_audio_state_changed(audio_state_changed);
  
  // Start Bluetooth A2DP
  a2dp_sink.start(BT_DEVICE_NAME);
  
  // Konfiguracja GAP
  esp_bt_gap_register_callback(esp_bt_gap_cb);
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  
  // PIN pairing
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT;
  esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t));
  
  Serial.println("BT:INIT:OK");
  Serial.println("BT:AUDIO: PCM5102");
  Serial.println("BT:I2S:BCK26_WS25_DATA22");
  Serial.print("BT:PIN:");
  Serial.println(BT_PIN_CODE);
  Serial.flush();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Jawny feed watchdog
  yield();
  
  uint32_t now = millis();
  
  // 1. Obsługa nowego połączenia - TYLKO RAZ
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
  
  // 2. Volume timing - z ochroną przed overflow
  if (volumeSetPending) {
    if ((now - volumeSetTime) < 0x80000000UL) {
      volumeSetPending = false;
      trySetVolume();
    }
  }
  
  // 3. Rozłączenie
  if (shouldPrintDisconnection) {
    shouldPrintDisconnection = false;
    Serial.println("BT:DISCONNECTED");
    Serial.flush();
  }
  
  // 4. Audio state changes
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
    
    Serial. flush();
  }
  
  // 5. Metadata - ARTIST
  if (artistChanged) {
    artistChanged = false;
    
    char localArtist[METADATA_BUFFER_SIZE];
    copyVolatileString(currentArtist, localArtist, METADATA_BUFFER_SIZE);
    
    if (localArtist[0] != '\0' && strcmp(localArtist, printedArtist) != 0) {
      Serial.print("BT:ARTIST:");
      Serial.println(localArtist);
      strcpy(printedArtist, localArtist);
      Serial.flush();
    }
  }
  
  // 6. Metadata - TITLE
  if (titleChanged) {
    titleChanged = false;
    
    char localTitle[METADATA_BUFFER_SIZE];
    copyVolatileString(currentTitle, localTitle, METADATA_BUFFER_SIZE);
    
    if (localTitle[0] != '\0' && strcmp(localTitle, printedTitle) != 0) {
      Serial.print("BT:TITLE:");
      Serial.println(localTitle);
      strcpy(printedTitle, localTitle);
      Serial.flush();
    }
  }
  
  delay(LOOP_DELAY_MS);
}