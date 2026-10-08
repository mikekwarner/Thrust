#pragma once

#include "raylib.h"
#include "Constants.h"
#include "LevelsData.h"
#include "Physics.h"
#include "Entities.h"
#include "Renderer.h"
#include <vector>
#include <string>

enum class GameState {
    TITLE,
    PLAYING,
    PLAYER_DYING,
    RESPAWNING,
    MISSION_COMPLETE,
    MISSION_INCOMPLETE,
    PLANET_DESTROYED,
    ENTER_HIGH_SCORE,
    HIGH_SCORES
};

class Game {
public:
    Game();
    ~Game();

    void Init();
    void Update(float dt);
    void Draw(int windowWidth, int windowHeight);
    bool ShouldQuit() const { return shouldQuit; }

private:
    GameState state = GameState::TITLE;
    bool paused = false;
    bool shouldQuit = false;
    int currentLevelIndex = 0;
    int missionNumber = 1;
    int score = 0;
    int lives = 4;
    int fuel = 1000;
    int hostileGunProbability = 2; // base probability / 256
    int planetDestroyedModifier = 0;

    bool reverseGravity = false;
    bool invisibleLandscape = false;

    ThrustPhysics physics;
    Renderer renderer;

    // Entities in current level
    std::vector<Entities::Turret> turrets;
    Entities::PowerPlant powerPlant;
    std::vector<Entities::FuelTank> fuelTanks;
    std::vector<Entities::Switch> switches;
    Entities::DoorState doorState;

    // Particles & Bullets
    std::vector<Entities::Bullet> bullets;
    std::vector<Entities::Particle> particles;
    std::vector<Entities::Star> stars;

    // Persistent High Scores
    std::vector<Entities::HighScoreEntry> highScores;
    std::string playerNameInput = "PLAYER";
    int lastHighScoreRank = -1;

    // Player shooting state
    int playerBulletIndex = 0;
    bool fireLatch = false;

    // Active input & beam visual states
    bool thrustActive = false;
    bool shieldActive = false;
    bool podConnecting = false;
    bool podClearedPedestal = false;
    bool fuelBeamActive = false;
    Vector2 fuelBeamTarget = { 0, 0 };

    // Timers & sequence state
    float stateTimer = 0.0f;
    float tick60Accum = 0.0f;
    int tick60Steps = 0;
    int tickCount = 0;
    std::string overlayMessage;
    std::string pendingOverlayMessage;
    int pendingBonus = 0;
    bool teleportSoundPlayed = false;

    void LoadLevel(int levelIdx, bool isRespawn = false);
    void BeginRespawnSequence(const std::string& popupText);
    void CheckpointRespawn();
    void HandleInput(ThrustInput& input);
    void UpdatePlaying(float dt, const ThrustInput& input);
    void UpdateTurrets(float dt);
    void UpdateBullets(float dt);
    void UpdateParticles(float dt);
    void UpdatePowerPlant(float dt);
    void UpdateDoors(float dt);
    void CheckCollisions(float dt);
    void SpawnExplosion(float x, float y, int count = 8);
    void SpawnSpark(float x, float y);
    void CheckOrbitEscape();
    void OnPlayerDeath();

    // Pixel-perfect vector polygon & segment collision helpers
    using Poly = std::vector<LevelsData::Point>;
    bool TestPointInPolygon(float wx, float wy, const Poly& poly) const;
    bool TestPointInPolygons(float wx, float wy, const std::vector<Poly>& polys) const;
    bool SegmentIntersectsPolygon(float x1, float y1, float x2, float y2, const Poly& poly) const;
    bool SegmentIntersectsPolygons(float x1, float y1, float x2, float y2, const std::vector<Poly>& polys) const;
    bool PolygonsIntersect(const Poly& polyA, const Poly& polyB, bool unwrapBToA = true) const;
    bool PolygonIntersectsPolygons(const Poly& hull, const std::vector<Poly>& polys, bool isTerrainPolys = false) const;

    Poly GetShipCollisionPoly() const;
    Poly GetPodCollisionPoly(float cx, float cy) const;
    std::vector<Poly> GetTurretCollisionPolys(const Entities::Turret& turret) const;
    std::vector<Poly> GetPowerPlantCollisionPolys(const LevelsData::LevelDef& def) const;
    std::vector<Poly> GetFuelTankCollisionPolys(const Entities::FuelTank& tank, const LevelsData::LevelDef& def) const;
    Poly GetSwitchCollisionPoly(const Entities::Switch& sw) const;
    std::vector<Poly> GetPedestalCollisionPolys(const LevelsData::LevelDef& def, bool includePodBall) const;

    // High score persistence
    std::string GetHighScoreFilePath() const;
    void LoadHighScores();
    void SaveHighScores() const;
    bool QualifiesForHighScore(int newScore) const;
    int InsertHighScore(const std::string& name, int newScore);
};
