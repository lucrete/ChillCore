#ifndef CCCOLOUR_H
#define CCCOLOUR_H

namespace CC
{
    struct Colour
    {
        float r, g, b, a;

        constexpr Colour()
            : r(0.0f), g(0.0f), b(0.0f), a(1.0f)
        {
        }

        constexpr Colour(float r, float g, float b, float a = 1.0f)
            : r(r), g(g), b(b), a(a)
        {
        }

        bool operator==(const Colour& other) const
        {
            return r == other.r && g == other.g && b == other.b && a == other.a;
        }

        operator const float*() const { return &r; }

        static constexpr Colour WHITE() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static constexpr Colour BLACK() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
        static constexpr Colour RED() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
        static constexpr Colour GREEN() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
        static constexpr Colour BLUE() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
        static constexpr Colour YELLOW() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
        static constexpr Colour TRANSPARENT() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    };
}

#endif // CCCOLOUR_H
