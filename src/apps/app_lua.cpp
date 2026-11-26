#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../storage.h"
#include "../ui.h"
#include "../ir_tx.h"
#include "../bug_report.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

extern void switchApp(AppId id);

namespace {

static String s_pendingLuaAppId = "";
static M5Canvas* s_curSpr = nullptr;
static bool s_exitRequested = false;
static size_t s_lastFailSize = 0;

static void* luaAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return nullptr;
    }
    void* p = realloc(ptr, nsize);
    if (!p) s_lastFailSize = nsize;
    return p;
}

static void openLightLuaLibs(lua_State* L) {
    static const luaL_Reg libs[] = {
        {"",              luaopen_base},
        {LUA_TABLIBNAME,  luaopen_table},
        {LUA_STRLIBNAME,  luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {nullptr,         nullptr}
    };
    for (const luaL_Reg* lib = libs; lib->func; ++lib) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

struct LuaFileReaderCtx {
    File f;
    char buf[256];
};

static const char* luaSdFileReader(lua_State* L, void* ud, size_t* size) {
    (void)L;
    auto* ctx = static_cast<LuaFileReaderCtx*>(ud);
    if (!ctx || !ctx->f || !ctx->f.available()) {
        *size = 0;
        return nullptr;
    }
    int n = ctx->f.readBytes(ctx->buf, sizeof(ctx->buf));
    if (n <= 0) {
        *size = 0;
        return nullptr;
    }
    *size = (size_t)n;
    return ctx->buf;
}

static uint16_t luaColor(const char* name) {
    const auto& th = activeTheme();
    uint16_t acc = activeAccent();
    if (!name) return th.text;
    if (strcmp(name, "bg") == 0)       return th.bg;
    if (strcmp(name, "surface") == 0)  return th.surface;
    if (strcmp(name, "surface2") == 0) return th.surface2;
    if (strcmp(name, "accent") == 0)   return acc;
    if (strcmp(name, "accent2") == 0)  return th.accent2;
    if (strcmp(name, "text") == 0)     return th.text;
    if (strcmp(name, "dim") == 0)      return th.textDim;
    if (strcmp(name, "border") == 0)   return th.border;
    if (strcmp(name, "ok") == 0)       return th.ok;
    if (strcmp(name, "warn") == 0)     return th.warn;
    if (strcmp(name, "err") == 0)      return th.err;
    return th.text;
}

static int l_clear(lua_State* L) {
    (void)L;
    if (s_curSpr) s_curSpr->fillScreen(activeTheme().bg);
    return 0;
}

static int l_statusBar(lua_State* L) {
    const char* title = luaL_optstring(L, 1, "App");
    if (s_curSpr) ui::drawStatusBar(*s_curSpr, title);
    return 0;
}

static int l_fillRect(lua_State* L) {
    if (!s_curSpr) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int r = (int)luaL_optinteger(L, 5, 0);
    uint16_t col = luaColor(luaL_optstring(L, 6, "surface"));
    if (r > 0) s_curSpr->fillRoundRect(x, y, w, h, r, col);
    else       s_curSpr->fillRect(x, y, w, h, col);
    return 0;
}

static int l_drawRect(lua_State* L) {
    if (!s_curSpr) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int r = (int)luaL_optinteger(L, 5, 0);
    uint16_t col = luaColor(luaL_optstring(L, 6, "border"));
    if (r > 0) s_curSpr->drawRoundRect(x, y, w, h, r, col);
    else       s_curSpr->drawRect(x, y, w, h, col);
    return 0;
}

static int l_circle(lua_State* L) {
    if (!s_curSpr) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint16_t col = luaColor(luaL_optstring(L, 4, "accent"));
    bool filled = lua_toboolean(L, 5) != 0;
    if (filled) s_curSpr->fillCircle(x, y, r, col);
    else        s_curSpr->drawCircle(x, y, r, col);
    return 0;
}

static int l_text(lua_State* L) {
    if (!s_curSpr) return 0;
    const char* s = luaL_optstring(L, 1, "");
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    uint16_t col = luaColor(luaL_optstring(L, 4, "text"));
    int sz = (int)luaL_optinteger(L, 5, 1);
    const char* align = luaL_optstring(L, 6, "L");
    uint8_t datum = TL_DATUM;
    if (align[0] == 'C' || align[0] == 'c')      datum = MC_DATUM;
    else if (align[0] == 'R' || align[0] == 'r') datum = TR_DATUM;
    else if (align[0] == 'T' || align[0] == 't') datum = TC_DATUM;

    s_curSpr->setTextSize(constrain(sz, 1, 3));
    s_curSpr->setTextColor(col);
    s_curSpr->setTextDatum(datum);
    s_curSpr->drawString(s, x, y);
    s_curSpr->setTextDatum(TL_DATUM);
    s_curSpr->setTextSize(1);
    return 0;
}

static int l_scrollbar(lua_State* L) {
    if (!s_curSpr) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);
    int total = (int)luaL_checkinteger(L, 4);
    int vis = (int)luaL_checkinteger(L, 5);
    int scroll = (int)luaL_checkinteger(L, 6);
    ui::drawScrollbar(*s_curSpr, x, y, h, total, vis, scroll);
    return 0;
}

static int l_toast(lua_State* L) {
    const char* msg = luaL_optstring(L, 1, "");
    uint32_t ms = (uint32_t)luaL_optinteger(L, 2, 1600);
    showToast(msg, ms);
    return 0;
}

static int l_millis(lua_State* L) {
    lua_pushnumber(L, (lua_Number)millis());
    return 1;
}

static int l_exit(lua_State* L) {
    (void)L;
    s_exitRequested = true;
    return 0;
}

static int l_irSend(lua_State* L) {
    const char* proto = luaL_checkstring(L, 1);
    const char* hexStr = luaL_checkstring(L, 2);
    uint8_t bits = (uint8_t)luaL_optinteger(L, 3, 32);
    if (!hexStr || !hexStr[0]) {
        lua_pushboolean(L, 0);
        return 1;
    }
    uint64_t code = (uint64_t)strtoull(hexStr, nullptr, 16);
    bool ok = irTxSend(proto, code, bits);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int l_readFile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    if ((!g_sys.sdMounted && !g_sys.spiffsMounted) || !activeFS().exists(path)) {
        lua_pushnil(L);
        return 1;
    }
    String txt = readTextFile(path, 16384);
    lua_pushlstring(L, txt.c_str(), txt.length());
    return 1;
}

static int l_writeFile(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 2, &len);
    String txt;
    txt.reserve(len + 1);
    for (size_t i = 0; i < len; ++i) txt += data[i];
    bool ok = writeTextFile(path, txt);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int l_fileExists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool ex = (g_sys.sdMounted || g_sys.spiffsMounted) && activeFS().exists(path);
    lua_pushboolean(L, ex ? 1 : 0);
    return 1;
}

static int l_countLines(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    if ((!g_sys.sdMounted && !g_sys.spiffsMounted) || !activeFS().exists(path)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    File f = activeFS().open(path, FILE_READ);
    if (!f) {
        lua_pushinteger(L, 0);
        return 1;
    }
    uint8_t buf[128];
    int count = 0;
    bool hasChars = false;
    while (f.available()) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                if (hasChars) { count++; hasChars = false; }
            } else if (buf[i] != '\r') {
                hasChars = true;
            }
        }
    }
    if (hasChars) count++;
    f.close();
    lua_pushinteger(L, count);
    return 1;
}

static int l_readLine(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int target = (int)luaL_checkinteger(L, 2); // 1-based non-empty line index
    if (target < 1 || (!g_sys.sdMounted && !g_sys.spiffsMounted) || !activeFS().exists(path)) {
        lua_pushnil(L);
        return 1;
    }
    File f = activeFS().open(path, FILE_READ);
    if (!f) {
        lua_pushnil(L);
        return 1;
    }
    char lineBuf[384];
    size_t lineLen = 0;
    uint8_t buf[128];
    int cur = 0;
    bool found = false;

    while (f.available() && !found) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (lineLen > 0) {
                    cur++;
                    if (cur == target) {
                        found = true;
                        break;
                    }
                    lineLen = 0;
                }
            } else if (cur + 1 == target) {
                if (lineLen + 1 < sizeof(lineBuf)) {
                    lineBuf[lineLen++] = c;
                }
            } else {
                // Non-empty marker for earlier lines without storing bytes
                lineLen = 1;
            }
        }
    }
    if (!found && lineLen > 0 && cur + 1 == target) {
        found = true;
    }
    f.close();

    if (found) {
        lua_pushlstring(L, lineBuf, lineLen);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_isKeyDown(lua_State* L) {
    const char* key = luaL_optstring(L, 1, "");
    if (!key || !key[0] || !M5Cardputer.Keyboard.isPressed()) {
        lua_pushboolean(L, 0);
        return 1;
    }
    auto st = M5Cardputer.Keyboard.keysState();
    auto hasCh = [&](char c1, char c2) -> bool {
        for (char c : st.word) {
            if (c == c1 || c == c2) return true;
        }
        return false;
    };
    bool hit = false;
    if (strcasecmp(key, "left") == 0) {
        hit = st.left || hasCh(',', ',') || hasCh('a', 'A');
    } else if (strcasecmp(key, "right") == 0) {
        hit = st.right || hasCh('/', '/') || hasCh('d', 'D');
    } else if (strcasecmp(key, "up") == 0) {
        hit = st.up || hasCh(';', ';') || hasCh('w', 'W');
    } else if (strcasecmp(key, "down") == 0) {
        hit = st.down || hasCh('.', '.') || hasCh('s', 'S');
    } else if (strcasecmp(key, "space") == 0) {
        hit = hasCh(' ', ' ');
    } else if (strcasecmp(key, "enter") == 0) {
        hit = st.enter;
    } else {
        char lo = (char)tolower((unsigned char)key[0]);
        char up = (char)toupper((unsigned char)key[0]);
        hit = hasCh(lo, up);
    }
    lua_pushboolean(L, hit ? 1 : 0);
    return 1;
}

static const luaL_Reg CARDOS_LUA_FUNCS[] = {
    {"clear",      l_clear},
    {"statusBar",  l_statusBar},
    {"fillRect",   l_fillRect},
    {"drawRect",   l_drawRect},
    {"circle",     l_circle},
    {"text",       l_text},
    {"scrollbar",  l_scrollbar},
    {"toast",      l_toast},
    {"millis",     l_millis},
    {"exit",       l_exit},
    {"irSend",     l_irSend},
    {"readFile",   l_readFile},
    {"writeFile",  l_writeFile},
    {"fileExists", l_fileExists},
    {"countLines", l_countLines},
    {"readLine",   l_readLine},
    {"isKeyDown",  l_isKeyDown},
    {nullptr, nullptr}
};

class LuaRunnerApp : public App {
public:
    AppId id() const override { return APP_LUA_RUNNER; }
    const char* name() const override { return "Lua App"; }

    void onEnter() override {
        cleanupVm();
        m_errorMsg = "";
        m_reportStatus = "";
        m_reportedOk = false;
        s_exitRequested = false;
        s_lastFailSize = 0;
        m_appId = s_pendingLuaAppId;

        if (m_appId.isEmpty()) {
            recordLuaError("init", "No app selected");
            return;
        }
        String path = "/store/" + m_appId + ".lua";
        if ((!g_sys.sdMounted && !g_sys.spiffsMounted) || !activeFS().exists(path.c_str())) {
            recordLuaError("init", ("Missing " + path).c_str());
            return;
        }

        // Release audio decoder RAM before booting Lua VM
        stopMusicPlayback();

        m_L = lua_newstate(luaAlloc, nullptr);
        if (!m_L) {
            recordLuaError("newstate", "Out of RAM for Lua VM");
            return;
        }
        lua_gc(m_L, LUA_GCSETPAUSE, 105);
        lua_gc(m_L, LUA_GCSETSTEPMUL, 400);
        openLightLuaLibs(m_L);
        luaL_register(m_L, "cardos", CARDOS_LUA_FUNCS);
        lua_pop(m_L, 1);

        LuaFileReaderCtx ctx;
        ctx.f = activeFS().open(path.c_str(), FILE_READ);
        if (!ctx.f) {
            recordLuaError("open", "Failed to open script");
            cleanupVm();
            return;
        }

        int loadErr = lua_load(m_L, luaSdFileReader, &ctx, path.c_str());
        ctx.f.close();

        if (loadErr != 0) {
            recordLuaError("load", lua_tostring(m_L, -1));
            cleanupVm();
            return;
        }
        if (lua_pcall(m_L, 0, 0, 0) != 0) {
            recordLuaError("exec", lua_tostring(m_L, -1));
            cleanupVm();
            return;
        }
        lua_gc(m_L, LUA_GCCOLLECT, 0);

        callLuaHook("onEnter");
        if (s_exitRequested) {
            s_exitRequested = false;
            switchApp(APP_HOME);
        }
    }

    void onExit() override {
        cleanupVm();
    }

    void onTick() override {
        if (!m_L) return;
        callLuaHook("onTick");
        if (m_L) lua_gc(m_L, LUA_GCSTEP, 2);
        if (s_exitRequested) {
            s_exitRequested = false;
            switchApp(APP_HOME);
        }
    }

    void onInput(const InputEvent& ev) override {
        if (!m_L) {
            if (ev.code == CK_CHAR && (ev.ch == 'r' || ev.ch == 'R')) {
                showToast("Reporting to GitHub...", 1200);
                m_reportedOk = submitBugReportToGithub("Lua:" + m_appId, m_errorMsg, m_reportStatus);
                showToast(m_reportStatus.c_str(), 2200);
                return;
            }
            if (isNavBack(ev) || ev.code == CK_ENTER) {
                switchApp(APP_HOME);
            }
            return;
        }

        const char* k = "char";
        char chStr[2] = {0, 0};
        switch (ev.code) {
            case CK_UP:        k = "up";    break;
            case CK_DOWN:      k = "down";  break;
            case CK_LEFT:      k = "left";  break;
            case CK_RIGHT:     k = "right"; break;
            case CK_ENTER:     k = "enter"; break;
            case CK_ESC:       k = "esc";   break;
            case CK_BACKSPACE: k = "back";  break;
            case CK_TAB:       k = "tab";   break;
            case CK_DEL:       k = "del";   break;
            case CK_CHAR:
                k = "char";
                chStr[0] = ev.ch;
                break;
            default:
                return;
        }

        lua_getglobal(m_L, "onInput");
        if (lua_isfunction(m_L, -1)) {
            lua_pushstring(m_L, k);
            lua_pushstring(m_L, chStr);
            if (lua_pcall(m_L, 2, 0, 0) != 0) {
                recordLuaError("onInput", lua_tostring(m_L, -1));
                cleanupVm();
            }
        } else {
            lua_pop(m_L, 1);
            if (isNavBack(ev)) s_exitRequested = true;
        }

        if (s_exitRequested) {
            s_exitRequested = false;
            switchApp(APP_HOME);
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();

        if (!m_L) {
            spr.fillScreen(th.bg);
            ui::drawStatusBar(spr, "Store App Error");
            spr.fillRoundRect(6, 20, SCREEN_W - 12, 92, 6, th.surface);
            spr.drawRoundRect(6, 20, SCREEN_W - 12, 92, 6, th.err);
            spr.setTextSize(1);
            spr.setTextColor(th.err);
            spr.drawString("Lua Error (" + m_appId + "):", 12, 26);
            spr.setTextColor(th.text);
            int y = 39;
            int maxY = m_reportStatus.isEmpty() ? 98 : 84;
            for (int pos = 0; pos < (int)m_errorMsg.length() && y < maxY; pos += 35) {
                spr.drawString(m_errorMsg.substring(pos, min((int)m_errorMsg.length(), pos + 35)), 12, y);
                y += 11;
            }
            if (!m_reportStatus.isEmpty()) {
                spr.setTextColor(m_reportedOk ? th.ok : th.warn);
                String rs = m_reportStatus;
                if ((int)rs.length() > 35) rs = rs.substring(0, 34) + "~";
                spr.drawString(rs, 12, 96);
            }
            spr.fillRoundRect(6, 116, 108, 16, 4, th.err);
            spr.setTextColor(th.bg);
            spr.setTextDatum(MC_DATUM);
            spr.drawString("[R] Report Error", 60, 124);
            spr.setTextDatum(TL_DATUM);
            spr.setTextColor(acc);
            spr.drawString("[ESC/Ent] Home", 122, 120);
            ui::drawToastOverlay(spr);
            return;
        }

        s_curSpr = &spr;
        spr.fillScreen(th.bg);
        callLuaHook("onRender");
        s_curSpr = nullptr;
        ui::drawToastOverlay(spr);
    }

private:
    lua_State* m_L = nullptr;
    String m_appId;
    String m_errorMsg;
    String m_reportStatus;
    bool m_reportedOk = false;

    void cleanupVm() {
        s_curSpr = nullptr;
        if (m_L) {
            lua_close(m_L);
            m_L = nullptr;
        }
    }

    void recordLuaError(const char* stage, const char* msg) {
        char buf[192];
        snprintf(buf, sizeof(buf), "[%s] %s | Free:%luK Max:%luK Req:%u",
                 stage ? stage : "lua",
                 msg ? msg : "err",
                 (unsigned long)(ESP.getFreeHeap() / 1024),
                 (unsigned long)(ESP.getMaxAllocHeap() / 1024),
                 (unsigned)s_lastFailSize);
        m_errorMsg = buf;
        Serial.println(buf);
    }

    void callLuaHook(const char* fn) {
        if (!m_L) return;
        lua_getglobal(m_L, fn);
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, 0) != 0) {
                recordLuaError(fn, lua_tostring(m_L, -1));
                cleanupVm();
            }
        } else {
            lua_pop(m_L, 1);
        }
    }
};

} // namespace

App* createLuaRunnerApp() { return new LuaRunnerApp(); }

void launchLuaApp(const String& appId) {
    s_pendingLuaAppId = appId;
    switchApp(APP_LUA_RUNNER);
}
