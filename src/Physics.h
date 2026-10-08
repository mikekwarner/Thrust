#pragma once

#include "Constants.h"
#include <cmath>

struct ThrustInput {
    int rotate = 0;   // -1 = anticlockwise, 0 = none, +1 = clockwise
    bool thrust = false;
    bool shield = false;
    bool fire = false;
};

struct PodState {
    float x = 0.0f;
    float y = 0.0f;
    int angleShipToPod = 0;    // 0..31
    float angleFrac = 0.0f;    // 0..255 sub-step fractional accumulator
    float angularVelocity = 0.0f;
    int tetherIndex = 14;      // top_nibble_index (starts at 14)
};

struct ThrustState {
    float x = 0.0f;            // Midpoint / physics position
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    int angle = 0;             // Ship angle 0..31
    float forceX = 0.0f;
    float forceY = 0.0f;

    bool podAttached = false;
    PodState pod;

    float shipX = 0.0f;
    float shipY = 0.0f;
    float podX = 0.0f;
    float podY = 0.0f;

    int level = 0;             // 0..5
    bool reverseGravity = false;
};

class ThrustPhysics {
public:
    ThrustState state;

    ThrustPhysics();

    void Update(float dtSeconds, const ThrustInput& input);
    void AttachPod(float podWorldX, float podWorldY);
    void DetachPod();
    void ResetMotion();
    void SetLevel(int level);
    void DerivePositions();

    struct TetherDelta { float dx; float dy; };
    TetherDelta CalculateTetherDelta() const;

private:
    float rotationTimer = 0.0f;

    void ApplyThrustTorque(int angleIdx, float torqueTicks);
    void IntegrateAngularVelocity(float ticks);

    float GetGravity() const;
    int GetMassShift() const;
};
