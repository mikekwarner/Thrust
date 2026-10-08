#pragma once

#include "raylib.h"
#include "Constants.h"
#include "LevelsData.h"
#include "Physics.h"
#include "Entities.h"
#include <vector>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();
    void Close();

    void BeginFrame(int windowWidth, int windowHeight);
    void EndFrame();

    void RenderGame(
        const ThrustPhysics& physics,
        const LevelsData::LevelDef& level,
        const std::vector<Entities::Turret>& turrets,
        const Entities::PowerPlant& powerPlant,
        const std::vector<Entities::FuelTank>& fuelTanks,
        const std::vector<Entities::Switch>& switches,
        const Entities::DoorState& doorState,
        const std::vector<Entities::Bullet>& bullets,
        const std::vector<Entities::Particle>& particles,
        const std::vector<Entities::Star>& stars,
        bool thrustActive,
        bool shieldActive,
        bool podConnecting,
        bool fuelBeamActive,
        Vector2 fuelBeamTarget,
        int tickCount,
        int score,
        int lives,
        int fuel,
        const std::string& overlayMessage,
        bool shipVisible = true,
        bool teleportAnimActive = false,
        float teleportProgress = 1.0f
    );

    void RenderTitleScreen(
        int windowWidth,
        int windowHeight,
        const std::vector<Entities::HighScoreEntry>& highScores,
        int tickCount
    );

    void RenderHighScoreScreen(
        int windowWidth,
        int windowHeight,
        int finalScore,
        bool enteringName,
        const std::string& currentName,
        int highlightedRank,
        const std::vector<Entities::HighScoreEntry>& highScores,
        int tickCount
    );

    // Coordinate conversion helpers
    Vector2 WorldToLogical(float wx, float wy, float camX, float camY) const;
    Vector2 LogicalToWindow(float lx, float ly) const;
    Vector2 UIToWindow(float ux, float uy) const;
    Vector2 WorldToWindow(float wx, float wy, float camX, float camY) const;

private:
    int winW = 800;
    int winH = 640;
    float scale = 2.5f;
    float uiScale = 2.5f;
    float viewLogicalW = 320.0f;
    float viewLogicalH = 256.0f;
    float uiOffsetX = 0.0f;
    float uiOffsetY = 0.0f;
    float lineThick = 2.0f;

    // Vector drawing primitives with CRT vector glow
    void DrawVecLineWin(Vector2 a, Vector2 b, Color color, float thickScale = 1.0f, bool glow = true) const;
    void DrawVecPolyWin(const std::vector<Vector2>& pts, bool closed, Color color, float thickScale = 1.0f, bool glow = true) const;
    void DrawVecLineWorld(float wx1, float wy1, float wx2, float wy2, float camX, float camY, Color color, float thickScale = 1.0f, bool glow = true) const;
    void DrawVecPolyWorld(const std::vector<Vector2>& worldPts, bool closed, float camX, float camY, Color color, float thickScale = 1.0f, bool glow = true) const;

    // Entity vector renderers
    void DrawVectorTerrain(const std::vector<LevelsData::Point>& pts, Color color, float camX, float camY, bool isDoor = false) const;
    void DrawVectorDoor(const Entities::DoorState& doorState, Color color, float camX, float camY) const;
    void DrawVectorPodStand(float x, float groundY, bool drawPodBall, Color standColor, Color podColor, float camX, float camY) const;
    void DrawVectorPodBall(float wx, float wy, Color podColor, float camX, float camY) const;
    void DrawVectorPowerPlant(const Entities::PowerPlant& pp, float groundY, Color objColor, Color detailColor, int tickCount, float camX, float camY) const;
    void DrawVectorFuelTank(float x, float groundY, Color objColor, Color textColor, float camX, float camY) const;
    void DrawVectorTurret(const Entities::Turret& turret, Color objColor, int tickCount, float camX, float camY) const;
    void DrawVectorSwitch(const Entities::Switch& sw, bool doorOpenActive, int tickCount, float camX, float camY) const;
    void DrawVectorShip(float wx, float wy, int angleIdx, bool thrustActive, bool shieldActive, Color shieldColor, int tickCount, float camX, float camY) const;
    void DrawTeleportMorph(float wx, float wy, const std::vector<Vector2>& targetLocalOutline, float squareHalfSize, float progress, Color outlineColor, float camX, float camY) const;
    void DrawVectorHUD(int score, int lives, int fuel, const Entities::PowerPlant& powerPlant, const std::string& overlayMessage) const;
};
