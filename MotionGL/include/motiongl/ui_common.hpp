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
#include <cstdio>
#include <string>
#include <utility>
#include "renderer2d.hpp"
#include "vec.hpp"
#include "view.hpp"
#include "window.hpp"

namespace mgl
{

    template <typename... Args>
    std::string fmt(const char *f, Args &&...args) {
        const int n = std::snprintf(nullptr, 0, f, std::forward<Args>(args)...);
        if (n <= 0)
            return {};
        std::string s(static_cast<size_t>(n), '\0');
        std::snprintf(&s[0], static_cast<size_t>(n) + 1, f, std::forward<Args>(args)...);
        return s;
    }

    std::string tickLabel(double value, double step);

    void drawBackground(Renderer2D &r, int w, int h, const Color &base);

    void drawHud(Renderer2D &r, int w, int h, const std::string &top, const std::string &bottom,
                 const Color &textColor, const Color &dimColor);
    constexpr double kHudTop = 30.0;  
    constexpr double kHudBottom = 34.0;

    bool drawOffscreenMarker(Renderer2D &r, Vec2 target, int w, int h, const Color &c);

    std::string viewStatus(const ViewControl &vc);

    bool viewHandleKey(ViewControl &vc, int key, int action, Vec2 windowCenterPx);

    void viewAttachMouse(Window &win, ViewControl &vc);

} // namespace mgl
