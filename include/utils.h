// utils.h - Funkcje pomocnicze
#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// Konwertuje adres MAC na string
void macToString(const uint8_t *mac, char *output);

// Bezpieczna kopia volatile string z mutex
void copyVolatileString(volatile char *src, char *dst, size_t size);

// Bezpieczny zapis do volatile string z mutex
void writeVolatileString(volatile char *dst, const char *src, size_t size);

#endif