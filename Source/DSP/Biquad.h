#pragma once
#include <cmath>

namespace kitsune
{
struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

namespace biquad
{
inline BiquadCoeffs norm (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
inline BiquadCoeffs highPass (double sr, double f, double q)
{
    const double w = 2.0 * M_PI * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}
inline BiquadCoeffs bandPass (double sr, double f, double q)
{
    const double w = 2.0 * M_PI * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return norm (al, 0, -al, 1 + al, -2 * c, 1 - al);
}
inline BiquadCoeffs peaking (double sr, double f, double q, double dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * M_PI * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    return norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}
inline BiquadCoeffs lowShelf (double sr, double f, double dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * M_PI * f / sr, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
    return norm (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
                 (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
}
inline BiquadCoeffs highShelf (double sr, double f, double dB)
{
    const double A = std::pow (10.0, dB / 40.0);
    const double w = 2.0 * M_PI * f / sr, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
    return norm (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
                 (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
}
} // namespace biquad

struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0, z2 = 0;
    inline float process (float xf)
    {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0; }
};
} // namespace kitsune
