#pragma once

#include <array>
#include <string_view>

namespace smf::features::vehicle_wheels {

struct WheelCategory {
    std::string_view name;
    int wheelType;
    const std::array<std::string_view, 31>* wheels;
};

inline constexpr std::array<std::string_view, 31> BennysBespoke{
    "Chrome OG Hunnets",
    "Gold OG Hunnets",
    "Chrome Wires",
    "Gold Wires",
    "Chrome Spoked Out",
    "Gold Spoked Out",
    "Chrome Knock-Offs",
    "Gold Knock-Offs",
    "Chrome Bigger Worm",
    "Gold Bigger Worm",
    "Chrome Vintage Wire",
    "Gold Vintage Wire",
    "Chrome Classic Wire",
    "Gold Classic Wire",
    "Chrome Smoothie",
    "Gold Smoothie",
    "Chrome Classic Rod",
    "Gold Classic Rod",
    "Chrome Dollar",
    "Gold Dollar",
    "Chrome Mighty Star",
    "Gold Mighty Star",
    "Chrome Decadent Dish",
    "Gold Decadent Dish",
    "Chrome Razor Style",
    "Gold Razor Style",
    "Chrome Celtic Knot",
    "Gold Celtic Knot",
    "Chrome Warrior Dish",
    "Gold Warrior Dish",
    "Gold Big Dog Spokes"
};

inline constexpr std::array<std::string_view, 31> BennysOriginals{
    "OG Hunnets",
    "OG Hunnets (Chrome Lip)",
    "Knock-Offs",
    "Knock-Offs (Chrome Lip)",
    "Spoked Out",
    "Spoked Out (Chrome Lip)",
    "Vintage Wire",
    "Vintage Wire (Chrome Lip)",
    "Smoothie",
    "Smoothie (Chrome Lip)",
    "Smoothie (Solid Color)",
    "Rod Me Up",
    "Rod Me Up (Chrome Lip)",
    "Rod Me Up (Solid Color)",
    "Clean",
    "Lotta Chrome",
    "Spindles",
    "Viking",
    "Triple Spoke",
    "Pharohe",
    "Tiger Style",
    "Three Wheelin",
    "Big Bar",
    "Biohazard",
    "Waves",
    "Lick Lick",
    "Spiralizer",
    "Hypnotics",
    "Psycho-Delic",
    "Half Cut",
    "Super Electric"
};

// GTA wheel type values used by SET_VEHICLE_WHEEL_TYPE.
inline constexpr std::array<WheelCategory, 2> Categories{{
    {"Benny's Originals", 8, &BennysOriginals},
    {"Benny's Bespoke", 9, &BennysBespoke}
}};

} // namespace smf::features::vehicle_wheels
