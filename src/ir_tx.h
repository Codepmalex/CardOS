#pragma once
#include <Arduino.h>
#include <stdint.h>

void initIrTx();
bool irTxSend(const char* proto, uint64_t code, uint8_t bits = 32);

