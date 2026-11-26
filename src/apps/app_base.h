#pragma once
#include <M5Cardputer.h>
#include "input.h"
#include "config.h"

class App {
public:
    virtual ~App() = default;
    virtual AppId id() const = 0;
    virtual const char* name() const = 0;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onInput(const InputEvent& ev) = 0;
    virtual void onTick() {}
    virtual void render(M5Canvas& spr) = 0;
};

