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

#include "motiongl/tube_animator.hpp"
#include "motiongl/gl_loader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace mgl
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        Vec2 dirFromDeg(double a) { return {std::cos(a * kPi / 180), std::sin(a * kPi / 180)}; }
        Vec2 unit(Vec2 d) {
            const double l = std::hypot(d.x, d.y);
            return l > 1e-12 ? d * (1.0 / l) : Vec2{1, 0};
        }
        double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
        constexpr double kHalfW = 11.0; 
    } // namespace

    TubeAnimator TubeAnimatorBuilder::build() const { return TubeAnimator(cfg_); }

    TubeAnimator::TubeAnimator(TubeConfig cfg)
        : cfg_(std::move(cfg)), timeScale_(cfg_.timeScale)
    {
        Window::Config wc;
        wc.width = cfg_.width;
        wc.height = cfg_.height;
        wc.title = cfg_.title;
        window_ = std::make_unique<Window>(wc);
        if (!gl::load())
            throw std::runtime_error("failed to load OpenGL functions");
        renderer_ = std::make_unique<Renderer2D>();
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
    }

    TubeAnimator::~TubeAnimator() = default;

    double TubeAnimator::findTransition(const IScalarLaw1D &s1) const {
        const double l = cfg_.l;
        const double hint = s1.durationHint();
        const double T = hint > 0 ? hint : cfg_.scanT1Max;
        const int N = 2000;
        for (int i = 1; i <= N; ++i) {
            double t = T * i / N;
            if (s1.position(t) >= l) {
                double lo = t - T / N, hi = t;
                for (int k = 0; k < 60; ++k) {
                    double mid = 0.5 * (lo + hi);
                    if (s1.position(mid) >= l)
                        hi = mid;
                    else
                        lo = mid;
                }
                return hi;
            }
        }
        return T;
    }

    void TubeAnimator::handleKey(int key, int action) {
        const auto [w, h] = window_->framebufferSize();
        if (viewHandleKey(vc_, key, action, {0.5 * w, 0.5 * h}))
            return;
        if (action == GLFW_RELEASE) return;
        switch (key) {
            case GLFW_KEY_SPACE:
                paused_ = !paused_;
                break;
            case GLFW_KEY_R:
                simTime_ = 0.0;
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
                timeScale_ = std::min(timeScale_ * 1.25, 16.0);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
                timeScale_ = std::max(timeScale_ / 1.25, 0.1);
                break;
            case GLFW_KEY_ESCAPE:
                window_->close();
                break;
            default:
                break;
        }
    }

    void TubeAnimator::run(const ScalarInput &in1, const ScalarInput &in2, double t2) {
        auto s1 = makeLaw(in1);
        auto s2 = makeLaw(in2);

        window_->onKey([this](int k, int a) { handleKey(k, a); });
        viewAttachMouse(*window_, vc_);

        const TubeScheme sch = TubeScheme::make(cfg_.variant);
        e1_ = dirFromDeg(sch.abDeg);
        e2_ = dirFromDeg(sch.bcDeg);

        tB_ = findTransition(*s1);
        const double hint2 = s2->durationHint();
        dur2_ = t2 > 0 ? t2 : (hint2 > 0 ? hint2 : 20.0);

        double hi2 = 0.0, lo2 = 0.0;
        for (int i = 0; i <= 2048; ++i) {
            const double x = s2->position(dur2_ * i / 2048);
            if (!std::isfinite(x)) continue;
            hi2 = std::max(hi2, x);
            lo2 = std::min(lo2, x);
        }
        lenBC_ = std::max(1.08 * hi2, 0.3 * cfg_.l);
        const double axisLen = lenBC_ * 1.12;

        bounds_ = Bounds{};
        bounds_.add(-e1_.x * cfg_.l, -e1_.y * cfg_.l);  // A
        bounds_.add(0.0, 0.0);                          // B
        bounds_.add(e2_.x * lenBC_, e2_.y * lenBC_);    // C
        bounds_.add(e2_.x * axisLen, e2_.y * axisLen);  
        bounds_.add(e2_.x * lo2, e2_.y * lo2);          

        simTime_ = 0.0;
        double last = glfwGetTime();
        const double total = tB_ + dur2_;
        while (!window_->shouldClose()) {
            window_->pollEvents();
            const double now = glfwGetTime();
            const double dt = std::min(now - last, 0.1);
            last = now;
            if (!paused_) {
                simTime_ += dt * timeScale_;
                if (simTime_ >= total)
                    simTime_ = cfg_.loop ? 0.0 : total;
            }
            render(*s1, *s2);
        }
    }

    void TubeAnimator::render(const IScalarLaw1D &s1, const IScalarLaw1D &s2) {
        auto [w, h] = window_->framebufferSize();
        if (w <= 0 || h <= 0) return;
        glViewport(0, 0, w, h);
        glClearColor(cfg_.background.r, cfg_.background.g, cfg_.background.b, cfg_.background.a);
        glClear(GL_COLOR_BUFFER_BIT);

        Renderer2D &r = *renderer_;
        r.begin(w, h);
        drawBackground(r, w, h, cfg_.background);
        const Color &wc = cfg_.wallColor;

        const double t = simTime_;
        Vec2 e = e1_;
        double coord = 0.0, vel = 0.0;
        Vec2 posM;
        const bool onFirst = t < tB_;
        if (onFirst) {
            coord = std::min(std::max(s1.position(t), 0.0), cfg_.l);
            vel = s1.velocity(t);
            posM = {-e1_.x * (cfg_.l - coord), -e1_.y * (cfg_.l - coord)};
        } else {
            coord = s2.position(t - tB_);
            vel = s2.velocity(t - tB_);
            posM = {e2_.x * coord, e2_.y * coord};
            e = e2_;
        }
        const bool posOk = std::isfinite(posM.x) && std::isfinite(posM.y);

        const Rect area{80.0, kHudBottom + 46.0, double(w) - 80.0, double(h) - kHudTop - 46.0};
        const View2D base = View2D::fit(bounds_, area, 50.0, /*center=*/true);
        if (vc_.follow && posOk)
            vc_.centerOn(base.toScreen(posM), {0.5 * w, 0.5 * h});
        const View2D view = vc_.apply(base);

        const Vec2 Apx = view.toScreen({-e1_.x * cfg_.l, -e1_.y * cfg_.l});
        const Vec2 Bpx = view.toScreen({0, 0});
        const Vec2 Cpx = view.toScreen({e2_.x * lenBC_, e2_.y * lenBC_});

        {
            const Vec2 d1 = unit(Bpx - Apx), d2 = unit(Cpx - Bpx);
            const Vec2 n1{-d1.y, d1.x}, n2{-d2.y, d2.x};
            Vec2 m = (n1 + n2) * (1.0 / std::max(1.0 + dot(n1, n2), 0.25));
            const double ml = std::hypot(m.x, m.y);
            if (ml > 2.0) m = m * (2.0 / ml);
            const double hw = kHalfW;
            const Vec2 AL = Apx + n1 * hw, AR = Apx - n1 * hw;
            const Vec2 BL = Bpx + m * hw, BR = Bpx - m * hw;
            const Vec2 CL = Cpx + n2 * hw, CR = Cpx - n2 * hw;
            const Color fill{0.14f, 0.18f, 0.28f, 0.95f};
            r.filledQuad(AL, BL, BR, AR, fill);
            r.filledQuad(BL, CL, CR, BR, fill);
            r.line(AL, BL, wc, 2.5f);
            r.line(BL, CL, wc, 2.5f);
            r.line(AR, BR, wc, 2.5f);
            r.line(BR, CR, wc, 2.5f);
            r.line(AL, AR, wc, 2.5f); 

            auto hatch = [&](Vec2 P, Vec2 Q, double sStart, double sEnd) {
                const Vec2 d = unit(Q - P);
                Vec2 nd{-d.y, d.x};
                if (nd.y > 0) nd = nd * -1.0; 
                const double len = std::hypot(Q.x - P.x, Q.y - P.y);
                for (double s = sStart; s < std::min(sEnd, len); s += 14.0) {
                    const Vec2 p = P + d * s + nd * hw;
                    r.line(p, p + nd * 10 - d * 7, wc, 1.5f);
                }
            };
            hatch(Apx, Bpx, 6.0, std::hypot(Bpx.x - Apx.x, Bpx.y - Apx.y) - 12.0);
            hatch(Bpx, Cpx, 12.0, 1e9);
        }

        const double axisLen = lenBC_ * 1.12;
        const Vec2 axisTip = view.toScreen({e2_.x * axisLen, e2_.y * axisLen});
        r.arrow(Bpx, axisTip, cfg_.axisColor, 1.5f, 10.f);
        r.text("X", axisTip + e2_ * 12 + Vec2{0, 14}, cfg_.textColor, 14);
        {
            Vec2 upN{-e2_.y, e2_.x};
            if (upN.y < 0) upN = upN * -1.0;
            const double step = niceStep(1.0, 64.0 / view.scale);
            const long long kMax = std::min<long long>(static_cast<long long>(std::floor(axisLen / step)), 400);
            for (long long k = 1; k <= kMax; ++k) {
                const Vec2 p = view.toScreen({e2_.x * k * step, e2_.y * k * step});
                r.line(p - upN * 4, p + upN * 4, cfg_.axisColor, 1.5f);
                const std::string s = tickLabel(k * step, step);
                const Vec2 c = p + upN * (kHalfW + 14.0);
                const float tw = Renderer2D::textWidth(s, 10.f);
                if (c.x - 0.5 * tw < 4.0 || c.x + 0.5 * tw > double(w) - 4.0 ||
                    c.y < kHudBottom + 6.0 || c.y > double(h) - kHudTop - 6.0)
                    continue;
                r.text(s, {c.x - 0.5 * tw, c.y + 5.0}, cfg_.axisColor, 10.f);
            }
        }

        r.text("A", Apx + Vec2{-6, 20}, cfg_.textColor, 14);
        r.text("B", Bpx + Vec2{-6, -26}, cfg_.textColor, 14);
        r.text("C", Cpx + Vec2{-6, 20}, cfg_.textColor, 14);

        auto drawAngle = [&](Vec2 P, double hdir) {
            for (double x = 0; x < 80; x += 12)
                r.line(P + Vec2{hdir * x, 0}, P + Vec2{hdir * std::min(x + 7, 80.0), 0}, wc, 1.5f);
            r.text("30^", P + Vec2{hdir * 36 - 9, 15}, cfg_.textColor, 12);
        };
        if (std::fabs(e1_.y) > 0.01) {
            const bool aLower = Apx.y < Bpx.y;
            const Vec2 P = aLower ? Apx : Bpx;
            const double hdir = aLower ? (Bpx.x > Apx.x ? 1 : -1) : (Apx.x > Bpx.x ? 1 : -1);
            drawAngle(P, hdir);
        }
        if (std::fabs(e2_.y) > 0.01) {
            const bool cLower = Cpx.y < Bpx.y;
            const Vec2 P = cLower ? Cpx : Bpx;
            const double hdir = cLower ? (Bpx.x > Cpx.x ? 1 : -1) : (Cpx.x > Bpx.x ? 1 : -1);
            drawAngle(P, hdir);
        }

        if (posOk) {
            const Vec2 p = view.toScreen(posM);
            const Vec2 perp{-e.y, e.x};
            const double a = 15, b = 7;
            r.radialGlow(p, 30.f, cfg_.bodyColor, 0.35f);
            const Color &bc = cfg_.bodyColor;
            const Color light{std::min(1.f, bc.r + 0.15f), std::min(1.f, bc.g + 0.22f), std::min(1.f, bc.b + 0.25f), bc.a};
            const Color dark{bc.r * 0.78f, bc.g * 0.72f, bc.b * 0.7f, bc.a};
            Vec2 upN = perp.y < 0 ? perp * -1.0 : perp;
            r.gradientQuad(p + e * a - upN * b, p + e * a + upN * b, p - e * a + upN * b, p - e * a - upN * b,
                           dark, light, light, dark);
            r.line(p + e * a + perp * b, p + e * a - perp * b, cfg_.textColor, 1.5f);
            r.line(p + e * a - perp * b, p - e * a - perp * b, cfg_.textColor, 1.5f);
            r.line(p - e * a - perp * b, p - e * a + perp * b, cfg_.textColor, 1.5f);
            r.line(p - e * a + perp * b, p + e * a + perp * b, cfg_.textColor, 1.5f);

            if (std::fabs(vel) > 1e-3) {
                const Vec2 vd = e * (vel > 0 ? 1 : -1);
                const double L = std::min(46.0, 8.0 + 10.0 * std::fabs(vel));
                r.arrow(p + vd * (a + 2), p + vd * (a + 2 + L), cfg_.forceColor, 2.f, 9.f);
            }
            drawOffscreenMarker(r, p, w, h, cfg_.bodyColor);
        }

        drawHud(r, w, h,
                fmt("T = %.2f S (T_B = %.2f)   SEG = %s   %s = %.3f M   V = %.3f M/S   TIME x%.2f%s%s",
                    t, tB_, onFirst ? "AB" : "BC", onFirst ? "S" : "X", coord, vel,
                    timeScale_, paused_ ? "   PAUSE" : "", viewStatus(vc_).c_str()),
                "SPACE - PAUSE   R - RESTART   + / - - TIME   WHEEL OR Z X - ZOOM   DRAG OR ARROWS - PAN   F - FIT   C - FOLLOW   ESC - QUIT",
                cfg_.textColor, cfg_.wallColor);

        r.end();
        window_->swapBuffers();
    }

} // namespace mgl
