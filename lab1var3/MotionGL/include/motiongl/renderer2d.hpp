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
#include <string>
#include <vector>
#include "gl_loader.hpp"
#include "vec.hpp"

namespace mgl
{

    class Renderer2D {
    public:
        Renderer2D();
        ~Renderer2D();
        Renderer2D(const Renderer2D &) = delete;
        Renderer2D &operator=(const Renderer2D &) = delete;

        void begin(int fbWidth, int fbHeight);
        void line(Vec2 a, Vec2 b, const Color &c, float thicknessPx = 1.f);
        void arrow(Vec2 a, Vec2 b, const Color &c, float thicknessPx = 2.f, float headLenPx = 10.f);
        void filledCircle(Vec2 center, float radiusPx, const Color &c, int segments = 24);
        void filledQuad(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, const Color& c);

        void gradientQuad(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3,
                          const Color &c0, const Color &c1, const Color &c2, const Color &c3);
        void rect(Vec2 bottomLeft, Vec2 topRight, const Color &c);
        void ring(Vec2 center, float radiusPx, float thicknessPx, const Color &c, int segments = 32);
        void radialGlow(Vec2 center, float radiusPx, const Color &c, float centerAlpha, int segments = 32);
        void dashedLine(Vec2 a, Vec2 b, const Color &c, float thicknessPx = 1.f,
                        float dashPx = 8.f, float gapPx = 6.f);
        void polyline(const std::vector<Vec2> &pts, const Color &c0, const Color &c1, float thicknessPx = 2.f);

        void text(const std::string &s, Vec2 topLeftPx, const Color &c, float charHeightPx = 14.f);
        static float textWidth(const std::string &s, float charHeightPx);
        void end();

    private:
        void quad(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, const Color &c);
        void pushVertex(Vec2 p, const Color &c);

        GLuint program_ = 0;
        GLuint vao_ = 0, vbo_ = 0;
        GLint uTransform_ = -1;
        int width_ = 1, height_ = 1;
        std::vector<float> vertices_; // x,y,r,g,b,a
    };

} // namespace mgl