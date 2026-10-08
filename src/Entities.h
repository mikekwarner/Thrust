#pragma once

#include "raylib.h"
#include "LevelsData.h"
#include <vector>
#include <string>
#include <algorithm>

namespace Entities {

struct Bullet {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float lifetime = 0.0f;
    bool isPlayerBullet = false;
};

struct Particle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float lifetime = 0.0f;
    Color color = WHITE;
    float maxLifetime = 10.0f;
    float size = 1.0f;
    float length = 0.0f;   // > 0 renders as a glowing vector shrapnel strut (in logical px)
    float angle = 0.0f;    // Orientation in radians
    float spin = 0.0f;     // Angular velocity in rad/tick
    bool isRing = false;   // Expanding vector shockwave ring
    bool bounceOnWalls = false;
};

inline float GetTurretDefaultAngle(LevelsData::TurretDir dir) {
    // Outward normal perpendicular to the 45-degree world slope in screen pixels (atan2(1, 2) = 26.565 deg = 2.3613 steps)
    switch (dir) {
        case LevelsData::TurretDir::UP_RIGHT:   return 2.3613f;
        case LevelsData::TurretDir::DOWN_RIGHT: return 13.6387f;
        case LevelsData::TurretDir::DOWN_LEFT:  return 18.3613f;
        case LevelsData::TurretDir::UP_LEFT:    return 29.6387f;
    }
    return 2.3613f;
}

struct Turret {
    float x = 0.0f;
    float y = 0.0f;
    LevelsData::TurretDir dir = LevelsData::TurretDir::UP_RIGHT;
    unsigned char gunParam = 0;
    bool alive = true;

    // Gun barrel rotation state (rotates over 0.5s to targetAngle before firing)
    float currentAngle = 2.3613f; // Angle in [0..32) units (0 = Up, 8 = Right, 16 = Down, 24 = Left)
    float startAngle = 2.3613f;
    float targetAngle = 2.3613f;
    float rotateTimer = 0.0f;
    bool aiming = false;

    Vector2 GetDomePivot() const {
        // Exact center of the turret dome on the orthogonal symmetry axis (s = 0.0, h = 1.30):
        // local (dx, dy) = (2.0 + 0.5 * 1.30, 4.0 - 2.0 * 1.30) = (2.65, 1.40)
        switch (dir) {
            case LevelsData::TurretDir::UP_RIGHT:   return { x + 2.65f, y + 1.40f };
            case LevelsData::TurretDir::UP_LEFT:    return { x + 2.35f, y + 1.40f };
            case LevelsData::TurretDir::DOWN_RIGHT: return { x + 1.65f, y + 4.60f };
            case LevelsData::TurretDir::DOWN_LEFT:  return { x + 3.35f, y + 4.60f };
        }
        return { x + 2.65f, y + 1.40f };
    }
};

struct PowerPlant {
    float x = 0.0f;
    float y = 0.0f;
    bool alive = true;
    int rechargeCounter = 0;
    int rechargeIncrease = 50;
    int countdownTimer = -1; // -1 = inactive, 10 = active (counts down 1 second every second)
    int countdownTicks = 32;
    float countdownSecTimer = 1.0f;
};

struct FuelTank {
    float x = 0.0f;
    float y = 0.0f;
    bool alive = true;
    int fuelRemaining = 300;
};

struct Switch {
    float x = 0.0f;
    float y = 0.0f;
    LevelsData::SwitchDir dir = LevelsData::SwitchDir::LEFT;
    bool alive = true;
};

struct DoorState {
    LevelsData::DoorDef def;
    float openHoldTimer = 0.0f; // Holds door open for 5.0 seconds when triggered
    float openFraction = 0.0f;  // 0.0 = closed, 1.0 = fully open

    std::vector<LevelsData::Point> GetPolygon() const {
        if (def.type == LevelsData::DoorType::NONE) return {};

        float innerX = static_cast<float>(def.innerX);
        float closedX = static_cast<float>(def.closedX);

        if (def.type == LevelsData::DoorType::CHEVRON) {
            float shiftX = openFraction * (closedX + 7.0f - innerX);
            float baseX = closedX - shiftX;
            float peakX = (closedX + 7.0f) - shiftX;
            if (peakX <= innerX + 0.05f) return {};

            float topY = static_cast<float>(def.worldY - 1);
            float midY = topY + 7.0f;
            float botY = topY + 14.0f;
            return {
                { innerX, topY },
                { std::max(innerX, baseX), topY },
                { std::max(innerX, peakX), midY },
                { std::max(innerX, baseX), botY },
                { innerX, botY }
            };
        }

        // SLIDE and STEP horizontal sliding blast doors
        float doorRightX = closedX - openFraction * (closedX - innerX);
        if (doorRightX <= innerX + 0.05f) return {};

        float topY = static_cast<float>(def.worldY);
        float botY = static_cast<float>(def.worldY + def.scanlines);
        return {
            { innerX, topY },
            { doorRightX, topY },
            { doorRightX, botY },
            { innerX, botY }
        };
    }
};

struct Star {
    float x = 0.0f;
    float y = 0.0f;
    Color color = WHITE;
    float phase = 0.0f;
    float speed = 2.5f;
};

struct HighScoreEntry {
    std::string name;
    int score = 0;
};

} // namespace Entities
