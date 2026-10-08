#pragma once

#include "raylib.h"

namespace Audio {

void Init();
void Close();

void PlayLaser();
void PlayExplosion();
void PlayFuelPickup();
void PlayCountdownBeep();
void PlayTeleport();
void SetEngineThrust(bool active);
void SetShieldSound(bool active);

void Update();

} // namespace Audio
