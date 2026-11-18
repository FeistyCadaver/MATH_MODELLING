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

#include "motiongl/animator.hpp"
#include "motiongl/gl_loader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace mgl
{
    namespace
    {
        constexpr double kMarginRight = 70.0, kMarginTop = 70.0;
    } // namespace

    Animator AnimatorBuilder::build() const { return Animator(cfg_); }

    Animator::Animator(AnimatorConfig cfg)
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

    Animator::~Animator() = default;

    void Animator::restart() {
        simTime_ = 0.0;
        trail_.clear();
    }

    // Один раз обходим всю траекторию: границы (с учётом отрицательных значений и
    // с пропуском NaN/inf), максимальная скорость и контур для превью.
    void Animator::analyze(const IMotionLaw &law, double duration) {
        bounds_ = Bounds{};
        bounds_.add(0.0, 0.0); // оси всегда видны
        maxSpeed_ = 0.0;
        path_.clear();
        const int N = 1024;
        for (int i = 0; i <= N; ++i) {
            const double t = duration * i / N;
            const Vec2 p = law.position(t);
            bounds_.add(p);
            path_.push_back(p);
            const Vec2 v = law.velocity(t);
            const double sp = std::hypot(v.x, v.y);
            if (std::isfinite(sp))
                maxSpeed_ = std::max(maxSpeed_, sp);
        }
    }

    void Animator::handleKey(int key, int action) {
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
            restart();
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

    void Animator::run(const IMotionLaw &law, double duration) {
        window_->onKey([this](int key, int action)
                       { handleKey(key, action); });
        viewAttachMouse(*window_, vc_);
        analyze(law, duration);
        restart();

        double last = glfwGetTime();
        while (!window_->shouldClose()) {
            window_->pollEvents();
            const double now = glfwGetTime();
            const double dt = std::min(now - last, 0.1); // долгий ресайз/пауза не должны «перематывать» время
            last = now;

            if (!paused_) {
                simTime_ += dt * timeScale_;
                if (simTime_ >= duration) {
                    if (cfg_.loop)
                        restart();
                    else
                        simTime_ = duration;
                }
            }
            while (static_cast<double>(trail_.size()) * trailDt_ < simTime_)
                trail_.push_back(law.position(static_cast<double>(trail_.size()) * trailDt_));

            render(law, duration);
        }
    }

    void Animator::render(const IMotionLaw &law, double duration) {
        auto [w, h] = window_->framebufferSize();
        if (w <= 0 || h <= 0)
            return;
        glViewport(0, 0, w, h);
        glClearColor(cfg_.background.r, cfg_.background.g, cfg_.background.b, cfg_.background.a);
        glClear(GL_COLOR_BUFFER_BIT);

        Renderer2D &r = *renderer_;
        r.begin(w, h);
        drawBackground(r, w, h, cfg_.background);

        // ---- вид: автоподбор по границам траектории (каждый кадр, поэтому ресайз безопасен) ----
        Rect area{cfg_.originPx.x, cfg_.originPx.y, double(w) - kMarginRight, double(h) - kMarginTop};
        area.x1 = std::max(area.x1, area.x0 + 50.0);
        area.y1 = std::max(area.y1, area.y0 + 50.0);
        const View2D base = cfg_.autoFit ? View2D::fit(bounds_, area, cfg_.pixelsPerMeter)
                                         : View2D{cfg_.pixelsPerMeter, cfg_.originPx};

        const Vec2 p = law.position(simTime_);
        const bool pOk = std::isfinite(p.x) && std::isfinite(p.y);
        if (vc_.follow && pOk)
            vc_.centerOn(base.toScreen(p), {0.5 * w, 0.5 * h});
        const View2D view = vc_.apply(base);

        const Vec2 O = view.toScreen({0.0, 0.0});
        const double axisY = std::min(std::max(O.y, cfg_.originPx.y), double(h) - kMarginTop);
        const double axisX = std::min(std::max(O.x, cfg_.originPx.x), double(w) - kMarginRight);
        const double yLo = kHudBottom, yHi = double(h) - kHudTop;

        // ---- деления и сетка ----
        const double step = niceStep(cfg_.tickStepM > 0.0 ? cfg_.tickStepM : 1.0, 64.0 / view.scale);
        const Vec2 wMin = view.toWorld({0.0, yLo}), wMax = view.toWorld({double(w), yHi});
        auto ticks = [&](double a, double b, auto &&fn) {
            long long k0 = static_cast<long long>(std::ceil(a / step));
            long long k1 = static_cast<long long>(std::floor(b / step));
            if (k1 - k0 > 400)
                k1 = k0 + 400;
            for (long long k = k0; k <= k1; ++k)
                fn(k * step, k);
        };
        ticks(wMin.x, wMax.x, [&](double m, long long k) {
            const double px = view.toScreen({m, 0.0}).x;
            if (cfg_.showGrid)
                r.line({px, yLo}, {px, yHi}, cfg_.gridColor, 1.f);
            if (k == 0)
                return;
            r.line({px, axisY - 4}, {px, axisY + 4}, cfg_.axisColor, 1.5f);
            const std::string s = tickLabel(m, step);
            const float tw = Renderer2D::textWidth(s, 11.f);
            if (px - 0.5 * tw < 4.0 || px + 0.5 * tw > double(w) - 4.0)
                return; // подпись не помещается в окно
            r.text(s, {px - 0.5 * tw, axisY - 10}, cfg_.axisColor, 11.f);
        });
        ticks(wMin.y, wMax.y, [&](double m, long long k) {
            const double py = view.toScreen({0.0, m}).y;
            if (cfg_.showGrid)
                r.line({0, py}, {double(w), py}, cfg_.gridColor, 1.f);
            if (k == 0)
                return;
            r.line({axisX - 4, py}, {axisX + 4, py}, cfg_.axisColor, 1.5f);
            const std::string s = tickLabel(m, step);
            if (py < yLo + 8.0 || py > yHi - 8.0)
                return;
            r.text(s, {axisX - 10 - Renderer2D::textWidth(s, 11.f), py + 4}, cfg_.axisColor, 11.f);
        });

        // ---- оси (на всю ширину окна; при зуме прилипают к краю, подписи остаются верными) ----
        r.arrow({24, axisY}, {double(w) - 24, axisY}, cfg_.axisColor, 2.f, 12.f);
        r.arrow({axisX, yLo + 6}, {axisX, yHi - 6}, cfg_.axisColor, 2.f, 12.f);
        r.text("X", {double(w) - 20, axisY + 22}, cfg_.textColor, 14);
        r.text("Y", {axisX + 10, yHi - 12}, cfg_.textColor, 14);
        if (Rect{0, yLo, double(w), yHi}.contains(O, 20.0))
            r.text("O", {O.x - 22, O.y + 16}, cfg_.textColor, 14);

        // ---- траектория: бледное превью всего пути + яркий след пройденного ----
        if (cfg_.showPath) {
            std::vector<Vec2> pts;
            pts.reserve(path_.size());
            for (const Vec2 &q : path_)
                pts.push_back(view.toScreen(q));
            Color faint = cfg_.trailColor;
            faint.a = 0.20f;
            r.polyline(pts, faint, faint, 1.5f);
        }
        {
            std::vector<Vec2> pts;
            pts.reserve(trail_.size());
            for (const Vec2 &q : trail_)
                pts.push_back(view.toScreen(q));
            Color tail = cfg_.trailColor, head = cfg_.trailColor;
            tail.a = 0.15f;
            head.a = 1.0f;
            r.polyline(pts, tail, head, 2.5f);
        }

        // ---- масштаб стрелок скорости: самая длинная ~ 16% окна, а не фиксированные px/(м/с) ----
        double velPx = cfg_.velocityScalePx;
        if (cfg_.autoFit && maxSpeed_ > 1e-12)
            velPx = 0.16 * std::min(area.width(), area.height()) / maxSpeed_;
        const Rect arrowBox{6.0, yLo + 4.0, double(w) - 6.0, yHi - 4.0};
        auto drawVel = [&](Vec2 from, Vec2 v) {
            if (!arrowBox.contains(from) || !std::isfinite(v.x) || !std::isfinite(v.y))
                return;
            const Vec2 tip = clipTipToRect(from, from + v * velPx, arrowBox);
            r.arrow(from, tip, cfg_.velocityColor, 2.f, 10.f);
        };
        drawVel(O, law.velocity(0.0));

        // ---- точка ----
        Vec2 v{};
        if (pOk) {
            const Vec2 pp = view.toScreen(p);
            r.radialGlow(pp, 26.f, cfg_.pointColor, 0.50f);
            r.filledCircle(pp, 6.5f, cfg_.pointColor);
            r.ring(pp, 6.5f, 1.5f, colors::white);
            v = law.velocity(simTime_);
            drawVel(pp, v);
            drawOffscreenMarker(r, pp, w, h, cfg_.pointColor);
        }

        const double speed = std::hypot(v.x, v.y);
        drawHud(r, w, h,
                fmt("T = %.2f / %.2f S   X = %.2f M   Y = %.2f M   V = %.2f M/S   TIME x%.2f%s%s",
                    simTime_, duration, p.x, p.y, speed, timeScale_, paused_ ? "   PAUSE" : "",
                    viewStatus(vc_).c_str()),
                "SPACE - PAUSE   R - RESTART   + / - - TIME   WHEEL OR Z X - ZOOM   DRAG OR ARROWS - PAN   F - FIT   C - FOLLOW   ESC - QUIT",
                cfg_.textColor, cfg_.axisColor);

        r.end();
        window_->swapBuffers();
    }

} // namespace mgl
