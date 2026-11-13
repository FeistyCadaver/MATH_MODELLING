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
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <utility>
#include "vec.hpp"

namespace mgl
{

    class Window {
    public:
        struct Config {
            int width = 1024;
            int height = 640;
            std::string title = "mgl";
            int msaaSamples = 4; ///< сглаживание краёв (0 - выключено)
        };

        explicit Window(const Config &cfg);
        ~Window();
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        bool shouldClose() const;
        void pollEvents();
        void swapBuffers();
        void close();
        std::pair<int, int> framebufferSize() const;

        /// Положение курсора в пикселях кадрового буфера, ось y направлена вверх
        /// (та же система координат, что у Renderer2D).
        Vec2 cursorPx() const;

        using KeyFn = std::function<void(int key, int action)>;
        using ScrollFn = std::function<void(double dy, Vec2 cursorPx)>;
        using MouseFn = std::function<void(int button, int action, Vec2 cursorPx)>;
        using CursorFn = std::function<void(Vec2 cursorPx)>;
        void onKey(KeyFn fn) { keyFn_ = std::move(fn); }
        void onScroll(ScrollFn fn) { scrollFn_ = std::move(fn); }
        void onMouseButton(MouseFn fn) { mouseFn_ = std::move(fn); }
        void onCursorMove(CursorFn fn) { cursorFn_ = std::move(fn); }

    private:
        GLFWwindow *handle_ = nullptr;
        KeyFn keyFn_;
        ScrollFn scrollFn_;
        MouseFn mouseFn_;
        CursorFn cursorFn_;
        static void keyCallback(GLFWwindow *w, int key, int scancode, int action, int mods);
        static void scrollCallback(GLFWwindow *w, double dx, double dy);
        static void mouseCallback(GLFWwindow *w, int button, int action, int mods);
        static void cursorCallback(GLFWwindow *w, double x, double y);
    };

} // namespace mgl
