#include "input.h"
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static QueueHandle_t s_inputQueue = nullptr;

// Repeat tracking
static String s_lastSig = "";
static uint32_t s_pressStartMs = 0;
static uint32_t s_lastRepeatMs = 0;

void initInput() {
    if (!s_inputQueue) {
        s_inputQueue = xQueueCreate(48, sizeof(InputEvent));
    }
}

void pushInputEvent(const InputEvent& ev) {
    if (s_inputQueue && ev.code != CK_NONE) {
        xQueueSend(s_inputQueue, &ev, 0);
    }
}

bool popInputEvent(InputEvent& ev) {
    if (!s_inputQueue) return false;
    return xQueueReceive(s_inputQueue, &ev, 0) == pdTRUE;
}

static void emitFromKeyboardState() {
    auto st = M5Cardputer.Keyboard.keysState();

    if (st.esc) {
        pushInputEvent({CK_ESC, 0, st.fn, st.ctrl, false});
        return;
    }
    if (st.backspace || st.del) {
        pushInputEvent({CK_BACKSPACE, 0, st.fn, st.ctrl, false});
        return;
    }
    if (st.up)    { pushInputEvent({CK_UP,    0, st.fn, st.ctrl, false}); return; }
    if (st.down)  { pushInputEvent({CK_DOWN,  0, st.fn, st.ctrl, false}); return; }
    if (st.left)  { pushInputEvent({CK_LEFT,  0, st.fn, st.ctrl, false}); return; }
    if (st.right) { pushInputEvent({CK_RIGHT, 0, st.fn, st.ctrl, false}); return; }
    if (st.enter) {
        pushInputEvent({CK_ENTER, 0, st.fn, st.ctrl, false});
        return;
    }
    if (st.tab) {
        pushInputEvent({CK_TAB, 0, st.fn, st.ctrl, false});
        return;
    }

    for (auto c : st.word) {
        if (st.fn) {
            if (c == ';')      { pushInputEvent({CK_UP, 0, true, st.ctrl, false}); continue; }
            if (c == '.')      { pushInputEvent({CK_DOWN, 0, true, st.ctrl, false}); continue; }
            if (c == ',')      { pushInputEvent({CK_LEFT, 0, true, st.ctrl, false}); continue; }
            if (c == '/')      { pushInputEvent({CK_RIGHT, 0, true, st.ctrl, false}); continue; }
            if (c == '`')      { pushInputEvent({CK_ESC, 0, true, st.ctrl, false}); continue; }
            if (c == ' ')      { pushInputEvent({CK_TAB, 0, true, st.ctrl, false}); continue; }
        }
        if (c == 0x1B) {
            pushInputEvent({CK_ESC, 0, st.fn, st.ctrl, false});
        } else if (c == 0x08 || c == 0x7F) {
            pushInputEvent({CK_BACKSPACE, 0, st.fn, st.ctrl, false});
        } else {
            pushInputEvent({CK_CHAR, (char)c, st.fn, st.ctrl, false});
        }
    }
}

void pollKeyboardInput() {
    bool changed = M5Cardputer.Keyboard.isChange();
    bool pressed = M5Cardputer.Keyboard.isPressed();

    if (!pressed) {
        s_lastSig = "";
        return;
    }

    auto st = M5Cardputer.Keyboard.keysState();
    bool hasSpecial = st.enter || st.backspace || st.del || st.tab || st.esc ||
                      st.up || st.down || st.left || st.right;

    // Ignore pure modifier-only state
    if (st.word.empty() && !hasSpecial) {
        return;
    }

    String sig;
    sig += st.fn ? 'F' : '_';
    sig += st.ctrl ? 'C' : '_';
    sig += st.enter ? 'E' : '_';
    sig += st.backspace ? 'B' : '_';
    sig += st.del ? 'D' : '_';
    sig += st.tab ? 'T' : '_';
    sig += st.esc ? 'X' : '_';
    sig += st.up ? 'U' : '_';
    sig += st.down ? 'N' : '_';
    sig += st.left ? 'L' : '_';
    sig += st.right ? 'R' : '_';
    for (auto c : st.word) sig += (char)c;

    uint32_t now = millis();
    if (changed || sig != s_lastSig) {
        s_lastSig = sig;
        s_pressStartMs = now;
        s_lastRepeatMs = now;
        emitFromKeyboardState();
    } else if (now - s_pressStartMs > 380 && now - s_lastRepeatMs > 75) {
        s_lastRepeatMs = now;
        emitFromKeyboardState();
    }
}
