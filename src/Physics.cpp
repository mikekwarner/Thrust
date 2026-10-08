#include "Physics.h"
#include <algorithm>

ThrustPhysics::ThrustPhysics() {
    state.pod.tetherIndex = 14;
}

float ThrustPhysics::GetGravity() const {
    int idx = std::clamp(state.level, 0, 5);
    float grav = Constants::LEVEL_GRAVITY[idx];
    if (state.reverseGravity) {
        return -grav;
    }
    return grav;
}

int ThrustPhysics::GetMassShift() const {
    return state.podAttached ? 5 : 4;
}

void ThrustPhysics::Update(float dtSeconds, const ThrustInput& input) {
    float dt = std::clamp(dtSeconds, 0.0f, 0.05f);
    float ticks = dt / Constants::TICK_TIME;

    // Rotation: 3 out of every 4 ticks (25 steps/sec = 0.04s per angle step)
    const float ROT_STEP_TIME = Constants::TICK_TIME / 0.75f;
    if (input.rotate != 0) {
        rotationTimer += dt;
        while (rotationTimer >= ROT_STEP_TIME) {
            rotationTimer -= ROT_STEP_TIME;
            state.angle = (state.angle + input.rotate + 32) % 32;
        }
    } else {
        // Prime timer so next tap rotates immediately
        rotationTimer = ROT_STEP_TIME;
    }

    // Active force slots: 6 out of every 16 ticks; torque slots: 4 out of every 16 ticks
    float activeTicks = ticks * (6.0f / 16.0f);
    float torqueTicks = ticks * (4.0f / 16.0f);
    int angleIdx = state.angle & 0x1F;

    // Gravity
    state.forceY += GetGravity() * activeTicks;

    // Thrust
    if (input.thrust) {
        float massDiv = static_cast<float>(1 << GetMassShift());
        float thrustY = (Constants::ANGLE_Y[angleIdx] / massDiv) * activeTicks;
        float thrustX = (Constants::ANGLE_X[angleIdx] / massDiv) * activeTicks;
        state.forceY += thrustY;
        state.forceX += thrustX;
    }

    // Torque (when pod attached and thrusting)
    if (state.podAttached && input.thrust) {
        ApplyThrustTorque(angleIdx, torqueTicks);
    }

    // Angular damping
    if (state.podAttached) {
        state.pod.angularVelocity *= std::pow(Constants::DRAG_ANGULAR, activeTicks);
    }

    // Linear drag
    state.forceX *= std::pow(Constants::DRAG_X, activeTicks);
    state.forceY *= std::pow(Constants::DRAG_Y, activeTicks);

    // Position integration with horizontal map wrap-around (0..256)
    state.vx = state.forceX;
    state.vy = state.forceY;
    state.x = Constants::WrapWorldX(state.x + state.forceX * ticks);
    state.y += state.forceY * ticks;

    if (state.podAttached) {
        IntegrateAngularVelocity(ticks);
    }

    DerivePositions();
}

void ThrustPhysics::ApplyThrustTorque(int angleIdx, float torqueTicks) {
    float podExactAngle = static_cast<float>(state.pod.angleShipToPod) + state.pod.angleFrac / 256.0f;
    float diffRad = ((static_cast<float>(angleIdx) - podExactAngle) / 32.0f) * 2.0f * 3.1415926535f;
    float tangentialForce = (std::sin(diffRad) * 1.25f) * 8.0f;
    state.pod.angularVelocity += (tangentialForce / 2.0f) * torqueTicks;
}

void ThrustPhysics::IntegrateAngularVelocity(float ticks) {
    auto& pod = state.pod;
    pod.angleFrac += pod.angularVelocity * ticks;

    while (pod.angleFrac >= 256.0f) {
        pod.angleFrac -= 256.0f;
        pod.angleShipToPod = (pod.angleShipToPod + 1) & 0x1F;
    }
    while (pod.angleFrac < 0.0f) {
        pod.angleFrac += 256.0f;
        pod.angleShipToPod = (pod.angleShipToPod - 1 + 32) & 0x1F;
    }
}

ThrustPhysics::TetherDelta ThrustPhysics::CalculateTetherDelta() const {
    const auto& pod = state.pod;
    float exactAngle32 = static_cast<float>(pod.angleShipToPod) + pod.angleFrac / 256.0f;
    float theta = (exactAngle32 / 32.0f) * 2.0f * 3.1415926535f;
    return { std::sin(theta) * 5.0f, -std::cos(theta) * 10.0f };
}

void ThrustPhysics::DerivePositions() {
    if (!state.podAttached) {
        state.shipX = state.x;
        state.shipY = state.y;
        state.podX = state.x;
        state.podY = state.y;
        return;
    }

    TetherDelta delta = CalculateTetherDelta();
    state.shipX = state.x + delta.dx;
    state.shipY = state.y + delta.dy;
    state.podX = state.x - delta.dx;
    state.podY = state.y - delta.dy;
}

void ThrustPhysics::AttachPod(float podWorldX, float podWorldY) {
    float dx = Constants::ShortestDeltaWorldX(podWorldX, state.shipX);
    float dy = state.shipY - podWorldY;

    // Compute exact continuous tether angle from pod to ship so neither ship nor camera jumps
    float theta = std::atan2(dx * 2.0f, -dy);
    if (theta < 0.0f) {
        theta += 2.0f * 3.1415926535f;
    }

    float exactAngle32 = (theta / (2.0f * 3.1415926535f)) * 32.0f;
    int intAngle = static_cast<int>(std::floor(exactAngle32)) & 0x1F;
    float frac = (exactAngle32 - std::floor(exactAngle32)) * 256.0f;

    state.pod.tetherIndex = 14;
    state.podAttached = true;
    state.pod.angleShipToPod = intAngle;
    state.pod.angleFrac = frac;

    TetherDelta delta = CalculateTetherDelta();
    state.x = Constants::WrapWorldX(state.shipX - delta.dx);
    state.y = state.shipY - delta.dy;

    state.forceX /= 2.0f;
    state.forceY /= 2.0f;
    state.pod.angularVelocity = 0.0f;

    DerivePositions();
}

void ThrustPhysics::DetachPod() {
    if (!state.podAttached) return;
    state.x = Constants::WrapWorldX(state.shipX);
    state.y = state.shipY;
    state.podAttached = false;
    state.pod.angularVelocity = 0.0f;
    state.pod.angleFrac = 0.0f;
    DerivePositions();
}

void ThrustPhysics::ResetMotion() {
    state.vx = 0.0f;
    state.vy = 0.0f;
    state.forceX = 0.0f;
    state.forceY = 0.0f;
    state.pod.angularVelocity = 0.0f;
    state.pod.angleFrac = 0.0f;
    rotationTimer = Constants::TICK_TIME / 0.75f;
}

void ThrustPhysics::SetLevel(int level) {
    state.level = std::clamp(level, 0, 5);
}
