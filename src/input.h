#pragma once
#include <Arduino.h>

enum KeyCode : uint8_t {
    CK_NONE = 0,
    CK_CHAR,
    CK_UP,
    CK_DOWN,
    CK_LEFT,
    CK_RIGHT,
    CK_ENTER,
    CK_ESC,
    CK_BACKSPACE,
    CK_TAB,
    CK_DEL
};

struct InputEvent {
    KeyCode code = CK_NONE;
    char ch = 0;
    bool fn = false;
    bool ctrl = false;
    bool fromSerial = false;

    InputEvent() = default;
    InputEvent(KeyCode c, char ch_ = 0, bool fn_ = false, bool ctrl_ = false, bool ser_ = false)
        : code(c), ch(ch_), fn(fn_), ctrl(ctrl_), fromSerial(ser_) {}
};

void initInput();
void pollKeyboardInput();
void pushInputEvent(const InputEvent& ev);
bool popInputEvent(InputEvent& ev);

// Helpers for screens that allow single-key arrow/back without holding Fn
inline bool isNavUp(const InputEvent& e)    { return e.code == CK_UP || (e.code == CK_CHAR && e.ch == ';'); }
inline bool isNavDown(const InputEvent& e)  { return e.code == CK_DOWN || (e.code == CK_CHAR && e.ch == '.'); }
inline bool isNavLeft(const InputEvent& e)  { return e.code == CK_LEFT || (e.code == CK_CHAR && e.ch == ','); }
inline bool isNavRight(const InputEvent& e) { return e.code == CK_RIGHT || (e.code == CK_CHAR && e.ch == '/'); }
inline bool isNavBack(const InputEvent& e)  { return e.code == CK_ESC || (e.code == CK_CHAR && e.ch == '`'); }

