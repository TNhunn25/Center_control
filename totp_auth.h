#pragma once

#include <Arduino.h>

bool generateTotpSecret(char *dest, size_t destSize);
bool verifyTotpCode(const char *base32Secret, const char *code, uint32_t unixTime, uint8_t windowSteps = 1);
bool isTotpCodeFormat(const char *code);
String buildTotpUri(const char *issuer, const char *account, const char *base32Secret);
