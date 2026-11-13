// MotionGL - OpenGL animation library for classical mechanics labworks
// Copyright (C) 2026 Stanislav Furmavnin (GitHub: saintninja)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "motiongl/ui_common.hpp"
#include <algorithm>
#include <cmath>

namespace mgl
{

    std::string tickLabel(double v, double step) {
        const int d = decimalsFor(step);
        if (std::fabs(v) < step * 1e-6)
            v = 0.0; // не печатать "-0"
        return fmt("%.*f", d, v);
    }

    void drawBackground(Renderer2D &r, int w, int h, const Color &base) {
        const Color top{std::min(1.f, base.r + 0.045f), std::min(1.f, base.g + 0.055f),
                        std::min(1.f, base.b + 0.085f), base.a};
        const Color bot{base.r * 0.80f, base.g * 0.80f, base.b * 0.85f, base.a};
        r.gradientQuad({0, 0}, {double(w), 0}, {double(w), double(h)}, {0, double(h)}, bot, bot, top, top);
    }

    void drawHud(Renderer2D &r, int w, int h, const std::string &top, const std::string &bottom,
                 const Color &textColor, const Color &dimColor) {
        const Color panel{0.02f, 0.03f, 0.06f, 0.62f};
        const Color edge{1.f, 1.f, 1.f, 0.10f};
        r.rect({0, double(h) - kHudTop}, {double(w), double(h)}, panel);
        r.rect({0, double(h) - kHudTop - 1}, {double(w), double(h) - kHudTop}, edge);
        r.rect({0, 0}, {double(w), kHudBottom}, panel);
        r.rect({0, kHudBottom}, {double(w), kHudBottom + 1}, edge);

        auto fitHeight = [&](const std::string &s, float want) {
            const float avail = float(w) - 24.f;
            float hgt = want;
            while (hgt > 6.f && Renderer2D::textWidth(s, hgt) > avail)
                hgt -= 0.5f;
            return hgt;
        };
        const float ht = fitHeight(top, 14.f);
        r.text(top, {12, double(h) - 0.5 * (kHudTop - ht)}, textColor, ht);
        const float hb = fitHeight(bottom, 11.f);
        r.text(bottom, {12, 0.5 * (kHudBottom + hb)}, dimColor, hb);
    }

    bool drawOffscreenMarker(Renderer2D &r, Vec2 t, int w, int h, const Color &c) {
        if (!std::isfinite(t.x) || !std::isfinite(t.y))
            return false;
        const Rect screen{6.0, kHudBottom + 4.0, double(w) - 6.0, double(h) - kHudTop - 4.0};
        if (screen.contains(t))
            return false;
        const Vec2 C{0.5 * double(w), 0.5 * double(h)};
        const Vec2 d = t - C;
        const double dl = std::hypot(d.x, d.y);
        if (dl < 1e-9)
            return false;
        const Rect box{34.0, kHudBottom + 30.0, double(w) - 34.0, double(h) - kHudTop - 30.0};
        double tt = 1e30;
        if (d.x > 1e-9) tt = std::min(tt, (box.x1 - C.x) / d.x);
        if (d.x < -1e-9) tt = std::min(tt, (box.x0 - C.x) / d.x);
        if (d.y > 1e-9) tt = std::min(tt, (box.y1 - C.y) / d.y);
        if (d.y < -1e-9) tt = std::min(tt, (box.y0 - C.y) / d.y);
        const Vec2 M = C + d * tt;
        const Vec2 dir = d * (1.0 / dl);
        r.radialGlow(M, 30.f, c, 0.55f);
        r.arrow(M - dir * 26.0, M + dir * 4.0, c, 3.5f, 16.f);
        const std::string label = "BODY OFF SCREEN - F";
        const float tw = Renderer2D::textWidth(label, 10.f);
        Vec2 lp = M - dir * 34.0;
        lp.x -= dir.x * tw * 0.5 + tw * 0.5;
        lp.x = std::min(std::max(lp.x, 8.0), double(w) - tw - 8.0);
        lp.y = std::min(std::max(lp.y + 5.0, kHudBottom + 16.0), double(h) - kHudTop - 8.0);
        r.text(label, lp, c, 10.f);
        return true;
    }

    std::string viewStatus(const ViewControl &vc) {
        std::string s;
        if (!vc.isDefault())
            s += fmt("   ZOOM x%.2f", vc.zoom);
        if (vc.follow)
            s += "   FOLLOW";
        return s;
    }

    bool viewHandleKey(ViewControl &vc, int key, int action, Vec2 c) {
        if (action == GLFW_RELEASE)
            return false;
        constexpr double kStep = 1.25, kPan = 48.0;
        switch (key) {
        case GLFW_KEY_Z:
        case GLFW_KEY_PAGE_UP:
            vc.zoomAt(c, kStep);
            return true;
        case GLFW_KEY_X:
        case GLFW_KEY_PAGE_DOWN:
            vc.zoomAt(c, 1.0 / kStep);
            return true;
        case GLFW_KEY_LEFT:
            vc.panBy({kPan, 0});
            return true;
        case GLFW_KEY_RIGHT:
            vc.panBy({-kPan, 0});
            return true;
        case GLFW_KEY_UP:
            vc.panBy({0, -kPan});
            return true;
        case GLFW_KEY_DOWN:
            vc.panBy({0, kPan});
            return true;
        case GLFW_KEY_F:
        case GLFW_KEY_0:
        case GLFW_KEY_HOME:
            vc.reset();
            return true;
        case GLFW_KEY_C:
            if (action == GLFW_PRESS)
                vc.follow = !vc.follow;
            return true;
        default:
            return false;
        }
    }

    void viewAttachMouse(Window &win, ViewControl &vc) {
        ViewControl *v = &vc;
        win.onScroll([v](double dy, Vec2 cur) {
            if (dy != 0.0)
                v->zoomAt(cur, std::pow(1.15, dy));
        });
        win.onMouseButton([v, &win](int button, int action, Vec2 cur) {
            if (button != GLFW_MOUSE_BUTTON_LEFT)
                return;
            v->dragging = (action == GLFW_PRESS);
            v->lastCursor = cur;
            (void)win;
        });
        win.onCursorMove([v](Vec2 cur) {
            if (v->dragging) {
                v->panBy(cur - v->lastCursor);
                v->lastCursor = cur;
            }
        });
    }

} // namespace mgl
