#include <3ds.h>
#include <citro3d.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <array>
#include <algorithm>

#include "vshader_shbin.h"
#include "nr_physics.hpp"
#include "nr_race.hpp"
#include "nr_garage.hpp"
#include "nr_world.hpp"
#include "nr_source_geometry.hpp"
#include "source_meshes.hpp"

using nr3ds::InputState;
using nr3ds::Vehicle;
using nr3ds::RaceSession;
using nr3ds::RacePhase;
using nr3ds::GarageState;
using nr3ds::UpgradeKind;
using nr3ds::RoadStyle;
using nr3ds::SourceProxyKind;

namespace {

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

constexpr u32 CLEAR_COLOR = 0x02040AFF;
constexpr int kCubeVerts = 36;
constexpr int kTrafficCount = 6;
constexpr float kLaneWidth = 3.55f;
constexpr float kRoadLimit = 4.90f;

enum class GameMode { Garage, Race };

struct Vertex { float x, y, z; };

struct TrafficCar {
    int laneIndex;
    int targetLaneIndex;
    float laneX;
    float distanceM;
    float speedKph;
    float desiredSpeedKph;
    float laneChangeCooldown;
    float r, g, b;
};

static const Vertex kCube[kCubeVerts] = {
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
    { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f},
    {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f},
    { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
    { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f},
    { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f},
    {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
    {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
    { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f},
    { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f},
};

DVLB_s* gShaderDvlb = nullptr;
shaderProgram_s gProgram{};
int gLocProjection = -1;
int gLocModelView = -1;
C3D_Mtx gProjection{};
void* gVbo = nullptr;
void* gSourceRoadVbo = nullptr;
void* gSourceTunnelRoofVbo = nullptr;
nr3ds::ExpresswayRoute gRoute;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * clampf(t, 0.0f, 1.0f);
}

float laneXFor(int laneIndex) {
    return float(laneIndex - 1) * kLaneWidth;
}

float roadCenterRelative(float playerWorldM, float aheadM) {
    return gRoute.centerAt(playerWorldM + aheadM) - gRoute.centerAt(playerWorldM);
}

float roadElevationRelative(float playerWorldM, float aheadM) {
    return gRoute.elevationAt(playerWorldM + aheadM) - gRoute.elevationAt(playerWorldM);
}

float roadYawRelative(float playerWorldM, float aheadM) {
    return gRoute.yawAt(playerWorldM + aheadM) - gRoute.yawAt(playerWorldM);
}

u32 clearColorForStyle(RoadStyle style) {
    switch (style) {
        case RoadStyle::SodiumFence: return 0x070503FF;
        case RoadStyle::Elevated:    return 0x020306FF;
        case RoadStyle::DenseCity:   return 0x010207FF;
        case RoadStyle::Underpass:   return 0x030303FF;
        case RoadStyle::Tunnel:      return 0x161107FF;
        case RoadStyle::Junction:    return 0x020307FF;
        case RoadStyle::HighLevel:   return 0x010205FF;
        case RoadStyle::Open:        return 0x010207FF;
    }
    return CLEAR_COLOR;
}

void setColor(float r, float g, float b, float a = 1.0f) {
    C3D_FixedAttribSet(1, r, g, b, a);
}

void drawCube(float x, float y, float z,
              float sx, float sy, float sz,
              float yaw,
              float r, float g, float b) {
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, z, true);
    Mtx_RotateY(&modelView, yaw, true);
    Mtx_Scale(&modelView, sx, sy, sz);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocModelView, &modelView);
    setColor(r, g, b, 1.0f);
    C3D_DrawArrays(GPU_TRIANGLES, 0, kCubeVerts);
}

void bindPositionVbo(void* vbo, int stride) {
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vbo, stride, 1, 0x0);
}

void drawSourceTriangles(void* vbo, int vertexCount,
                         float x, float y, float z,
                         float sx, float sy, float sz,
                         float yaw,
                         float r, float g, float b) {
    if (!vbo || vertexCount <= 0) return;
    bindPositionVbo(vbo, sizeof(SourceVertex));
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, z, true);
    Mtx_RotateY(&modelView, yaw, true);
    Mtx_Scale(&modelView, sx, sy, sz);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocModelView, &modelView);
    setColor(r, g, b, 1.0f);
    C3D_DrawArrays(GPU_TRIANGLES, 0, vertexCount);
    // Restore the cube stream immediately; all procedural scenery assumes it.
    bindPositionVbo(gVbo, sizeof(Vertex));
}

void sceneInit() {
    gShaderDvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&gProgram);
    shaderProgramSetVsh(&gProgram, &gShaderDvlb->DVLE[0]);
    C3D_BindProgram(&gProgram);

    gLocProjection = shaderInstanceGetUniformLocation(gProgram.vertexShader, "projection");
    gLocModelView = shaderInstanceGetUniformLocation(gProgram.vertexShader, "modelView");

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddFixed(attrInfo, 1);
    setColor(1, 1, 1, 1);

    gVbo = linearAlloc(sizeof(kCube));
    std::memcpy(gVbo, kCube, sizeof(kCube));

    gSourceRoadVbo = linearAlloc(sizeof(kSourceRoadVerts));
    if (gSourceRoadVbo)
        std::memcpy(gSourceRoadVbo, kSourceRoadVerts, sizeof(kSourceRoadVerts));
    gSourceTunnelRoofVbo = linearAlloc(sizeof(kSourceTunnelRoofVerts));
    if (gSourceTunnelRoofVbo)
        std::memcpy(gSourceTunnelRoofVbo, kSourceTunnelRoofVerts, sizeof(kSourceTunnelRoofVerts));

    bindPositionVbo(gVbo, sizeof(Vertex));

    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both,
                  GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void sceneExit() {
    if (gSourceTunnelRoofVbo) linearFree(gSourceTunnelRoofVbo);
    if (gSourceRoadVbo) linearFree(gSourceRoadVbo);
    if (gVbo) linearFree(gVbo);
    shaderProgramFree(&gProgram);
    if (gShaderDvlb) DVLB_Free(gShaderDvlb);
}

void updateProjection(float speedKph) {
    const float speedT = clampf(speedKph / 220.0f, 0.0f, 1.0f);
    const float fov = lerpf(56.0f, 64.5f, speedT);
    Mtx_PerspTilt(&gProjection, C3D_AngleFromDegrees(fov),
                  C3D_AspectRatioTop, 0.05f, 320.0f, false);
}

std::array<TrafficCar, kTrafficCount> makeTraffic() {
    return {{
        {0, 0, laneXFor(0),  28.0f,  88.0f,  88.0f, 0.0f, 0.10f, 0.22f, 0.62f},
        {1, 1, laneXFor(1),  46.0f, 104.0f, 104.0f, 0.0f, 0.50f, 0.50f, 0.54f},
        {2, 2, laneXFor(2),  68.0f,  76.0f,  76.0f, 0.0f, 0.08f, 0.50f, 0.22f},
        {0, 0, laneXFor(0),  92.0f, 118.0f, 118.0f, 0.0f, 0.62f, 0.10f, 0.10f},
        {1, 1, laneXFor(1), 125.0f,  96.0f,  96.0f, 0.0f, 0.46f, 0.18f, 0.62f},
        {2, 2, laneXFor(2), 158.0f, 112.0f, 112.0f, 0.0f, 0.68f, 0.52f, 0.12f},
    }};
}

bool laneClear(const std::array<TrafficCar, kTrafficCount>& traffic,
               int selfIndex, int candidateLane,
               float playerX, float playerDistance) {
    const float targetX = laneXFor(candidateLane);
    for (int j = 0; j < kTrafficCount; ++j) {
        if (j == selfIndex) continue;
        const auto& other = traffic[std::size_t(j)];
        if (std::fabs(other.laneX - targetX) < 1.4f &&
            std::fabs(other.distanceM - traffic[std::size_t(selfIndex)].distanceM) < 18.0f) {
            return false;
        }
    }

    const auto& self = traffic[std::size_t(selfIndex)];
    if (std::fabs(playerX - targetX) < 1.5f &&
        std::fabs(self.distanceM - playerDistance) < 13.0f) {
        return false;
    }
    return true;
}

void requestLaneChange(std::array<TrafficCar, kTrafficCount>& traffic,
                       int index, int direction,
                       float playerX) {
    auto& t = traffic[std::size_t(index)];
    if (t.laneChangeCooldown > 0.0f) return;
    const int candidate = t.targetLaneIndex + direction;
    if (candidate < 0 || candidate > 2) return;
    if (!laneClear(traffic, index, candidate, playerX, 0.0f)) return;
    t.targetLaneIndex = candidate;
    t.laneChangeCooldown = 2.5f;
}

void updateTraffic(std::array<TrafficCar, kTrafficCount>& traffic,
                   const nr3ds::Telemetry& s,
                   float dt) {
    const float playerX = clampf(s.posX, -kRoadLimit, kRoadLimit);

    for (int i = 0; i < kTrafficCount; ++i) {
        auto& t = traffic[std::size_t(i)];
        t.laneChangeCooldown = std::max(0.0f, t.laneChangeCooldown - dt);
        t.speedKph += (t.desiredSpeedKph - t.speedKph) * clampf(dt * 0.7f, 0.0f, 1.0f);
        const float relMps = (t.speedKph - s.speedKph) / 3.6f;
        t.distanceM += relMps * dt;

        if (t.distanceM < -14.0f) {
            t.distanceM = 105.0f + float((i * 29) % 85);
            t.targetLaneIndex = (i + 1) % 3;
            t.laneIndex = t.targetLaneIndex;
            t.laneX = laneXFor(t.laneIndex);
            t.laneChangeCooldown = 1.0f;
        } else if (t.distanceM > 215.0f) {
            t.distanceM = 65.0f + float((i * 31) % 110);
        }
    }

    // Lightweight deterministic lane-change AI. Cars move out of the player's way
    // when rapidly approached and try to pass slower traffic.
    for (int i = 0; i < kTrafficCount; ++i) {
        auto& t = traffic[std::size_t(i)];
        if (t.laneChangeCooldown <= 0.0f &&
            t.distanceM > 2.0f && t.distanceM < 24.0f &&
            s.speedKph > t.speedKph + 18.0f &&
            std::fabs(playerX - t.laneX) < 1.5f) {
            const int preferred = (t.targetLaneIndex == 0) ? +1 : -1;
            requestLaneChange(traffic, i, preferred, playerX);
            if (t.targetLaneIndex == t.laneIndex)
                requestLaneChange(traffic, i, -preferred, playerX);
        }

        for (int j = 0; j < kTrafficCount; ++j) {
            if (i == j) continue;
            const auto& ahead = traffic[std::size_t(j)];
            const float gap = ahead.distanceM - t.distanceM;
            if (gap > 0.0f && gap < 20.0f &&
                std::fabs(ahead.laneX - t.laneX) < 1.3f &&
                ahead.speedKph + 6.0f < t.speedKph) {
                const int preferred = (t.targetLaneIndex < 2) ? +1 : -1;
                requestLaneChange(traffic, i, preferred, playerX);
                break;
            }
        }

        const float targetX = laneXFor(t.targetLaneIndex);
        const float maxStep = 1.65f * dt;
        const float dx = targetX - t.laneX;
        t.laneX += clampf(dx, -maxStep, maxStep);
        if (std::fabs(dx) < 0.03f) {
            t.laneX = targetX;
            t.laneIndex = t.targetLaneIndex;
        }
    }
}

bool resolveTrafficCollision(Vehicle& car,
                             std::array<TrafficCar, kTrafficCount>& traffic,
                             float& cooldown) {
    if (cooldown > 0.0f) return false;
    const auto& s = car.telemetry();
    const float playerX = clampf(s.posX, -kRoadLimit, kRoadLimit);

    for (int i = 0; i < kTrafficCount; ++i) {
        auto& t = traffic[std::size_t(i)];
        if (std::fabs(t.distanceM) < 2.6f && std::fabs(t.laneX - playerX) < 1.35f) {
            float kick = (playerX <= t.laneX) ? -1.5f : 1.5f;
            if (std::fabs(playerX - t.laneX) < 0.15f)
                kick = (i & 1) ? 1.5f : -1.5f;
            car.applyImpact(0.66f, kick * 0.72f);
            t.distanceM += 4.5f;
            t.speedKph *= 0.88f;
            cooldown = 0.70f;
            return true;
        }
    }
    return false;
}

void drawTrafficCar(const TrafficCar& t, float cameraX, float playerWorldM) {
    if (t.distanceM < -3.0f || t.distanceM > 195.0f) return;

    const auto rf = gRoute.localFrame(playerWorldM, t.distanceM);
    const float center = rf.lateralM;
    const float yaw = rf.yawRad;
    const float z = -4.7f - rf.forwardM;
    const float elev = rf.elevationM;
    const float fog = 1.0f - clampf(t.distanceM / 220.0f, 0.0f, 0.82f);
    const float x = t.laneX + center - cameraX;
    const float y = -0.62f + elev * 0.34f;

    drawCube(x, y, z, 1.45f, 0.38f, 2.85f, yaw,
             t.r * fog, t.g * fog, t.b * fog);
    drawCube(x, y + 0.37f, z - 0.12f, 1.08f, 0.30f, 1.25f, yaw,
             0.05f * fog, 0.08f * fog, 0.11f * fog);

    drawCube(x - 0.44f, y + 0.15f, z + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
    drawCube(x + 0.44f, y + 0.15f, z + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
}

void drawRaceOpponent(const nr3ds::RaceTelemetry& race,
                      float cameraX, float playerWorldM) {
    const float relM = race.opponentProgressM - race.playerProgressM;
    if (relM < -4.5f || relM > 195.0f) return;

    const auto rf = gRoute.localFrame(playerWorldM, relM);
    const float center = rf.lateralM;
    const float yaw = rf.yawRad;
    const float z = -4.7f - rf.forwardM;
    const float elev = rf.elevationM;
    const float fog = 1.0f - clampf(std::max(relM, 0.0f) / 220.0f, 0.0f, 0.82f);
    const float x = race.opponentLaneX + center - cameraX;
    const float y = -0.60f + elev * 0.34f;

    drawCube(x, y, z, 1.52f, 0.40f, 3.00f, yaw,
             0.78f * fog, 0.26f * fog, 0.055f * fog);
    drawCube(x, y + 0.38f, z - 0.15f, 1.08f, 0.31f, 1.30f, yaw,
             0.055f * fog, 0.075f * fog, 0.095f * fog);
    drawCube(x - 0.46f, y + 0.15f, z + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.03f * fog, 0.01f * fog);
    drawCube(x + 0.46f, y + 0.15f, z + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.03f * fog, 0.01f * fog);
}

void drawRaceGate(float aheadM, float cameraX, float playerWorldM,
                  bool finishGate, bool countdownGate) {
    if (aheadM < 1.5f || aheadM > 190.0f) return;

    const auto rf = gRoute.localFrame(playerWorldM, aheadM);
    const float center = rf.lateralM;
    const float yaw = rf.yawRad;
    const float z = -4.7f - rf.forwardM;
    const float elev = rf.elevationM;
    const float fog = 1.0f - clampf(aheadM / 220.0f, 0.0f, 0.82f);
    const float roadX = center - cameraX;
    const float gy = elev * 0.34f;

    float r = 0.08f, g = 0.62f, b = 0.78f;
    if (finishGate) { r = 0.84f; g = 0.70f; b = 0.12f; }
    if (countdownGate) { r = 0.82f; g = 0.08f; b = 0.04f; }

    drawCube(roadX - 5.45f, 0.75f + gy, z, 0.18f, 3.7f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX + 5.45f, 0.75f + gy, z, 0.18f, 3.7f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX, 2.65f + gy, z, 11.2f, 0.18f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX, 2.35f + gy, z - 0.12f, 4.8f, 0.55f, 0.10f, yaw,
             r * fog, g * fog, b * fog);
}

bool resolveOpponentCollision(Vehicle& car, RaceSession& race, float& cooldown) {
    if (cooldown > 0.0f || race.telemetry().phase != RacePhase::Racing) return false;
    const auto& s = car.telemetry();
    const auto& rt = race.telemetry();
    const float relM = rt.opponentProgressM - rt.playerProgressM;
    const float playerX = clampf(s.posX, -kRoadLimit, kRoadLimit);
    if (std::fabs(relM) < 2.7f && std::fabs(rt.opponentLaneX - playerX) < 1.35f) {
        const float kick = (playerX <= rt.opponentLaneX) ? -0.75f : 0.75f;
        car.applyImpact(0.78f, kick);
        race.applyOpponentImpact(0.82f, 2.2f);
        cooldown = 0.65f;
        return true;
    }
    return false;
}


void drawSourceFence(float roadX, float roadY, float z, float yaw,
                     float roadHalf, float fog, bool orange) {
    const auto& profile = nr3ds::sourceGeometryProfile(SourceProxyKind::Fence);
    (void)profile;
    const float postR = orange ? 0.52f : 0.24f;
    const float postG = orange ? 0.22f : 0.25f;
    const float postB = orange ? 0.045f : 0.27f;

    // Proxy proportions are derived from _TATSUMI_MESH FENCE0_LOD0. Rather
    // than drawing the original high-poly fence, render a concrete toe, posts,
    // top rail and two thin mesh strips. It reads as the same type of roadside
    // structure at 400x240 for a tiny fraction of the geometry cost.
    for (int side = -1; side <= 1; side += 2) {
        const float x = roadX + float(side) * (roadHalf + 0.36f);
        drawCube(x, roadY + 0.48f, z,
                 0.18f, 0.72f, 10.1f, yaw,
                 0.27f * fog, 0.27f * fog, 0.27f * fog);
        drawCube(x, roadY + 1.56f, z,
                 0.07f, 1.48f, 0.07f, yaw,
                 postR * fog, postG * fog, postB * fog);
        drawCube(x, roadY + 2.27f, z,
                 0.07f, 0.07f, 10.0f, yaw,
                 postR * fog, postG * fog, postB * fog);
        drawCube(x, roadY + 1.58f, z,
                 0.035f, 1.25f, 9.65f, yaw,
                 (orange ? 0.26f : 0.13f) * fog,
                 (orange ? 0.12f : 0.15f) * fog,
                 (orange ? 0.035f : 0.17f) * fog);
    }
}

void drawSourceSupport(float roadX, float roadY, float z, float yaw,
                       float roadHalf, float fog, int variant) {
    const auto& profile = nr3ds::sourceGeometryProfile(SourceProxyKind::Support);
    (void)profile;
    const float pierOffset = roadHalf + 1.85f;
    const float h = (variant & 1) ? 4.7f : 5.4f;

    // Reduced proxy of AREA_2_SUPPORTS2.001/.002: twin piers, cap beam and a
    // darker under-deck. The original source meshes are enormous spans; this
    // version repeats a short module that streams cheaply.
    drawCube(roadX - pierOffset, roadY + h * 0.45f, z,
             0.72f, h, 0.82f, yaw,
             0.17f * fog, 0.18f * fog, 0.19f * fog);
    drawCube(roadX + pierOffset, roadY + h * 0.45f, z,
             0.72f, h, 0.82f, yaw,
             0.17f * fog, 0.18f * fog, 0.19f * fog);
    drawCube(roadX, roadY + h - 0.12f, z,
             roadHalf * 2.0f + 5.0f, 0.52f, 1.05f, yaw,
             0.19f * fog, 0.20f * fog, 0.21f * fog);
    drawCube(roadX, roadY + h + 0.23f, z,
             roadHalf * 2.0f + 5.6f, 0.18f, 2.0f, yaw,
             0.10f * fog, 0.11f * fog, 0.12f * fog);
}

void drawSourceTunnelModule(float roadX, float roadY, float z, float yaw,
                            float roadHalf, float fog, int segmentIndex) {
    // The actual AREA_TUNNEL_2,1 mesh lives in a different additive Unity
    // scene than the files currently supplied, so v0.013 preserves the exact
    // recovered centerline while using a low-poly shell informed by the source
    // road/fence proportions and the provided tunnel footage.
    const float wallX = roadHalf + 0.46f;
    drawCube(roadX - wallX, roadY + 1.58f, z,
             0.72f, 3.50f, 10.15f, yaw,
             0.50f * fog, 0.43f * fog, 0.23f * fog);
    drawCube(roadX + wallX, roadY + 1.58f, z,
             0.72f, 3.50f, 10.15f, yaw,
             0.50f * fog, 0.43f * fog, 0.23f * fog);
    drawCube(roadX, roadY + 3.53f, z,
             roadHalf * 2.0f + 1.45f, 0.32f, 10.2f, yaw,
             0.39f * fog, 0.36f * fog, 0.25f * fog);

    // Actual developer-authorized source LOD geometry from
    // _TUNNEL_OG_2x2_LANE_HIGH_ROOF.520. The offline converter normalized
    // the original Unity mesh into road-local coordinates, so we can retain
    // its silhouette without carrying Unity's runtime/material system.
    if ((segmentIndex & 1) == 0) {
        const float sourceScale = roadHalf + 0.42f;
        drawSourceTriangles(gSourceTunnelRoofVbo, kSourceTunnelRoofVertsCount,
                            roadX, roadY + 2.70f, z,
                            sourceScale, sourceScale, sourceScale, yaw,
                            0.54f * fog, 0.47f * fog, 0.29f * fog);
    }

    // Ribbing is one of the strongest tunnel depth cues in the original.
    if ((segmentIndex & 1) == 0) {
        drawCube(roadX - wallX + 0.18f, roadY + 1.72f, z,
                 0.10f, 3.35f, 0.12f, yaw,
                 0.70f * fog, 0.60f * fog, 0.34f * fog);
        drawCube(roadX + wallX - 0.18f, roadY + 1.72f, z,
                 0.10f, 3.35f, 0.12f, yaw,
                 0.70f * fog, 0.60f * fog, 0.34f * fog);
        drawCube(roadX, roadY + 3.35f, z,
                 roadHalf * 2.0f + 0.65f, 0.10f, 0.14f, yaw,
                 0.74f * fog, 0.68f * fog, 0.46f * fog);
        drawCube(roadX - 2.65f, roadY + 3.27f, z,
                 2.10f, 0.075f, 0.32f, yaw,
                 1.0f * fog, 0.94f * fog, 0.70f * fog);
        drawCube(roadX + 2.65f, roadY + 3.27f, z,
                 2.10f, 0.075f, 0.32f, yaw,
                 1.0f * fog, 0.94f * fog, 0.70f * fog);
    }

    // Recessed emergency/utility boxes break up the otherwise flat walls.
    if ((segmentIndex % 6) == 3) {
        drawCube(roadX - wallX + 0.55f, roadY + 0.92f, z - 1.0f,
                 0.11f, 1.25f, 1.15f, yaw,
                 0.15f * fog, 0.19f * fog, 0.16f * fog);
        drawCube(roadX - wallX + 0.50f, roadY + 1.02f, z - 1.0f,
                 0.03f, 0.44f, 0.40f, yaw,
                 0.72f * fog, 0.15f * fog, 0.055f * fog);
    }
}

void drawTatsumiSourceProxyCluster(float roadX, float roadY, float z,
                                   float yaw, float fog) {
    const auto& building = nr3ds::sourceGeometryProfile(SourceProxyKind::TatsumiBuilding);
    const auto& vending = nr3ds::sourceGeometryProfile(SourceProxyKind::Vending);
    (void)building; (void)vending;

    // A tiny source-asset landmark cluster derived from AREA_TATSUMI_BUILDING
    // LOD0, AREA_TATSUMI_VENDING_LOD0 and the Tatsumi fence set. This is used as
    // distant dressing only; it is not claiming that the current AREA_2,1 route
    // physically contains Tatsumi PA.
    drawCube(roadX + 10.8f, roadY + 3.2f, z - 5.0f,
             8.5f, 6.5f, 8.0f, yaw,
             0.055f * fog, 0.060f * fog, 0.066f * fog);
    drawCube(roadX + 7.2f, roadY + 0.95f, z - 0.8f,
             0.85f, 1.95f, 0.72f, yaw,
             0.58f * fog, 0.12f * fog, 0.08f * fog);
    drawCube(roadX + 7.2f, roadY + 1.05f, z - 1.18f,
             0.56f, 0.55f, 0.04f, yaw,
             0.82f * fog, 0.77f * fog, 0.58f * fog);
    for (int j = 0; j < 3; ++j) {
        drawCube(roadX + 13.3f + float(j) * 0.75f, roadY + 0.48f, z + 2.8f,
                 0.62f, 0.95f, 0.62f, yaw,
                 0.020f * fog, 0.021f * fog, 0.024f * fog);
    }
}

void drawHighway(const nr3ds::Telemetry& s,
                 const std::array<TrafficCar, kTrafficCount>& traffic,
                 const nr3ds::RaceTelemetry& race,
                 float nextGateM,
                 bool braking,
                 float routeProgressM) {
    const float segLen = 10.0f;
    // IMPORTANT: segment identity is absolute. v0.013 regenerated all prop
    // patterns from loop index 0 whenever baseM crossed a 10 m boundary. That
    // made lamps, lane dashes, supports and tunnel ribs visibly "reset" at
    // speed. Keep a stable world segment ID and only slide the camera through it.
    const int firstSeg = std::max(0, int(std::floor(std::max(routeProgressM, 0.0f) / segLen)) - 1);

    const float worldCarX = clampf(s.posX, -kRoadLimit, kRoadLimit);
    const float cameraX = worldCarX * 0.78f;
    const float carX = worldCarX - cameraX;
    const RoadStyle styleNow = gRoute.styleAt(routeProgressM);

    for (int i = 0; i < 36; ++i) {
        const int segId = firstSeg + i;
        const float worldM = float(segId) * segLen;
        if (worldM < 0.0f || worldM > gRoute.totalLengthM()) continue;
        const float aheadM = worldM - routeProgressM;
        if (aheadM < -7.0f) continue;

        const int chunk = gRoute.chunkIndex(worldM);
        if (!gRoute.chunkActive(chunk, routeProgressM)) continue;

        const auto& section = gRoute.sectionAt(worldM);
        const RoadStyle style = section.style;
        const auto rf = gRoute.localFrame(routeProgressM, aheadM);
        if (rf.forwardM < -12.0f || rf.forwardM > 300.0f) continue;
        const float z = -4.7f - rf.forwardM;
        const float center = rf.lateralM;
        const float elev = rf.elevationM * 0.34f;
        const float yaw = rf.yawRad;
        const float roadX = center - cameraX;
        const float roadY = -1.34f + elev;
        const float roadHalf = section.roadWidthM * 0.5f;
        const float baseFog = 1.0f - clampf(std::max(rf.forwardM, 0.0f) / 300.0f, 0.0f, 0.90f);
        // New far segments fade in while still deep in the fog instead of
        // appearing as a whole chunk on one frame.
        const float horizonFade = clampf((300.0f - rf.forwardM) / 32.0f, 0.0f, 1.0f);
        const float fog = baseFog * horizonFade;
        if (fog <= 0.003f) continue;

        float roadR = 0.070f, roadG = 0.078f, roadB = 0.095f;
        float barrierR = 0.28f, barrierG = 0.29f, barrierB = 0.30f;
        if (style == RoadStyle::HighLevel) {
            roadR = 0.062f; roadG = 0.070f; roadB = 0.086f;
            barrierR = 0.25f; barrierG = 0.26f; barrierB = 0.28f;
        } else if (style == RoadStyle::SodiumFence) {
            roadR = 0.090f; roadG = 0.075f; roadB = 0.052f;
            barrierR = 0.31f; barrierG = 0.27f; barrierB = 0.20f;
        } else if (style == RoadStyle::Underpass) {
            roadR = 0.052f; roadG = 0.054f; roadB = 0.058f;
            barrierR = 0.20f; barrierG = 0.20f; barrierB = 0.20f;
        } else if (style == RoadStyle::Tunnel) {
            roadR = 0.105f; roadG = 0.098f; roadB = 0.072f;
            barrierR = 0.62f; barrierG = 0.53f; barrierB = 0.29f;
        }

        // Road deck and close concrete edges. The narrower width, brighter lane
        // paint and close roadside geometry are deliberate reference-driven changes.
        drawCube(roadX, roadY, z, section.roadWidthM, 0.12f, segLen + 0.30f, yaw,
                 roadR * fog, roadG * fog, roadB * fog);
        drawCube(roadX - roadHalf - 0.16f, roadY + 0.58f, z,
                 0.22f, 0.72f, segLen, yaw,
                 barrierR * fog, barrierG * fog, barrierB * fog);
        drawCube(roadX + roadHalf + 0.16f, roadY + 0.58f, z,
                 0.22f, 0.72f, segLen, yaw,
                 barrierR * fog, barrierG * fog, barrierB * fog);

        // First actual source-road geometry pass. AREA_2 double single
        // template.013 is drawn sparsely over the procedural safety deck so
        // holes or material differences cannot break drivability. Its stable
        // segId cadence also prevents the old re-phasing/reset artifact.
        if ((style == RoadStyle::HighLevel || style == RoadStyle::Junction) &&
            (segId % 4) == 0) {
            const float sourceScale = roadHalf * 0.96f;
            drawSourceTriangles(gSourceRoadVbo, kSourceRoadVertsCount,
                                roadX, roadY + 0.08f, z,
                                sourceScale, sourceScale, sourceScale, yaw,
                                roadR * 1.18f * fog, roadG * 1.18f * fog, roadB * 1.14f * fog);
        }

        // Highly visible reflective lane markings. These are a cheap but strong
        // speed cue on the 3DS screen.
        if ((segId & 1) == 0) {
            const float lineGlow = (style == RoadStyle::Tunnel) ? 1.0f : 0.90f;
            drawCube(roadX - kLaneWidth * 0.5f, roadY + 0.14f, z,
                     0.075f, 0.018f, 3.45f, yaw,
                     lineGlow * fog, lineGlow * fog, 0.80f * fog);
            drawCube(roadX + kLaneWidth * 0.5f, roadY + 0.14f, z,
                     0.075f, 0.018f, 3.45f, yaw,
                     lineGlow * fog, lineGlow * fog, 0.80f * fog);
        }

        // Pools of sodium/fluorescent light are geometry overlays rather than
        // dynamic lights. This keeps the Old 3DS renderer cheap.
        if ((segId & 1) == 0 &&
            (style == RoadStyle::SodiumFence || style == RoadStyle::Tunnel ||
             style == RoadStyle::Underpass)) {
            const bool tunnel = style == RoadStyle::Tunnel;
            drawCube(roadX, roadY + 0.10f, z,
                     section.roadWidthM - 0.55f, 0.012f, 3.8f, yaw,
                     (tunnel ? 0.26f : 0.16f) * fog,
                     (tunnel ? 0.22f : 0.11f) * fog,
                     (tunnel ? 0.12f : 0.045f) * fog);
        }

        // Source-geometry proxy pass. These details are scaled from the original
        // Unity road/fence/support mesh families, then reduced to cuboids so they
        // remain viable on Old 3DS.
        if (style == RoadStyle::HighLevel || style == RoadStyle::Junction) {
            if ((segId & 1) == 0) {
                drawSourceFence(roadX, roadY, z, yaw, roadHalf, fog,
                                style == RoadStyle::Junction);
            }
            if ((segId % 6) == 2) {
                drawSourceSupport(roadX, roadY, z, yaw, roadHalf, fog, segId / 6);
            }
        }
        if (style == RoadStyle::Tunnel) {
            drawSourceTunnelModule(roadX, roadY, z, yaw, roadHalf, fog, segId);
        }
        if (style == RoadStyle::Junction && (segId % 12) == 7) {
            drawTatsumiSourceProxyCluster(roadX, roadY, z, yaw, fog);
        }

        // Orange mesh/fence corridor from the highway reference footage.
        if (style == RoadStyle::SodiumFence) {
            if ((segId & 1) == 0) {
                for (int side = -1; side <= 1; side += 2) {
                    const float fx = roadX + float(side) * (roadHalf + 0.42f);
                    drawCube(fx, roadY + 1.62f, z - 2.7f,
                             0.08f, 1.65f, 0.08f, yaw,
                             0.58f * fog, 0.24f * fog, 0.055f * fog);
                    drawCube(fx, roadY + 1.62f, z + 2.7f,
                             0.08f, 1.65f, 0.08f, yaw,
                             0.58f * fog, 0.24f * fog, 0.055f * fog);
                    drawCube(fx, roadY + 2.35f, z,
                             0.08f, 0.08f, segLen, yaw,
                             0.55f * fog, 0.20f * fog, 0.045f * fog);
                }
            }
        }

        // Normal roadside lamps; much closer to the player than the old scene.
        if ((segId % 3) == 1 && style != RoadStyle::Tunnel && style != RoadStyle::Underpass) {
            for (int side = -1; side <= 1; side += 2) {
                const float lx = roadX + float(side) * (roadHalf + 1.05f);
                drawCube(lx, roadY + 1.42f, z, 0.12f, 3.15f, 0.12f, yaw,
                         0.12f * fog, 0.11f * fog, 0.09f * fog);
                drawCube(lx, roadY + 3.02f, z, 0.32f, 0.10f, 0.30f, yaw,
                         0.98f * fog, 0.52f * fog, 0.12f * fog);
            }
        }

        if (style == RoadStyle::HighLevel) {
            // The recovered source route is explicitly the AREA_2,1 HIGH branch.
            // Keep the player on an exposed upper deck with close infrastructure
            // rather than drawing the old temporary overhead-road corridor.
            if ((segId % 4) == 1) {
                for (int side = -1; side <= 1; side += 2) {
                    const float sx = roadX + float(side) * (roadHalf + 1.4f);
                    drawCube(sx, roadY - 2.0f, z, 0.55f, 4.2f, 0.55f, yaw,
                             0.11f * fog, 0.12f * fog, 0.14f * fog);
                }
            }
            if ((segId % 5) == 2) {
                drawCube(roadX - roadHalf - 4.6f, roadY + 1.2f, z - 3.0f,
                         5.0f, 5.0f, 6.0f, yaw,
                         0.025f * fog, 0.035f * fog, 0.060f * fog);
                drawCube(roadX + roadHalf + 4.8f, roadY + 0.7f, z + 2.0f,
                         5.5f, 4.0f, 5.5f, yaw,
                         0.030f * fog, 0.038f * fog, 0.058f * fog);
            }
        } else if (style == RoadStyle::Elevated) {
            // Elevated roadway immediately overhead, with regularly spaced piers.
            drawCube(roadX - 1.0f, roadY + 4.20f, z,
                     13.0f, 0.35f, segLen + 0.25f, yaw,
                     0.12f * fog, 0.13f * fog, 0.14f * fog);
            if ((segId % 4) == 1) {
                drawCube(roadX - roadHalf - 1.7f, roadY + 1.65f, z,
                         0.75f, 3.6f, 0.75f, yaw,
                         0.15f * fog, 0.15f * fog, 0.15f * fog);
                drawCube(roadX + roadHalf + 1.7f, roadY + 1.65f, z,
                         0.75f, 3.6f, 0.75f, yaw,
                         0.15f * fog, 0.15f * fog, 0.15f * fog);
            }
        } else if (style == RoadStyle::Underpass) {
            drawCube(roadX, roadY + 3.55f, z,
                     14.0f, 0.34f, segLen + 0.25f, yaw,
                     0.14f * fog, 0.14f * fog, 0.13f * fog);
            if ((segId & 1) == 0) {
                drawCube(roadX - 3.3f, roadY + 3.34f, z,
                         2.0f, 0.08f, 0.30f, yaw,
                         0.78f * fog, 0.72f * fog, 0.50f * fog);
                drawCube(roadX + 3.3f, roadY + 3.34f, z,
                         2.0f, 0.08f, 0.30f, yaw,
                         0.78f * fog, 0.72f * fog, 0.50f * fog);
            }
        } else if (false && style == RoadStyle::Tunnel) {
            // Superseded by source-derived tunnel proxy above.
            drawCube(roadX - roadHalf - 0.55f, roadY + 1.65f, z,
                     0.85f, 3.7f, segLen + 0.20f, yaw,
                     0.58f * fog, 0.48f * fog, 0.24f * fog);
            drawCube(roadX + roadHalf + 0.55f, roadY + 1.65f, z,
                     0.85f, 3.7f, segLen + 0.20f, yaw,
                     0.58f * fog, 0.48f * fog, 0.24f * fog);
            drawCube(roadX, roadY + 3.62f, z,
                     13.0f, 0.30f, segLen + 0.20f, yaw,
                     0.45f * fog, 0.40f * fog, 0.27f * fog);
            if ((segId & 1) == 0) {
                drawCube(roadX - 2.7f, roadY + 3.38f, z,
                         2.2f, 0.08f, 0.32f, yaw,
                         1.0f * fog, 0.95f * fog, 0.72f * fog);
                drawCube(roadX + 2.7f, roadY + 3.38f, z,
                         2.2f, 0.08f, 0.32f, yaw,
                         1.0f * fog, 0.95f * fog, 0.72f * fog);
            }
        }

        // Buildings move much closer to the roadway in city/open sections.
        if ((segId % 4) == 0 &&
            (style == RoadStyle::Open || style == RoadStyle::DenseCity ||
             style == RoadStyle::Junction || style == RoadStyle::SodiumFence)) {
            const bool dense = style == RoadStyle::DenseCity || style == RoadStyle::Junction;
            const float sideDist = dense ? (roadHalf + 4.0f) : (roadHalf + 7.0f);
            const float hL = (dense ? 6.0f : 4.0f) + float((segId * 7) % 5) * 1.05f;
            const float hR = (dense ? 5.5f : 3.5f) + float((segId * 5) % 6) * 0.95f;
            drawCube(roadX - sideDist, roadY - 0.2f + hL * 0.5f, z - 2.0f,
                     dense ? 4.0f : 5.0f, hL, 6.0f, yaw,
                     0.028f * fog, 0.038f * fog, 0.060f * fog);
            drawCube(roadX + sideDist, roadY - 0.2f + hR * 0.5f, z + 1.0f,
                     dense ? 4.5f : 5.5f, hR, 6.5f, yaw,
                     0.032f * fog, 0.036f * fog, 0.055f * fog);
            if (dense) {
                drawCube(roadX - sideDist + 2.0f, roadY + 2.1f, z - 4.7f,
                         1.5f, 0.08f, 0.05f, yaw,
                         0.72f * fog, 0.48f * fog, 0.12f * fog);
                drawCube(roadX + sideDist - 2.0f, roadY + 1.6f, z + 4.0f,
                         1.4f, 0.08f, 0.05f, yaw,
                         0.22f * fog, 0.42f * fog, 0.68f * fog);
            }
        }

        // Junction/exit language: gantries plus a lightweight diverging ramp.
        if (style == RoadStyle::Junction) {
            const float secT = clampf((worldM - section.startM) / section.lengthM, 0.0f, 1.0f);
            const float rampX = roadX + roadHalf + 2.0f + secT * 6.0f;
            drawCube(rampX, roadY - 0.05f, z,
                     3.2f, 0.10f, segLen + 0.15f, yaw - 0.05f * secT,
                     0.065f * fog, 0.073f * fog, 0.088f * fog);
            if ((segId % 5) == 2) {
                drawCube(roadX, roadY + 2.75f, z,
                         11.8f, 0.13f, 0.18f, yaw,
                         0.16f * fog, 0.17f * fog, 0.17f * fog);
                drawCube(roadX - 2.0f, roadY + 2.48f, z - 0.10f,
                         2.7f, 0.58f, 0.08f, yaw,
                         0.055f * fog, 0.28f * fog, 0.20f * fog);
                drawCube(roadX + 2.2f, roadY + 2.48f, z - 0.10f,
                         2.9f, 0.58f, 0.08f, yaw,
                         0.055f * fog, 0.20f * fog, 0.34f * fog);
            }
        }
    }

    for (const auto& t : traffic) {
        drawTrafficCar(t, cameraX, routeProgressM);
    }

    drawRaceOpponent(race, cameraX, routeProgressM);
    if (race.phase == RacePhase::Countdown) {
        drawRaceGate(16.0f, cameraX, routeProgressM, false, true);
    } else if (race.phase == RacePhase::Racing) {
        const bool finishGate = race.checkpointIndex >= int(RaceSession::kCheckpointCount);
        drawRaceGate(nextGateM, cameraX, routeProgressM, finishGate, false);
    }

    // Cheap speed streaks near the barriers. They appear only at high speed and
    // exploit close geometry instead of a full-screen blur pass.
    const float speedFx = clampf((s.speedKph - 145.0f) / 90.0f, 0.0f, 1.0f);
    if (speedFx > 0.01f) {
        for (int i = 0; i < 5; ++i) {
            const float z = -8.0f - float(i) * 6.0f;
            const float len = 0.8f + speedFx * 2.6f;
            drawCube(-5.15f - cameraX, -0.48f, z, 0.035f, 0.035f, len, 0.0f,
                     0.72f * speedFx, 0.38f * speedFx, 0.10f * speedFx);
            drawCube( 5.15f - cameraX, -0.48f, z - 2.5f, 0.035f, 0.035f, len, 0.0f,
                     0.76f * speedFx, 0.72f * speedFx, 0.54f * speedFx);
        }
    }

    // Headlight pool. Tunnel lighting carries more of the scene, so the fake
    // headlights are deliberately reduced there.
    const float headlight = styleNow == RoadStyle::Tunnel ? 0.10f : 0.21f;
    drawCube(carX, -1.15f, -10.5f, 4.3f, 0.016f, 9.0f, 0.0f,
             headlight, headlight * 0.92f, headlight * 0.56f);

    // Lower/closer chase framing and a slightly fuller low-poly player car.
    const float carYaw = clampf(-s.driftAngleDeg * 0.012f, -0.42f, 0.42f);
    const float carZ = -4.35f;
    drawCube(carX, -0.60f, carZ, 1.64f, 0.43f, 3.38f, carYaw,
             0.78f, 0.035f, 0.025f);
    drawCube(carX, -0.30f, carZ - 0.50f, 1.46f, 0.20f, 1.28f, carYaw,
             0.73f, 0.030f, 0.022f);
    drawCube(carX, -0.18f, carZ - 0.28f, 1.20f, 0.36f, 1.35f, carYaw,
             0.045f, 0.075f, 0.10f);
    drawCube(carX, -0.38f, carZ + 1.42f, 1.58f, 0.18f, 0.30f, carYaw,
             0.58f, 0.025f, 0.020f);

    // Wheels.
    for (int side = -1; side <= 1; side += 2) {
        drawCube(carX + float(side) * 0.86f, -0.77f, carZ + 1.02f,
                 0.21f, 0.34f, 0.50f, carYaw,
                 0.014f, 0.014f, 0.017f);
        drawCube(carX + float(side) * 0.86f, -0.77f, carZ - 1.02f,
                 0.21f, 0.34f, 0.50f, carYaw,
                 0.014f, 0.014f, 0.017f);
    }

    const float brakeGlow = braking ? 1.0f : 0.66f;
    drawCube(carX - 0.48f, -0.46f, carZ + 1.72f,
             0.25f, 0.12f, 0.07f, carYaw,
             brakeGlow, 0.022f, 0.008f);
    drawCube(carX + 0.48f, -0.46f, carZ + 1.72f,
             0.25f, 0.12f, 0.07f, carYaw,
             brakeGlow, 0.022f, 0.008f);
}


void drawGaragePart(float baseX, float baseY, float baseZ,
                    float localX, float localY, float localZ,
                    float sx, float sy, float sz,
                    float yaw,
                    float r, float g, float b) {
    const float c = std::cos(yaw);
    const float sn = std::sin(yaw);
    const float worldX = baseX + localX * c + localZ * sn;
    const float worldZ = baseZ - localX * sn + localZ * c;
    drawCube(worldX, baseY + localY, worldZ, sx, sy, sz, yaw, r, g, b);
}

void drawShowroomCar(const GarageState& garage, float spin) {
    const float baseX = 0.15f;
    const float baseY = -0.62f;
    const float baseZ = -7.65f;
    const float yaw = 0.34f + std::sin(spin * 0.70f) * 0.18f;

    const float tireAccent = 0.22f + 0.10f * float(garage.tireLevel());
    const float turboAccent = 0.22f + 0.10f * float(garage.turboLevel());
    const float engineAccent = 0.72f + 0.055f * float(garage.engineLevel());

    // Low-poly 1980s/1990s Japanese-coupe-inspired silhouette. Everything is
    // still generated from cuboids so this remains tiny and Old-3DS friendly.
    drawGaragePart(baseX, baseY, baseZ, 0.0f, 0.05f, 0.0f,
                   1.72f, 0.34f, 3.48f, yaw,
                   engineAccent, 0.050f, 0.035f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, -0.13f, 0.10f,
                   1.80f, 0.17f, 3.65f, yaw,
                   0.42f, 0.030f, 0.026f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, 0.23f, 0.92f,
                   1.55f, 0.16f, 1.05f, yaw,
                   engineAccent * 0.96f, 0.045f, 0.032f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, 0.46f, -0.22f,
                   1.24f, 0.49f, 1.42f, yaw,
                   0.075f, 0.105f, 0.125f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, 0.69f, -0.24f,
                   1.02f, 0.10f, 1.08f, yaw,
                   0.34f, 0.040f, 0.035f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, 0.17f, -1.44f,
                   1.55f, 0.18f, 0.52f, yaw,
                   engineAccent * 0.92f, 0.042f, 0.030f);

    // Bumpers / skirts.
    drawGaragePart(baseX, baseY, baseZ, 0.0f, -0.05f, 1.78f,
                   1.76f, 0.20f, 0.20f, yaw,
                   0.30f, 0.025f, 0.023f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, -0.04f, -1.78f,
                   1.72f, 0.18f, 0.18f, yaw,
                   0.30f, 0.025f, 0.023f);
    drawGaragePart(baseX, baseY, baseZ, -0.91f, -0.06f, 0.0f,
                   0.11f, 0.16f, 2.80f, yaw,
                   0.20f, 0.020f, 0.020f);
    drawGaragePart(baseX, baseY, baseZ,  0.91f, -0.06f, 0.0f,
                   0.11f, 0.16f, 2.80f, yaw,
                   0.20f, 0.020f, 0.020f);

    // Wheels and hubs.
    const float wheelZ[2] = {1.12f, -1.10f};
    for (int axle = 0; axle < 2; ++axle) {
        for (int side = -1; side <= 1; side += 2) {
            const float lx = float(side) * 0.91f;
            drawGaragePart(baseX, baseY, baseZ, lx, -0.18f, wheelZ[axle],
                           0.25f, 0.47f, 0.62f, yaw,
                           0.018f, 0.018f, 0.020f);
            drawGaragePart(baseX, baseY, baseZ, lx, -0.18f, wheelZ[axle],
                           0.27f, 0.26f, 0.26f, yaw,
                           tireAccent, tireAccent, tireAccent);
        }
    }

    // Front lamps, marker strip, plate and upgrade-visible intercooler.
    drawGaragePart(baseX, baseY, baseZ, -0.55f, 0.13f, 1.79f,
                   0.38f, 0.11f, 0.08f, yaw,
                   0.95f, 0.86f, 0.58f);
    drawGaragePart(baseX, baseY, baseZ,  0.55f, 0.13f, 1.79f,
                   0.38f, 0.11f, 0.08f, yaw,
                   0.95f, 0.86f, 0.58f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, -0.01f, 1.82f,
                   0.52f, 0.12f, 0.06f, yaw,
                   0.10f, 0.13f + turboAccent * 0.20f, 0.16f + turboAccent * 0.28f);
    drawGaragePart(baseX, baseY, baseZ, 0.0f, -0.18f, 1.90f,
                   0.38f, 0.16f, 0.04f, yaw,
                   0.72f, 0.74f, 0.70f);

    // Rear lamps give the rotating display a readable back side too.
    drawGaragePart(baseX, baseY, baseZ, -0.50f, 0.09f, -1.79f,
                   0.34f, 0.11f, 0.07f, yaw,
                   0.90f, 0.025f, 0.012f);
    drawGaragePart(baseX, baseY, baseZ,  0.50f, 0.09f, -1.79f,
                   0.34f, 0.11f, 0.07f, yaw,
                   0.90f, 0.025f, 0.012f);
}

void drawGarageShelf(float x, float z, float width, float height, float depth) {
    const float post = 0.10f;
    drawCube(x - width * 0.5f, 0.35f, z, post, height, depth, 0.0f,
             0.18f, 0.16f, 0.12f);
    drawCube(x + width * 0.5f, 0.35f, z, post, height, depth, 0.0f,
             0.18f, 0.16f, 0.12f);
    for (int i = 0; i < 4; ++i) {
        const float y = -0.82f + float(i) * 0.72f;
        drawCube(x, y, z, width, 0.08f, depth, 0.0f,
                 0.16f, 0.15f, 0.13f);
        // Sparse product boxes so the shelves read as a parts store without
        // requiring texture atlases yet.
        if (i < 3) {
            drawCube(x - width * 0.23f, y + 0.27f, z - 0.04f,
                     width * 0.24f, 0.38f, depth * 0.70f, 0.0f,
                     0.58f, 0.48f, 0.12f);
            drawCube(x + width * 0.16f, y + 0.23f, z + 0.02f,
                     width * 0.28f, 0.30f, depth * 0.64f, 0.0f,
                     0.26f, 0.30f, 0.34f);
        }
    }
}

void drawTireStack(float x, float z, int count) {
    for (int i = 0; i < count; ++i) {
        drawCube(x, -1.02f + float(i) * 0.36f, z,
                 0.82f, 0.30f, 0.82f, 0.0f,
                 0.025f, 0.025f, 0.028f);
        drawCube(x, -1.01f + float(i) * 0.36f, z,
                 0.35f, 0.32f, 0.35f, 0.0f,
                 0.12f, 0.12f, 0.13f);
    }
}

void drawGarageScene(const GarageState& garage, float spin) {
    // v0.012 showroom pass: bright Japanese tuning-shop proportions inspired by
    // the reference mood, but built from original low-poly geometry and colors.
    const float wallR = 0.36f, wallG = 0.34f, wallB = 0.27f;

    // Bright tiled floor, back wall, side walls and ceiling.
    drawCube(0.0f, -1.40f, -11.2f, 16.5f, 0.10f, 24.0f, 0.0f,
             0.63f, 0.61f, 0.52f);
    drawCube(0.0f, 3.0f, -22.2f, 16.5f, 9.2f, 0.20f, 0.0f,
             wallR, wallG, wallB);
    drawCube(-8.15f, 1.8f, -12.0f, 0.20f, 7.0f, 21.0f, 0.0f,
             0.25f, 0.24f, 0.21f);
    drawCube( 8.15f, 1.8f, -12.0f, 0.20f, 7.0f, 21.0f, 0.0f,
             0.25f, 0.24f, 0.21f);
    drawCube(0.0f, 4.35f, -11.5f, 16.5f, 0.12f, 22.5f, 0.0f,
             0.44f, 0.43f, 0.38f);

    // Ceiling tile beams and fluorescent panels.
    for (int row = 0; row < 4; ++row) {
        const float z = -5.2f - float(row) * 4.5f;
        drawCube(0.0f, 4.20f, z, 15.5f, 0.05f, 0.08f, 0.0f,
                 0.22f, 0.22f, 0.20f);
        for (int col = -1; col <= 1; ++col) {
            drawCube(float(col) * 4.4f, 4.11f, z,
                     2.6f, 0.07f, 0.42f, 0.0f,
                     0.92f, 0.92f, 0.82f);
        }
    }

    // Branded back-wall color band. This deliberately avoids copying the PC
    // game's exact logo/art while hitting the same warm tuner-shop atmosphere.
    drawCube(0.0f, 3.15f, -21.96f, 13.2f, 1.15f, 0.10f, 0.0f,
             0.68f, 0.54f, 0.12f);
    drawCube(0.0f, 3.67f, -21.88f, 13.2f, 0.14f, 0.06f, 0.0f,
             0.72f, 0.06f, 0.045f);
    drawCube(-4.2f, 3.67f, -21.80f, 3.4f, 0.13f, 0.05f, 0.0f,
             0.86f, 0.86f, 0.80f);
    drawCube( 4.2f, 3.67f, -21.80f, 3.4f, 0.13f, 0.05f, 0.0f,
             0.86f, 0.86f, 0.80f);
    drawCube(0.0f, 3.12f, -21.78f, 5.6f, 0.66f, 0.05f, 0.0f,
             0.80f, 0.80f, 0.73f);
    drawCube(0.0f, 3.12f, -21.71f, 4.7f, 0.40f, 0.04f, 0.0f,
             0.68f, 0.08f, 0.055f);

    // Long parts shelves, yellow aisle posts and small hanging banner blocks.
    drawGarageShelf(-5.7f, -16.8f, 3.4f, 3.4f, 1.0f);
    drawGarageShelf(-5.7f, -12.3f, 3.4f, 3.4f, 1.0f);
    drawGarageShelf( 5.7f, -16.8f, 3.4f, 3.4f, 1.0f);
    drawGarageShelf( 5.7f, -12.3f, 3.4f, 3.4f, 1.0f);
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 3; ++i) {
            const float x = float(side) * 4.05f;
            const float z = -11.0f - float(i) * 3.6f;
            drawCube(x, 0.35f, z, 0.16f, 3.3f, 0.16f, 0.0f,
                     0.78f, 0.60f, 0.10f);
            drawCube(x, 1.62f, z + 0.02f, 0.46f, 0.64f, 0.08f, 0.0f,
                     0.70f, 0.12f, 0.07f);
        }
    }

    // Vending-machine silhouette on the left.
    drawCube(-6.55f, -0.10f, -7.0f, 1.15f, 2.35f, 0.72f, 0.0f,
             0.18f, 0.20f, 0.22f);
    drawCube(-6.55f, 0.58f, -6.60f, 0.90f, 0.72f, 0.05f, 0.0f,
             0.68f, 0.80f, 0.72f);
    drawCube(-6.55f, -0.46f, -6.60f, 0.78f, 0.55f, 0.05f, 0.0f,
             0.52f, 0.12f, 0.08f);

    // Tire display on the right and a low tool chest.
    drawTireStack(6.45f, -7.0f, 4);
    drawTireStack(5.45f, -7.2f, 3);
    drawCube(6.0f, -0.72f, -10.0f, 2.5f, 0.95f, 0.82f, 0.0f,
             0.20f, 0.055f, 0.045f);
    for (int i = 0; i < 3; ++i) {
        drawCube(6.0f, -0.44f + float(i) * 0.23f, -9.57f,
                 2.1f, 0.06f, 0.04f, 0.0f,
                 0.34f, 0.08f, 0.055f);
    }

    // Dark textured-looking display mat and thin warm accent edge.
    drawCube(0.15f, -1.26f, -7.65f, 5.5f, 0.05f, 7.4f, 0.0f,
             0.07f, 0.065f, 0.060f);
    drawCube(0.15f, -1.19f, -5.00f, 5.6f, 0.035f, 0.10f, 0.0f,
             0.42f, 0.08f, 0.06f);

    drawShowroomCar(garage, spin);
}

void printUpgradeLine(int row, bool selected, const char* name,
                      int level, int nextCost) {
    const char* meter = level <= 0 ? "[---]" : (level == 1 ? "[#--]" : (level == 2 ? "[##-]" : "[###]"));
    if (nextCost < 0) {
        std::printf("\x1b[%d;1H%c %-8s %-5s Lv%d  MAX       \x1b[K",
                    row, selected ? '>' : ' ', name, meter, level);
    } else {
        std::printf("\x1b[%d;1H%c %-8s %-5s Lv%d  $%-5d    \x1b[K",
                    row, selected ? '>' : ' ', name, meter, level, nextCost);
    }
}

void drawGarageHud(const GarageState& garage, int selection, const char* status) {
    const auto cfg = garage.makeVehicleConfig();
    std::printf("\x1b[1;1HNR TUNING // v0.012 SHOWROOM      \x1b[K");
    std::printf("\x1b[2;1H$%-6d   RECORD %dW / %dL            \x1b[K",
                garage.cash(), garage.wins(), garage.losses());
    std::printf("\x1b[3;1H%.0fNm  +%.0fhp turbo  grip %.2f/%.2f\x1b[K",
                cfg.baseTorqueNm, cfg.turboMaxExtraHp,
                cfg.frontGrip, cfg.rearGrip);

    std::printf("\x1b[5;1H-- PARTS ---------------------------\x1b[K");
    printUpgradeLine(6, selection == 0, "ENGINE", garage.engineLevel(),
                     garage.nextCost(UpgradeKind::Engine));
    printUpgradeLine(7, selection == 1, "TURBO", garage.turboLevel(),
                     garage.nextCost(UpgradeKind::Turbo));
    printUpgradeLine(8, selection == 2, "TIRES", garage.tireLevel(),
                     garage.nextCost(UpgradeKind::Tires));

    std::printf("\x1b[10;1H-- TRANSMISSION --------------------\x1b[K");
    std::printf("\x1b[11;1H%c FINAL DRIVE     < %.2f >       \x1b[K",
                selection == 3 ? '>' : ' ', garage.finalDrive());
    for (int i = 0; i < 6; ++i) {
        std::printf("\x1b[%d;1H%c GEAR %d          < %.2f >       \x1b[K",
                    12 + i, selection == 4 + i ? '>' : ' ', i + 1,
                    garage.gearRatio(std::size_t(i)));
    }

    std::printf("\x1b[19;1H-- CONTROLS ------------------------\x1b[K");
    std::printf("\x1b[20;1HUD select  A buy   LR tune          \x1b[K");
    std::printf("\x1b[21;1HC-Pad rotates display   Y EXPRESSWAY\x1b[K");
    std::printf("\x1b[23;1H%-38s\x1b[K", status ? status : "");
    std::printf("\x1b[25;1HSTART exits                        \x1b[K");
}

} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    consoleInit(GFX_BOTTOM, nullptr);

    C3D_RenderTarget* top = C3D_RenderTargetCreate(
        240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    sceneInit();
    GarageState garage;
    Vehicle car(garage.makeVehicleConfig());
    RaceSession race;
    auto traffic = makeTraffic();

    GameMode mode = GameMode::Garage;
    int garageSelection = 0;
    const char* garageStatus = "Tune the build, rotate the car, then press Y.";
    float garageSpin = 0.0f;

    int collisionCount = 0;
    int wallHitCount = 0;
    int rivalHitCount = 0;
    float collisionCooldown = 0.0f;
    float opponentCollisionCooldown = 0.0f;
    float wallCooldown = 0.0f;
    bool rewardGiven = false;

    auto resetRace = [&]() {
        car.setConfig(garage.makeVehicleConfig());
        race.reset();
        traffic = makeTraffic();
        collisionCount = 0;
        wallHitCount = 0;
        rivalHitCount = 0;
        collisionCooldown = 0.0f;
        opponentCollisionCooldown = 0.0f;
        wallCooldown = 0.0f;
        rewardGiven = false;
    };

    consoleClear();

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        const u32 held = hidKeysHeld();
        if (down & KEY_START) break;

        constexpr float dt = 1.0f / 60.0f;

        if (mode == GameMode::Garage) {
            if (down & KEY_DUP) {
                garageSelection = (garageSelection + 9) % 10;
                garageStatus = "";
            }
            if (down & KEY_DDOWN) {
                garageSelection = (garageSelection + 1) % 10;
                garageStatus = "";
            }

            bool configChanged = false;
            if (down & KEY_A) {
                bool bought = false;
                if (garageSelection == 0) bought = garage.purchase(UpgradeKind::Engine);
                if (garageSelection == 1) bought = garage.purchase(UpgradeKind::Turbo);
                if (garageSelection == 2) bought = garage.purchase(UpgradeKind::Tires);
                if (garageSelection <= 2) {
                    configChanged = bought;
                    if (bought) garageStatus = "Upgrade installed.";
                    else {
                        const UpgradeKind kind = garageSelection == 0 ? UpgradeKind::Engine
                            : (garageSelection == 1 ? UpgradeKind::Turbo : UpgradeKind::Tires);
                        garageStatus = garage.nextCost(kind) < 0 ? "That upgrade is already MAX."
                                                                 : "Not enough cash.";
                    }
                }
            }

            const float tuneDir = (down & KEY_DRIGHT) ? 1.0f : ((down & KEY_DLEFT) ? -1.0f : 0.0f);
            if (tuneDir != 0.0f) {
                if (garageSelection == 3) {
                    garage.adjustFinalDrive(0.05f * tuneDir);
                    configChanged = true;
                    garageStatus = "Final drive adjusted.";
                } else if (garageSelection >= 4) {
                    garage.adjustGearRatio(std::size_t(garageSelection - 4), 0.05f * tuneDir);
                    configChanged = true;
                    garageStatus = "Gear ratio adjusted.";
                }
            }

            if (configChanged) {
                car.setConfig(garage.makeVehicleConfig());
            }

            if (down & KEY_Y) {
                resetRace();
                mode = GameMode::Race;
                consoleClear();
                continue;
            }

            circlePosition garageCp{};
            hidCircleRead(&garageCp);
            const float viewInput = clampf(float(garageCp.dx) / 156.0f, -1.0f, 1.0f);
            if (std::fabs(viewInput) > 0.08f)
                garageSpin += viewInput * dt * 2.2f;
            else
                garageSpin += dt * 0.22f;

            Mtx_PerspTilt(&gProjection, C3D_AngleFromDegrees(55.0f),
                          C3D_AspectRatioTop, 0.05f, 80.0f, false);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x0A0907FF, 0);
            C3D_FrameDrawOn(top);
            C3D_BindProgram(&gProgram);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocProjection, &gProjection);
            drawGarageScene(garage, garageSpin);
            C3D_FrameEnd(0);

            drawGarageHud(garage, garageSelection, garageStatus);
            continue;
        }

        // Race mode.
        if (race.telemetry().phase == RacePhase::Finished && (down & KEY_Y)) {
            mode = GameMode::Garage;
            garageStatus = "Race reward banked. Tune or start again.";
            consoleClear();
            continue;
        }

        if (down & KEY_SELECT) {
            resetRace();
        }

        circlePosition cp{};
        hidCircleRead(&cp);

        InputState in{};
        in.throttle = (held & KEY_A) ? 1.0f : 0.0f;
        in.brake = (held & KEY_B) ? 1.0f : 0.0f;
        in.handbrake = (held & KEY_X) ? 1.0f : 0.0f;
        in.steer = clampf(float(cp.dx) / 156.0f, -1.0f, 1.0f);
        in.shiftDown = (down & KEY_L) != 0;
        in.shiftUp = (down & KEY_R) != 0;

        collisionCooldown = std::max(0.0f, collisionCooldown - dt);
        opponentCollisionCooldown = std::max(0.0f, opponentCollisionCooldown - dt);
        wallCooldown = std::max(0.0f, wallCooldown - dt);

        InputState simIn = in;
        if (race.telemetry().phase == RacePhase::Countdown) {
            simIn.throttle = 0.0f;
            simIn.brake = 0.55f;
            simIn.handbrake = 0.0f;
        }

        car.step(simIn, dt);
        if (std::fabs(car.telemetry().posX) > kRoadLimit && wallCooldown <= 0.0f) {
            ++wallHitCount;
            wallCooldown = 0.5f;
        }
        car.constrainLateral(-kRoadLimit, kRoadLimit, 0.93f);

        race.update(car.telemetry().speedKph, dt);
        updateTraffic(traffic, car.telemetry(), dt);
        if (resolveTrafficCollision(car, traffic, collisionCooldown)) {
            ++collisionCount;
        }
        if (resolveOpponentCollision(car, race, opponentCollisionCooldown)) {
            ++rivalHitCount;
        }

        const auto& s = car.telemetry();
        const auto& rt = race.telemetry();
        if (rt.phase == RacePhase::Finished && !rewardGiven) {
            garage.rewardRace(rt.playerWon);
            rewardGiven = true;
        }
        updateProjection(s.speedKph);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x020204FF, 0);
        C3D_FrameDrawOn(top);
        C3D_BindProgram(&gProgram);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocProjection, &gProjection);
        drawHighway(s, traffic, rt, race.nextGateDistanceM(), in.brake > 0.1f, rt.playerProgressM);
        C3D_FrameEnd(0);

        if (rt.phase == RacePhase::Countdown) {
            const int count = std::max(1, int(std::ceil(rt.countdown)));
            std::printf("\x1b[1;1HNR3DS v0.014 - SMOOTH SOURCE C1    \x1b[K");
            std::printf("\x1b[5;1HRACE: GET READY  %d          \x1b[K", count);
        } else if (rt.phase == RacePhase::Racing) {
            std::printf("\x1b[1;1HNR3DS v0.014 - SMOOTH SOURCE C1    \x1b[K");
            std::printf("\x1b[5;1HRACE: GO  CP %d/%d             \x1b[K",
                        rt.checkpointIndex, int(RaceSession::kCheckpointCount));
        } else {
            std::printf("\x1b[1;1HNR3DS v0.014 - SMOOTH SOURCE C1    \x1b[K");
            std::printf("\x1b[5;1HRESULT: %s                 \x1b[K",
                        rt.playerWon ? "YOU WIN +$1000" : "RIVAL WINS +$300");
            std::printf("\x1b[6;1HY=garage  SELECT=retry          \x1b[K");
        }

        if (rt.phase != RacePhase::Finished) {
            std::printf("\x1b[6;1HTime: %6.2f  Dist: %4.0f/%3.0fm \x1b[K",
                        rt.elapsed, rt.playerProgressM, RaceSession::kCourseLengthM);
        }
        std::printf("\x1b[7;1HRival gap: %+6.1fm  %5.0f km/h\x1b[K",
                    rt.gapM, rt.opponentSpeedKph);
        std::printf("\x1b[8;1HCash: $%d  W/L: %d/%d           \x1b[K",
                    garage.cash(), garage.wins(), garage.losses());
        std::printf("\x1b[10;1HSpeed: %6.1f km/h Gear:%d    \x1b[K", s.speedKph, s.gear);
        std::printf("\x1b[11;1HRPM:%6.0f Turbo:%4.2f Slip:%4.2f\x1b[K",
                    s.rpm, s.turboSpool, s.rearSlip);
        std::printf("\x1b[12;1HSteer raw/filt: %5.2f/%5.2f \x1b[K", in.steer, s.steerFiltered);
        std::printf("\x1b[13;1HHits traffic/rival/wall: %d/%d/%d\x1b[K",
                    collisionCount, rivalHitCount, wallHitCount);
        const auto& currentSection = gRoute.sectionAt(rt.playerProgressM);
        std::printf("\x1b[14;1HZone: %-12s chunks %d-%d\x1b[K",
                    nr3ds::ExpresswayRoute::styleName(currentSection.style),
                    gRoute.activeChunkFirst(rt.playerProgressM),
                    gRoute.activeChunkLast(rt.playerProgressM));
        std::printf("\x1b[15;1HSource: %-18s\x1b[K", currentSection.sourceName);
        std::printf("\x1b[16;1HGeo: source LOD + stable stream\x1b[K");
        std::printf("\x1b[18;1HCPU: %6.2f%% GPU: %6.2f%%\x1b[K",
                    C3D_GetProcessingTime() * 6.0f,
                    C3D_GetDrawingTime() * 6.0f);
    }

    sceneExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
