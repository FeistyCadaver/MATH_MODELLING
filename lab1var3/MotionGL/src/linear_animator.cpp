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

#include "motiongl/linear_animator.hpp"
#include "motiongl/gl_loader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace mgl
{
    namespace
    {
        struct AxisFit {
            Vec2 O;          
            double ppm;     
            double xSupA;   
            double xSupB;   
        };

        AxisFit fitAxis(const Scheme1D &sc, int w, int h, double lo, double hi) {
            const Vec2 e = sc.dir();
            const double padFwd = sc.forceDir > 0 ? 90.0 : 40.0; 
            const double padBack = sc.forceDir < 0 ? 90.0 : 40.0;
            const Rect R{40.0, kHudBottom + 30.0, double(w) - 40.0, double(h) - kHudTop - 30.0};

            double sA = -100.0, sB = sc.lengthPx;
            if (!chordRange(sc.origin, e, R, sA, sB)) {
                sA = -100.0;
                sB = sc.lengthPx;
            }
            const double S0 = sA + padBack, S1 = sB - padFwd; 
            const double range = hi - lo;

            double ppm = std::numeric_limits<double>::infinity();
            if (hi > 1e-12)
                ppm = (sc.lengthPx - 90.0) / hi;
            if (range > 1e-12 && S1 > S0)
                ppm = std::min(ppm, (S1 - S0) / range);
            if (!std::isfinite(ppm) || !(ppm > 0.0))
                ppm = 50.0;

            const double shLo = S0 - lo * ppm, shHi = S1 - hi * ppm;
            const double shift = shLo <= shHi ? std::min(std::max(0.0, shLo), shHi) : 0.5 * (shLo + shHi);

            AxisFit f;
            f.O = sc.origin + e * shift;
            f.ppm = ppm;
            f.xSupA = lo < 0.0 ? lo - 60.0 / ppm : 0.0;
            f.xSupB = hi + 90.0 / ppm;
            return f;
        }
    } // namespace

    LinearAnimator LinearAnimatorBuilder::build() const { return LinearAnimator(cfg_); }

    LinearAnimator::LinearAnimator(LinearConfig cfg)
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

    LinearAnimator::~LinearAnimator() = default;

    void LinearAnimator::run(std::function<double(double)> xOfT, double duration) {
        run(AnalyticLaw1D(std::move(xOfT)), duration);
    }

    void LinearAnimator::run(std::vector<std::pair<double, double>> samples) {
        SampledLaw1D law(std::move(samples));
        run(law, law.durationHint());
    }

    void LinearAnimator::handleKey(int key, int action) {
        const auto [w, h] = window_->framebufferSize();
        if (viewHandleKey(vc_, key, action, {0.5 * w, 0.5 * h}))
            return;
        if (action == GLFW_RELEASE)
            return;
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

    void LinearAnimator::analyze(const IScalarLaw1D &law, double duration) {
        lo_ = 0.0;
        hi_ = 0.0;
        const int N = 2048;
        for (int i = 0; i <= N; ++i) {
            const double x = law.position(duration * i / N);
            if (!std::isfinite(x))
                continue;
            lo_ = std::min(lo_, x);
            hi_ = std::max(hi_, x);
        }
        if (hi_ - lo_ < 1e-12)
            hi_ = lo_ + 1.0; 
    }

    void LinearAnimator::run(const IScalarLaw1D &law, double duration) {
        window_->onKey([this](int key, int action)
                       { handleKey(key, action); });
        viewAttachMouse(*window_, vc_);
        analyze(law, duration);
        simTime_ = 0.0;

        double last = glfwGetTime();
        while (!window_->shouldClose()) {
            window_->pollEvents();
            const double now = glfwGetTime();
            const double dt = std::min(now - last, 0.1);
            last = now;
            if (!paused_) {
                simTime_ += dt * timeScale_;
                if (simTime_ >= duration) {
                    if (cfg_.loop)
                        simTime_ = 0.0;
                    else
                        simTime_ = duration;
                }
            }
            render(law, duration);
        }
    }

    void LinearAnimator::render(const IScalarLaw1D &law, double duration) {
        auto [w, h] = window_->framebufferSize();
        if (w <= 0 || h <= 0)
            return;
        glViewport(0, 0, w, h);
        glClearColor(cfg_.background.r, cfg_.background.g, cfg_.background.b, cfg_.background.a);
        glClear(GL_COLOR_BUFFER_BIT);

        Renderer2D &r = *renderer_;
        r.begin(w, h);
        drawBackground(r, w, h, cfg_.background);

        scheme_ = Scheme1D::make(cfg_.variant, w, h);
        const AxisFit fit = fitAxis(scheme_, w, h, lo_, hi_);

        const Vec2 e = scheme_.dir();
        const Vec2 perp{-e.y, e.x};
        const Vec2 down = perp.y < 0 ? perp : Vec2{-perp.x, -perp.y};
        const Vec2 up{-down.x, -down.y};

        const double x = law.position(simTime_);
        const bool xOk = std::isfinite(x);
        if (vc_.follow && xOk)
            vc_.centerOn(fit.O + e * (x * fit.ppm), {0.5 * w, 0.5 * h});
        const Vec2 O = vc_.map(fit.O);         
        const double ppm = fit.ppm * vc_.zoom;  
        const Color &sc = cfg_.supportColor;
        const bool plane = scheme_.support == SupportKind::Plane;

        double vA = 0.0, vB = 0.0;
        if (!chordRange(O, e, Rect{-60.0, -60.0, double(w) + 60.0, double(h) + 60.0}, vA, vB))
            vA = vB = 0.0;
        const double supA = fit.xSupA * ppm, supB = fit.xSupB * ppm;
        const double dA = std::max(vA, supA), dB = std::min(vB, supB);

        if (dA < dB) {
            if (plane) {
                r.line(O + e * dA, O + e * dB, sc, 2.5f);
                const long long k0 = static_cast<long long>(std::ceil((dA - 8.0) / 14.0));
                for (long long k = k0; 14.0 * k + 8.0 < dB; ++k) {
                    const Vec2 p = O + e * (14.0 * k + 8.0);
                    r.line(p, p + down * 10 - e * 7, sc, 1.5f);
                }
            } else {
                const double a = std::max(dA, 0.0);
                if (a < dB) {
                    r.line(O + e * a + perp * 3, O + e * dB + perp * 3, sc, 1.5f);
                    r.line(O + e * a - perp * 3, O + e * dB - perp * 3, sc, 1.5f);
                }
                const Vec2 q = O - e * 6;
                r.line(q - perp * 26, q + perp * 26, sc, 2.5f);
                for (double s = -24; s <= 24; s += 8)
                    r.line(q + perp * s, q + perp * s - e * 9, sc, 1.5f);
            }
        }

        if (supB > 0.0) {
            if (supB <= vB)
                r.arrow(O, O + e * supB, sc, 2.f, 12.f);
            else
                r.line(O, O + e * std::max(vB, 0.0), sc, 2.f);
        }
        const double step = niceStep(1.0, 64.0 / ppm);
        long long k0 = static_cast<long long>(std::ceil(dA / ppm / step));
        long long k1 = static_cast<long long>(std::floor(dB / ppm / step));
        if (k1 - k0 > 400)
            k1 = k0 + 400;
        const double labelOff = plane ? 26.0 : 30.0;
        for (long long k = k0; k <= k1; ++k) {
            const double m = k * step;
            const Vec2 p = O + e * (m * ppm);
            r.line(p - perp * 4, p + perp * 4, sc, 1.5f);
            if (k == 0)
                continue;
            const std::string s = tickLabel(m, step);
            const Vec2 c = p + down * labelOff;
            const float tw = Renderer2D::textWidth(s, 10.f);
            if (c.x - 0.5 * tw < 4.0 || c.x + 0.5 * tw > double(w) - 4.0 ||
                c.y < kHudBottom + 6.0 || c.y > double(h) - kHudTop - 6.0)
                continue; 
            r.text(s, {c.x - 0.5 * tw, c.y + 5.0}, cfg_.textColor, 10.f);
        }
        if (supB <= vB)
            r.text("X", O + e * supB + e * 14 + up * 16, cfg_.textColor, 14);
        if (Rect{0, kHudBottom, double(w), double(h) - kHudTop}.contains(O, 10.0))
            r.text("O", O - e * 10 - up * 4 + Vec2{-14, -18}, cfg_.textColor, 14);

        if (scheme_.angleText) {
            const Vec2 A = O + e * (scheme_.angleAnchor == 0 ? supA : supB);
            if (Rect{0, kHudBottom, double(w), double(h) - kHudTop}.contains(A, 4.0)) {
                const double hd = double(scheme_.angleHDir);
                for (double s = 0; s < 90; s += 12)
                    r.line(A + Vec2{hd * s, 0}, A + Vec2{hd * std::min(s + 7, 90.0), 0}, sc, 1.5f);
                r.text(std::string(scheme_.angleText) + "^", A + Vec2{hd * 52 - 6, 16}, cfg_.textColor, 12);
            }
        }

        if (xOk) {
            const Vec2 p = O + e * (x * ppm);
            const Vec2 c = plane ? p + up * 13 : p;
            const double a = 23, b = 13;
            const Vec2 sh = down * 4.0;
            r.filledQuad(c + sh + e * a + perp * b, c + sh + e * a - perp * b,
                         c + sh - e * a - perp * b, c + sh - e * a + perp * b, Color{0, 0, 0, 0.28f});
            const Color &bc = cfg_.bodyColor;
            const Color light{std::min(1.f, bc.r + 0.15f), std::min(1.f, bc.g + 0.22f), std::min(1.f, bc.b + 0.25f), bc.a};
            const Color dark{bc.r * 0.78f, bc.g * 0.72f, bc.b * 0.7f, bc.a};
            r.gradientQuad(c + e * a - up * b, c + e * a + up * b, c - e * a + up * b, c - e * a - up * b,
                           dark, light, light, dark);
            const Color &tc = cfg_.textColor;
            r.line(c + e * a + perp * b, c + e * a - perp * b, tc, 1.5f);
            r.line(c + e * a - perp * b, c - e * a - perp * b, tc, 1.5f);
            r.line(c - e * a - perp * b, c - e * a + perp * b, tc, 1.5f);
            r.line(c - e * a + perp * b, c + e * a + perp * b, tc, 1.5f);
            r.text("M", c + up * 40, tc, 13);

            const double fd = double(scheme_.forceDir);
            const Vec2 fs = c + e * fd * (a + 3);
            r.radialGlow(fs + e * fd * 18, 26.f, cfg_.forceColor, 0.22f);
            r.arrow(fs, fs + e * fd * 36, cfg_.forceColor, 2.5f, 10.f);
            r.text("F", fs + e * fd * 46 + up * 12, cfg_.forceColor, 13);

            drawOffscreenMarker(r, c, w, h, cfg_.bodyColor);
        }

        drawHud(r, w, h,
                fmt("T = %.2f / %.1f S   X = %.3f M   V = %.3f M/S   TIME x%.2f%s%s",
                    simTime_, duration, x, law.velocity(simTime_), timeScale_,
                    paused_ ? "   PAUSE" : "", viewStatus(vc_).c_str()),
                "SPACE - PAUSE   R - RESTART   + / - - TIME   WHEEL OR Z X - ZOOM   DRAG OR ARROWS - PAN   F - FIT   C - FOLLOW   ESC - QUIT",
                cfg_.textColor, cfg_.supportColor);

        r.end();
        window_->swapBuffers();
    }

} // namespace mgl
