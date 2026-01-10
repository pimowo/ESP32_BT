// callbacks.h - Deklaracje funkcji callback
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <esp_a2d.h>
#include <esp_avrc_api.h>

// Callback dla metadanych AVRCP
void avrc_metadata_callback(uint8_t id, const uint8_t *text);

// Callback dla GAP BT
void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

// Callback dla stanu połączenia A2DP
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr);

// Callback dla stanu audio A2DP
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr);

#endif