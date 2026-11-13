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

#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include "vec.hpp"

namespace mgl
{

    struct Rect {
        double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        double width() const { return x1 - x0; }
        double height() const { return y1 - y0; }
        bool contains(Vec2 p, double pad = 0.0) const {
            return p.x >= x0 + pad && p.x <= x1 - pad && p.y >= y0 + pad && p.y <= y1 - pad;
        }
    };

    struct Bounds {
        double minX = std::numeric_limits<double>::infinity();
        double minY = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();

        void add(double x, double y) {
            if (!std::isfinite(x) || !std::isfinite(y))
                return;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
        void add(Vec2 p) { add(p.x, p.y); }
        bool valid() const { return minX <= maxX && minY <= maxY; }
        double spanX() const { return valid() ? maxX - minX : 0.0; }
        double spanY() const { return valid() ? maxY - minY : 0.0; }
    };

    struct View2D {
        double scale = 1.0;
        Vec2 off{0.0, 0.0};

        Vec2 toScreen(Vec2 w) const { return {off.x + w.x * scale, off.y + w.y * scale}; }
        Vec2 toWorld(Vec2 s) const { return {(s.x - off.x) / scale, (s.y - off.y) / scale}; }

        static View2D fit(Bounds b, const Rect &area, double fallbackScale, bool center = false) {
            if (!b.valid())
                b.add(0.0, 0.0);
            const double aw = std::max(area.width(), 1.0);
            const double ah = std::max(area.height(), 1.0);
            const double sx = b.spanX() > 1e-12 ? aw / b.spanX() : std::numeric_limits<double>::infinity();
            const double sy = b.spanY() > 1e-12 ? ah / b.spanY() : std::numeric_limits<double>::infinity();
            double s = std::min(sx, sy);
            if (!std::isfinite(s) || !(s > 0.0))
                s = fallbackScale > 0.0 ? fallbackScale : 10.0;
            View2D v;
            v.scale = s;
            v.off = {area.x0 - b.minX * s, area.y0 - b.minY * s};
            if (center) {
                v.off.x += 0.5 * (aw - b.spanX() * s);
                v.off.y += 0.5 * (ah - b.spanY() * s);
            }
            return v;
        }
    };

    inline bool chordRange(Vec2 O, Vec2 e, const Rect &r, double &sA, double &sB) {
        sA = -std::numeric_limits<double>::infinity();
        sB = std::numeric_limits<double>::infinity();
        const double o[2] = {O.x, O.y}, d[2] = {e.x, e.y};
        const double lo[2] = {r.x0, r.y0}, hi[2] = {r.x1, r.y1};
        for (int i = 0; i < 2; ++i) {
            if (std::fabs(d[i]) < 1e-12) {
                if (o[i] < lo[i] || o[i] > hi[i])
                    return false;
                continue;
            }
            double t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
            if (t1 > t2)
                std::swap(t1, t2);
            sA = std::max(sA, t1);
            sB = std::min(sB, t2);
        }
        return sA <= sB;
    }

    inline Vec2 clipTipToRect(Vec2 a, Vec2 b, const Rect &r) {
        double t = 1.0;
        const Vec2 d = b - a;
        if (d.x > 1e-12) t = std::min(t, (r.x1 - a.x) / d.x);
        if (d.x < -1e-12) t = std::min(t, (r.x0 - a.x) / d.x);
        if (d.y > 1e-12) t = std::min(t, (r.y1 - a.y) / d.y);
        if (d.y < -1e-12) t = std::min(t, (r.y0 - a.y) / d.y);
        return a + d * std::max(t, 0.0);
    }

    inline double niceStep(double base, double minWorld) {
        if (!(base > 0.0) || !std::isfinite(base))
            base = 1.0;
        if (!(minWorld > 0.0) || !std::isfinite(minWorld))
            return base;
        const double ratio = minWorld / base;
        const double p = std::pow(10.0, std::floor(std::log10(ratio)));
        for (double m : {1.0, 2.0, 5.0, 10.0})
            if (m * p >= ratio * (1.0 - 1e-9))
                return base * m * p;
        return base * 10.0 * p;
    }

    inline int decimalsFor(double step) {
        for (int d = 0; d <= 6; ++d) {
            const double v = step * std::pow(10.0, d);
            if (std::fabs(v - std::round(v)) < 1e-6 * std::max(1.0, std::fabs(v)))
                return d;
        }
        return 6;
    }

    class ViewControl {
    public:
        static constexpr double kMinZoom = 0.2;
        static constexpr double kMaxZoom = 200.0;

        double zoom = 1.0;
        Vec2 pan{0.0, 0.0};
        bool follow = false; 
        bool dragging = false; 
        Vec2 lastCursor{0.0, 0.0};

        bool isDefault() const { return zoom == 1.0 && pan.x == 0.0 && pan.y == 0.0; }
        void reset() {
            zoom = 1.0;
            pan = {0.0, 0.0};
            follow = false;
        }

        Vec2 map(Vec2 p) const { return {pan.x + zoom * p.x, pan.y + zoom * p.y}; }

        View2D apply(const View2D &v) const {
            View2D r;
            r.scale = v.scale * zoom;
            r.off = {pan.x + zoom * v.off.x, pan.y + zoom * v.off.y};
            return r;
        }

        void zoomAt(Vec2 cursor, double k) {
            const double nz = std::min(std::max(zoom * k, kMinZoom), kMaxZoom);
            k = nz / zoom;
            pan = cursor - (cursor - pan) * k;
            zoom = nz;
        }
        void panBy(Vec2 d) {
            pan = pan + d;
            follow = false;
        }
        void centerOn(Vec2 basePoint, Vec2 target) { pan = target - basePoint * zoom; }
    };

} // namespace mgl
