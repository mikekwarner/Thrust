#pragma once

#include <cmath>

namespace Constants {

// Display & Resolution
constexpr int INTERNAL_WIDTH = 320;
constexpr int INTERNAL_HEIGHT = 256;
constexpr float WORLD_SCALE_X = 4.0f;
constexpr float WORLD_SCALE_Y = 2.0f;
constexpr float WORLD_UNITS_X = 256.0f;
constexpr float WORLD_WIDTH = WORLD_UNITS_X * WORLD_SCALE_X;

inline float WrapWorldX(float wx) {
    float m = std::fmod(wx, WORLD_UNITS_X);
    return (m < 0.0f) ? (m + WORLD_UNITS_X) : m;
}

inline float ShortestDeltaWorldX(float fromX, float toX) {
    float d = std::fmod(toX - fromX, WORLD_UNITS_X);
    if (d > WORLD_UNITS_X * 0.5f) d -= WORLD_UNITS_X;
    if (d < -WORLD_UNITS_X * 0.5f) d += WORLD_UNITS_X;
    return d;
}

// Timing: BBC Micro MOS system timer waits 3 centiseconds = 33.333 Hz
constexpr float TICK_TIME = 0.030f; // 30ms

inline bool IsPhysicsActiveSlot(int slot) {
    int s = slot & 0x0F;
    return (s == 0 || s == 3 || s == 5 || s == 8 || s == 11 || s == 13);
}

inline bool IsRotationActiveSlot(int slot) {
    return (slot & 0x03) != 0;
}

// Signed Q7.8 precomputed angle force vectors (32 entries)
// Index 0 = straight up (0, -2.5), Index 8 = right (1.25, 0), Index 16 = down (0, 2.5), Index 24 = left (-1.25, 0)
inline const float ANGLE_X[32] = {
     0.000000f,  0.242188f,  0.476562f,  0.691406f,
     0.882812f,  1.039062f,  1.152344f,  1.222656f,
     1.250000f,  1.222656f,  1.152344f,  1.039062f,
     0.882812f,  0.691406f,  0.476562f,  0.242188f,
     0.000000f, -0.242188f, -0.476562f, -0.691406f,
    -0.882812f, -1.039062f, -1.152344f, -1.222656f,
    -1.250000f, -1.222656f, -1.152344f, -1.039062f,
    -0.882812f, -0.691406f, -0.476562f, -0.242188f
};

inline const float ANGLE_Y[32] = {
    -2.500000f, -2.449219f, -2.308594f, -2.078125f,
    -1.765625f, -1.386719f, -0.953125f, -0.484375f,
     0.000000f,  0.484375f,  0.953125f,  1.386719f,
     1.765625f,  2.078125f,  2.308594f,  2.449219f,
     2.500000f,  2.449219f,  2.308594f,  2.078125f,
     1.765625f,  1.386719f,  0.953125f,  0.484375f,
     0.000000f, -0.484375f, -0.953125f, -1.386719f,
    -1.765625f, -2.078125f, -2.308594f, -2.449219f
};

// Top nibble table for tether accumulation
inline const unsigned char LOOKUP_TOP_NIBBLE[15] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
    0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0
};

// Level gravities (fractional byte / 256.0f)
inline const float LEVEL_GRAVITY[6] = {
    0x05 / 256.0f,
    0x07 / 256.0f,
    0x09 / 256.0f,
    0x0B / 256.0f,
    0x0C / 256.0f,
    0x0D / 256.0f
};

// Drag coefficients per gravity tick
constexpr float DRAG_X = 1.0f - (1.0f / 64.0f);   // 63/64
constexpr float DRAG_Y = 1.0f - (1.0f / 256.0f);  // 255/256
constexpr float DRAG_ANGULAR = 1.0f - (1.0f / 64.0f);

// Mass shift divisors
constexpr float MASS_DIVISOR_SOLO = 16.0f;     // shift 4
constexpr float MASS_DIVISOR_ATTACHED = 32.0f; // shift 5

// Tractor & Pod thresholds
constexpr float TRACTOR_START_DIST = 117.0f; // 0x75
constexpr float TRACTOR_ATTACH_DIST = 132.0f; // 0x84

// Bullet parameters
constexpr int PLAYER_BULLET_LIFETIME = 80;
constexpr float BULLET_SPEED = 1.5f;

} // namespace Constants
