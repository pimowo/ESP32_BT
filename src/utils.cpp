// utils.cpp - Implementacje funkcji pomocniczych
#include "utils.h"

// Konwertuje adres MAC na string
void macToString(const uint8_t *mac, char *output) {
  if (!mac || !output) return;
  snprintf(output, MAC_BUFFER_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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