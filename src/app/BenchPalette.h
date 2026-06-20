#pragma once

#include <algorithm>
#include <cmath>

namespace samplebench
{
struct Rgb
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

namespace palette
{
inline constexpr Rgb background { 24, 25, 26 };
inline constexpr Rgb panel { 31, 32, 33 };
inline constexpr Rgb text { 238, 239, 235 };
inline constexpr Rgb mutedText { 158, 163, 160 };
inline constexpr Rgb inverseText { 255, 255, 255 };
inline constexpr Rgb border { 71, 75, 75 };
inline constexpr Rgb darkButton { 43, 45, 46 };
inline constexpr Rgb button { 56, 59, 60 };
inline constexpr Rgb accent { 178, 70, 28 };
inline constexpr Rgb accentBlue { 18, 165, 190 };
inline constexpr Rgb accentPink { 204, 47, 105 };
inline constexpr Rgb editor { 20, 21, 22 };
inline constexpr Rgb rack { 38, 40, 41 };
inline constexpr Rgb keepRegion { 66, 132, 91 };
inline constexpr Rgb warmupRegion { 70, 74, 75 };
inline constexpr Rgb tailRegion { 55, 58, 60 };
}

inline double relativeLuminance (Rgb colour)
{
    const auto channel = [] (unsigned char value)
    {
        const auto linear = static_cast<double> (value) / 255.0;
        return linear <= 0.03928 ? linear / 12.92 : std::pow ((linear + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * channel (colour.red)
         + 0.7152 * channel (colour.green)
         + 0.0722 * channel (colour.blue);
}

inline double contrastRatio (Rgb first, Rgb second)
{
    const auto firstLum = relativeLuminance (first);
    const auto secondLum = relativeLuminance (second);
    const auto lighter = std::max (firstLum, secondLum);
    const auto darker = std::min (firstLum, secondLum);
    return (lighter + 0.05) / (darker + 0.05);
}
}
