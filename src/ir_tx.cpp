#include "ir_tx.h"
#include "config.h"
#include <esp_arduino_version.h>
#include <string.h>

static bool s_irInited = false;
static uint32_t s_curFreq = 38000;
static bool s_rcToggle = false;
static constexpr uint8_t IR_LEDC_CH = 2; // Timer 1; avoids M5GFX Light_PWM channel 7 (Timer 3)

static void irSetCarrier(uint32_t freqHz) {
    if (!s_irInited || s_curFreq != freqHz) {
        s_curFreq = freqHz;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcDetach(IR_TX_PIN);
        ledcAttach(IR_TX_PIN, freqHz, 8);
        ledcWrite(IR_TX_PIN, 0);
#else
        ledcSetup(IR_LEDC_CH, freqHz, 8);
        ledcAttachPin(IR_TX_PIN, IR_LEDC_CH);
        ledcWrite(IR_LEDC_CH, 0);
#endif
        s_irInited = true;
    }
}

static inline void irMark(uint32_t us) {
    if (us == 0) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(IR_TX_PIN, 85); // ~33% duty cycle for crisp IR LED drive
#else
    ledcWrite(IR_LEDC_CH, 85);
#endif
    delayMicroseconds(us);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(IR_TX_PIN, 0);
#else
    ledcWrite(IR_LEDC_CH, 0);
#endif
}

static inline void irSpace(uint32_t us) {
    if (us == 0) return;
    if (us >= 16000) {
        delay(us / 1000);
        delayMicroseconds(us % 1000);
    } else {
        delayMicroseconds(us);
    }
}

void initIrTx() {
    pinMode(IR_TX_PIN, OUTPUT);
    digitalWrite(IR_TX_PIN, LOW);
}

static void sendPulseDistMsb(uint64_t data, uint8_t bits,
                             uint32_t hdrMark, uint32_t hdrSpace,
                             uint32_t bitMark, uint32_t oneSpace, uint32_t zeroSpace,
                             uint32_t stopMark) {
    if (hdrMark) irMark(hdrMark);
    if (hdrSpace) irSpace(hdrSpace);
    for (int i = bits - 1; i >= 0; --i) {
        irMark(bitMark);
        bool one = ((data >> i) & 1ULL) != 0;
        irSpace(one ? oneSpace : zeroSpace);
    }
    if (stopMark) irMark(stopMark);
}

static void sendSony(uint64_t data, uint8_t bits) {
    irSetCarrier(40000);
    for (int rep = 0; rep < 3; ++rep) {
        uint32_t startUs = micros();
        irMark(2400);
        irSpace(600);
        for (int i = bits - 1; i >= 0; --i) {
            bool one = ((data >> i) & 1ULL) != 0;
            irMark(one ? 1200 : 600);
            irSpace(600);
        }
        uint32_t elapsed = micros() - startUs;
        if (rep < 2 && elapsed < 45000) {
            irSpace(45000 - elapsed);
        }
    }
}

static inline void manchesterBit(bool one, uint32_t halfUs) {
    // IEEE 802.3 / RC5 / RC6 convention: 1 = low then high (space then mark), 0 = mark then space
    if (one) {
        irSpace(halfUs);
        irMark(halfUs);
    } else {
        irMark(halfUs);
        irSpace(halfUs);
    }
}

static void sendRc5(uint64_t data, uint8_t bits) {
    irSetCarrier(36000);
    if (bits < 12) bits = 13;
    // Toggle bit is bit 11 in standard 13/14-bit RC5
    uint64_t frame = data;
    if (s_rcToggle) frame |= (1ULL << 11);
    else            frame &= ~(1ULL << 11);
    s_rcToggle = !s_rcToggle;

    for (int rep = 0; rep < 2; ++rep) {
        for (int i = bits - 1; i >= 0; --i) {
            manchesterBit(((frame >> i) & 1ULL) != 0, 889);
        }
        irSpace(89000);
    }
}

static void sendRc6(uint64_t data, uint8_t bits) {
    irSetCarrier(36000);
    if (bits < 16) bits = 20;
    uint64_t frame = data;
    if (bits >= 17) {
        // Flip trailer toggle bit (bit 16 in 20-bit RC6 mode 0)
        if (s_rcToggle) frame |= (1ULL << 16);
        else            frame &= ~(1ULL << 16);
    }
    s_rcToggle = !s_rcToggle;

    // Leader: 2666us mark, 889us space
    irMark(2666);
    irSpace(889);
    // Start bit (1, inverted in RC6 physical: mark 444, space 444)
    irMark(444);
    irSpace(444);
    // Remaining bits MSB-first; trailer bit at index 16 is double width (889us)
    for (int i = bits - 1; i >= 0; --i) {
        bool one = ((frame >> i) & 1ULL) != 0;
        uint32_t halfUs = (i == 16) ? 889 : 444;
        if (one) {
            irMark(halfUs);
            irSpace(halfUs);
        } else {
            irSpace(halfUs);
            irMark(halfUs);
        }
    }
    irSpace(2666);
}

static void sendSharp(uint64_t data, uint8_t bits) {
    irSetCarrier(38000);
    if (bits == 0) bits = 15;
    sendPulseDistMsb(data, bits, 0, 0, 320, 1680, 680, 320);
    irSpace(40000);
    // Sharp second frame inverts lower 10 bits (command + expansion/check)
    uint64_t inv = data ^ 0x3FFULL;
    sendPulseDistMsb(inv, bits, 0, 0, 320, 1680, 680, 320);
}

bool irTxSend(const char* proto, uint64_t code, uint8_t bits) {
    if (!proto || !proto[0] || code == 0) return false;
    if (!s_irInited) initIrTx();

    if (strcasecmp(proto, "NEC") == 0) {
        irSetCarrier(38000);
        sendPulseDistMsb(code, bits ? bits : 32, 9000, 4500, 560, 1690, 560, 560);
    } else if (strcasecmp(proto, "SAMSUNG") == 0 || strcasecmp(proto, "SAMSUNG32") == 0) {
        irSetCarrier(38000);
        sendPulseDistMsb(code, bits ? bits : 32, 4500, 4500, 560, 1690, 560, 560);
    } else if (strncasecmp(proto, "SONY", 4) == 0) {
        uint8_t sb = bits;
        if (strcasecmp(proto, "SONY12") == 0) sb = 12;
        else if (strcasecmp(proto, "SONY15") == 0) sb = 15;
        else if (strcasecmp(proto, "SONY20") == 0) sb = 20;
        else if (sb == 0) sb = 12;
        sendSony(code, sb);
    } else if (strcasecmp(proto, "RC5") == 0) {
        sendRc5(code, bits ? bits : 13);
    } else if (strcasecmp(proto, "RC6") == 0) {
        sendRc6(code, bits ? bits : 20);
    } else if (strcasecmp(proto, "PANASONIC") == 0 || strcasecmp(proto, "KASEIKYO") == 0) {
        irSetCarrier(38000);
        sendPulseDistMsb(code, bits ? bits : 48, 3500, 1750, 435, 1305, 435, 435);
    } else if (strcasecmp(proto, "JVC") == 0) {
        irSetCarrier(38000);
        sendPulseDistMsb(code, bits ? bits : 16, 8400, 4200, 525, 1575, 525, 525);
        irSpace(18000);
        sendPulseDistMsb(code, bits ? bits : 16, 0, 0, 525, 1575, 525, 525);
    } else if (strcasecmp(proto, "SHARP") == 0) {
        sendSharp(code, bits ? bits : 15);
    } else {
        // Fallback NEC 38 kHz
        irSetCarrier(38000);
        sendPulseDistMsb(code, bits ? bits : 32, 9000, 4500, 560, 1690, 560, 560);
    }

    // Ensure IR LED is completely off after transmit
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(IR_TX_PIN, 0);
#else
    ledcWrite(IR_LEDC_CH, 0);
#endif
    return true;
}

