#include "Renderer.h"
#include <algorithm>
#include <cmath>

Renderer::Renderer() {}

Renderer::~Renderer() {
    Close();
}

void Renderer::Init() {}

void Renderer::Close() {}

void Renderer::BeginFrame(int windowWidth, int windowHeight) {
    winW = std::max(1, windowWidth);
    winH = std::max(1, windowHeight);

    // Keep graphics aspect ratio 100% constant (uniform isotropic scale) while
    // showing more or less horizontal gameplay area depending on the window aspect ratio
    scale = static_cast<float>(winH) / Constants::INTERNAL_HEIGHT;
    viewLogicalW = static_cast<float>(winW) / scale;
    viewLogicalH = static_cast<float>(Constants::INTERNAL_HEIGHT);

    // UI scale ensures the 320x256 HUD and menu screens always fit inside the window
    uiScale = std::min(
        static_cast<float>(winW) / Constants::INTERNAL_WIDTH,
        static_cast<float>(winH) / Constants::INTERNAL_HEIGHT
    );
    uiOffsetX = (winW - Constants::INTERNAL_WIDTH * uiScale) * 0.5f;
    uiOffsetY = (winH - Constants::INTERNAL_HEIGHT * uiScale) * 0.5f;

    // Crisp vector stroke thickness scaled cleanly to window resolution
    lineThick = std::clamp(scale * 0.82f, 1.5f, 4.2f);

    BeginDrawing();
    ClearBackground(BLACK);
    BeginScissorMode(0, 0, winW, winH);
}

void Renderer::EndFrame() {
    EndScissorMode();
    EndDrawing();
}

Vector2 Renderer::WorldToLogical(float wx, float wy, float camX, float camY) const {
    return {
        wx * Constants::WORLD_SCALE_X - camX,
        wy * Constants::WORLD_SCALE_Y - camY
    };
}

Vector2 Renderer::LogicalToWindow(float lx, float ly) const {
    return {
        lx * scale,
        ly * scale
    };
}

Vector2 Renderer::UIToWindow(float ux, float uy) const {
    return {
        uiOffsetX + ux * uiScale,
        uiOffsetY + uy * uiScale
    };
}

Vector2 Renderer::WorldToWindow(float wx, float wy, float camX, float camY) const {
    Vector2 l = WorldToLogical(wx, wy, camX, camY);
    return LogicalToWindow(l.x, l.y);
}

void Renderer::DrawVecLineWin(Vector2 a, Vector2 b, Color color, float thickScale, bool glow) const {
    float t = std::max(1.0f, lineThick * thickScale);

    // Subtle outer vector phosphor glow pass
    if (glow) {
        float gt = t * 2.3f;
        Color gCol = Fade(color, 0.20f);
        DrawLineEx(a, b, gt, gCol);
    }

    // Crisp core stroke
    DrawLineEx(a, b, t, color);
    if (t >= 1.5f) {
        DrawCircleV(a, t * 0.48f, color);
        DrawCircleV(b, t * 0.48f, color);
    }
}

void Renderer::DrawVecPolyWin(const std::vector<Vector2>& pts, bool closed, Color color, float thickScale, bool glow) const {
    if (pts.size() < 2) return;
    float t = std::max(1.0f, lineThick * thickScale);
    size_t count = closed ? pts.size() : pts.size() - 1;

    // Outer vector phosphor glow pass
    if (glow) {
        float gt = t * 2.3f;
        Color gCol = Fade(color, 0.20f);
        for (size_t i = 0; i < count; ++i) {
            DrawLineEx(pts[i], pts[(i + 1) % pts.size()], gt, gCol);
        }
    }

    // Crisp core stroke
    for (size_t i = 0; i < count; ++i) {
        DrawLineEx(pts[i], pts[(i + 1) % pts.size()], t, color);
    }
    if (t >= 1.5f) {
        for (const auto& p : pts) {
            DrawCircleV(p, t * 0.48f, color);
        }
    }
}

void Renderer::DrawVecLineWorld(float wx1, float wy1, float wx2, float wy2, float camX, float camY, Color color, float thickScale, bool glow) const {
    DrawVecLineWin(WorldToWindow(wx1, wy1, camX, camY), WorldToWindow(wx2, wy2, camX, camY), color, thickScale, glow);
}

void Renderer::DrawVecPolyWorld(const std::vector<Vector2>& worldPts, bool closed, float camX, float camY, Color color, float thickScale, bool glow) const {
    std::vector<Vector2> winPts;
    winPts.reserve(worldPts.size());
    for (const auto& wp : worldPts) {
        winPts.push_back(WorldToWindow(wp.x, wp.y, camX, camY));
    }
    DrawVecPolyWin(winPts, closed, color, thickScale, glow);
}

void Renderer::DrawVectorTerrain(const std::vector<LevelsData::Point>& pts, Color color, float camX, float camY, bool isDoor) const {
    if (pts.size() < 3) return;

    std::vector<Vector2> winPts;
    winPts.reserve(pts.size());
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;

    for (const auto& p : pts) {
        Vector2 wp = WorldToWindow(p.x, p.y, camX, camY);
        winPts.push_back(wp);
        minX = std::min(minX, wp.x);
        maxX = std::max(maxX, wp.x);
        minY = std::min(minY, wp.y);
        maxY = std::max(maxY, wp.y);
    }

    int viewTop = 0;
    int viewBot = winH;
    int viewLeft = 0;
    int viewRight = winW;

    // Skip off-screen wrapped copies
    if (maxX < viewLeft - 16.0f || minX > viewRight + 16.0f ||
        maxY < viewTop - 16.0f  || minY > viewBot + 16.0f) {
        return;
    }

    // 1. Solid horizontal span fill across every window pixel row
    int startY = std::max(viewTop, static_cast<int>(std::floor(minY)));
    int endY = std::min(viewBot, static_cast<int>(std::ceil(maxY)));

    // Slightly deeper solid interior so the glowing vector surface outline has crisp contrast
    Color solidFill = ColorBrightness(color, -0.16f);
    solidFill.a = 255;

    std::vector<float> intersections;
    intersections.reserve(16);

    for (int py = startY; py <= endY; ++py) {
        float sampleY = static_cast<float>(py) + 0.5f;
        intersections.clear();

        for (size_t i = 0; i < winPts.size(); ++i) {
            Vector2 a = winPts[i];
            Vector2 b = winPts[(i + 1) % winPts.size()];

            if ((a.y <= sampleY && b.y > sampleY) || (b.y <= sampleY && a.y > sampleY)) {
                float t = (sampleY - a.y) / (b.y - a.y);
                intersections.push_back(a.x + t * (b.x - a.x));
            }
        }

        std::sort(intersections.begin(), intersections.end());

        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x1 = std::max(viewLeft, static_cast<int>(std::floor(intersections[i])));
            int x2 = std::min(viewRight, static_cast<int>(std::ceil(intersections[i + 1])));
            if (x2 >= x1) {
                DrawRectangle(x1, py, x2 - x1 + 1, 1, solidFill);
            }
        }
    }

    // 2. Crisp glowing vector boundary outline along all cave surface edges
    size_t surfaceEdgeCount = (!isDoor && pts.size() > 3) ? (pts.size() - 3) : pts.size();
    for (size_t i = 0; i < surfaceEdgeCount; ++i) {
        Vector2 wa = winPts[i];
        Vector2 wb = winPts[(i + 1) % pts.size()];

        if ((wa.y < viewTop - 16.0f && wb.y < viewTop - 16.0f) ||
            (wa.y > viewBot + 16.0f && wb.y > viewBot + 16.0f) ||
            (wa.x < viewLeft - 16.0f && wb.x < viewLeft - 16.0f) ||
            (wa.x > viewRight + 16.0f && wb.x > viewRight + 16.0f)) {
            continue;
        }

        DrawVecLineWin(wa, wb, color, 1.15f, true);
    }
}

void Renderer::DrawVectorDoor(const Entities::DoorState& doorState, Color color, float camX, float camY) const {
    auto doorPoly = doorState.GetPolygon();
    if (doorPoly.empty()) return;

    // 1. Draw solid door body with glowing vector border
    DrawVectorTerrain(doorPoly, color, camX, camY, true);

    // 2. Add internal vector blast-door reinforcement ribs so it clearly reads as a sliding mechanical door
    float innerX = static_cast<float>(doorState.def.innerX);
    float topY = static_cast<float>(doorState.def.worldY);
    float botY = static_cast<float>(doorState.def.worldY + doorState.def.scanlines);
    float midY = (topY + botY) * 0.5f;

    if (doorState.def.type != LevelsData::DoorType::CHEVRON) {
        float doorRightX = doorState.def.closedX - doorState.openFraction * (doorState.def.closedX - innerX);
        if (doorRightX > innerX + 1.5f) {
            Color ribCol = ColorBrightness(color, 0.35f);
            DrawVecLineWorld(innerX + 0.5f, midY, doorRightX - 0.5f, midY, camX, camY, ribCol, 0.75f, false);
            for (float rx = doorRightX - 3.0f; rx > innerX + 1.5f; rx -= 4.0f) {
                DrawVecLineWorld(rx, topY + 1.5f, rx, botY - 1.5f, camX, camY, ribCol, 0.7f, false);
            }
        }
    }
}

void Renderer::DrawVectorPodBall(float wx, float wy, Color podColor, float camX, float camY) const {
    Vector2 center = WorldToWindow(wx, wy, camX, camY);
    float radius = 5.5f * scale;

    // 1. Subtle interior radial fill so the pod sphere looks substantial
    DrawCircleV(center, radius * 0.92f, Fade(podColor, 0.16f));

    // 2. High-precision 32-segment outer vector sphere ring
    int segments = 32;
    std::vector<Vector2> circlePts;
    circlePts.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        float a = (static_cast<float>(i) / segments) * 2.0f * PI;
        circlePts.push_back({ center.x + std::cos(a) * radius, center.y + std::sin(a) * radius });
    }
    DrawVecPolyWin(circlePts, true, podColor, 1.05f, true);

    // 3. Curved equatorial & meridian geodesic arcs for a true 3D vector sphere look
    std::vector<Vector2> eqPts;
    for (int i = 0; i <= 12; ++i) {
        float t = (static_cast<float>(i) / 12.0f) * PI;
        eqPts.push_back({
            center.x - std::cos(t) * (radius * 0.92f),
            center.y + std::sin(t) * (radius * 0.32f)
        });
    }
    DrawVecPolyWin(eqPts, false, Fade(podColor, 0.65f), 0.65f, false);

    // 4. Bright specular highlight arc (top-left)
    std::vector<Vector2> highlightPts;
    for (int i = 0; i <= 8; ++i) {
        float a = PI * 0.92f + (static_cast<float>(i) / 8.0f) * PI * 0.62f;
        highlightPts.push_back({ center.x + std::cos(a) * (radius * 0.62f), center.y + std::sin(a) * (radius * 0.62f) });
    }
    DrawVecPolyWin(highlightPts, false, WHITE, 0.72f, false);
}

void Renderer::DrawVectorPodStand(float x, float groundY, bool drawPodBall, Color standColor, Color podColor, float camX, float camY) const {
    // Pedestal stand occupies x + [0.0 .. 2.75], sitting 100% flush on the ground at groundY,
    // with its upper concave cradle hugging the pod sphere (centered at groundY - 5.75, bottom at groundY - 3.00).
    // 1. Upper concave mechanical cradle claws flush against the pod sphere
    DrawVecPolyWorld({
        { x + 0.05f, groundY - 4.85f },
        { x + 0.35f, groundY - 3.85f },
        { x + 0.85f, groundY - 3.05f },
        { x + 1.90f, groundY - 3.05f },
        { x + 2.40f, groundY - 3.85f },
        { x + 2.70f, groundY - 4.85f }
    }, false, camX, camY, standColor, 1.0f, true);

    // 2. Latticed vertical support tower
    DrawVecLineWorld(x + 0.90f, groundY - 3.05f, x + 0.75f, groundY - 1.05f, camX, camY, standColor, 0.9f, true);
    DrawVecLineWorld(x + 1.85f, groundY - 3.05f, x + 2.00f, groundY - 1.05f, camX, camY, standColor, 0.9f, true);

    // Diagonal internal X-bracing
    DrawVecLineWorld(x + 0.88f, groundY - 2.95f, x + 1.95f, groundY - 1.15f, camX, camY, standColor, 0.65f, false);
    DrawVecLineWorld(x + 1.87f, groundY - 2.95f, x + 0.80f, groundY - 1.15f, camX, camY, standColor, 0.65f, false);

    // Horizontal stabilizer collar
    DrawVecLineWorld(x + 0.45f, groundY - 2.05f, x + 2.30f, groundY - 2.05f, camX, camY, standColor, 0.85f, true);

    // 3. Splayed base feet sitting 100% flush on the ground at groundY
    DrawVecPolyWorld({
        { x + 0.00f, groundY },
        { x + 0.60f, groundY - 1.05f },
        { x + 2.15f, groundY - 1.05f },
        { x + 2.75f, groundY }
    }, true, camX, camY, standColor, 1.0f, true);

    if (drawPodBall) {
        DrawVectorPodBall(x + 1.375f, groundY - 5.75f, podColor, camX, camY);
    }
}

void Renderer::DrawVectorPowerPlant(const Entities::PowerPlant& pp, float groundY, Color objColor, Color detailColor, int tickCount, float camX, float camY) const {
    float x = pp.x;
    float y = groundY - 8.00f; // Anchor bottom of reactor hull (y + 8.00f) flush on groundY

    // Flash reactor dome when damaged / counting down
    Color domeCol = YELLOW;
    if (pp.countdownTimer >= 0 && (tickCount % 6 < 3)) {
        domeCol = RED;
    } else if (pp.rechargeCounter > 0 && (tickCount % 4 < 2)) {
        domeCol = WHITE;
    }

    // 1. Dark interior backing for the building so internal vector details pop
    Vector2 bTL = WorldToWindow(x + 0.1f, y + 3.1f, camX, camY);
    Vector2 bBR = WorldToWindow(x + 4.9f, y + 7.9f, camX, camY);
    DrawRectangleRec({ bTL.x, bTL.y, bBR.x - bTL.x, bBR.y - bTL.y }, Fade(objColor, 0.15f));

    // 2. Main reactor building hull (x + [0..5], y + [3..8]), sitting flush on ground at y + 8.0 (= groundY)
    DrawVecPolyWorld({
        { x + 0.00f, y + 8.00f },
        { x + 0.00f, y + 3.55f },
        { x + 0.40f, y + 3.00f },
        { x + 4.60f, y + 3.00f },
        { x + 5.00f, y + 3.55f },
        { x + 5.00f, y + 8.00f }
    }, true, camX, camY, objColor, 1.05f, true);

    // Foundation plinth & structural pilasters
    DrawVecLineWorld(x + 0.00f, y + 7.05f, x + 5.00f, y + 7.05f, camX, camY, objColor, 0.85f, false);
    DrawVecLineWorld(x + 3.45f, y + 3.00f, x + 3.45f, y + 7.05f, camX, camY, objColor, 0.75f, false);

    // Cooling louver vents on right bay
    DrawVecLineWorld(x + 3.80f, y + 4.20f, x + 4.60f, y + 4.20f, camX, camY, objColor, 0.65f, false);
    DrawVecLineWorld(x + 3.80f, y + 5.10f, x + 4.60f, y + 5.10f, camX, camY, objColor, 0.65f, false);
    DrawVecLineWorld(x + 3.80f, y + 6.00f, x + 4.60f, y + 6.00f, camX, camY, objColor, 0.65f, false);

    // 3. Flanged Chimney Stack on top-right (x + [3.75..4.65], y + [-1.0..3.0])
    DrawVecPolyWorld({
        { x + 3.80f, y + 3.00f },
        { x + 3.80f, y - 0.80f },
        { x + 3.65f, y - 1.10f },
        { x + 4.75f, y - 1.10f },
        { x + 4.60f, y - 0.80f },
        { x + 4.60f, y + 3.00f }
    }, false, camX, camY, objColor, 0.95f, true);

    // 4. Multi-segment hemispherical Yellow Reactor Dome on top-left
    std::vector<Vector2> domePts;
    float domeCx = x + 1.90f;
    float domeCy = y + 3.00f;
    float domeRx = 1.48f;
    float domeRy = 2.10f;
    for (int i = 0; i <= 10; ++i) {
        float a = PI - (static_cast<float>(i) / 10.0f) * PI;
        domePts.push_back({ domeCx + std::cos(a) * domeRx, domeCy - std::sin(a) * domeRy });
    }
    DrawVecPolyWorld(domePts, false, camX, camY, domeCol, 1.05f, true);

    // Dome meridian ribs
    DrawVecLineWorld(domeCx - 0.55f, y + 1.05f, domeCx - 0.65f, y + 3.00f, camX, camY, domeCol, 0.70f, false);
    DrawVecLineWorld(domeCx + 0.55f, y + 1.05f, domeCx + 0.65f, y + 3.00f, camX, camY, domeCol, 0.70f, false);
    DrawVecLineWorld(domeCx - 1.32f, y + 2.05f, domeCx + 1.32f, y + 2.05f, camX, camY, domeCol, 0.70f, false);

    // 5. Inner Reactor Core Target Window (in terrainColor, pulsing core diamond)
    DrawVecPolyWorld({
        { x + 0.75f, y + 4.00f },
        { x + 2.95f, y + 4.00f },
        { x + 2.95f, y + 6.20f },
        { x + 0.75f, y + 6.20f }
    }, true, camX, camY, detailColor, 0.85f, true);

    Color corePulse = ((tickCount / 6) % 2 == 0) ? YELLOW : detailColor;
    DrawVecPolyWorld({
        { x + 1.85f, y + 4.35f },
        { x + 2.55f, y + 5.10f },
        { x + 1.85f, y + 5.85f },
        { x + 1.15f, y + 5.10f }
    }, true, camX, camY, corePulse, 0.75f, false);
}

void Renderer::DrawVectorFuelTank(float x, float groundY, Color objColor, Color textColor, float camX, float camY) const {
    // Anchor landing struts (y + 6.00f) 100% flush on groundY
    float y = groundY - 6.00f;

    // 1. Braced landing struts sitting flush on the ground at y + 6.0 (= groundY)
    DrawVecLineWorld(x + 0.80f, y + 4.35f, x + 0.65f, y + 6.00f, camX, camY, objColor, 1.0f, true);
    DrawVecLineWorld(x + 1.35f, y + 4.35f, x + 0.75f, y + 5.65f, camX, camY, objColor, 0.7f, false);
    DrawVecLineWorld(x + 0.25f, y + 6.00f, x + 1.40f, y + 6.00f, camX, camY, objColor, 1.0f, true);

    DrawVecLineWorld(x + 3.20f, y + 4.35f, x + 3.35f, y + 6.00f, camX, camY, objColor, 1.0f, true);
    DrawVecLineWorld(x + 2.65f, y + 4.35f, x + 3.25f, y + 5.65f, camX, camY, objColor, 0.7f, false);
    DrawVecLineWorld(x + 2.60f, y + 6.00f, x + 3.75f, y + 6.00f, camX, camY, objColor, 1.0f, true);

    // 2. Subtle dark interior fill for canister
    Vector2 fTL = WorldToWindow(x + 0.1f, y - 0.7f, camX, camY);
    Vector2 fBR = WorldToWindow(x + 3.9f, y + 4.3f, camX, camY);
    DrawRectangleRec({ fTL.x, fTL.y, fBR.x - fTL.x, fBR.y - fTL.y }, Fade(YELLOW, 0.12f));

    // 3. Yellow Pressurized Canister Body
    DrawVecPolyWorld({
        { x + 0.50f, y - 0.85f },
        { x + 3.50f, y - 0.85f },
        { x + 4.00f, y - 0.10f },
        { x + 4.00f, y + 3.65f },
        { x + 3.50f, y + 4.40f },
        { x + 0.50f, y + 4.40f },
        { x + 0.00f, y + 3.65f },
        { x + 0.00f, y - 0.10f }
    }, true, camX, camY, YELLOW, 1.05f, true);

    // Top & bottom pressure seam bands
    DrawVecLineWorld(x + 0.05f, y + 0.05f, x + 3.95f, y + 0.05f, camX, camY, Fade(YELLOW, 0.65f), 0.65f, false);
    DrawVecLineWorld(x + 0.05f, y + 3.55f, x + 3.95f, y + 3.55f, camX, camY, Fade(YELLOW, 0.65f), 0.65f, false);

    // Top manifold valve cap
    DrawVecPolyWorld({
        { x + 1.25f, y - 0.85f },
        { x + 1.45f, y - 1.30f },
        { x + 2.55f, y - 1.30f },
        { x + 2.75f, y - 0.85f }
    }, false, camX, camY, YELLOW, 0.85f, true);

    // 4. Precision Vector "FUEL" typography inside the tank
    float lt = 0.75f;
    float yTop = y + 0.55f;
    float yMid = y + 1.80f;
    float yBot = y + 3.05f;

    // 'F'
    DrawVecPolyWorld({ { x + 1.05f, yTop }, { x + 0.48f, yTop }, { x + 0.48f, yBot } }, false, camX, camY, textColor, lt, false);
    DrawVecLineWorld(x + 0.48f, yMid, x + 0.92f, yMid, camX, camY, textColor, lt, false);

    // 'U'
    DrawVecPolyWorld({ { x + 1.32f, yTop }, { x + 1.32f, yBot }, { x + 1.90f, yBot }, { x + 1.90f, yTop } }, false, camX, camY, textColor, lt, false);

    // 'E'
    DrawVecPolyWorld({ { x + 2.75f, yTop }, { x + 2.20f, yTop }, { x + 2.20f, yBot }, { x + 2.75f, yBot } }, false, camX, camY, textColor, lt, false);
    DrawVecLineWorld(x + 2.20f, yMid, x + 2.65f, yMid, camX, camY, textColor, lt, false);

    // 'L'
    DrawVecPolyWorld({ { x + 3.05f, yTop }, { x + 3.05f, yBot }, { x + 3.58f, yBot } }, false, camX, camY, textColor, lt, false);
}

void Renderer::DrawVectorTurret(const Entities::Turret& turret, Color objColor, int tickCount, float camX, float camY) const {
    // Canonical turret bunker defined in local (dx, dy) for UP_RIGHT:
    // - Diagonal base lies along (-0.5, 1.5) -> (4.5, 6.5), collinear with the 45-deg terrain slope,
    //   with exact midpoint at (2.0, 4.0).
    // - In screen pixels (WORLD_SCALE_X=4, WORLD_SCALE_Y=2), the along-wall vector (+1.0, +1.0) = (+4px, +2px)
    //   and the outward-normal vector (+0.5, -2.0) = (+2px, -4px) are strictly orthogonal (dot = 0) and
    //   have identical Euclidean length (sqrt(20) px).
    auto mapPt = [&](float dx, float dy) -> Vector2 {
        switch (turret.dir) {
            case LevelsData::TurretDir::UP_RIGHT:
                return { turret.x + dx, turret.y + dy };
            case LevelsData::TurretDir::UP_LEFT:
                return { turret.x + (5.0f - dx), turret.y + dy };
            case LevelsData::TurretDir::DOWN_RIGHT:
                return { turret.x + (dx - 1.0f), turret.y + (6.0f - dy) };
            case LevelsData::TurretDir::DOWN_LEFT:
                return { turret.x + (6.0f - dx), turret.y + (6.0f - dy) };
        }
        return { turret.x + dx, turret.y + dy };
    };

    // Orthogonal, isometric turret-local coordinates:
    // s = signed distance along wall from center (s = 0), h = height perpendicular to wall
    auto turretPt = [&](float s, float h) -> Vector2 {
        return mapPt(2.0f + s + 0.5f * h, 4.0f + s - 2.0f * h);
    };

    // 1. Symmetrical sloped armored bunker base flush on the terrain wall (s in [-2.50, +2.50], h in [0.00, 0.65])
    DrawVecPolyWorld({
        turretPt(-2.50f, 0.00f),
        turretPt(-2.05f, 0.65f),
        turretPt( 2.05f, 0.65f),
        turretPt( 2.50f, 0.00f)
    }, true, camX, camY, objColor, 1.0f, true);

    // Symmetrical internal structural truss struts inside base
    Vector2 sL0 = turretPt(-1.25f, 0.00f), sL1 = turretPt(-1.05f, 0.65f);
    Vector2 sC0 = turretPt( 0.00f, 0.00f), sC1 = turretPt( 0.00f, 0.65f);
    Vector2 sR0 = turretPt( 1.25f, 0.00f), sR1 = turretPt( 1.05f, 0.65f);
    DrawVecLineWorld(sL0.x, sL0.y, sL1.x, sL1.y, camX, camY, objColor, 0.7f, false);
    DrawVecLineWorld(sC0.x, sC0.y, sC1.x, sC1.y, camX, camY, objColor, 0.7f, false);
    DrawVecLineWorld(sR0.x, sR0.y, sR1.x, sR1.y, camX, camY, objColor, 0.7f, false);

    // 2. Centered Rotating Gun Barrel frame at the exact center of the turret dome (s = 0.00, h = 1.30)
    // Computed before drawing the dome so the dome vector cleanly stops at the gun barrel edges
    Vector2 pivotWorld = turret.GetDomePivot();
    Vector2 pivotLogical = WorldToLogical(pivotWorld.x, pivotWorld.y, camX, camY);

    float theta = (turret.currentAngle / 32.0f) * 2.0f * PI;
    float dirLX = std::sin(theta);
    float dirLY = -std::cos(theta);
    float perpLX = -dirLY;
    float perpLY = dirLX;

    auto barrelToWin = [&](float alongPx, float acrossPx) -> Vector2 {
        return LogicalToWindow(
            pivotLogical.x + dirLX * alongPx + perpLX * acrossPx,
            pivotLogical.y + dirLY * alongPx + perpLY * acrossPx
        );
    };

    // 3. Symmetrical Semicircular Armored Turret Dome (clipped against the gun barrel so no dome vector shows underneath it)
    const int domeSegs = 24;
    for (int i = 0; i < domeSegs; ++i) {
        float a1 = PI - (static_cast<float>(i) / domeSegs) * PI;
        float a2 = PI - (static_cast<float>(i + 1) / domeSegs) * PI;
        float aMid = 0.5f * (a1 + a2);

        Vector2 wMid = turretPt(std::cos(aMid) * 1.45f, 0.65f + std::sin(aMid) * 1.45f);
        Vector2 lMid = WorldToLogical(wMid.x, wMid.y, camX, camY);
        float dxL = lMid.x - pivotLogical.x;
        float dyL = lMid.y - pivotLogical.y;
        float along = dxL * dirLX + dyL * dirLY;
        float across = dxL * perpLX + dyL * perpLY;

        // Skip dome segments that lie underneath the gun barrel
        if (along >= -0.4f && along <= 7.0f && std::abs(across) <= 1.35f) {
            continue;
        }

        Vector2 w1 = turretPt(std::cos(a1) * 1.45f, 0.65f + std::sin(a1) * 1.45f);
        Vector2 w2 = turretPt(std::cos(a2) * 1.45f, 0.65f + std::sin(a2) * 1.45f);
        DrawVecLineWorld(w1.x, w1.y, w2.x, w2.y, camX, camY, objColor, 1.05f, true);
    }

    // Solid black occlusion mask inside the gun barrel & hub so nothing underneath ever bleeds through
    Vector2 b0 = barrelToWin(0.3f, -1.25f);
    Vector2 b1 = barrelToWin(6.8f, -0.95f);
    Vector2 b2 = barrelToWin(6.8f,  0.95f);
    Vector2 b3 = barrelToWin(0.3f,  1.25f);
    DrawTriangle(b0, b1, b2, BLACK);
    DrawTriangle(b2, b1, b0, BLACK);
    DrawTriangle(b0, b2, b3, BLACK);
    DrawTriangle(b3, b2, b0, BLACK);

    // Turret trunnion hub at pivot
    Vector2 pivotWin = LogicalToWindow(pivotLogical.x, pivotLogical.y);
    DrawCircleV(pivotWin, std::max(2.2f, 2.3f * scale), BLACK);
    DrawCircleV(pivotWin, std::max(1.8f, 1.9f * scale), objColor);

    // Symmetrical twin-rail vector gun barrel + flared muzzle tip
    Color barrelCol = (turret.aiming && (tickCount % 4 < 2)) ? WHITE : objColor;
    DrawVecPolyWin({
        barrelToWin(0.5f, -1.1f),
        barrelToWin(6.8f, -0.8f),
        barrelToWin(6.8f,  0.8f),
        barrelToWin(0.5f,  1.1f)
    }, true, barrelCol, 0.95f, true);

    // Muzzle brake cross-bar at tip of barrel
    DrawVecLineWin(barrelToWin(6.8f, -1.5f), barrelToWin(6.8f, 1.5f), barrelCol, 0.95f, true);
}

void Renderer::DrawVectorSwitch(const Entities::Switch& sw, bool doorOpenActive, int tickCount, float camX, float camY) const {
    auto mapPt = [&](float dx, float dy) -> Vector2 {
        if (sw.dir == LevelsData::SwitchDir::LEFT) {
            return { sw.x + dx, sw.y + dy };
        } else {
            return { sw.x + (2.0f - dx), sw.y + dy };
        }
    };

    // Outer armored C-bracket mounted flush on the vertical wall at dx = 2.0
    DrawVecPolyWorld({
        mapPt(2.00f, -0.50f),
        mapPt(0.55f, -0.50f),
        mapPt(0.00f,  0.60f),
        mapPt(0.00f,  5.40f),
        mapPt(0.55f,  6.50f),
        mapPt(2.00f,  6.50f)
    }, true, camX, camY, YELLOW, 1.0f, true);

    // Inner actuator plunger (depresses toward wall and pulses green/white while door timer is active)
    float plungerX = doorOpenActive ? 1.15f : 0.65f;
    Color plungerCol = doorOpenActive ? ((tickCount % 6 < 3) ? GREEN : WHITE) : YELLOW;

    DrawVecPolyWorld({
        mapPt(2.00f, 1.40f),
        mapPt(plungerX, 1.40f),
        mapPt(plungerX, 4.60f),
        mapPt(2.00f, 4.60f)
    }, false, camX, camY, plungerCol, 0.9f, true);

    DrawVecLineWorld(
        mapPt(0.00f, 3.00f).x, mapPt(0.00f, 3.00f).y,
        mapPt(plungerX, 3.00f).x, mapPt(plungerX, 3.00f).y,
        camX, camY, plungerCol, 0.9f, false
    );
}

void Renderer::DrawVectorShip(float wx, float wy, int angleIdx, bool thrustActive, bool shieldActive, Color shieldColor, int tickCount, float camX, float camY) const {
    Vector2 shipLogical = WorldToLogical(wx, wy, camX, camY);

    float theta = (static_cast<float>(angleIdx & 0x1F) / 32.0f) * 2.0f * PI;
    float cosA = std::cos(theta);
    float sinA = std::sin(theta);

    auto rotToWin = [&](float lx, float ly) -> Vector2 {
        float rx = lx * cosA - ly * sinA;
        float ry = lx * sinA + ly * cosA;
        return LogicalToWindow(shipLogical.x + rx, shipLogical.y + ry);
    };

    // 0. Multi-Stage Vector Rocket Afterburner Plume when Thrusting
    if (thrustActive) {
        float timeSec = static_cast<float>(GetTime());
        float pulse1 = 0.5f + 0.5f * std::sin(timeSec * 65.0f);
        float pulse2 = 0.5f + 0.5f * std::cos(timeSec * 43.0f);
        float jitterX = std::sin(timeSec * 90.0f) * 0.45f;

        float outerLen = 11.2f + pulse1 * 4.8f;
        float midLen   = 8.8f  + pulse2 * 3.4f;
        float coreLen  = 6.6f  + pulse1 * 2.2f;

        // Outer crimson/orange expansion cone
        DrawVecPolyWin({
            rotToWin(-1.55f, 4.6f),
            rotToWin(-2.10f, 7.2f),
            rotToWin(jitterX, outerLen),
            rotToWin( 2.10f, 7.2f),
            rotToWin( 1.55f, 4.6f)
        }, false, RED, 0.95f, true);

        // Middle golden-orange supersonic flame jet
        DrawVecPolyWin({
            rotToWin(-1.20f, 4.6f),
            rotToWin(-1.45f, 6.6f),
            rotToWin(jitterX * 0.6f, midLen),
            rotToWin( 1.45f, 6.6f),
            rotToWin( 1.20f, 4.6f)
        }, false, ORANGE, 1.0f, true);

        // Inner white/yellow plasma core + Mach shock diamonds
        Color coreCol = (tickCount % 2 == 0) ? WHITE : YELLOW;
        DrawVecPolyWin({
            rotToWin(-0.75f, 4.6f),
            rotToWin(jitterX * 0.3f, coreLen),
            rotToWin( 0.75f, 4.6f)
        }, false, coreCol, 1.05f, true);

        // Supersonic Mach shock diamond inside the exhaust plume
        float shockY = 5.8f + pulse2 * 1.4f;
        DrawVecPolyWin({
            rotToWin( 0.0f,  shockY - 0.9f),
            rotToWin( 0.95f, shockY),
            rotToWin( 0.0f,  shockY + 1.2f),
            rotToWin(-0.95f, shockY)
        }, true, WHITE, 0.75f, false);
    }

    // 1. Main Delta-Wing Interceptor Hull
    std::vector<Vector2> hullPts = {
        rotToWin( 0.0f, -8.4f), // Needle nose tip
        rotToWin( 2.4f, -3.2f), // Starboard forward chine
        rotToWin( 8.0f,  2.5f), // Starboard outer wingtip
        rotToWin( 6.7f,  4.5f), // Starboard rear wingtip
        rotToWin( 3.8f,  0.5f), // Starboard inner wing root
        rotToWin( 2.0f, -1.5f), // Cockpit bay right
        rotToWin(-2.0f, -1.5f), // Cockpit bay left
        rotToWin(-3.8f,  0.5f), // Port inner wing root
        rotToWin(-6.7f,  4.5f), // Port rear wingtip
        rotToWin(-8.0f,  2.5f), // Port outer wingtip
        rotToWin(-2.4f, -3.2f)  // Port forward chine
    };
    DrawVecPolyWin(hullPts, true, YELLOW, 1.05f, true);

    // 2. Inner Wing Strakes & Cockpit Canopy Detail for high-definition vector look
    DrawVecPolyWin({
        rotToWin( 0.0f, -5.8f),
        rotToWin( 1.6f, -2.2f),
        rotToWin( 0.0f, -1.2f),
        rotToWin(-1.6f, -2.2f)
    }, true, Fade(YELLOW, 0.85f), 0.7f, false);

    DrawVecLineWin(rotToWin( 2.4f, -3.2f), rotToWin( 5.8f, 2.6f), Fade(YELLOW, 0.75f), 0.65f, false);
    DrawVecLineWin(rotToWin(-2.4f, -3.2f), rotToWin(-5.8f, 2.6f), Fade(YELLOW, 0.75f), 0.65f, false);

    // 3. Central Thruster Pod & Nozzle Bell
    std::vector<Vector2> enginePts = {
        rotToWin(-2.1f, 1.1f),
        rotToWin( 2.1f, 1.1f),
        rotToWin( 1.6f, 4.6f),
        rotToWin(-1.6f, 4.6f)
    };
    DrawVecPolyWin(enginePts, true, YELLOW, 0.9f, true);
    DrawVecLineWin(rotToWin(0.0f, 1.1f), rotToWin(0.0f, 4.6f), Fade(YELLOW, 0.75f), 0.65f, false);

    // 4. Multi-Ring Vector Energy Shield when Space is held
    if (shieldActive) {
        Color sCol = ((tickCount >> 1) & 1) ? shieldColor : WHITE;
        int numSegs = 16;
        for (int i = 0; i < numSegs; ++i) {
            if ((i + tickCount) % 2 != 0) continue;
            float a1 = (static_cast<float>(i) / numSegs) * 2.0f * PI;
            float a2 = (static_cast<float>(i + 1) / numSegs) * 2.0f * PI;
            Vector2 p1 = LogicalToWindow(shipLogical.x + std::cos(a1) * 10.6f, shipLogical.y + std::sin(a1) * 10.6f);
            Vector2 p2 = LogicalToWindow(shipLogical.x + std::cos(a2) * 10.6f, shipLogical.y + std::sin(a2) * 10.6f);
            DrawVecLineWin(p1, p2, sCol, 1.05f, true);
        }
    }
}

void Renderer::DrawTeleportMorph(
    float wx,
    float wy,
    const std::vector<Vector2>& targetLocalOutline,
    float squareHalfSize,
    float progress,
    Color outlineColor,
    float camX,
    float camY
) const {
    Vector2 centerLogical = WorldToLogical(wx, wy, camX, camY);
    float S = squareHalfSize;
    float u = std::clamp(progress, 0.0f, 1.0f);

    if (u <= 0.58f) {
        // Phase 1: Cross of 6 graduated yellow bars on all 4 arms converging into a hollow square
        // At u = 0.0 (spread = 1.0), matches the exact reference cross pattern around an empty central square.
        float spread = std::clamp(1.0f - (u / 0.58f), 0.0f, 1.0f);
        float pitch = 1.65f * S;
        float maxThick = 1.02f * S;
        float edgeThick = 1.05f;

        auto drawBarLogical = [&](float lx1, float ly1, float lx2, float ly2) {
            Vector2 tl = LogicalToWindow(centerLogical.x + std::min(lx1, lx2), centerLogical.y + std::min(ly1, ly2));
            Vector2 br = LogicalToWindow(centerLogical.x + std::max(lx1, lx2), centerLogical.y + std::max(ly1, ly2));
            float rw = std::max(1.5f, br.x - tl.x);
            float rh = std::max(1.5f, br.y - tl.y);
            DrawRectangleRec({ tl.x - 1.5f, tl.y - 1.5f, rw + 3.0f, rh + 3.0f }, Fade(YELLOW, 0.22f));
            DrawRectangleRec({ tl.x, tl.y, rw, rh }, YELLOW);
        };

        for (int i = 0; i < 6; ++i) {
            float fullThick = ((6.0f - static_cast<float>(i)) / 6.0f) * maxThick;
            float barThick = edgeThick + (fullThick - edgeThick) * spread;
            float innerDist = (S - edgeThick) + (edgeThick + static_cast<float>(i) * pitch) * spread;
            float outerDist = innerDist + barThick;

            // Top, Bottom, Left, Right arms
            drawBarLogical(-S, -outerDist,  S, -innerDist);
            drawBarLogical(-S,  innerDist,  S,  outerDist);
            drawBarLogical(-outerDist, -S, -innerDist,  S);
            drawBarLogical( innerDist, -S,  outerDist,  S);
        }
        return;
    }

    // Phase 2: Hollow square [-S, +S] x [-S, +S] transforms smoothly into the entity's vector outline
    float morph = std::clamp((u - 0.58f) / 0.42f, 0.0f, 1.0f);

    auto squarePerimPt = [&](float s) -> Vector2 {
        float d = std::fmod(s, 1.0f);
        if (d < 0.0f) d += 1.0f;
        float p = d * 8.0f * S;
        if (p <= S)        return { p, -S };
        if (p <= 3.0f * S) return { S, -S + (p - S) };
        if (p <= 5.0f * S) return { S - (p - 3.0f * S), S };
        if (p <= 7.0f * S) return { -S, S - (p - 5.0f * S) };
        return { -S + (p - 7.0f * S), -S };
    };

    size_t n = targetLocalOutline.size();
    if (n < 3) return;

    std::vector<float> cumLen(n + 1, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const Vector2& a = targetLocalOutline[i];
        const Vector2& b = targetLocalOutline[(i + 1) % n];
        cumLen[i + 1] = cumLen[i] + std::hypot(b.x - a.x, b.y - a.y);
    }
    float totalLen = std::max(1e-4f, cumLen[n]);

    auto targetPerimPt = [&](float s) -> Vector2 {
        float targetDist = std::clamp(s, 0.0f, 1.0f) * totalLen;
        for (size_t i = 0; i < n; ++i) {
            if (targetDist <= cumLen[i + 1] || i + 1 == n) {
                float segLen = std::max(1e-5f, cumLen[i + 1] - cumLen[i]);
                float t = std::clamp((targetDist - cumLen[i]) / segLen, 0.0f, 1.0f);
                const Vector2& a = targetLocalOutline[i];
                const Vector2& b = targetLocalOutline[(i + 1) % n];
                return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
            }
        }
        return targetLocalOutline[0];
    };

    // Union of all target polygon vertex parameters and the 4 exact square corner parameters
    std::vector<float> params = { 0.125f, 0.375f, 0.625f, 0.875f };
    for (size_t i = 0; i < n; ++i) {
        params.push_back(cumLen[i] / totalLen);
    }
    std::sort(params.begin(), params.end());

    std::vector<Vector2> morphedWinPts;
    morphedWinPts.reserve(params.size());
    float prevS = -1.0f;
    for (float s : params) {
        if (prevS >= 0.0f && (s - prevS) < 1e-4f) continue;
        prevS = s;
        Vector2 sq = squarePerimPt(s);
        Vector2 tg = targetPerimPt(s);
        float lx = centerLogical.x + sq.x * (1.0f - morph) + tg.x * morph;
        float ly = centerLogical.y + sq.y * (1.0f - morph) + tg.y * morph;
        morphedWinPts.push_back(LogicalToWindow(lx, ly));
    }

    Color col = {
        static_cast<unsigned char>(YELLOW.r + (outlineColor.r - YELLOW.r) * morph),
        static_cast<unsigned char>(YELLOW.g + (outlineColor.g - YELLOW.g) * morph),
        static_cast<unsigned char>(YELLOW.b + (outlineColor.b - YELLOW.b) * morph),
        255
    };
    DrawVecPolyWin(morphedWinPts, true, col, 1.05f, true);
}

void Renderer::DrawVectorHUD(int score, int lives, int fuel, const Entities::PowerPlant& powerPlant, const std::string& overlayMessage) const {
    float hudH = 24.0f * uiScale;
    DrawRectangleRec({ 0.0f, 0.0f, static_cast<float>(winW), hudH }, BLACK);

    auto hudPt = [&](float ux, float uy) -> Vector2 {
        return { uiOffsetX + ux * uiScale, uy * uiScale };
    };

    // Chamfered BBC Micro vector status bar frame (constant aspect ratio, centered horizontally at top)
    std::vector<Vector2> borderPts = {
        hudPt( 10.0f,  2.0f),
        hudPt(310.0f,  2.0f),
        hudPt(317.0f, 11.0f),
        hudPt(310.0f, 20.0f),
        hudPt( 10.0f, 20.0f),
        hudPt(  3.0f, 11.0f)
    };
    DrawVecPolyWin(borderPts, true, YELLOW, 0.95f, true);

    int fontSize = std::max(10, static_cast<int>(std::round(9.5f * uiScale)));
    int textY = static_cast<int>(6.0f * uiScale);

    DrawText("FUEL", static_cast<int>(hudPt(18.0f, 6.0f).x), textY, fontSize, GREEN);
    DrawText(TextFormat("%04d", fuel), static_cast<int>(hudPt(58.0f, 6.0f).x), textY, fontSize, YELLOW);

    DrawText("LIVES", static_cast<int>(hudPt(125.0f, 6.0f).x), textY, fontSize, GREEN);
    DrawText(TextFormat("%d", lives), static_cast<int>(hudPt(172.0f, 6.0f).x), textY, fontSize, YELLOW);

    DrawText("SCORE", static_cast<int>(hudPt(205.0f, 6.0f).x), textY, fontSize, GREEN);
    DrawText(TextFormat("%06d", score), static_cast<int>(hudPt(255.0f, 6.0f).x), textY, fontSize, YELLOW);

    if (powerPlant.countdownTimer >= 0) {
        Color countCol = (std::fmod(powerPlant.countdownSecTimer, 0.5f) > 0.25f) ? RED : YELLOW;
        int countFont = std::max(12, static_cast<int>(std::round(12.0f * uiScale)));
        const char* countText = TextFormat("COUNTDOWN: %02d", powerPlant.countdownTimer);
        int tw = MeasureText(countText, countFont);
        DrawText(countText, static_cast<int>((winW - tw) * 0.5f),
                 static_cast<int>(28.0f * uiScale), countFont, countCol);
    }

    if (!overlayMessage.empty()) {
        int msgFont = std::max(14, static_cast<int>(std::round(14.0f * uiScale)));
        int textW = MeasureText(overlayMessage.c_str(), msgFont);
        float boxW = textW + 24.0f * uiScale;
        float boxH = 24.0f * uiScale;
        float boxX = (winW - boxW) * 0.5f;
        float boxY = (winH - boxH) * 0.44f;

        DrawRectangleRec({ boxX, boxY, boxW, boxH }, Fade(BLACK, 0.88f));
        DrawVecPolyWin({
            { boxX, boxY },
            { boxX + boxW, boxY },
            { boxX + boxW, boxY + boxH },
            { boxX, boxY + boxH }
        }, true, YELLOW, 0.9f, true);

        DrawText(overlayMessage.c_str(),
                 static_cast<int>((winW - textW) * 0.5f),
                 static_cast<int>(boxY + (boxH - msgFont) * 0.5f),
                 msgFont, YELLOW);
    }
}

void Renderer::RenderGame(
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
    bool shipVisible,
    bool teleportAnimActive,
    float teleportProgress
) {
    float shipLogX = physics.state.shipX * Constants::WORLD_SCALE_X;
    float shipLogY = physics.state.shipY * Constants::WORLD_SCALE_Y;
    float camX = shipLogX - viewLogicalW * 0.5f;
    float camY = shipLogY - viewLogicalH * 0.5f;

    // 1. Draw Twinkling Starfield tiled across the visible logical viewport (viewLogicalW x viewLogicalH)
    float baseStarSize = std::max(1.5f, scale * 0.8f);
    float timeSec = static_cast<float>(GetTime());
    float parallaxRefX = shipLogX - Constants::INTERNAL_WIDTH * 0.5f;
    float parallaxRefY = shipLogY - Constants::INTERNAL_HEIGHT * 0.5f;

    for (const auto& star : stars) {
        float sx = std::fmod(star.x - parallaxRefX * 0.3125f, static_cast<float>(Constants::INTERNAL_WIDTH));
        float sy = std::fmod(star.y - parallaxRefY * 0.2f, static_cast<float>(Constants::INTERNAL_HEIGHT));
        if (sx < 0) sx += Constants::INTERNAL_WIDTH;
        if (sy < 0) sy += Constants::INTERNAL_HEIGHT;

        float wave1 = 0.5f + 0.5f * std::sin(timeSec * star.speed + star.phase);
        float wave2 = 0.5f + 0.5f * std::sin(timeSec * (star.speed * 1.73f) - star.phase * 1.3f);
        float twinkle = wave1 * 0.7f + wave2 * 0.3f;
        float alpha = 0.14f + 0.86f * (twinkle * twinkle);
        float sz = baseStarSize * (0.75f + 0.55f * twinkle);
        Color starCol = Fade(star.color, alpha);

        for (float tileY = 0.0f; tileY < viewLogicalH; tileY += Constants::INTERNAL_HEIGHT) {
            for (float tileX = 0.0f; tileX < viewLogicalW; tileX += Constants::INTERNAL_WIDTH) {
                float lx = sx + tileX;
                float ly = sy + tileY;
                if (lx >= viewLogicalW || ly >= viewLogicalH) continue;

                Vector2 winStar = LogicalToWindow(lx, ly);
                DrawRectangleV({ winStar.x - sz * 0.5f, winStar.y - sz * 0.5f }, { sz, sz }, starCol);

                // Sparkle cross glint when star reaches peak twinkle brightness
                if (twinkle > 0.84f) {
                    float glint = ((twinkle - 0.84f) / 0.16f) * scale * 2.2f;
                    Color glintCol = Fade(WHITE, (twinkle - 0.84f) / 0.16f * 0.75f);
                    DrawLineEx({ winStar.x - glint, winStar.y }, { winStar.x + glint, winStar.y }, 1.0f, glintCol);
                    DrawLineEx({ winStar.x, winStar.y - glint }, { winStar.x, winStar.y + glint }, 1.0f, glintCol);
                }
            }
        }
    }

    float pedGroundY = LevelsData::GetGroundY(level, level.podPedestal.x + 1.375f, level.podPedestal.y + 8.0f);
    float ppGroundY = LevelsData::GetGroundY(level, powerPlant.x + 2.5f, powerPlant.y + 7.0f);
    bool doorOpenActive = (doorState.openHoldTimer > 0.0f || doorState.openFraction > 0.01f);
    float partSize = std::max(2.0f, std::round(scale * 1.1f));
    float bulletRadius = std::max(2.0f, scale * 1.05f);

    // Render world across horizontal wrap copies (-1, 0, +1) so scrolling past x=0 or x=256 is seamless
    for (int wrap = -1; wrap <= 1; ++wrap) {
        float wrapCamX = camX - wrap * Constants::WORLD_WIDTH;

        // 2. Draw Solid Vector Terrain
        for (const auto& poly : level.polygons) {
            DrawVectorTerrain(poly, level.terrainColor, wrapCamX, camY, false);
        }

        // 2b. Draw Sliding Blast Door (if present on this level)
        DrawVectorDoor(doorState, level.terrainColor, wrapCamX, camY);

        // 3. Draw Vector Level Objects (anchored 100% flush to ground/walls)
        DrawVectorPodStand(level.podPedestal.x, pedGroundY, !physics.state.podAttached, YELLOW, level.objectColor, wrapCamX, camY);

        if (powerPlant.alive) {
            DrawVectorPowerPlant(powerPlant, ppGroundY, level.objectColor, level.terrainColor, tickCount, wrapCamX, camY);
        }

        for (const auto& tank : fuelTanks) {
            if (!tank.alive) continue;
            float tankGroundY = LevelsData::GetGroundY(level, tank.x + 2.0f, tank.y + 5.0f);
            DrawVectorFuelTank(tank.x, tankGroundY, level.objectColor, level.terrainColor, wrapCamX, camY);
        }

        for (const auto& turret : turrets) {
            if (!turret.alive) continue;
            DrawVectorTurret(turret, level.objectColor, tickCount, wrapCamX, camY);
        }

        for (const auto& sw : switches) {
            if (!sw.alive) continue;
            DrawVectorSwitch(sw, doorOpenActive, tickCount, wrapCamX, camY);
        }

        // 4. Draw Particles (Exhaust pixel trail, vector shrapnel, shockwave rings, sparks, smoke)
        for (const auto& part : particles) {
            Vector2 pWin = WorldToWindow(part.x, part.y, wrapCamX, camY);
            float alpha = (part.maxLifetime > 0.0f) ? std::clamp(part.lifetime / part.maxLifetime, 0.15f, 1.0f) : 1.0f;

            if (part.isRing) {
                float ringRad = part.size * scale;
                Color ringCol = Fade((part.lifetime > part.maxLifetime * 0.6f) ? WHITE : YELLOW, alpha);
                const int segs = 20;
                for (int i = 0; i < segs; ++i) {
                    float a1 = (static_cast<float>(i) / segs) * 2.0f * PI;
                    float a2 = (static_cast<float>(i + 1) / segs) * 2.0f * PI;
                    Vector2 r1 = { pWin.x + std::cos(a1) * ringRad, pWin.y + std::sin(a1) * ringRad };
                    Vector2 r2 = { pWin.x + std::cos(a2) * ringRad, pWin.y + std::sin(a2) * ringRad };
                    DrawVecLineWin(r1, r2, ringCol, 1.15f, true);
                }
                if (part.lifetime > part.maxLifetime * 0.65f) {
                    DrawCircleV(pWin, ringRad * 0.55f, Fade(WHITE, alpha * 0.45f));
                }
            } else if (part.length > 0.0f) {
                float halfLen = part.length * 0.5f * scale;
                float dx = std::cos(part.angle) * halfLen;
                float dy = std::sin(part.angle) * halfLen;
                DrawVecLineWin({ pWin.x - dx, pWin.y - dy }, { pWin.x + dx, pWin.y + dy }, Fade(part.color, alpha), 1.0f, true);
            } else {
                float sz = partSize * part.size;
                if (part.bounceOnWalls) {
                    DrawCircleV(pWin, sz * 1.1f, Fade(part.color, alpha * 0.35f));
                }
                DrawRectangleV({ pWin.x - sz * 0.5f, pWin.y - sz * 0.5f }, { sz, sz }, Fade(part.color, alpha));
            }
        }

        // 5. Draw Bullets with subtle glow
        for (const auto& bullet : bullets) {
            Vector2 bWin = WorldToWindow(bullet.x, bullet.y, wrapCamX, camY);
            Color bCol = bullet.isPlayerBullet ? YELLOW : WHITE;
            DrawCircleV(bWin, bulletRadius * 1.8f, Fade(bCol, 0.25f));
            DrawCircleV(bWin, bulletRadius, bCol);
        }
    }

    if (shipVisible) {
        if (teleportAnimActive) {
            // Build rotated ship silhouette in logical pixel offsets
            static const Vector2 rawHull[] = {
                {  0.0f, -8.4f },
                {  2.4f, -3.2f },
                {  8.0f,  2.5f },
                {  6.7f,  4.5f },
                {  3.8f,  0.5f },
                {  2.1f,  1.1f },
                {  1.6f,  4.6f },
                { -1.6f,  4.6f },
                { -2.1f,  1.1f },
                { -3.8f,  0.5f },
                { -6.7f,  4.5f },
                { -8.0f,  2.5f },
                { -2.4f, -3.2f }
            };
            float theta = (static_cast<float>(physics.state.angle & 0x1F) / 32.0f) * 2.0f * PI;
            float cosA = std::cos(theta);
            float sinA = std::sin(theta);

            std::vector<Vector2> shipLocalOutline;
            shipLocalOutline.reserve(13);
            for (const auto& v : rawHull) {
                shipLocalOutline.push_back({ v.x * cosA - v.y * sinA, v.x * sinA + v.y * cosA });
            }

            DrawTeleportMorph(physics.state.shipX, physics.state.shipY, shipLocalOutline, 5.5f, teleportProgress, YELLOW, camX, camY);

            if (physics.state.podAttached) {
                float wrappedPodX = physics.state.shipX + Constants::ShortestDeltaWorldX(physics.state.shipX, physics.state.podX);
                std::vector<Vector2> podLocalOutline;
                podLocalOutline.reserve(24);
                for (int i = 0; i < 24; ++i) {
                    float a = -PI * 0.5f + (static_cast<float>(i) / 24.0f) * 2.0f * PI;
                    podLocalOutline.push_back({ std::cos(a) * 5.5f, std::sin(a) * 5.5f });
                }
                if (teleportProgress > 0.80f) {
                    DrawVecLineWorld(physics.state.shipX, physics.state.shipY,
                                     wrappedPodX, physics.state.podY,
                                     camX, camY, level.terrainColor, 0.85f, true);
                }
                DrawTeleportMorph(wrappedPodX, physics.state.podY, podLocalOutline, 5.5f, teleportProgress, level.objectColor, camX, camY);
            }
        } else {
            // 6. Draw Pod & Tow Bar (if attached or connecting)
            if (physics.state.podAttached) {
                float wrappedPodX = physics.state.shipX + Constants::ShortestDeltaWorldX(physics.state.shipX, physics.state.podX);
                DrawVecLineWorld(physics.state.shipX, physics.state.shipY,
                                 wrappedPodX, physics.state.podY,
                                 camX, camY, level.terrainColor, 0.85f, true);
                DrawVectorPodBall(wrappedPodX, physics.state.podY, level.objectColor, camX, camY);
            } else if (podConnecting) {
                float rawPodCenterX = level.podPedestal.x + 1.375f;
                float podCenterX = physics.state.shipX + Constants::ShortestDeltaWorldX(physics.state.shipX, rawPodCenterX);
                float podCenterY = pedGroundY - 5.75f;
                DrawVecLineWorld(physics.state.shipX, physics.state.shipY,
                                 podCenterX, podCenterY,
                                 camX, camY, level.terrainColor, 0.85f, true);
            }

            // 7. Draw Fuel Tractor Siphon Beam
            if (fuelBeamActive) {
                float beamTankGroundY = LevelsData::GetGroundY(level, fuelBeamTarget.x + 2.0f, fuelBeamTarget.y + 5.0f);
                float wrappedTankX = physics.state.shipX + Constants::ShortestDeltaWorldX(physics.state.shipX, fuelBeamTarget.x);

                Vector2 tankLeft  = { wrappedTankX + 0.20f, beamTankGroundY - 6.85f };
                Vector2 tankRight = { wrappedTankX + 3.80f, beamTankGroundY - 6.85f };
                Vector2 tankValve = { wrappedTankX + 2.00f, beamTankGroundY - 7.30f };
                Vector2 shipLeft  = { physics.state.shipX - 1.15f, physics.state.shipY + 1.40f };
                Vector2 shipRight = { physics.state.shipX + 1.15f, physics.state.shipY + 1.40f };
                Vector2 shipPort  = { physics.state.shipX,         physics.state.shipY + 1.15f };

                // Outer vector containment V-beam rails
                DrawVecLineWorld(shipLeft.x, shipLeft.y, tankLeft.x, tankLeft.y,
                                 camX, camY, level.terrainColor, 0.95f, true);
                DrawVecLineWorld(shipRight.x, shipRight.y, tankRight.x, tankRight.y,
                                 camX, camY, level.terrainColor, 0.95f, true);

                // Inner secondary containment rails from the tank manifold valve
                DrawVecLineWorld(shipLeft.x + 0.35f, shipLeft.y, tankValve.x - 0.65f, tankValve.y,
                                 camX, camY, Fade(level.terrainColor, 0.55f), 0.65f, false);
                DrawVecLineWorld(shipRight.x - 0.35f, shipRight.y, tankValve.x + 0.65f, tankValve.y,
                                 camX, camY, Fade(level.terrainColor, 0.55f), 0.65f, false);

                // Upward-scrolling vector energy chevron waves drawing fuel from tank (t=0) to ship (t=1)
                const int numWaves = 6;
                float scrollPhase = std::fmod(static_cast<float>(tickCount) * 0.14f, 1.0f);
                for (int i = 0; i < numWaves; ++i) {
                    float t = std::fmod((static_cast<float>(i) + scrollPhase) / static_cast<float>(numWaves), 1.0f);
                    float env = std::sin(t * PI); // Smooth fade-in at tank and fade-out at ship intake
                    float lx = tankLeft.x  + (shipLeft.x  - tankLeft.x)  * t;
                    float ly = tankLeft.y  + (shipLeft.y  - tankLeft.y)  * t;
                    float rx = tankRight.x + (shipRight.x - tankRight.x) * t;
                    float ry = tankRight.y + (shipRight.y - tankRight.y) * t;

                    // Inset slightly from outer rails and arch upward toward the ship
                    float spanX = rx - lx;
                    float lInX = lx + spanX * 0.08f;
                    float rInX = rx - spanX * 0.08f;
                    float midX = (lx + rx) * 0.5f;
                    float midY = (ly + ry) * 0.5f - (0.65f * (1.0f - t * 0.45f));

                    Color waveCol = ((i + (tickCount / 3)) % 2 == 0)
                        ? Fade(YELLOW, 0.35f + 0.60f * env)
                        : Fade(WHITE,  0.30f + 0.55f * env);
                    DrawVecPolyWorld({
                        { lInX, ly },
                        { midX, midY },
                        { rInX, ry }
                    }, false, camX, camY, waveCol, 0.80f, true);
                }

                // Central oscillating fuel plasma stream from manifold valve into ship collector
                const int streamSegs = 8;
                std::vector<Vector2> streamPts;
                streamPts.reserve(streamSegs + 1);
                for (int s = 0; s <= streamSegs; ++s) {
                    float st = static_cast<float>(s) / static_cast<float>(streamSegs);
                    float sx = tankValve.x + (shipPort.x - tankValve.x) * st;
                    float sy = tankValve.y + (shipPort.y - tankValve.y) * st;
                    float waveAmp = std::sin(st * PI) * 0.32f;
                    float osc = std::sin(st * 10.0f - static_cast<float>(tickCount) * 0.85f) * waveAmp;
                    streamPts.push_back({ sx + osc, sy });
                }
                DrawVecPolyWorld(streamPts, false, camX, camY, Fade(YELLOW, 0.85f), 0.75f, true);

                // Glowing extraction rings at the fuel tank manifold valve and ship intake
                Vector2 valveWin = WorldToWindow(tankValve.x, tankValve.y, camX, camY);
                Vector2 portWin  = WorldToWindow(shipPort.x, shipPort.y, camX, camY);
                float pulseR = (1.6f + 0.6f * std::sin(static_cast<float>(tickCount) * 0.6f)) * scale;
                DrawCircleV(valveWin, pulseR * 1.6f, Fade(YELLOW, 0.28f));
                DrawCircleV(valveWin, pulseR * 0.75f, Fade(WHITE, 0.80f));
                DrawCircleV(portWin,  pulseR * 1.3f, Fade(level.terrainColor, 0.30f));
            }

            // 8. Draw Vector Ship & Shield
            DrawVectorShip(
                physics.state.shipX,
                physics.state.shipY,
                physics.state.angle,
                thrustActive,
                shieldActive,
                level.terrainColor,
                tickCount,
                camX,
                camY
            );
        }
    }

    // 9. Draw Vector HUD & Status Overlays
    DrawVectorHUD(score, lives, fuel, powerPlant, overlayMessage);
}

void Renderer::RenderTitleScreen(
    int windowWidth,
    int windowHeight,
    const std::vector<Entities::HighScoreEntry>& highScores,
    int /*tickCount*/
) {
    BeginFrame(windowWidth, windowHeight);

    // Outer vector border frame
    DrawVecPolyWin({
        UIToWindow( 12.0f,  10.0f),
        UIToWindow(308.0f,  10.0f),
        UIToWindow(308.0f, 246.0f),
        UIToWindow( 12.0f, 246.0f)
    }, true, YELLOW, 1.0f, true);

    int titleFont = std::max(20, static_cast<int>(std::round(22.0f * uiScale)));
    int subFont   = std::max(10, static_cast<int>(std::round(8.5f * uiScale)));
    int bodyFont  = std::max(10, static_cast<int>(std::round(7.5f * uiScale)));

    auto drawCentered = [&](const char* txt, float ly, int font, Color col) {
        int tw = MeasureText(txt, font);
        Vector2 p = UIToWindow(160.0f, ly);
        DrawText(txt, static_cast<int>(p.x - tw * 0.5f), static_cast<int>(p.y), font, col);
    };

    drawCentered("T H R U S T", 18.0f, titleFont, YELLOW);
    drawCentered("BBC MICRO VECTOR RECREATION", 42.0f, subFont, RED);

    // Controls section
    drawCentered("CONTROLS", 58.0f, subFont, WHITE);
    drawCentered("A / D  -  ROTATE       SHIFT  -  THRUST", 70.0f, bodyFont, GREEN);
    drawCentered("SPACE  -  SHIELD / POD / FUEL   RETURN - FIRE", 80.0f, bodyFont, GREEN);
    drawCentered("P - PAUSE    F11 - FULLSCREEN    ESC - MAIN SCREEN", 90.0f, bodyFont, GREEN);

    // High Scores Table section on Title Screen
    DrawVecLineWin(UIToWindow(36.0f, 104.0f), UIToWindow(284.0f, 104.0f), YELLOW, 0.75f, true);
    drawCentered("HALL OF FAME", 109.0f, subFont, { 0, 255, 255, 255 });

    for (size_t i = 0; i < highScores.size() && i < 8; ++i) {
        float rowY = 123.0f + i * 11.5f;
        Vector2 leftP = UIToWindow(58.0f, rowY);
        Vector2 rightP = UIToWindow(262.0f, rowY);
        Color rowCol = (i == 0) ? YELLOW : WHITE;

        DrawText(TextFormat("%d. %-12s", static_cast<int>(i + 1), highScores[i].name.c_str()),
                 static_cast<int>(leftP.x), static_cast<int>(leftP.y), bodyFont, rowCol);

        const char* scTxt = TextFormat("%06d", highScores[i].score);
        int scW = MeasureText(scTxt, bodyFont);
        DrawText(scTxt, static_cast<int>(rightP.x - scW), static_cast<int>(rightP.y), bodyFont, YELLOW);
    }

    drawCentered("PRESS SPACE OR RETURN TO PLAY", 220.0f, subFont, YELLOW);
    drawCentered("PRESS Q TO QUIT", 232.0f, bodyFont, WHITE);

    EndFrame();
}

void Renderer::RenderHighScoreScreen(
    int windowWidth,
    int windowHeight,
    int finalScore,
    bool enteringName,
    const std::string& currentName,
    int highlightedRank,
    const std::vector<Entities::HighScoreEntry>& highScores,
    int tickCount
) {
    BeginFrame(windowWidth, windowHeight);

    // Vector frame
    DrawVecPolyWin({
        UIToWindow( 14.0f,  12.0f),
        UIToWindow(306.0f,  12.0f),
        UIToWindow(306.0f, 244.0f),
        UIToWindow( 14.0f, 244.0f)
    }, true, YELLOW, 1.0f, true);

    int headerFont = std::max(16, static_cast<int>(std::round(16.0f * uiScale)));
    int subFont    = std::max(10, static_cast<int>(std::round(9.5f * uiScale)));
    int rowFont    = std::max(10, static_cast<int>(std::round(8.5f * uiScale)));

    auto drawCentered = [&](const char* txt, float ly, int font, Color col) {
        int tw = MeasureText(txt, font);
        Vector2 p = UIToWindow(160.0f, ly);
        DrawText(txt, static_cast<int>(p.x - tw * 0.5f), static_cast<int>(p.y), font, col);
    };

    drawCentered("G A M E   O V E R", 22.0f, headerFont, RED);
    drawCentered(TextFormat("FINAL SCORE: %06d", finalScore), 44.0f, subFont, YELLOW);

    if (enteringName) {
        drawCentered("NEW HIGH SCORE! ENTER YOUR NAME:", 64.0f, rowFont, GREEN);

        // Input box with blinking cursor
        std::string displayStr = currentName + ((tickCount % 30 < 15) ? "_" : " ");
        Vector2 boxTL = UIToWindow(90.0f, 78.0f);
        Vector2 boxBR = UIToWindow(230.0f, 96.0f);
        DrawVecPolyWin({
            { boxTL.x, boxTL.y },
            { boxBR.x, boxTL.y },
            { boxBR.x, boxBR.y },
            { boxTL.x, boxBR.y }
        }, true, { 0, 255, 255, 255 }, 0.9f, true);

        drawCentered(displayStr.c_str(), 82.0f, subFont, YELLOW);
    } else {
        drawCentered("HIGH SCORE TABLE", 68.0f, subFont, { 0, 255, 255, 255 });
    }

    float tableStartY = enteringName ? 106.0f : 90.0f;
    for (size_t i = 0; i < highScores.size() && i < 8; ++i) {
        float rowY = tableStartY + i * 14.0f;
        Vector2 leftP = UIToWindow(52.0f, rowY);
        Vector2 rightP = UIToWindow(268.0f, rowY);

        bool isHighlighted = (static_cast<int>(i) == highlightedRank);
        Color rowCol = isHighlighted ? GREEN : ((i == 0) ? YELLOW : WHITE);

        DrawText(TextFormat("%d.  %-12s", static_cast<int>(i + 1), highScores[i].name.c_str()),
                 static_cast<int>(leftP.x), static_cast<int>(leftP.y), rowFont, rowCol);

        const char* scTxt = TextFormat("%06d", highScores[i].score);
        int scW = MeasureText(scTxt, rowFont);
        DrawText(scTxt, static_cast<int>(rightP.x - scW), static_cast<int>(rightP.y), rowFont, isHighlighted ? GREEN : YELLOW);
    }

    if (enteringName) {
        drawCentered("PRESS RETURN TO CONFIRM", 224.0f, rowFont, YELLOW);
    } else {
        drawCentered("PRESS SPACE OR RETURN TO CONTINUE", 224.0f, rowFont, YELLOW);
    }

    EndFrame();
}
