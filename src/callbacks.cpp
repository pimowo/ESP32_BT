// callbacks.cpp - Implementacje funkcji callback
#include "callbacks.h"
#include "utils.h"
#include <Arduino.h>

// Zewnętrzne zmienne (z main.cpp)
extern volatile char currentTitle[METADATA_BUFFER_SIZE];
extern volatile char currentArtist[METADATA_BUFFER_SIZE];
extern portMUX_TYPE metadataMux;
extern volatile bool titleChanged;
extern volatile bool artistChanged;
extern volatile bool shouldPrintDisconnection;
extern volatile bool audioStateChanged;
extern volatile uint8_t currentAudioState;
extern volatile bool connectionPending;
extern char pendingDeviceName[NAME_BUFFER_SIZE];
extern char pendingDeviceMAC[MAC_BUFFER_SIZE];

// Callback dla metadanych AVRCP
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

// Callback dla GAP BT
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (!param) return;

  switch (event) {
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT: {
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

    case ESP_BT_GAP_PIN_REQ_EVT: {
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

// Callback dla stanu połączenia A2DP
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    connectionPending = true;
    // volumeSetPending = true; // Przeniesione do main
    // volumeSetTime = millis() + VOLUME_DELAY_MS;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    // Logika rozłączenia przeniesiona do main
    shouldPrintDisconnection = true;
  }
}

// Callback dla stanu audio A2DP
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  currentAudioState = (uint8_t)state;
  audioStateChanged = true;
}