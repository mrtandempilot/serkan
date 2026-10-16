#pragma once
#include <cmath>
#include <algorithm>

// Volume curves. evaluate() returns gain 0..1 for phase 0..1 within one period.
namespace shapes
{
constexpr int count = 16;

inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }
inline float smooth (float x)  { x = clamp01 (x); return x * x * (3.0f - 2.0f * x); }

inline float sigmoid (float p, float centre, float steep)
{
    auto s  = [&] (float x) { return 1.0f / (1.0f + std::exp (-(x - centre) * steep)); };
    float s0 = s (0.0f), s1 = s (1.0f);
    return clamp01 ((s (p) - s0) / (s1 - s0));
}

inline float evaluate (int id, float p)
{
    constexpr float pi = 3.14159265f;
    p = clamp01 (p);
    switch (id)
    {
        // row 1 — tight pumps
        case 0:  return 1.0f - std::exp (-p * 10.0f);                           // fast release
        case 1:  return smooth (p / 0.5f);                                      // smooth
        case 2:  return sigmoid (p, 0.35f, 18.0f);                              // S-curve
        case 3:  return clamp01 (p / 0.6f);                                     // linear
        case 4:  return p < 0.3f ? 0.0f : smooth ((p - 0.3f) / 0.05f);          // gate
        case 5:  return p < 0.15f ? 0.0f : 1.0f - std::exp (-(p - 0.15f) * 12.0f); // hold + release
        case 6:  return std::sqrt (clamp01 (p / 0.7f));                         // soft knee
        case 7:  return std::pow (clamp01 (p / 0.9f), 2.5f);                    // slow swell
        // row 2 — longer / creative
        case 8:  return 1.0f - std::exp (-p * 5.0f);                            // long release
        case 9:  return smooth (p / 0.8f);                                      // long smooth
        case 10: return sigmoid (p, 0.55f, 14.0f);                              // late S
        case 11: return p < 0.5f ? 0.0f : smooth ((p - 0.5f) / 0.05f);          // half gate
        case 12: return 1.0f - std::pow (std::sin (pi * p), 1.5f);              // mid dip
        case 13: return 1.0f - smooth ((p - 0.6f) / 0.4f);                      // pre-duck (falls at end)
        case 14:                                                                // bounce
        {
            float rise = smooth (p / 0.35f);
            float d = std::sin (pi * clamp01 ((p - 0.45f) / 0.5f));
            return rise * (1.0f - 0.35f * d * d);
        }
        case 15: return 0.5f + 0.5f * smooth (p / 0.5f);                        // half depth
        default: return 1.0f;
    }
}

inline const char* name (int id)
{
    static const char* names[count] = {
        "Fast", "Smooth", "S-Curve", "Linear", "Gate", "Hold", "Soft", "Swell",
        "Long", "Long Smooth", "Late S", "Half Gate", "Dip", "Pre-Duck", "Bounce", "Half Depth" };
    return (id >= 0 && id < count) ? names[id] : "";
}
}
