// callbacks.h - Deklaracje funkcji callback dla BT
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <Arduino.h>
#include <esp_gap_bt_api.h>
#include "BluetoothA2DPCommon.h"

// Callback dla metadanych AVRCP (tytuł/artysta z muzyki)
void avrc_metadata_callback(uint8_t id, const uint8_t *text);

// Callback dla GAP BT (parowanie, nazwa urządzenia)
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

// Callback dla stanu połączenia A2DP (połączony/rozłączony)
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr);

// Callback dla stanu audio A2DP (odtwarzanie/zatrzymanie)
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr);

#endif