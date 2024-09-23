#pragma once

#include "math/types.h"
#include <immintrin.h>

namespace nk {
    template <NumericOrBoolean T>
    struct Ivec2 {
        // Proxy struct for swizzled components
        struct Swizzle2 {
            T& a;
            T& b;

            // Constructor
            Swizzle2(T& a, T& b) : a(a), b(b) {}

            // Explicit Conversion to Vector2
            Ivec2 get() { return Ivec2(a, b); }

            // Implicit Conversion to Vector2
            Ivec2 operator()() { return Ivec2(a, b); }

            // Implicit Conversion to Vector2
            operator Ivec2() const { return Ivec2(a, b); }

            // Assignment from Vector2
            Swizzle2& operator=(const Ivec2& v) {
                a = v.x;
                b = v.y;
                return *this;
            }
        };

        union {
            __m128 simd;  // SIMD register
            T elements[2];
            struct {
                T x, y;
            };
            struct {
                T r, g;
            };
            struct {
                T s, t;
            };
            struct {
                T u, v;
            };
            struct {
                T width, height;
            };
        };

        Ivec2() : simd(_mm_setzero_ps()) {}

        Ivec2(T* elements) : simd(_mm_set_ps(0.0f, 0.0f, elements[1], elements[0])) {}

        Ivec2(T x, T y)
            : simd(_mm_set_ps(0.0f, 0.0f, y, x)),
              xy{this->x, this->y} {}

        // Constructor to initialize from an __m128 value
        Ivec2(__m128 simd_value) : simd(simd_value) {}

        // SIMD addition
        Ivec2 operator+(const Ivec2& other) const {
            return Ivec2(_mm_add_ps(this->simd, other.simd));
        }

        // SIMD subtraction
        Ivec2 operator-(const Ivec2& other) const {
            return Ivec2(_mm_sub_ps(this->simd, other.simd));
        }
    };
}
