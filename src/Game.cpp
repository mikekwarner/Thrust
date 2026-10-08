#include "Game.h"
#include "Audio.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

constexpr uint32_t HIGHSCORE_DAT_MAGIC = 0x54485253; // 'THRS'

struct BinaryHighScoreRecord {
  char name[16];
  int32_t score;
};

} // namespace

Game::Game() {}
Game::~Game() {}

std::string Game::GetHighScoreFilePath() const {
  const char *appDir = GetApplicationDirectory();
  if (appDir && appDir[0] != '\0') {
    return std::string(appDir) + "highscores.dat";
  }
  return "highscores.dat";
}

void Game::LoadHighScores() {
  highScores.clear();
  std::ifstream in(GetHighScoreFilePath(), std::ios::binary);
  if (in.is_open()) {
    uint32_t magic = 0;
    uint32_t count = 0;
    if (in.read(reinterpret_cast<char *>(&magic), sizeof(magic)) &&
        in.read(reinterpret_cast<char *>(&count), sizeof(count)) &&
        magic == HIGHSCORE_DAT_MAGIC && count <= 8) {
      for (uint32_t i = 0; i < count; ++i) {
        BinaryHighScoreRecord rec{};
        if (!in.read(reinterpret_cast<char *>(&rec), sizeof(rec)))
          break;
        rec.name[sizeof(rec.name) - 1] = '\0';
        std::string name(rec.name);
        if (!name.empty() && rec.score >= 0) {
          highScores.push_back({name, static_cast<int>(rec.score)});
        }
      }
    }
    in.close();
  }

  if (highScores.empty()) {
    // Default classic BBC Micro Thrust Hall of Fame entries
    highScores = {{"ACORN", 50000},    {"BBC MICRO", 30000},
                  {"ELECTRON", 20000}, {"SUPERIOR", 10000},
                  {"THRUST", 6000},    {"VECTOR", 4500},
                  {"CADET", 3000},     {"ROOKIE", 1500}};
    SaveHighScores();
  }
}

void Game::SaveHighScores() const {
  std::ofstream out(GetHighScoreFilePath(), std::ios::binary | std::ios::trunc);
  if (out.is_open()) {
    uint32_t magic = HIGHSCORE_DAT_MAGIC;
    uint32_t count =
        static_cast<uint32_t>(std::min<size_t>(highScores.size(), 8));
    out.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    for (uint32_t i = 0; i < count; ++i) {
      BinaryHighScoreRecord rec{};
      std::strncpy(rec.name, highScores[i].name.c_str(), sizeof(rec.name) - 1);
      rec.score = static_cast<int32_t>(highScores[i].score);
      out.write(reinterpret_cast<const char *>(&rec), sizeof(rec));
    }
    out.close();
  }
}

bool Game::QualifiesForHighScore(int newScore) const {
  if (newScore <= 0)
    return false;
  if (highScores.size() < 8)
    return true;
  return newScore > highScores.back().score;
}

int Game::InsertHighScore(const std::string &name, int newScore) {
  std::string cleanName = name;
  while (!cleanName.empty() && cleanName.back() == ' ')
    cleanName.pop_back();
  if (cleanName.empty())
    cleanName = "PILOT";

  int insertedRank = -1;
  for (size_t i = 0; i < highScores.size(); ++i) {
    if (newScore > highScores[i].score) {
      highScores.insert(highScores.begin() + i, {cleanName, newScore});
      insertedRank = static_cast<int>(i);
      break;
    }
  }
  if (insertedRank < 0 && highScores.size() < 8) {
    insertedRank = static_cast<int>(highScores.size());
    highScores.push_back({cleanName, newScore});
  }
  if (highScores.size() > 8) {
    highScores.resize(8);
  }
  SaveHighScores();
  return insertedRank;
}

void Game::Init() {
  Audio::Init();
  renderer.Init();
  LoadHighScores();

  // Generate random background stars with individual twinkle rates & phases
  stars.resize(65);
  for (auto &s : stars) {
    s.x = static_cast<float>(std::rand() % Constants::INTERNAL_WIDTH);
    s.y = static_cast<float>(std::rand() % Constants::INTERNAL_HEIGHT);
    int brightness = 150 + (std::rand() % 106);
    int tint = std::rand() % 3;
    unsigned char r = static_cast<unsigned char>(brightness);
    unsigned char g = static_cast<unsigned char>(brightness);
    unsigned char b = static_cast<unsigned char>(brightness);
    if (tint == 1) {
      r = static_cast<unsigned char>(std::max(120, brightness - 25));
    } else if (tint == 2) {
      b = static_cast<unsigned char>(std::max(120, brightness - 30));
    }
    s.color = {r, g, b, 255};
    s.phase = static_cast<float>(std::rand() % 628) / 100.0f;
    s.speed = 1.6f + static_cast<float>(std::rand() % 420) / 100.0f;
  }

  state = GameState::TITLE;
}

void Game::LoadLevel(int levelIdx, bool isRespawn) {
  const auto &allLevels = LevelsData::GetLevels();
  currentLevelIndex =
      std::clamp(levelIdx, 0, static_cast<int>(allLevels.size()) - 1);
  const auto &def = allLevels[currentLevelIndex];

  physics.SetLevel(currentLevelIndex);
  physics.state.reverseGravity = reverseGravity;

  // Reset entities from level definition
  turrets.clear();
  for (const auto &t : def.turrets) {
    float defAngle = Entities::GetTurretDefaultAngle(t.dir);
    turrets.push_back({t.x, t.y, t.dir, t.gunParam, true, defAngle, defAngle,
                       defAngle, 0.0f, false});
  }

  powerPlant = {def.powerPlant.x, def.powerPlant.y, true, 0, 50, -1, 32, 1.0f};

  fuelTanks.clear();
  for (const auto &f : def.fuel) {
    fuelTanks.push_back({f.x, f.y, true, 300});
  }

  switches.clear();
  for (const auto &s : def.switches) {
    switches.push_back({s.x, s.y, s.dir, true});
  }

  doorState.def = def.door;
  doorState.openHoldTimer = 0.0f;
  doorState.openFraction = 0.0f;

  bullets.clear();
  particles.clear();
  thrustActive = false;
  shieldActive = false;
  podConnecting = false;
  podClearedPedestal = false;
  fuelBeamActive = false;

  // Set spawn point (face upwards in normal gravity, downwards in reverse gravity)
  if (!def.spawnPoints.empty()) {
    const auto &sp = def.spawnPoints[0];
    physics.state.shipX = sp.midpointX;
    physics.state.shipY = sp.midpointY;
    physics.state.x = sp.midpointX;
    physics.state.y = sp.midpointY;
    physics.state.angle = reverseGravity ? 16 : 0;
    physics.ResetMotion();
  }

  if (!isRespawn) {
    physics.DetachPod();
  }
}

void Game::BeginRespawnSequence(const std::string &popupText) {
  state = GameState::RESPAWNING;
  overlayMessage = popupText;
  pendingOverlayMessage.clear();
  pendingBonus = 0;
  teleportSoundPlayed = false;
  stateTimer = 2.4f; // First 1.2s: popup shown (ship hidden). Next 1.2s: popup
                     // closes, ship start animation plays.
}

void Game::CheckpointRespawn() {
  const auto &allLevels = LevelsData::GetLevels();
  const auto &def = allLevels[currentLevelIndex];

  float curY = physics.state.y;
  size_t bestIdx = 0;

  if (physics.state.podAttached) {
    // When carrying a pod, respawn at the lower spawn point rather than the
    // higher spawn point
    bestIdx = def.spawnPoints.size() - 1;
    for (size_t i = 0; i < def.spawnPoints.size(); ++i) {
      if (def.spawnPoints[i].midpointY + 18.0f >= curY) {
        bestIdx = i;
        break;
      }
    }
  } else {
    // When descending without a pod, respawn at the higher spawn point already
    // passed
    for (size_t i = 0; i < def.spawnPoints.size(); ++i) {
      if (def.spawnPoints[i].midpointY <= curY) {
        bestIdx = i;
      }
    }
  }

  const auto &sp = def.spawnPoints[bestIdx];
  physics.state.x = sp.midpointX;
  physics.state.y = sp.midpointY;
  physics.state.angle = reverseGravity ? 16 : 0;
  physics.ResetMotion();

  if (physics.state.podAttached) {
    // In normal gravity, ship spawns above (midpointY - 10) and pod below (midpointY + 10).
    // In reverse gravity, swap them so pod spawns at (midpointY - 10) and ship at (midpointY + 10).
    physics.state.pod.tetherIndex = 14;
    physics.state.pod.angleShipToPod = reverseGravity ? 16 : 0;
    physics.state.pod.angleFrac = 0.0f;
    physics.state.pod.angularVelocity = 0.0f;
    podClearedPedestal = true;
    physics.DerivePositions();
  } else {
    physics.state.shipX = sp.midpointX;
    physics.state.shipY = sp.midpointY;
    physics.DetachPod();
    podClearedPedestal = false;
  }

  doorState.openHoldTimer = 0.0f;
  doorState.openFraction = 0.0f;

  bullets.clear();
  particles.clear();
  thrustActive = false;
  shieldActive = false;
  podConnecting = false;
  fuelBeamActive = false;
}

void Game::HandleInput(ThrustInput &input) {
  // Controls:
  // a / d  - rotate
  // space  - collect pod / fuel / shield
  // return - fire
  // shift  - thrust
  input.rotate = 0;
  if (IsKeyDown(KEY_A))
    input.rotate -= 1;
  if (IsKeyDown(KEY_D))
    input.rotate += 1;

  input.thrust =
      (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && (fuel > 0);
  input.shield = IsKeyDown(KEY_SPACE) && (fuel > 0);
  input.fire = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_KP_ENTER);

  // Level jump shortcuts (1-6, 0 for reverse gravity)
  auto jumpToLevel = [&](int idx) {
    Audio::SetEngineThrust(false);
    Audio::SetShieldSound(false);
    LoadLevel(idx);
    BeginRespawnSequence(TextFormat("LEVEL %d", idx + 1));
  };
  if (IsKeyPressed(KEY_ONE))
    jumpToLevel(0);
  if (IsKeyPressed(KEY_TWO))
    jumpToLevel(1);
  if (IsKeyPressed(KEY_THREE))
    jumpToLevel(2);
  if (IsKeyPressed(KEY_FOUR))
    jumpToLevel(3);
  if (IsKeyPressed(KEY_FIVE))
    jumpToLevel(4);
  if (IsKeyPressed(KEY_SIX))
    jumpToLevel(5);
  if (IsKeyPressed(KEY_ZERO)) {
    reverseGravity = !reverseGravity;
    physics.state.reverseGravity = reverseGravity;
  }
}

void Game::Update(float dt) {
  Audio::Update();

  if (state == GameState::TITLE) {
    tick60Accum += std::clamp(dt, 0.0f, 0.05f);
    while (tick60Accum >= (1.0f / 60.0f)) {
      tick60Accum -= (1.0f / 60.0f);
      tickCount++;
    }

    if (IsKeyPressed(KEY_Q)) {
      shouldQuit = true;
      return;
    }

    // Start game at Level 1 via Space/Return, or hidden F1-F6 testing shortcuts
    // for Levels 1-6 (Shift + F1-F6 starts in reverse gravity mode)
    int startLevel = -1;
    bool startReverseGravity = false;
    bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsKeyPressed(KEY_KP_ENTER)) {
      startLevel = 0;
    } else if (IsKeyPressed(KEY_F1)) {
      startLevel = 0;
      startReverseGravity = shiftHeld;
    } else if (IsKeyPressed(KEY_F2)) {
      startLevel = 1;
      startReverseGravity = shiftHeld;
    } else if (IsKeyPressed(KEY_F3)) {
      startLevel = 2;
      startReverseGravity = shiftHeld;
    } else if (IsKeyPressed(KEY_F4)) {
      startLevel = 3;
      startReverseGravity = shiftHeld;
    } else if (IsKeyPressed(KEY_F5)) {
      startLevel = 4;
      startReverseGravity = shiftHeld;
    } else if (IsKeyPressed(KEY_F6)) {
      startLevel = 5;
      startReverseGravity = shiftHeld;
    }

    if (startLevel >= 0) {
      score = 0;
      lives = 4;
      fuel = 1000;
      currentLevelIndex = startLevel;
      missionNumber = startReverseGravity ? 2 : 1;
      hostileGunProbability = 2 + startLevel + (startReverseGravity ? 6 : 0);
      planetDestroyedModifier = 0;
      reverseGravity = startReverseGravity;
      paused = false;
      LoadLevel(currentLevelIndex);
      BeginRespawnSequence(TextFormat("LEVEL %d", currentLevelIndex + 1));
    }
    return;
  }

  // Escape exits back to the main screen from any in-game or high-score state
  if (IsKeyPressed(KEY_ESCAPE)) {
    Audio::SetEngineThrust(false);
    Audio::SetShieldSound(false);
    paused = false;
    thrustActive = false;
    shieldActive = false;
    podConnecting = false;
    fuelBeamActive = false;
    overlayMessage = "";
    pendingOverlayMessage = "";
    pendingBonus = 0;
    teleportSoundPlayed = false;
    state = GameState::TITLE;
    return;
  }

  // P toggles Pause during active gameplay
  if (state == GameState::PLAYING && IsKeyPressed(KEY_P)) {
    paused = !paused;
    if (paused) {
      Audio::SetEngineThrust(false);
      Audio::SetShieldSound(false);
      thrustActive = false;
      shieldActive = false;
    }
  }

  if (paused) {
    Audio::SetEngineThrust(false);
    Audio::SetShieldSound(false);
    return;
  }

  tick60Accum += std::clamp(dt, 0.0f, 0.05f);
  tick60Steps = 0;
  while (tick60Accum >= (1.0f / 60.0f)) {
    tick60Accum -= (1.0f / 60.0f);
    tick60Steps++;
    tickCount++;
  }

  if (state == GameState::ENTER_HIGH_SCORE) {
    int key = GetCharPressed();
    while (key > 0) {
      if (key >= 32 && key <= 125 && playerNameInput.size() < 12) {
        char c = static_cast<char>(key);
        if (c >= 'a' && c <= 'z')
          c = static_cast<char>(c - 'a' + 'A');
        playerNameInput.push_back(c);
      }
      key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !playerNameInput.empty()) {
      playerNameInput.pop_back();
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
      lastHighScoreRank = InsertHighScore(playerNameInput, score);
      state = GameState::HIGH_SCORES;
    }
    return;
  }

  if (state == GameState::HIGH_SCORES) {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsKeyPressed(KEY_KP_ENTER)) {
      state = GameState::TITLE;
    }
    return;
  }

  if (state == GameState::RESPAWNING) {
    stateTimer -= dt;
    // Phase 1 (2.4s -> 1.2s): Level/Ready popup shown, ship hidden.
    // Phase 2 (1.2s -> 0.0s): Popup closes, ship start animation plays.
    if (stateTimer <= 1.2f && !teleportSoundPlayed) {
      overlayMessage = "";
      teleportSoundPlayed = true;
      Audio::PlayTeleport();
    }
    if (stateTimer <= 0.0f) {
      state = GameState::PLAYING;
      stateTimer = 0.0f;
      overlayMessage = "";
    }
    return;
  }

  if (state == GameState::MISSION_COMPLETE) {
    stateTimer -= dt;
    // Phase 1 (2.7s -> 1.5s): Ship & pod end animation plays, no popup.
    // Phase 2 (1.5s -> 0.0s): End animation finishes, Bonus popup appears.
    if (stateTimer <= 1.5f && overlayMessage.empty()) {
      score += pendingBonus;
      pendingBonus = 0;
      overlayMessage = pendingOverlayMessage;
    }
    if (stateTimer <= 0.0f) {
      currentLevelIndex++;
      hostileGunProbability = std::min(35, hostileGunProbability + 1);
      planetDestroyedModifier = 0;
      if (currentLevelIndex >=
          static_cast<int>(LevelsData::GetLevels().size())) {
        currentLevelIndex = 0;
        missionNumber++;
        reverseGravity = !reverseGravity;
      }
      LoadLevel(currentLevelIndex);
      BeginRespawnSequence(TextFormat("LEVEL %d", currentLevelIndex + 1));
    }
    return;
  }

  if (state == GameState::MISSION_INCOMPLETE) {
    stateTimer -= dt;
    if (stateTimer <= 1.5f && overlayMessage.empty()) {
      overlayMessage = pendingOverlayMessage;
    }
    if (stateTimer <= 0.0f) {
      if (lives <= 0 || fuel <= 0) {
        lastHighScoreRank = -1;
        if (QualifiesForHighScore(score)) {
          state = GameState::ENTER_HIGH_SCORE;
        } else {
          state = GameState::HIGH_SCORES;
        }
      } else {
        LoadLevel(currentLevelIndex);
        BeginRespawnSequence("READY");
      }
    }
    return;
  }

  if (state == GameState::PLANET_DESTROYED) {
    stateTimer -= dt;
    if (stateTimer <= 1.5f && overlayMessage.empty()) {
      overlayMessage = pendingOverlayMessage;
    }
    if (stateTimer <= 0.0f) {
      currentLevelIndex++;
      hostileGunProbability = std::min(35, hostileGunProbability + 1);
      planetDestroyedModifier = 8;
      if (currentLevelIndex >=
          static_cast<int>(LevelsData::GetLevels().size())) {
        currentLevelIndex = 0;
        missionNumber++;
        reverseGravity = !reverseGravity;
      }
      LoadLevel(currentLevelIndex);
      BeginRespawnSequence("PLANET ESCAPED!");
    }
    return;
  }

  if (state == GameState::PLAYER_DYING) {
    stateTimer -= dt;
    UpdateParticles(dt);
    if (stateTimer <= 0.0f) {
      lives--;
      if (lives <= 0 || fuel <= 0) {
        lastHighScoreRank = -1;
        if (QualifiesForHighScore(score)) {
          state = GameState::ENTER_HIGH_SCORE;
        } else {
          state = GameState::HIGH_SCORES;
        }
      } else {
        CheckpointRespawn();
        BeginRespawnSequence("READY");
      }
    }
    return;
  }

  // PLAYING State
  ThrustInput input;
  HandleInput(input);
  UpdatePlaying(dt, input);
}

void Game::UpdatePlaying(float dt, const ThrustInput &input) {
  if (stateTimer > 0.0f) {
    stateTimer -= dt;
    if (stateTimer <= 0.0f)
      overlayMessage = "";
  }

  thrustActive = input.thrust;
  shieldActive = input.shield;

  // 1. Audio engine thrust & shield background hum
  Audio::SetEngineThrust(input.thrust && fuel > 0);
  Audio::SetShieldSound(input.shield && fuel > 0);

  // 2. Physics update (smooth continuous dt integration)
  physics.Update(dt, input);

  // 3. Pod attachment logic (Pod center is at pedestal + (1.375, groundY
  // - 5.75))
  const auto &def = LevelsData::GetLevels()[currentLevelIndex];
  if (!physics.state.podAttached) {
    float pedGroundY = LevelsData::GetGroundY(def, def.podPedestal.x + 1.375f,
                                              def.podPedestal.y + 8.0f);
    float podCenterX = def.podPedestal.x + 1.375f;
    float podCenterY = pedGroundY - 5.75f;

    float dxPx =
        Constants::ShortestDeltaWorldX(podCenterX, physics.state.shipX) *
        Constants::WORLD_SCALE_X;
    float dyPx = (physics.state.shipY - podCenterY) * Constants::WORLD_SCALE_Y;
    float distPx = std::sqrt(dxPx * dxPx + dyPx * dyPx);

    if (input.shield) {
      if (distPx <= 44.0f) {
        podConnecting = true;
      }
      if (podConnecting && distPx >= 39.0f) {
        physics.AttachPod(podCenterX, podCenterY);
        podConnecting = false;
        podClearedPedestal = false;
        Audio::PlayFuelPickup();
      }
    } else {
      podConnecting = false;
    }
  } else {
    podConnecting = false;
  }

  // 4. Fuel tank tractor beam refuel (each fuel pod contains 300 units of fuel)
  fuelBeamActive = false;
  if (input.shield && !physics.state.podAttached && !podConnecting) {
    for (auto &tank : fuelTanks) {
      if (!tank.alive)
        continue;
      float tankCenterX = tank.x + 2.0f;
      float tankCenterY = tank.y + 2.5f;
      float dx = std::abs(
          Constants::ShortestDeltaWorldX(physics.state.shipX, tankCenterX));
      float dy = tankCenterY - physics.state.shipY;

      if (dx < 8.0f && dy > 2.0f && dy < 28.0f) {
        fuelBeamActive = true;
        fuelBeamTarget = {tank.x, tank.y};
        if (tick60Steps > 0) {
          int transfer = std::min(tank.fuelRemaining, 12 * tick60Steps);
          tank.fuelRemaining -= transfer;
          fuel = std::min(9999, fuel + transfer);

          if (tank.fuelRemaining <= 0) {
            tank.alive = false;
            score += 300;
            Audio::PlayFuelPickup();
          }
        }
        break;
      }
    }
  }

  // 5. Fuel consumption (60Hz timebase; shield drain suppressed while siphoning
  // fuel)
  for (int s = 0; s < tick60Steps; ++s) {
    int tc = tickCount - s;
    if (input.thrust && (tc % 6 == 0)) {
      fuel = std::max(0, fuel - 1);
    }
    if (input.shield && !fuelBeamActive && (tc % 3 == 0)) {
      fuel = std::max(0, fuel - 1);
    }
  }

  // 6. Spawn vector exhaust streaks and embers when thrusting (60Hz emission
  // rate)
  if (input.thrust && fuel > 0 && tick60Steps > 0) {
    int angleIdx = physics.state.angle & 0x1F;
    float dirX = Constants::ANGLE_X[angleIdx];
    float dirY = Constants::ANGLE_Y[angleIdx];
    int perpIdx = (angleIdx + 8) & 0x1F;
    float perpX = Constants::ANGLE_X[perpIdx];
    float perpY = Constants::ANGLE_Y[perpIdx];

    // Emit from the nozzle exit in logical screen space (5.8 logical px behind
    // ship center), scaled by WORLD_SCALE_X (4.0) and WORLD_SCALE_Y (2.0) so
    // exhaust aligns at all 32 angles
    for (int i = 0; i < 3; ++i) {
      float lateralLog = ((std::rand() % 100) - 50) /
                         42.0f; // [-1.2, +1.2] logical px across nozzle
      float spreadVelLog = ((std::rand() % 100) - 50) / 85.0f;
      float jetSpeedLog =
          2.0f + (std::rand() % 180) / 100.0f; // [2.0, 3.8] logical px/tick

      float spawnLogX = -dirX * 5.8f + perpX * lateralLog;
      float spawnLogY = -dirY * 5.8f + perpY * lateralLog;
      float velLogX = -dirX * jetSpeedLog + perpX * spreadVelLog;
      float velLogY = -dirY * jetSpeedLog + perpY * spreadVelLog;

      Entities::Particle p;
      p.x = physics.state.shipX + spawnLogX / Constants::WORLD_SCALE_X;
      p.y = physics.state.shipY + spawnLogY / Constants::WORLD_SCALE_Y;
      p.vx = physics.state.vx + velLogX / Constants::WORLD_SCALE_X;
      p.vy = physics.state.vy + velLogY / Constants::WORLD_SCALE_Y;
      p.maxLifetime = static_cast<float>(6 + (std::rand() % 8));
      p.lifetime = p.maxLifetime;

      if (i == 0) {
        // High-velocity vector exhaust streak aligned with jet direction
        p.length = 2.2f + (std::rand() % 18) / 10.0f;
        p.angle = std::atan2(velLogY, velLogX);
        p.spin = 0.0f;
        p.color = ((std::rand() % 3) == 0) ? WHITE : YELLOW;
      } else {
        // Glowing afterburner ember
        p.size = 0.85f + (std::rand() % 50) / 100.0f;
        int cPick = std::rand() % 4;
        p.color = (cPick == 0)   ? WHITE
                  : (cPick == 1) ? YELLOW
                  : (cPick == 2) ? ORANGE
                                 : RED;
      }
      particles.push_back(p);
    }
  }

  // 6b. Player shooting (velocity reduced to 0.5x previous velocity)
  if (!input.shield) {
    if (input.fire) {
      if (!fireLatch) {
        fireLatch = true;
        Entities::Bullet b;
        int angleIdx = physics.state.angle & 0x1F;
        b.vx = (Constants::ANGLE_X[angleIdx] * 1.5f + physics.state.vx) * 0.5f;
        b.vy = (Constants::ANGLE_Y[angleIdx] * 1.5f + physics.state.vy) * 0.5f;
        b.x = physics.state.shipX + Constants::ANGLE_X[angleIdx] * 0.8f;
        b.y = physics.state.shipY + Constants::ANGLE_Y[angleIdx] * 0.8f;
        b.lifetime = static_cast<float>(Constants::PLAYER_BULLET_LIFETIME);
        b.isPlayerBullet = true;
        bullets.push_back(b);
        Audio::PlayLaser();
      }
    } else {
      fireLatch = false;
    }
  } else {
    fireLatch = true;
  }

  // 7. Update subsystems
  UpdateTurrets(dt);
  UpdatePowerPlant(dt);
  UpdateDoors(dt);
  UpdateBullets(dt);
  UpdateParticles(dt);
  CheckCollisions(dt);
  CheckOrbitEscape();
}

void Game::UpdateTurrets(float dt) {
  int shootProb = hostileGunProbability + planetDestroyedModifier;
  bool ceasefire =
      (powerPlant.rechargeCounter > 0) || (powerPlant.countdownTimer >= 0);

  int winW = std::max(1, GetScreenWidth());
  int winH = std::max(1, GetScreenHeight());
  float viewLogicalW = static_cast<float>(Constants::INTERNAL_HEIGHT) *
                       (static_cast<float>(winW) / static_cast<float>(winH));
  float viewLogicalH = static_cast<float>(Constants::INTERNAL_HEIGHT);

  float camY = physics.state.shipY * Constants::WORLD_SCALE_Y -
               viewLogicalH * 0.5f;

  for (auto &t : turrets) {
    if (!t.alive)
      continue;
    if (ceasefire) {
      t.aiming = false;
      continue;
    }

    Vector2 pivot = t.GetDomePivot();
    float sx = viewLogicalW * 0.5f +
               Constants::ShortestDeltaWorldX(physics.state.shipX, pivot.x) *
                   Constants::WORLD_SCALE_X;
    float sy = pivot.y * Constants::WORLD_SCALE_Y - camY;
    bool isVisible = (sx >= 0.0f && sx < viewLogicalW &&
                      sy >= 0.0f && sy < viewLogicalH);

    if (t.aiming) {
      // Rotate the gun barrel smoothly over 0.5 seconds to targetAngle before
      // firing
      t.rotateTimer += dt;
      float progress = std::min(1.0f, t.rotateTimer / 0.5f);

      float diff = t.targetAngle - t.startAngle;
      while (diff > 16.0f)
        diff -= 32.0f;
      while (diff < -16.0f)
        diff += 32.0f;
      t.currentAngle = std::fmod(t.startAngle + diff * progress + 32.0f, 32.0f);

      if (t.rotateTimer >= 0.5f) {
        t.currentAngle = t.targetAngle;
        t.aiming = false;

        if (isVisible) {
          int angle = static_cast<int>(std::round(t.targetAngle)) & 0x1F;
          Entities::Bullet b;
          // Spawn bullet right at the rotated gun barrel tip (6.8 logical px
          // = 1.36 * ANGLE)
          b.x = Constants::WrapWorldX(pivot.x +
                                      Constants::ANGLE_X[angle] * 1.36f);
          b.y = pivot.y + Constants::ANGLE_Y[angle] * 1.36f;
          // Velocity reduced to 0.5x previous velocity (0.4f instead of 0.8f)
          b.vx = Constants::ANGLE_X[angle] * 0.4f;
          b.vy = Constants::ANGLE_Y[angle] * 0.4f;
          b.lifetime = 90.0f;
          b.isPlayerBullet = false;
          bullets.push_back(b);
        }
      }
      continue;
    }

    if (!isVisible || tick60Steps == 0)
      continue;

    // Roll probability to start aiming a new shot (at 60Hz rate)
    if ((std::rand() % 256) < shootProb) {
      const int SPREAD_TABLE[4] = {0x01, 0x03, 0x07, 0x0F};
      int spreadMask = SPREAD_TABLE[t.gunParam & 0x03];
      int baseAngle = t.gunParam & 0x1C;
      int rawAngle =
          (baseAngle + (std::rand() & spreadMask) + (std::rand() & 0x03)) &
          0x1F;

      // Constrain angle within the turret's outward firing arc (+/- 5 angle
      // steps around normal) so the gun barrel never rotates into the rock wall
      // behind the turret
      int normalAngle =
          static_cast<int>(std::round(Entities::GetTurretDefaultAngle(t.dir))) &
          0x1F;
      int rel = ((rawAngle - normalAngle + 48) & 0x1F) - 16; // -16 .. +15
      if (rel < -5 || rel > 5) {
        rel = (std::rand() % 11) - 5;
      }
      int finalAngle = (normalAngle + rel + 32) & 0x1F;

      t.startAngle = t.currentAngle;
      t.targetAngle = static_cast<float>(finalAngle);
      t.rotateTimer = 0.0f;
      t.aiming = true;
    }
  }
}

void Game::UpdatePowerPlant(float dt) {
  for (int s = 0; s < tick60Steps; ++s) {
    int tc = tickCount - s;
    if (powerPlant.rechargeCounter > 0 && (tc % 2 == 0)) {
      powerPlant.rechargeCounter--;
    }

    if (powerPlant.alive && powerPlant.rechargeCounter == 0 &&
        powerPlant.countdownTimer < 0 && (tc % 8 == 0)) {
      const auto &def = LevelsData::GetLevels()[currentLevelIndex];
      float ppGroundY =
          LevelsData::GetGroundY(def, powerPlant.x + 2.5f, powerPlant.y + 7.0f);
      Entities::Particle p;
      p.x = powerPlant.x + 4.20f;
      p.y = ppGroundY - 9.20f;
      p.vx = 0.0f;
      p.vy = -0.5f;
      p.lifetime = 14.0f;
      p.color = WHITE;
      particles.push_back(p);
    }
  }

  // Countdown timer ticks strictly 1 second every real-time second
  if (powerPlant.countdownTimer >= 0) {
    powerPlant.countdownSecTimer -= dt;
    if (powerPlant.countdownSecTimer <= 0.0f) {
      powerPlant.countdownSecTimer += 1.0f;
      if (powerPlant.countdownTimer > 0) {
        powerPlant.countdownTimer--;
        Audio::PlayCountdownBeep();
      } else {
        OnPlayerDeath();
      }
    }
  }
}

void Game::UpdateDoors(float dt) {
  if (doorState.def.type == LevelsData::DoorType::NONE)
    return;

  if (doorState.openHoldTimer > 0.0f) {
    // Slide door open smoothly (0.4s transit), then hold open for 5.0 seconds
    if (doorState.openFraction < 1.0f) {
      doorState.openFraction =
          std::min(1.0f, doorState.openFraction + dt * 2.5f);
    } else {
      doorState.openHoldTimer = std::max(0.0f, doorState.openHoldTimer - dt);
    }
  } else {
    // Slide door closed smoothly after the 5.0-second hold expires
    if (doorState.openFraction > 0.0f) {
      doorState.openFraction =
          std::max(0.0f, doorState.openFraction - dt * 2.5f);
    }
  }
}

void Game::UpdateBullets(float dt) {
  float frameScale = std::clamp(dt, 0.0f, 0.05f) * 60.0f;
  for (size_t i = 0; i < bullets.size();) {
    bullets[i].x =
        Constants::WrapWorldX(bullets[i].x + bullets[i].vx * frameScale);
    bullets[i].y += bullets[i].vy * frameScale;
    bullets[i].lifetime -= frameScale;

    if (bullets[i].lifetime <= 0.0f) {
      bullets.erase(bullets.begin() + i);
    } else {
      ++i;
    }
  }
}

void Game::UpdateParticles(float dt) {
  float frameScale = std::clamp(dt, 0.0f, 0.05f) * 60.0f;
  for (size_t i = 0; i < particles.size();) {
    particles[i].x =
        Constants::WrapWorldX(particles[i].x + particles[i].vx * frameScale);
    particles[i].y += particles[i].vy * frameScale;
    particles[i].angle += particles[i].spin * frameScale;
    if (particles[i].isRing) {
      particles[i].size += 0.95f * frameScale;
    }
    particles[i].lifetime -= frameScale;

    if (particles[i].lifetime <= 0.0f) {
      particles.erase(particles.begin() + i);
    } else {
      ++i;
    }
  }
}

void Game::SpawnExplosion(float x, float y, int count) {
  float wx = Constants::WrapWorldX(x);

  // 1. Expanding vector shockwave ring
  {
    Entities::Particle ring;
    ring.x = wx;
    ring.y = y;
    ring.lifetime = 18.0f;
    ring.maxLifetime = 18.0f;
    ring.size = 2.0f;
    ring.isRing = true;
    ring.color = YELLOW;
    particles.push_back(ring);
  }

  // 2. Spinning glowing vector shrapnel struts
  int shrapnelCount = std::max(10, count);
  for (int i = 0; i < shrapnelCount; ++i) {
    Entities::Particle s;
    s.x = wx;
    s.y = y;
    float theta = (static_cast<float>(i) / shrapnelCount) * 2.0f * PI +
                  ((std::rand() % 100) - 50) * 0.004f;
    float speed = 0.28f + (std::rand() % 110) / 100.0f;
    s.vx = (std::sin(theta) * speed) / 2.0f;
    s.vy = -std::cos(theta) * speed;
    s.lifetime = static_cast<float>(28 + (std::rand() % 24));
    s.maxLifetime = s.lifetime;
    s.length = 3.2f + (std::rand() % 45) / 10.0f;
    s.angle = static_cast<float>(std::rand() % 628) / 100.0f;
    s.spin = (((std::rand() % 200) - 100) / 100.0f) * 0.35f;
    s.bounceOnWalls = true;
    if (i % 3 == 0)
      s.color = WHITE;
    else if (i % 3 == 1)
      s.color = YELLOW;
    else
      s.color = ORANGE;
    particles.push_back(s);
  }

  // 3. Dense multi-stage fiery spark & ember burst
  int sparkCount = count * 3;
  for (int i = 0; i < sparkCount; ++i) {
    Entities::Particle p;
    p.x = wx;
    p.y = y;
    float theta = (static_cast<float>(i) / sparkCount) * 2.0f * PI +
                  ((std::rand() % 100) - 50) * 0.006f;
    float speed = 0.15f + (std::rand() % 165) / 100.0f;
    p.vx = (std::sin(theta) * speed) / 2.0f;
    p.vy = -std::cos(theta) * speed;
    p.lifetime = static_cast<float>(16 + (std::rand() % 30));
    p.maxLifetime = p.lifetime;
    p.size = 1.1f + (std::rand() % 14) / 10.0f;
    p.bounceOnWalls = true;
    int colIdx = i % 4;
    if (colIdx == 0)
      p.color = WHITE;
    else if (colIdx == 1)
      p.color = YELLOW;
    else if (colIdx == 2)
      p.color = ORANGE;
    else
      p.color = RED;
    particles.push_back(p);
  }

  Audio::PlayExplosion();
}

void Game::SpawnSpark(float x, float y) {
  float wx = Constants::WrapWorldX(x);
  for (int i = 0; i < 3; ++i) {
    Entities::Particle p;
    p.x = wx;
    p.y = y;
    p.vx = ((std::rand() % 200) - 100) / 100.0f;
    p.vy = -((std::rand() % 100) / 100.0f);
    p.lifetime = 8.0f;
    p.color = YELLOW;
    particles.push_back(p);
  }
}

namespace {

inline float Cross2D(float ax, float ay, float bx, float by, float cx,
                     float cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

inline bool OnSegment2D(float ax, float ay, float bx, float by, float px,
                        float py) {
  return px >= std::min(ax, bx) - 1e-4f && px <= std::max(ax, bx) + 1e-4f &&
         py >= std::min(ay, by) - 1e-4f && py <= std::max(ay, by) + 1e-4f;
}

inline bool SegmentsIntersect2D(const LevelsData::Point &p1,
                                const LevelsData::Point &p2,
                                const LevelsData::Point &q1,
                                const LevelsData::Point &q2) {
  if (std::max(p1.x, p2.x) < std::min(q1.x, q2.x) ||
      std::min(p1.x, p2.x) > std::max(q1.x, q2.x) ||
      std::max(p1.y, p2.y) < std::min(q1.y, q2.y) ||
      std::min(p1.y, p2.y) > std::max(q1.y, q2.y)) {
    return false;
  }
  float d1 = Cross2D(q1.x, q1.y, q2.x, q2.y, p1.x, p1.y);
  float d2 = Cross2D(q1.x, q1.y, q2.x, q2.y, p2.x, p2.y);
  float d3 = Cross2D(p1.x, p1.y, p2.x, p2.y, q1.x, q1.y);
  float d4 = Cross2D(p1.x, p1.y, p2.x, p2.y, q2.x, q2.y);

  if (((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
      ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f))) {
    return true;
  }
  if (std::abs(d1) <= 1e-5f && OnSegment2D(q1.x, q1.y, q2.x, q2.y, p1.x, p1.y))
    return true;
  if (std::abs(d2) <= 1e-5f && OnSegment2D(q1.x, q1.y, q2.x, q2.y, p2.x, p2.y))
    return true;
  if (std::abs(d3) <= 1e-5f && OnSegment2D(p1.x, p1.y, p2.x, p2.y, q1.x, q1.y))
    return true;
  if (std::abs(d4) <= 1e-5f && OnSegment2D(p1.x, p1.y, p2.x, p2.y, q2.x, q2.y))
    return true;
  return false;
}

} // namespace

bool Game::TestPointInPolygon(float wx, float wy, const Poly &poly) const {
  if (poly.size() < 3)
    return false;
  bool inside = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    if (((poly[i].y > wy) != (poly[j].y > wy)) &&
        (wx <
         (poly[j].x - poly[i].x) * (wy - poly[i].y) / (poly[j].y - poly[i].y) +
             poly[i].x)) {
      inside = !inside;
    }
  }
  return inside;
}

bool Game::TestPointInPolygons(float wx, float wy,
                               const std::vector<Poly> &polys) const {
  wx = Constants::WrapWorldX(wx);
  for (const auto &poly : polys) {
    if (TestPointInPolygon(wx, wy, poly))
      return true;
  }
  return false;
}

bool Game::SegmentIntersectsPolygon(float x1, float y1, float x2, float y2,
                                    const Poly &poly) const {
  if (poly.size() < 2)
    return false;

  // Unwrap polygon vertices into the segment's local horizontal space around x1
  Poly localPoly;
  localPoly.reserve(poly.size());
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (const auto &p : poly) {
    float ux = x1 + Constants::ShortestDeltaWorldX(x1, p.x);
    localPoly.push_back({ux, p.y});
    minX = std::min(minX, ux);
    maxX = std::max(maxX, ux);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }

  float segX2 = x1 + Constants::ShortestDeltaWorldX(x1, x2);
  if (std::max(x1, segX2) < minX || std::min(x1, segX2) > maxX ||
      std::max(y1, y2) < minY || std::min(y1, y2) > maxY) {
    return false;
  }

  if (TestPointInPolygon(x1, y1, localPoly) ||
      TestPointInPolygon(segX2, y2, localPoly)) {
    return true;
  }

  LevelsData::Point s1 = {x1, y1};
  LevelsData::Point s2 = {segX2, y2};
  for (size_t i = 0; i < localPoly.size(); ++i) {
    if (SegmentsIntersect2D(s1, s2, localPoly[i],
                            localPoly[(i + 1) % localPoly.size()])) {
      return true;
    }
  }
  return false;
}

bool Game::SegmentIntersectsPolygons(float x1, float y1, float x2, float y2,
                                     const std::vector<Poly> &polys) const {
  for (const auto &poly : polys) {
    if (SegmentIntersectsPolygon(x1, y1, x2, y2, poly))
      return true;
  }
  return false;
}

bool Game::PolygonsIntersect(const Poly &polyA, const Poly &polyB,
                             bool unwrapBToA) const {
  if (polyA.size() < 3 || polyB.size() < 3)
    return false;

  float refX = polyA[0].x;
  float minAX = 1e9f, maxAX = -1e9f, minAY = 1e9f, maxAY = -1e9f;
  for (const auto &p : polyA) {
    minAX = std::min(minAX, p.x);
    maxAX = std::max(maxAX, p.x);
    minAY = std::min(minAY, p.y);
    maxAY = std::max(maxAY, p.y);
  }

  Poly localB;
  localB.reserve(polyB.size());
  float minBX = 1e9f, maxBX = -1e9f, minBY = 1e9f, maxBY = -1e9f;
  for (const auto &p : polyB) {
    float bx =
        unwrapBToA ? (refX + Constants::ShortestDeltaWorldX(refX, p.x)) : p.x;
    localB.push_back({bx, p.y});
    minBX = std::min(minBX, bx);
    maxBX = std::max(maxBX, bx);
    minBY = std::min(minBY, p.y);
    maxBY = std::max(maxBY, p.y);
  }

  if (maxAX < minBX || minAX > maxBX || maxAY < minBY || minAY > maxBY) {
    return false;
  }

  // 1. Exact edge-vs-edge intersection test
  for (size_t i = 0; i < polyA.size(); ++i) {
    const auto &a1 = polyA[i];
    const auto &a2 = polyA[(i + 1) % polyA.size()];
    for (size_t j = 0; j < localB.size(); ++j) {
      // Skip only the 3 artificial underground/outer-border closure edges on
      // terrain polygons
      if (!unwrapBToA && polyB.size() > 5 && j >= polyB.size() - 3) {
        continue;
      }

      const auto &b1 = localB[j];
      const auto &b2 = localB[(j + 1) % localB.size()];

      if (SegmentsIntersect2D(a1, a2, b1, b2)) {
        return true;
      }
    }
  }

  // 2. Vertex containment test across all vertices (in case one polygon or edge
  // penetrates inside the other)
  for (const auto &pa : polyA) {
    if (TestPointInPolygon(pa.x, pa.y, localB))
      return true;
  }
  for (const auto &pb : localB) {
    if (TestPointInPolygon(pb.x, pb.y, polyA))
      return true;
  }

  return false;
}

bool Game::PolygonIntersectsPolygons(const Poly &hull,
                                     const std::vector<Poly> &polys,
                                     bool isTerrainPolys) const {
  if (hull.size() < 3)
    return false;

  if (!isTerrainPolys) {
    for (const auto &poly : polys) {
      if (PolygonsIntersect(hull, poly, true))
        return true;
    }
    return false;
  }

  // For terrain/door polygons (which span [0..256]), wrap hull around [0..256)
  // and test across wrap seam
  float refX = hull[0].x;
  float wrappedRefX = Constants::WrapWorldX(refX);
  float shiftBase = wrappedRefX - refX;

  for (int wrap = -1; wrap <= 1; ++wrap) {
    if (wrap == -1 && wrappedRefX < 240.0f)
      continue;
    if (wrap == 1 && wrappedRefX > 16.0f)
      continue;

    Poly shiftedHull;
    shiftedHull.reserve(hull.size());
    for (const auto &p : hull) {
      shiftedHull.push_back(
          {p.x + shiftBase - wrap * Constants::WORLD_UNITS_X, p.y});
    }

    for (const auto &poly : polys) {
      if (PolygonsIntersect(shiftedHull, poly, false))
        return true;
    }
  }
  return false;
}

Game::Poly Game::GetShipCollisionPoly() const {
  // Exact outer vector silhouette of the ship from Renderer::DrawVectorShip
  static const Vector2 localVerts[] = {
      {0.0f, -8.4f}, // Needle nose tip
      {2.4f, -3.2f}, // Starboard forward chine
      {8.0f, 2.5f},  // Starboard outer wingtip
      {6.7f, 4.5f},  // Starboard rear wingtip
      {3.8f, 0.5f},  // Starboard inner wing root
      {2.1f, 1.1f},  // Engine thruster right top
      {1.6f, 4.6f},  // Engine nozzle right bottom
      {-1.6f, 4.6f}, // Engine nozzle left bottom
      {-2.1f, 1.1f}, // Engine thruster left top
      {-3.8f, 0.5f}, // Port inner wing root
      {-6.7f, 4.5f}, // Port rear wingtip
      {-8.0f, 2.5f}, // Port outer wingtip
      {-2.4f, -3.2f} // Port forward chine
  };

  float theta =
      (static_cast<float>(physics.state.angle & 0x1F) / 32.0f) * 2.0f * PI;
  float cosA = std::cos(theta);
  float sinA = std::sin(theta);

  Poly poly;
  poly.reserve(13);
  for (const auto &lv : localVerts) {
    float rx = lv.x * cosA - lv.y * sinA;
    float ry = lv.x * sinA + lv.y * cosA;
    poly.push_back({physics.state.shipX + rx / Constants::WORLD_SCALE_X,
                    physics.state.shipY + ry / Constants::WORLD_SCALE_Y});
  }
  return poly;
}

Game::Poly Game::GetPodCollisionPoly(float cx, float cy) const {
  // Exact 32-segment vector sphere outline matching Renderer::DrawVectorPodBall
  // (including outer vector stroke)
  const int segments = 32;
  const float rx = 5.75f / Constants::WORLD_SCALE_X; // 1.4375 world X
  const float ry = 5.75f / Constants::WORLD_SCALE_Y; // 2.8750 world Y

  Poly poly;
  poly.reserve(segments);
  for (int i = 0; i < segments; ++i) {
    float a = (static_cast<float>(i) / segments) * 2.0f * PI;
    poly.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
  }
  return poly;
}

std::vector<Game::Poly>
Game::GetTurretCollisionPolys(const Entities::Turret &turret) const {
  auto mapPt = [&](float dx, float dy) -> LevelsData::Point {
    switch (turret.dir) {
    case LevelsData::TurretDir::UP_RIGHT:
      return {turret.x + dx, turret.y + dy};
    case LevelsData::TurretDir::UP_LEFT:
      return {turret.x + (5.0f - dx), turret.y + dy};
    case LevelsData::TurretDir::DOWN_RIGHT:
      return {turret.x + (dx - 1.0f), turret.y + (6.0f - dy)};
    case LevelsData::TurretDir::DOWN_LEFT:
      return {turret.x + (6.0f - dx), turret.y + (6.0f - dy)};
    }
    return {turret.x + dx, turret.y + dy};
  };

  auto turretPt = [&](float s, float h) -> LevelsData::Point {
    return mapPt(2.0f + s + 0.5f * h, 4.0f + s - 2.0f * h);
  };

  // 1. Symmetrical armored bunker base polygon from Renderer::DrawVectorTurret
  Poly basePoly = {turretPt(-2.50f, 0.00f), turretPt(-2.05f, 0.65f),
                   turretPt(2.05f, 0.65f), turretPt(2.50f, 0.00f)};

  // 2. Symmetrical semicircular turret dome polygon from
  // Renderer::DrawVectorTurret
  Poly domePoly;
  domePoly.reserve(9);
  for (int i = 0; i <= 8; ++i) {
    float a = PI - (static_cast<float>(i) / 8.0f) * PI;
    domePoly.push_back(
        turretPt(std::cos(a) * 1.45f, 0.65f + std::sin(a) * 1.45f));
  }

  // 3. Exact rotating gun barrel polygon from Renderer::DrawVectorTurret
  Vector2 pivot = turret.GetDomePivot();
  float theta = (turret.currentAngle / 32.0f) * 2.0f * PI;
  float dirLX = std::sin(theta);
  float dirLY = -std::cos(theta);
  float perpLX = -dirLY;
  float perpLY = dirLX;

  auto barrelPt = [&](float alongPx, float acrossPx) -> LevelsData::Point {
    return {pivot.x + (dirLX * alongPx + perpLX * acrossPx) /
                          Constants::WORLD_SCALE_X,
            pivot.y + (dirLY * alongPx + perpLY * acrossPx) /
                          Constants::WORLD_SCALE_Y};
  };

  Poly barrelPoly = {barrelPt(0.5f, -1.5f), barrelPt(6.8f, -1.5f),
                     barrelPt(6.8f, 1.5f), barrelPt(0.5f, 1.5f)};

  return {basePoly, domePoly, barrelPoly};
}

std::vector<Game::Poly>
Game::GetPowerPlantCollisionPolys(const LevelsData::LevelDef &def) const {
  float groundY =
      LevelsData::GetGroundY(def, powerPlant.x + 2.5f, powerPlant.y + 7.0f);
  float x = powerPlant.x;
  float y = groundY - 8.00f;

  // 1. Main reactor building hull
  Poly hullPoly = {{x + 0.00f, y + 8.00f}, {x + 0.00f, y + 3.55f},
                   {x + 0.40f, y + 3.00f}, {x + 4.60f, y + 3.00f},
                   {x + 5.00f, y + 3.55f}, {x + 5.00f, y + 8.00f}};

  // 2. Flanged chimney stack
  Poly chimneyPoly = {{x + 3.80f, y + 3.00f}, {x + 3.80f, y - 0.80f},
                      {x + 3.65f, y - 1.10f}, {x + 4.75f, y - 1.10f},
                      {x + 4.60f, y - 0.80f}, {x + 4.60f, y + 3.00f}};

  // 3. Hemispherical yellow reactor dome
  Poly domePoly;
  float domeCx = x + 1.90f;
  float domeCy = y + 3.00f;
  for (int i = 0; i <= 10; ++i) {
    float a = PI - (static_cast<float>(i) / 10.0f) * PI;
    domePoly.push_back(
        {domeCx + std::cos(a) * 1.48f, domeCy - std::sin(a) * 2.10f});
  }

  return {hullPoly, chimneyPoly, domePoly};
}

std::vector<Game::Poly>
Game::GetFuelTankCollisionPolys(const Entities::FuelTank &tank,
                                const LevelsData::LevelDef &def) const {
  float groundY = LevelsData::GetGroundY(def, tank.x + 2.0f, tank.y + 5.0f);
  float x = tank.x;
  float y = groundY - 6.00f;

  // 1. Pressurized canister body + top valve cap
  Poly canisterPoly = {
      {x + 0.50f, y - 0.85f}, {x + 1.25f, y - 0.85f}, {x + 1.45f, y - 1.30f},
      {x + 2.55f, y - 1.30f}, {x + 2.75f, y - 0.85f}, {x + 3.50f, y - 0.85f},
      {x + 4.00f, y - 0.10f}, {x + 4.00f, y + 3.65f}, {x + 3.50f, y + 4.40f},
      {x + 0.50f, y + 4.40f}, {x + 0.00f, y + 3.65f}, {x + 0.00f, y - 0.10f}};

  // 2. Landing struts & foot pads
  Poly strutsPoly = {{x + 0.65f, y + 4.35f},
                     {x + 3.35f, y + 4.35f},
                     {x + 3.75f, y + 6.00f},
                     {x + 0.25f, y + 6.00f}};

  return {canisterPoly, strutsPoly};
}

Game::Poly Game::GetSwitchCollisionPoly(const Entities::Switch &sw) const {
  auto mapPt = [&](float dx, float dy) -> LevelsData::Point {
    if (sw.dir == LevelsData::SwitchDir::LEFT) {
      return {sw.x + dx, sw.y + dy};
    }
    return {sw.x + (2.0f - dx), sw.y + dy};
  };

  return {mapPt(2.00f, -0.50f), mapPt(0.55f, -0.50f), mapPt(0.00f, 0.60f),
          mapPt(0.00f, 5.40f),  mapPt(0.55f, 6.50f),  mapPt(2.00f, 6.50f)};
}

std::vector<Game::Poly>
Game::GetPedestalCollisionPolys(const LevelsData::LevelDef &def,
                                bool includePodBall) const {
  float groundY = LevelsData::GetGroundY(def, def.podPedestal.x + 1.375f,
                                         def.podPedestal.y + 8.0f);
  float x = def.podPedestal.x;

  // 1. Pedestal stand collision boundary (excludes the upper concave cradle so
  // the pod ball
  //    can lift out cleanly without clipping the cradle horns; covers the
  //    support tower + base feet)
  Poly standPoly = {{x + 0.90f, groundY - 2.95f}, {x + 1.85f, groundY - 2.95f},
                    {x + 2.00f, groundY - 1.05f}, {x + 2.75f, groundY},
                    {x + 0.00f, groundY},         {x + 0.75f, groundY - 1.05f}};

  if (!includePodBall) {
    return {standPoly};
  }
  return {standPoly, GetPodCollisionPoly(x + 1.375f, groundY - 5.75f)};
}

void Game::CheckCollisions(float dt) {
  const auto &def = LevelsData::GetLevels()[currentLevelIndex];
  float frameScale = std::clamp(dt, 0.0f, 0.05f) * 60.0f;

  // Build active polygon list including the dynamic sliding door polygon
  auto activePolys = def.polygons;
  auto doorPoly = doorState.GetPolygon();
  if (!doorPoly.empty()) {
    activePolys.push_back(doorPoly);
  }

  Poly attachedPodPoly;
  if (physics.state.podAttached) {
    attachedPodPoly =
        GetPodCollisionPoly(physics.state.podX, physics.state.podY);
  }

  // 1. Player Bullet Collisions against World Objects (pixel-perfect swept
  // segment vs object polygons)
  for (size_t bi = 0; bi < bullets.size(); ++bi) {
    if (bullets[bi].lifetime <= 0.0f || !bullets[bi].isPlayerBullet)
      continue;
    float bx = bullets[bi].x;
    float by = bullets[bi].y;
    float prevX = bx - bullets[bi].vx * frameScale;
    float prevY = by - bullets[bi].vy * frameScale;

    // Player bullet vs Carried Pod ("Ball"): shooting the carried pod triggers
    // player death
    if (physics.state.podAttached &&
        SegmentIntersectsPolygon(prevX, prevY, bx, by, attachedPodPoly)) {
      bullets[bi].lifetime = 0.0f;
      OnPlayerDeath();
      return;
    }

    // Bullets vs Turrets
    for (auto &turret : turrets) {
      if (!turret.alive)
        continue;
      if (SegmentIntersectsPolygons(prevX, prevY, bx, by,
                                    GetTurretCollisionPolys(turret))) {
        turret.alive = false;
        bullets[bi].lifetime = 0.0f;
        Vector2 pivot = turret.GetDomePivot();
        SpawnExplosion(pivot.x, pivot.y, 24);
        score += 750;
        break;
      }
    }
    if (bullets[bi].lifetime <= 0.0f)
      continue;

    // Bullets vs Power Plant
    if (powerPlant.alive &&
        SegmentIntersectsPolygons(prevX, prevY, bx, by,
                                  GetPowerPlantCollisionPolys(def))) {
      bullets[bi].lifetime = 0.0f;
      SpawnSpark(bx, by);
      int newRecharge = (std::rand() % 32) + powerPlant.rechargeIncrease;
      powerPlant.rechargeIncrease = newRecharge;

      if (newRecharge > 255) {
        if (powerPlant.countdownTimer < 0) {
          powerPlant.countdownTimer = (currentLevelIndex == 5) ? 60 : 10;
          powerPlant.countdownTicks = 32;
          powerPlant.countdownSecTimer = 1.0f;
          powerPlant.rechargeCounter = 255;
          Audio::PlayCountdownBeep();
        }
      } else {
        powerPlant.rechargeCounter = newRecharge;
      }
      continue;
    }

    // Bullets vs Fuel Tanks
    for (auto &tank : fuelTanks) {
      if (!tank.alive)
        continue;
      if (SegmentIntersectsPolygons(prevX, prevY, bx, by,
                                    GetFuelTankCollisionPolys(tank, def))) {
        tank.alive = false;
        bullets[bi].lifetime = 0.0f;
        SpawnExplosion(tank.x + 2.0f, tank.y + 2.0f, 16);
        score += 150;
        break;
      }
    }
    if (bullets[bi].lifetime <= 0.0f)
      continue;

    // Bullets vs Door Switches (opens sliding door for 5.0 seconds)
    for (auto &sw : switches) {
      if (!sw.alive)
        continue;
      if (SegmentIntersectsPolygon(prevX, prevY, bx, by,
                                   GetSwitchCollisionPoly(sw))) {
        bullets[bi].lifetime = 0.0f;
        SpawnSpark(bx, by);
        doorState.openHoldTimer = 5.0f;
        Audio::PlayFuelPickup();
        break;
      }
    }
  }

  // 2. All Bullets (Player & Turret) vs Terrain & Sliding Doors (sub-stepped to
  // prevent tunneling)
  for (size_t bi = 0; bi < bullets.size(); ++bi) {
    if (bullets[bi].lifetime <= 0.0f)
      continue;
    float bx = bullets[bi].x;
    float by = bullets[bi].y;
    float stepX = bullets[bi].vx * frameScale;
    float stepY = bullets[bi].vy * frameScale;
    float prevX = bx - stepX;
    float prevY = by - stepY;

    for (int step = 1; step <= 6; ++step) {
      float t = static_cast<float>(step) / 6.0f;
      float sx = prevX + stepX * t;
      float sy = prevY + stepY * t;
      if (TestPointInPolygons(sx, sy, activePolys)) {
        bullets[bi].lifetime = 0.0f;
        SpawnSpark(prevX + stepX * ((step - 1) / 6.0f),
                   prevY + stepY * ((step - 1) / 6.0f));
        break;
      }
    }
  }

  // Build exact ship collision polygon once for bullet and obstacle checks
  Poly shipPoly = GetShipCollisionPoly();

  // 3. Hostile Bullet Collisions against Carried Pod, Ship Hull, or Shield
  for (size_t bi = 0; bi < bullets.size(); ++bi) {
    if (bullets[bi].lifetime <= 0.0f || bullets[bi].isPlayerBullet)
      continue;
    float bx = bullets[bi].x;
    float by = bullets[bi].y;
    float prevX = bx - bullets[bi].vx * frameScale;
    float prevY = by - bullets[bi].vy * frameScale;

    // Enemy bullet vs Carried Pod ("Ball"): shooting the carried pod triggers
    // player death
    if (physics.state.podAttached &&
        SegmentIntersectsPolygon(prevX, prevY, bx, by, attachedPodPoly)) {
      bullets[bi].lifetime = 0.0f;
      OnPlayerDeath();
      return;
    }

    if (shieldActive) {
      // Shield is a circle of radius 10.6 logical pixels around the ship
      float dxPx = Constants::ShortestDeltaWorldX(physics.state.shipX, bx) *
                   Constants::WORLD_SCALE_X;
      float dyPx = (by - physics.state.shipY) * Constants::WORLD_SCALE_Y;
      if (dxPx * dxPx + dyPx * dyPx <= 10.6f * 10.6f) {
        bullets[bi].lifetime = 0.0f;
        SpawnSpark(bx, by);
      }
    } else if (SegmentIntersectsPolygon(prevX, prevY, bx, by, shipPoly)) {
      bullets[bi].lifetime = 0.0f;
      OnPlayerDeath();
      return;
    }
  }

  // Remove destroyed bullets immediately so they never render inside walls
  bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                               [](const Entities::Bullet &b) {
                                 return b.lifetime <= 0.0f;
                               }),
                bullets.end());

  // Bounce explosion shrapnel/sparks off cave walls; remove exhaust particles
  // that enter terrain
  for (auto &p : particles) {
    if (p.isRing)
      continue;
    if (TestPointInPolygons(p.x, p.y, activePolys)) {
      if (p.bounceOnWalls) {
        p.x = Constants::WrapWorldX(p.x - p.vx * frameScale * 1.5f);
        p.y -= p.vy * frameScale * 1.5f;
        p.vx = -p.vx * 0.65f;
        p.vy = -p.vy * 0.65f;
      } else {
        p.lifetime = 0.0f;
      }
    }
  }
  particles.erase(std::remove_if(particles.begin(), particles.end(),
                                 [](const Entities::Particle &p) {
                                   return p.lifetime <= 0.0f;
                                 }),
                  particles.end());

  // 4. Pixel-Perfect Ship & Attached Pod ("Ball") vs Cave Walls, Doors, and All
  // Level Objects Collect all active level object polygons (turrets, power
  // plant, fuel tanks, switches)
  std::vector<Poly> objectPolys;
  for (const auto &turret : turrets) {
    if (!turret.alive)
      continue;
    auto tPolys = GetTurretCollisionPolys(turret);
    objectPolys.insert(objectPolys.end(), tPolys.begin(), tPolys.end());
  }
  if (powerPlant.alive) {
    auto ppPolys = GetPowerPlantCollisionPolys(def);
    objectPolys.insert(objectPolys.end(), ppPolys.begin(), ppPolys.end());
  }
  for (const auto &tank : fuelTanks) {
    if (!tank.alive)
      continue;
    auto fPolys = GetFuelTankCollisionPolys(tank, def);
    objectPolys.insert(objectPolys.end(), fPolys.begin(), fPolys.end());
  }
  for (const auto &sw : switches) {
    if (!sw.alive)
      continue;
    objectPolys.push_back(GetSwitchCollisionPoly(sw));
  }

  // 4a. Ship vs Cave Walls, Sliding Doors, World Objects, and Pedestal (+
  // unattached Pod Ball)
  auto shipPedestalPolys =
      GetPedestalCollisionPolys(def, !physics.state.podAttached);
  if (PolygonIntersectsPolygons(shipPoly, activePolys, true) ||
      PolygonIntersectsPolygons(shipPoly, objectPolys, false) ||
      PolygonIntersectsPolygons(shipPoly, shipPedestalPolys, false)) {
    OnPlayerDeath();
    return;
  }

  // 4b. Attached Pod ("Ball") vs Cave Walls, Sliding Doors, World Objects, and
  // Pedestal Stand
  if (physics.state.podAttached) {
    Poly podPoly = GetPodCollisionPoly(physics.state.podX, physics.state.podY);
    auto standOnlyPolys = GetPedestalCollisionPolys(def, false);

    if (!podClearedPedestal) {
      if (!PolygonIntersectsPolygons(podPoly, standOnlyPolys, false)) {
        podClearedPedestal = true;
      }
    }

    if (PolygonIntersectsPolygons(podPoly, activePolys, true) ||
        PolygonIntersectsPolygons(podPoly, objectPolys, false) ||
        (podClearedPedestal &&
         PolygonIntersectsPolygons(podPoly, standOnlyPolys, false))) {
      OnPlayerDeath();
      return;
    }
  }
}

void Game::OnPlayerDeath() {
  Audio::SetEngineThrust(false);
  Audio::SetShieldSound(false);
  thrustActive = false;
  shieldActive = false;
  podConnecting = false;
  fuelBeamActive = false;
  SpawnExplosion(physics.state.shipX, physics.state.shipY, 16);
  if (physics.state.podAttached) {
    SpawnExplosion(physics.state.podX, physics.state.podY, 12);
  }
  state = GameState::PLAYER_DYING;
  stateTimer = 1.5f;
}

void Game::CheckOrbitEscape() {
  if (physics.state.shipY < 288.0f) {
    Audio::PlayTeleport();
    Audio::SetEngineThrust(false);
    Audio::SetShieldSound(false);
    thrustActive = false;
    shieldActive = false;
    podConnecting = false;
    fuelBeamActive = false;
    overlayMessage = ""; // Bonus/status popup appears only after ship/pod end
                         // animation finishes

    if (physics.state.podAttached) {
      int bonus = (powerPlant.countdownTimer >= 0)
                      ? (4000 + 100 * powerPlant.countdownTimer)
                      : 3000;
      pendingBonus = bonus;
      pendingOverlayMessage = TextFormat("BONUS +%d", bonus);
      state = GameState::MISSION_COMPLETE;
      stateTimer =
          2.7f; // First 1.2s: ship/pod end animation. Next 1.5s: Bonus popup.
    } else if (powerPlant.countdownTimer >= 0) {
      pendingBonus = 0;
      pendingOverlayMessage = "PLANET DESTROYED";
      state = GameState::PLANET_DESTROYED;
      stateTimer = 2.7f;
    } else {
      lives--;
      pendingBonus = 0;
      pendingOverlayMessage = "MISSION INCOMPLETE";
      state = GameState::MISSION_INCOMPLETE;
      stateTimer = 2.7f;
    }
  }
}

void Game::Draw(int windowWidth, int windowHeight) {
  if (state == GameState::TITLE) {
    renderer.RenderTitleScreen(windowWidth, windowHeight, highScores,
                               tickCount);
    return;
  }

  if (state == GameState::ENTER_HIGH_SCORE || state == GameState::HIGH_SCORES) {
    renderer.RenderHighScoreScreen(
        windowWidth, windowHeight, score, state == GameState::ENTER_HIGH_SCORE,
        playerNameInput, lastHighScoreRank, highScores, tickCount);
    return;
  }

  const auto &def = LevelsData::GetLevels()[currentLevelIndex];

  bool shipVisible = (state != GameState::PLAYER_DYING);
  bool teleportAnimActive = false;
  float teleportProgress = 1.0f;

  if (state == GameState::RESPAWNING) {
    if (stateTimer > 1.2f) {
      // Level/Ready popup is open: ship has not materialized yet
      shipVisible = false;
      teleportAnimActive = false;
    } else {
      // Level/Ready popup has closed: run ship start animation
      shipVisible = true;
      teleportAnimActive = true;
      teleportProgress = std::clamp(1.0f - (stateTimer / 1.2f), 0.0f, 1.0f);
    }
  } else if (state == GameState::MISSION_COMPLETE ||
             state == GameState::PLANET_DESTROYED ||
             state == GameState::MISSION_INCOMPLETE) {
    if (stateTimer > 1.5f) {
      // Before Bonus/status popup appears: run ship & pod end animation
      shipVisible = true;
      teleportAnimActive = true;
      teleportProgress = std::clamp((stateTimer - 1.5f) / 1.2f, 0.0f, 1.0f);
    } else {
      // End animation finished, Bonus/status popup is now open: ship & pod have
      // vanished
      shipVisible = false;
      teleportAnimActive = false;
    }
  }

  renderer.BeginFrame(windowWidth, windowHeight);
  renderer.RenderGame(physics, def, turrets, powerPlant, fuelTanks, switches,
                      doorState, bullets, particles, stars, thrustActive,
                      shieldActive, podConnecting, fuelBeamActive,
                      fuelBeamTarget, tickCount, score, lives, fuel,
                      paused ? "PAUSED" : overlayMessage, shipVisible,
                      teleportAnimActive, teleportProgress);
  renderer.EndFrame();
}
