#pragma once

#include "raylib.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace LevelsData {

struct Point {
    float x;
    float y;
};

enum class TurretDir {
    UP_LEFT,
    UP_RIGHT,
    DOWN_LEFT,
    DOWN_RIGHT
};

struct TurretDef {
    float x;
    float y;
    TurretDir dir;
    unsigned char gunParam;
};

enum class SwitchDir {
    LEFT,
    RIGHT
};

struct SwitchDef {
    float x;
    float y;
    SwitchDir dir;
};

enum class DoorType {
    NONE,
    SLIDE,
    STEP,
    CHEVRON
};

struct DoorDef {
    DoorType type = DoorType::NONE;
    int worldY = 0;
    int threshold = 0;
    int scanlines = 0;
    int closedX = 0;
    int openX = 0;
    int innerX = 0;
};

struct SpawnPointDef {
    float midpointX;
    float midpointY;
    float windowX;
    float windowY;
};

struct LevelDef {
    std::string name;
    Color terrainColor;
    Color objectColor;
    std::vector<SpawnPointDef> spawnPoints;
    std::vector<std::vector<Point>> polygons;
    std::vector<TurretDef> turrets;
    Point powerPlant;
    Point podPedestal;
    std::vector<Point> fuel;
    std::vector<SwitchDef> switches;
    DoorDef door;
};

inline const std::vector<LevelDef>& GetLevels() {
    static const std::vector<LevelDef> levels = {
        {
            "Level 0",
            RED,
            GREEN,
            {
                {117.0f, 398.0f, 86.0f, 292.0f},
            },
            {
                {
                    {0.0f, 425.0f},
                    {84.0f, 425.0f},
                    {100.0f, 441.0f},
                    {121.0f, 441.0f},
                    {133.0f, 453.0f},
                    {158.0f, 453.0f},
                    {158.0f, 600.0f},
                    {0.0f, 600.0f},
                },
                {
                    {256.0f, 425.0f},
                    {182.0f, 425.0f},
                    {173.0f, 434.0f},
                    {158.0f, 434.0f},
                    {158.0f, 453.0f},
                    {158.0f, 600.0f},
                    {256.0f, 600.0f},
                },
            },
            {
                {125.0f, 443.0f, TurretDir::UP_RIGHT, 0x1e},
            },
            {160.0f, 427.0f},
            {143.0f, 445.0f},
            {
                {110.0f, 435.0f},
            },
            {
            },
            {DoorType::NONE, 0, 0, 0, 0, 0, 0},
        },
        {
            "Level 1",
            GREEN,
            RED,
            {
                {115.0f, 398.0f, 86.0f, 292.0f},
                {142.0f, 495.0f, 86.0f, 380.0f},
            },
            {
                {
                    {0.0f, 429.0f},
                    {74.0f, 429.0f},
                    {85.0f, 440.0f},
                    {110.0f, 440.0f},
                    {133.0f, 463.0f},
                    {133.0f, 518.0f},
                    {110.0f, 541.0f},
                    {110.0f, 561.0f},
                    {125.0f, 576.0f},
                    {145.0f, 576.0f},
                    {145.0f, 750.0f},
                    {0.0f, 750.0f},
                },
                {
                    {256.0f, 429.0f},
                    {179.0f, 429.0f},
                    {152.0f, 456.0f},
                    {152.0f, 514.0f},
                    {169.0f, 531.0f},
                    {169.0f, 552.0f},
                    {145.0f, 576.0f},
                    {145.0f, 750.0f},
                    {256.0f, 750.0f},
                },
            },
            {
                {116.0f, 532.0f, TurretDir::DOWN_RIGHT, 0x06},
                {158.0f, 522.0f, TurretDir::DOWN_LEFT, 0x0f},
            },
            {100.0f, 433.0f},
            {127.0f, 568.0f},
            {
                {139.0f, 571.0f},
            },
            {
            },
            {DoorType::NONE, 0, 0, 0, 0, 0, 0},
        },
        {
            "Level 2",
            {0, 255, 255, 255},
            GREEN,
            {
                {112.0f, 398.0f, 86.0f, 292.0f},
                {138.0f, 557.0f, 111.0f, 426.0f},
                {74.0f, 662.0f, 50.0f, 547.0f},
            },
            {
                {
                    {0.0f, 439.0f},
                    {135.0f, 439.0f},
                    {135.0f, 519.0f},
                    {125.0f, 529.0f},
                    {125.0f, 580.0f},
                    {95.0f, 580.0f},
                    {85.0f, 590.0f},
                    {85.0f, 621.0f},
                    {70.0f, 621.0f},
                    {60.0f, 631.0f},
                    {60.0f, 716.0f},
                    {70.0f, 726.0f},
                    {91.0f, 726.0f},
                    {91.0f, 900.0f},
                    {0.0f, 900.0f},
                },
                {
                    {256.0f, 439.0f},
                    {179.0f, 439.0f},
                    {179.0f, 458.0f},
                    {156.0f, 458.0f},
                    {156.0f, 520.0f},
                    {180.0f, 520.0f},
                    {180.0f, 540.0f},
                    {170.0f, 550.0f},
                    {150.0f, 550.0f},
                    {150.0f, 611.0f},
                    {120.0f, 611.0f},
                    {120.0f, 663.0f},
                    {100.0f, 663.0f},
                    {91.0f, 672.0f},
                    {91.0f, 726.0f},
                    {91.0f, 900.0f},
                    {256.0f, 900.0f},
                },
            },
            {
                {93.0f, 663.0f, TurretDir::UP_LEFT, 0x1b},
                {62.0f, 626.0f, TurretDir::DOWN_RIGHT, 0x06},
                {88.0f, 584.0f, TurretDir::DOWN_RIGHT, 0x0a},
                {171.0f, 542.0f, TurretDir::UP_LEFT, 0x16},
                {129.0f, 522.0f, TurretDir::DOWN_RIGHT, 0x04},
            },
            {164.0f, 451.0f},
            {78.0f, 718.0f},
            {
                {120.0f, 433.0f},
                {151.0f, 545.0f},
                {157.0f, 545.0f},
                {163.0f, 545.0f},
                {125.0f, 606.0f},
                {103.0f, 657.0f},
            },
            {
            },
            {DoorType::NONE, 0, 0, 0, 0, 0, 0},
        },
        {
            "Level 3",
            GREEN,
            {255, 0, 255, 255},
            {
                {116.0f, 395.0f, 86.0f, 292.0f},
                {128.0f, 478.0f, 87.0f, 352.0f},
                {164.0f, 583.0f, 118.0f, 472.0f},
            },
            {
                {
                    {0.0f, 414.0f},
                    {90.0f, 414.0f},
                    {109.0f, 433.0f},
                    {126.0f, 433.0f},
                    {126.0f, 455.0f},
                    {88.0f, 493.0f},
                    {88.0f, 513.0f},
                    {98.0f, 523.0f},
                    {98.0f, 529.0f},
                    {78.0f, 549.0f},
                    {78.0f, 584.0f},
                    {103.0f, 584.0f},
                    {123.0f, 604.0f},
                    {156.0f, 604.0f},
                    {156.0f, 643.0f},
                    {128.0f, 671.0f},
                    {128.0f, 707.0f},
                    {138.0f, 717.0f},
                    {138.0f, 737.0f},
                    {138.0f, 1150.0f},
                    {0.0f, 1150.0f},
                },
                {
                    {256.0f, 414.0f},
                    {140.0f, 414.0f},
                    {140.0f, 517.0f},
                    {110.0f, 517.0f},
                    {110.0f, 536.0f},
                    {134.0f, 560.0f},
                    {174.0f, 560.0f},
                    {174.0f, 693.0f},
                    {150.0f, 717.0f},
                    {150.0f, 737.0f},
                    {138.0f, 737.0f},
                    {138.0f, 1150.0f},
                    {256.0f, 1150.0f},
                },
            },
            {
                {114.0f, 464.0f, TurretDir::DOWN_RIGHT, 0x06},
                {90.0f, 513.0f, TurretDir::UP_RIGHT, 0x06},
                {90.0f, 534.0f, TurretDir::DOWN_RIGHT, 0x06},
                {120.0f, 548.0f, TurretDir::DOWN_LEFT, 0x12},
                {109.0f, 588.0f, TurretDir::UP_RIGHT, 0x1f},
                {138.0f, 658.0f, TurretDir::DOWN_RIGHT, 0x06},
                {162.0f, 698.0f, TurretDir::UP_LEFT, 0x1e},
            },
            {91.0f, 576.0f},
            {142.0f, 729.0f},
            {
                {146.0f, 599.0f},
            },
            {
                {172.0f, 593.0f, SwitchDir::LEFT},
                {172.0f, 647.0f, SwitchDir::LEFT},
            },
            {DoorType::SLIDE, 617, 16, 13, 174, 158, 156},
        },
        {
            "Level 4",
            RED,
            {255, 0, 255, 255},
            {
                {116.0f, 401.0f, 86.0f, 292.0f},
                {126.0f, 612.0f, 88.0f, 494.0f},
                {115.0f, 732.0f, 67.0f, 614.0f},
                {133.0f, 784.0f, 100.0f, 671.0f},
            },
            {
                {
                    {0.0f, 419.0f},
                    {88.0f, 419.0f},
                    {109.0f, 440.0f},
                    {109.0f, 462.0f},
                    {132.0f, 462.0f},
                    {132.0f, 520.0f},
                    {122.0f, 520.0f},
                    {110.0f, 532.0f},
                    {110.0f, 560.0f},
                    {120.0f, 560.0f},
                    {120.0f, 601.0f},
                    {100.0f, 621.0f},
                    {80.0f, 621.0f},
                    {80.0f, 708.0f},
                    {100.0f, 728.0f},
                    {100.0f, 742.0f},
                    {90.0f, 742.0f},
                    {90.0f, 771.0f},
                    {102.0f, 783.0f},
                    {120.0f, 783.0f},
                    {120.0f, 814.0f},
                    {132.0f, 826.0f},
                    {152.0f, 826.0f},
                    {152.0f, 909.0f},
                    {160.0f, 917.0f},
                    {170.0f, 917.0f},
                    {170.0f, 1050.0f},
                    {0.0f, 1050.0f},
                },
                {
                    {256.0f, 419.0f},
                    {146.0f, 419.0f},
                    {146.0f, 520.0f},
                    {160.0f, 520.0f},
                    {170.0f, 530.0f},
                    {170.0f, 560.0f},
                    {134.0f, 560.0f},
                    {134.0f, 601.0f},
                    {142.0f, 601.0f},
                    {142.0f, 642.0f},
                    {132.0f, 652.0f},
                    {98.0f, 652.0f},
                    {98.0f, 687.0f},
                    {130.0f, 719.0f},
                    {130.0f, 764.0f},
                    {140.0f, 764.0f},
                    {150.0f, 774.0f},
                    {150.0f, 796.0f},
                    {166.0f, 796.0f},
                    {166.0f, 859.0f},
                    {182.0f, 875.0f},
                    {182.0f, 905.0f},
                    {170.0f, 917.0f},
                    {170.0f, 1050.0f},
                    {256.0f, 1050.0f},
                },
            },
            {
                {114.0f, 525.0f, TurretDir::DOWN_RIGHT, 0x05},
                {162.0f, 524.0f, TurretDir::DOWN_LEFT, 0x14},
                {134.0f, 643.0f, TurretDir::UP_LEFT, 0x1a},
                {93.0f, 772.0f, TurretDir::UP_RIGHT, 0x02},
                {142.0f, 768.0f, TurretDir::DOWN_LEFT, 0x12},
                {123.0f, 815.0f, TurretDir::UP_RIGHT, 0x1e},
                {172.0f, 867.0f, TurretDir::DOWN_LEFT, 0x19},
            },
            {143.0f, 553.0f},
            {162.0f, 909.0f},
            {
                {124.0f, 457.0f},
                {154.0f, 555.0f},
                {160.0f, 555.0f},
                {104.0f, 647.0f},
                {105.0f, 778.0f},
                {111.0f, 778.0f},
                {137.0f, 821.0f},
                {143.0f, 821.0f},
            },
            {
                {164.0f, 805.0f, SwitchDir::LEFT},
                {152.0f, 885.0f, SwitchDir::RIGHT},
            },
            {DoorType::STEP, 835, 21, 21, 166, 152, 152},
        },
        {
            "Level 5",
            {255, 0, 255, 255},
            {0, 255, 255, 255},
            {
                {112.0f, 401.0f, 86.0f, 292.0f},
                {168.0f, 591.0f, 140.0f, 472.0f},
                {160.0f, 724.0f, 130.0f, 602.0f},
                {139.0f, 806.0f, 110.0f, 692.0f},
                {178.0f, 920.0f, 135.0f, 795.0f},
            },
            {
                {
                    {0.0f, 381.0f},
                    {77.0f, 381.0f},
                    {77.0f, 444.0f},
                    {100.0f, 444.0f},
                    {180.0f, 524.0f},
                    {180.0f, 565.0f},
                    {160.0f, 565.0f},
                    {150.0f, 575.0f},
                    {150.0f, 737.0f},
                    {133.0f, 737.0f},
                    {133.0f, 792.0f},
                    {120.0f, 805.0f},
                    {120.0f, 825.0f},
                    {174.0f, 879.0f},
                    {174.0f, 893.0f},
                    {161.0f, 906.0f},
                    {161.0f, 937.0f},
                    {151.0f, 947.0f},
                    {151.0f, 1004.0f},
                    {160.0f, 1004.0f},
                    {160.0f, 1035.0f},
                    {160.0f, 1200.0f},
                    {0.0f, 1200.0f},
                },
                {
                    {256.0f, 381.0f},
                    {182.0f, 381.0f},
                    {139.0f, 424.0f},
                    {139.0f, 444.0f},
                    {194.0f, 499.0f},
                    {194.0f, 564.0f},
                    {214.0f, 584.0f},
                    {214.0f, 605.0f},
                    {189.0f, 605.0f},
                    {161.0f, 633.0f},
                    {161.0f, 667.0f},
                    {179.0f, 685.0f},
                    {179.0f, 705.0f},
                    {169.0f, 715.0f},
                    {169.0f, 765.0f},
                    {148.0f, 765.0f},
                    {148.0f, 805.0f},
                    {192.0f, 849.0f},
                    {192.0f, 879.0f},
                    {199.0f, 886.0f},
                    {192.0f, 893.0f},
                    {192.0f, 949.0f},
                    {167.0f, 974.0f},
                    {167.0f, 1012.0f},
                    {179.0f, 1012.0f},
                    {179.0f, 1035.0f},
                    {160.0f, 1035.0f},
                    {160.0f, 1200.0f},
                    {256.0f, 1200.0f},
                },
            },
            {
                {175.0f, 959.0f, TurretDir::UP_LEFT, 0x1a},
                {155.0f, 940.0f, TurretDir::DOWN_RIGHT, 0x06},
                {162.0f, 902.0f, TurretDir::DOWN_RIGHT, 0x09},
                {155.0f, 814.0f, TurretDir::DOWN_LEFT, 0x12},
                {123.0f, 799.0f, TurretDir::DOWN_RIGHT, 0x06},
                {172.0f, 705.0f, TurretDir::UP_LEFT, 0x16},
                {172.0f, 680.0f, TurretDir::DOWN_LEFT, 0x12},
                {172.0f, 615.0f, TurretDir::UP_LEFT, 0x1b},
                {202.0f, 574.0f, TurretDir::DOWN_LEFT, 0x12},
                {153.0f, 569.0f, TurretDir::DOWN_RIGHT, 0x05},
                {153.0f, 460.0f, TurretDir::DOWN_LEFT, 0x0e},
            },
            {169.0f, 1028.0f},
            {154.0f, 996.0f},
            {
                {154.0f, 760.0f},
                {193.0f, 599.0f},
            },
            {
                {161.0f, 920.0f, SwitchDir::RIGHT},
                {190.0f, 861.0f, SwitchDir::LEFT},
            },
            {DoorType::CHEVRON, 880, 18, 15, 192, 174, 174},
        },
    };
    return levels;
}

inline float GetGroundY(const LevelDef& level, float centerX, float approxBaseY) {
    float bestY = approxBaseY;
    float bestDist = 1e9f;
    for (const auto& poly : level.polygons) {
        for (size_t i = 0; i < poly.size(); ++i) {
            const auto& a = poly[i];
            const auto& b = poly[(i + 1) % poly.size()];
            if (std::abs(a.y - b.y) <= 1.5f) {
                float minX = std::min(a.x, b.x) - 1.5f;
                float maxX = std::max(a.x, b.x) + 1.5f;
                if (centerX >= minX && centerX <= maxX) {
                    float dx = b.x - a.x;
                    float t = (std::abs(dx) > 0.001f) ? std::clamp((centerX - a.x) / dx, 0.0f, 1.0f) : 0.0f;
                    float edgeY = a.y + t * (b.y - a.y);
                    float dist = std::abs(edgeY - approxBaseY);
                    if (dist < bestDist && dist <= 4.0f) {
                        bestDist = dist;
                        bestY = edgeY;
                    }
                }
            }
        }
    }
    return bestY;
}

} // namespace LevelsData
