# CardOS Developer Guide

Welcome to building apps for **CardOS** on the M5Stack **Cardputer ADV** (ESP32-S3, `240x135` TFT display, QWERTY keyboard, SD card, IR TX, speaker/AUX codec, microphone, and RGB LED).

You can build CardOS apps in **two ways**:
1. **Lua App Store Apps** (recommended for games, tools, and shareable Store apps — runs directly from SD `/store/<id>.lua` without recompiling firmware).
2. **Native C++ Built-in Apps** (for hardware-heavy features like I2S audio, BLE, Wi-Fi servers, or background tasks).

---

## Option 1: Building a Lua App (No Firmware Compile Needed)

### 1. Hardware Constraints to Know
- Display resolution: **`240 x 135` px** (Top status bar takes `y = 0..15`; draw app content in `y = 16..134`).
- Free SRAM for Lua VM: ESP32-S3 without PSRAM has ~320 KB total SRAM. Keep scripts concise (ideally **< 8 KB**), avoid allocating big tables every frame, and let `onTick()`/`onRender()` reuse variables cleanly.

### 2. Required Lifecycle Hooks
Every CardOS Lua script defines up to 4 global hooks called by `LuaRunnerApp`:

```lua
function onEnter()
  -- Called once when the app opens.
  -- Initialize ALL tables/variables here before onRender() runs!
end

function onTick()
  -- Called every frame (~16-35ms).
  -- Update animations, physics, and smooth key polling here.
end

function onInput(key, ch)
  -- Called on discrete key presses/repeats.
  -- key: "esc", "enter", "up", "down", "left", "right", "back", "tab", "del", "char"
  -- ch:  single-character string when key == "char" (e.g. "a", "1", " ")
  if key == "esc" or ch == "`" then
    cardos.exit()
  end
end

function onRender()
  -- Draw the entire frame inside the sprite buffer.
  cardos.clear()
  cardos.statusBar("My App")
  cardos.text("Hello CardOS!", 120, 68, "accent", 1, "C")
end
```

### 3. Complete `cardos.*` Lua API Reference

#### Drawing & Colors
Valid color names (theme-aware): `"bg"`, `"surface"`, `"surface2"`, `"accent"`, `"accent2"`, `"text"`, `"dim"`, `"border"`, `"ok"`, `"warn"`, `"err"`.

- `cardos.clear([color])` — Clears screen (defaults to `"bg"`).
- `cardos.statusBar(title)` — Draws the 16 px OS status bar with Wi-Fi, battery, and time.
- `cardos.fillRect(x, y, w, h, radius, color)` — Filled rounded rectangle (`radius = 0` for sharp rectangle).
- `cardos.drawRect(x, y, w, h, radius, color)` — Stroked rounded rectangle.
- `cardos.circle(x, y, r, color, [filled])` — Circle outline (`filled = false`) or solid disc (`filled = true`).
- `cardos.text(str, x, y, [color], [size], [align])` — Draws text:
  - `size`: integer scale (`1`, `2`, `3`...).
  - `align`: `"L"` (left, default), `"C"` (center), `"R"` (right).
- `cardos.scrollbar(x, y, h, totalItems, visibleItems, scrollIdx)` — Draws matching OS scrollbar.
- `cardos.toast(msg, [ms])` — Shows an OS toast popup (`ms` defaults to `1600`).

#### Input, Timing & System
- `cardos.isKeyDown(key)` -> `boolean` — Live non-blocking held-key check (ideal inside `onTick()` for silky-smooth games!):
  - Supported directions: `"left"` (also `A` / `,`), `"right"` (also `D` / `/`), `"up"` (also `W` / `;`), `"down"` (also `S` / `.`), `"space"`, `"enter"`, or any single character like `"1"`, `"q"`.
- `cardos.millis()` -> `number` — Milliseconds since boot.
- `cardos.exit()` — Closes the app and returns to Home.

#### Storage & Infrared Hardware
- `cardos.fileExists(path)` -> `boolean` — Checks file on active storage (`/store/...`).
- `cardos.readFile(path)` -> `string | nil` — Reads file (up to 4 KB).
- `cardos.writeFile(path, content)` -> `boolean` — Writes text file (great for high scores / saves!).
- `cardos.countLines(path)` -> `number` — Counts lines in large SD files without loading into RAM.
- `cardos.readLine(path, lineNum)` -> `string | nil` — Reads 1-indexed line from SD.
- `cardos.irSend(protocol, hexCode, [bits])` -> `boolean` — Transmits IR signal from Cardputer's IR LED (`"NEC"`, `"SAMSUNG"`, `"SONY"`, `"LG"`, `"RC5"`, `"RC6"`, etc.).

### 4. Example: Smooth Animated Starter Game (`/store/myapp.lua`)
```lua
local x, lm = 110, 0
function onEnter()
  x, lm = 110, cardos.millis()
end
function onTick()
  local now = cardos.millis()
  if now - lm < 25 then return end
  lm = now
  if cardos.isKeyDown("left")  then x = math.max(8, x - 3) end
  if cardos.isKeyDown("right") then x = math.min(212, x + 3) end
end
function onInput(k, c)
  if k == "esc" or c == "`" then cardos.exit() end
end
function onRender()
  cardos.clear()
  cardos.statusBar("Smooth Starter")
  cardos.fillRect(math.floor(x), 110, 20, 8, 2, "accent")
  cardos.text("Hold A/D or Left/Right!", 120, 60, "dim", 1, "C")
end
```

### 5. Testing on SD Card & Publishing to GitHub Store
1. Test locally first: copy `myapp.lua` to SD `/store/myapp.lua` and create `/store/myapp.json`:
   ```json
   {"id":"myapp","name":"My App","version":"1.0","description":"Short summary","script":"apps/myapp.lua"}
   ```
   (And register `"myapp"` in `/cardos_layout.json`, or install from Store!)
2. Add your files to `store/apps/myapp.lua` and `store/apps/myapp.json`, and add an entry to `store/catalog.json`:
   ```json
   {"id":"myapp","name":"My App","category":"Games","version":"1.0","desc":"Short description","pkg":"apps/myapp.json"}
   ```
3. To push an app update to users, bump `"version"` in **both** `store/apps/myapp.json` and `store/catalog.json` (e.g. `"1.0"` -> `"1.1"`).

---

## Option 2: Building a Native C++ App
1. Add your `APP_MYAPP` enum value to `enum AppId` in `src/config.h` (before `APP_LUA_RUNNER`).
2. Add `{APP_MYAPP, "myapp"}` to `APP_KEYS[]` in `src/state.cpp`.
3. Add a vector icon case in `ui::drawAppIcon()` in `src/ui.cpp`. Use `ui::drawWrappedText()` and `ui::drawTextModal()` so text wraps cleanly within `240x135`.
4. Implement a subclass of `App` (`id()`, `name()`, `onEnter()`, `onInput()`, `render()`, optional `onTick()`/`onExit()`) in `src/apps/app_myapp.cpp`, declare `App* createMyApp();` in `src/apps/apps_registry.h`, and register it in `src/main.cpp`.
5. Build with PlatformIO: `python3 -m platformio run`.

