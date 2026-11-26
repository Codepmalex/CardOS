#include "apps_registry.h"
#include "../config.h"
#include "../state.h"
#include "../ui.h"
#include <math.h>
#include <vector>

extern void switchApp(AppId id);

namespace {

// Simple recursive-descent expression parser
class ExprParser {
public:
    bool eval(const String& s, double& out, String& err) {
        m_src = s;
        m_pos = 0;
        m_err = "";
        out = parseExpr();
        skipWs();
        if (!m_err.isEmpty()) { err = m_err; return false; }
        if (m_pos < (int)m_src.length()) { err = "Syntax err"; return false; }
        if (isnan(out) || isinf(out)) { err = "Math err"; return false; }
        return true;
    }

private:
    String m_src;
    int m_pos = 0;
    String m_err;

    void skipWs() {
        while (m_pos < (int)m_src.length() && isspace(m_src[m_pos])) m_pos++;
    }
    char peek() {
        skipWs();
        return (m_pos < (int)m_src.length()) ? m_src[m_pos] : '\0';
    }

    double parseExpr() {
        double v = parseTerm();
        while (m_err.isEmpty()) {
            char c = peek();
            if (c == '+') { m_pos++; v += parseTerm(); }
            else if (c == '-') { m_pos++; v -= parseTerm(); }
            else break;
        }
        return v;
    }

    double parseTerm() {
        double v = parsePower();
        while (m_err.isEmpty()) {
            char c = peek();
            if (c == '*') { m_pos++; v *= parsePower(); }
            else if (c == '/') {
                m_pos++;
                double d = parsePower();
                if (d == 0.0) { m_err = "Div by 0"; return 0; }
                v /= d;
            } else break;
        }
        return v;
    }

    double parsePower() {
        double v = parseUnary();
        if (peek() == '^') {
            m_pos++;
            double e = parsePower();
            v = pow(v, e);
        }
        return v;
    }

    double parseUnary() {
        char c = peek();
        if (c == '-') { m_pos++; return -parseUnary(); }
        if (c == '+') { m_pos++; return parseUnary(); }
        return parsePrimary();
    }

    double parsePrimary() {
        char c = peek();
        if (c == '(') {
            m_pos++;
            double v = parseExpr();
            if (peek() == ')') m_pos++;
            else m_err = "Missing )";
            return v;
        }
        if (isdigit(c) || c == '.') {
            int start = m_pos;
            while (m_pos < (int)m_src.length() && (isdigit(m_src[m_pos]) || m_src[m_pos] == '.')) m_pos++;
            return m_src.substring(start, m_pos).toDouble();
        }
        if (isalpha(c)) {
            int start = m_pos;
            while (m_pos < (int)m_src.length() && isalpha(m_src[m_pos])) m_pos++;
            String id = m_src.substring(start, m_pos);
            id.toLowerCase();
            if (id == "pi") return M_PI;
            if (id == "e")  return M_E;
            if (peek() == '(') {
                m_pos++;
                double arg = parseExpr();
                if (peek() == ')') m_pos++;
                else { m_err = "Missing )"; return 0; }
                if (id == "sqrt") return sqrt(arg);
                if (id == "sin")  return sin(arg * M_PI / 180.0);
                if (id == "cos")  return cos(arg * M_PI / 180.0);
                if (id == "tan")  return tan(arg * M_PI / 180.0);
                if (id == "log")  return log10(arg);
                if (id == "ln")   return log(arg);
                if (id == "abs")  return fabs(arg);
            }
            m_err = "Unknown " + id;
            return 0;
        }
        m_err = "Empty";
        return 0;
    }
};

class CalcApp : public App {
public:
    AppId id() const override { return APP_CALC; }
    const char* name() const override { return "Calc"; }

    void onInput(const InputEvent& ev) override {
        if (ev.code == CK_ESC || (ev.code == CK_CHAR && ev.ch == '`')) {
            switchApp(APP_HOME);
            return;
        }
        if (ev.code == CK_BACKSPACE) {
            if (!m_expr.isEmpty()) m_expr.remove(m_expr.length() - 1);
            return;
        }
        if (ev.code == CK_CHAR && (ev.ch == 'c' || ev.ch == 'C')) {
            // Only clear if not typing cos(
            if (m_expr.isEmpty() || ev.ch == 'C') {
                m_expr = "";
                m_result = "";
                return;
            }
        }
        if (ev.code == CK_ENTER || (ev.code == CK_CHAR && ev.ch == '=')) {
            evaluate();
            return;
        }
        if (ev.code == CK_UP && !m_history.empty()) {
            m_expr = m_history.back().first;
            return;
        }
        if (ev.code == CK_CHAR && ev.ch >= 32 && ev.ch <= 126) {
            if (m_expr.length() < 34) m_expr += ev.ch;
        }
    }

    void render(M5Canvas& spr) override {
        const auto& th = activeTheme();
        uint16_t acc = activeAccent();
        spr.fillScreen(th.bg);
        ui::drawStatusBar(spr, "Calculator");

        // History (up to 3 items)
        spr.setTextSize(1);
        int hy = 20;
        int startH = max(0, (int)m_history.size() - 3);
        for (int i = startH; i < (int)m_history.size(); ++i) {
            spr.setTextColor(th.textDim);
            spr.drawString(m_history[i].first + " = " + m_history[i].second, 8, hy);
            hy += 11;
        }

        // Expression Box
        spr.fillRoundRect(6, 56, SCREEN_W - 12, 46, 5, th.surface);
        spr.drawRoundRect(6, 56, SCREEN_W - 12, 46, 5, acc);

        spr.setTextColor(th.text);
        String dispE = m_expr + (((millis() / 450) % 2 == 0) ? "_" : "");
        if ((int)dispE.length() > 35) dispE = dispE.substring(dispE.length() - 35);
        spr.drawString(dispE, 12, 63);

        spr.setTextSize(2);
        spr.setTextColor(acc);
        spr.setTextDatum(TR_DATUM);
        String res = m_result.isEmpty() ? "0" : m_result;
        if ((int)res.length() > 16) res = res.substring(0, 16);
        spr.drawString(res, SCREEN_W - 12, 80);
        spr.setTextDatum(TL_DATUM);

        // Quick functions hint
        spr.setTextSize(1);
        spr.setTextColor(th.textDim);
        spr.drawString("+ - * / ^ ( ) sqrt sin cos log pi", 8, 108);
        spr.drawString("Enter: =   C: Clear   Up: Last", 8, 120);
    }

private:
    String m_expr;
    String m_result;
    std::vector<std::pair<String, String>> m_history;

    void evaluate() {
        if (m_expr.isEmpty()) return;
        ExprParser p;
        double val = 0;
        String err;
        if (p.eval(m_expr, val, err)) {
            char buf[32];
            if (fabs(val - round(val)) < 1e-9 && fabs(val) < 1e12) {
                snprintf(buf, sizeof(buf), "%.0f", round(val));
            } else {
                snprintf(buf, sizeof(buf), "%.6g", val);
            }
            m_result = buf;
            m_history.push_back({m_expr, m_result});
            if (m_history.size() > 12) m_history.erase(m_history.begin());
        } else {
            m_result = err;
        }
    }
};

} // namespace

App* createCalcApp() { return new CalcApp(); }

