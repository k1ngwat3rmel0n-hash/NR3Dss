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

using nr3ds::InputState;
using nr3ds::Vehicle;
using nr3ds::RaceSession;
using nr3ds::RacePhase;
using nr3ds::GarageState;
using nr3ds::UpgradeKind;

namespace {

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

constexpr u32 CLEAR_COLOR = 0x02040AFF;
constexpr int kCubeVerts = 36;
constexpr int kTrafficCount = 6;
constexpr float kLaneWidth = 3.8f;

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

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * clampf(t, 0.0f, 1.0f);
}

float laneXFor(int laneIndex) {
    return float(laneIndex - 1) * kLaneWidth;
}

float roadCenterAbs(float worldM) {
    return std::sin(worldM * 0.0085f) * 2.15f
         + std::sin(worldM * 0.0031f + 0.8f) * 0.95f;
}

float roadCenterRelative(float playerWorldM, float aheadM) {
    return roadCenterAbs(playerWorldM + aheadM) - roadCenterAbs(playerWorldM);
}

float roadYawRelative(float playerWorldM, float aheadM) {
    const float back = roadCenterRelative(playerWorldM, aheadM - 2.5f);
    const float front = roadCenterRelative(playerWorldM, aheadM + 2.5f);
    return std::atan2(front - back, 5.0f);
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

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, gVbo, sizeof(Vertex), 1, 0x0);

    C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both,
                  GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void sceneExit() {
    if (gVbo) linearFree(gVbo);
    shaderProgramFree(&gProgram);
    if (gShaderDvlb) DVLB_Free(gShaderDvlb);
}

void updateProjection(float speedKph) {
    const float speedT = clampf(speedKph / 220.0f, 0.0f, 1.0f);
    const float fov = lerpf(60.0f, 68.0f, speedT);
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
    const float playerX = clampf(s.posX, -5.1f, 5.1f);

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
    const float playerX = clampf(s.posX, -5.1f, 5.1f);

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

    const float center = roadCenterRelative(playerWorldM, t.distanceM);
    const float yaw = roadYawRelative(playerWorldM, t.distanceM);
    const float z = -5.2f - t.distanceM;
    const float fog = 1.0f - clampf(t.distanceM / 220.0f, 0.0f, 0.82f);
    const float x = t.laneX + center - cameraX;

    drawCube(x, -0.66f, z, 1.45f, 0.38f, 2.85f, yaw,
             t.r * fog, t.g * fog, t.b * fog);
    drawCube(x, -0.29f, z - 0.12f, 1.08f, 0.30f, 1.25f, yaw,
             0.05f * fog, 0.08f * fog, 0.11f * fog);

    drawCube(x - 0.44f, -0.51f, z + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
    drawCube(x + 0.44f, -0.51f, z + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
}

void drawRaceOpponent(const nr3ds::RaceTelemetry& race,
                      float cameraX, float playerWorldM) {
    const float relM = race.opponentProgressM - race.playerProgressM;
    if (relM < -4.5f || relM > 195.0f) return;

    const float center = roadCenterRelative(playerWorldM, relM);
    const float yaw = roadYawRelative(playerWorldM, relM);
    const float z = -5.2f - relM;
    const float fog = 1.0f - clampf(std::max(relM, 0.0f) / 220.0f, 0.0f, 0.82f);
    const float x = race.opponentLaneX + center - cameraX;

    drawCube(x, -0.64f, z, 1.52f, 0.40f, 3.00f, yaw,
             0.78f * fog, 0.26f * fog, 0.055f * fog);
    drawCube(x, -0.26f, z - 0.15f, 1.08f, 0.31f, 1.30f, yaw,
             0.055f * fog, 0.075f * fog, 0.095f * fog);
    drawCube(x - 0.46f, -0.49f, z + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.03f * fog, 0.01f * fog);
    drawCube(x + 0.46f, -0.49f, z + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.03f * fog, 0.01f * fog);
}

void drawRaceGate(float aheadM, float cameraX, float playerWorldM,
                  bool finishGate, bool countdownGate) {
    if (aheadM < 1.5f || aheadM > 190.0f) return;

    const float center = roadCenterRelative(playerWorldM, aheadM);
    const float yaw = roadYawRelative(playerWorldM, aheadM);
    const float z = -5.2f - aheadM;
    const float fog = 1.0f - clampf(aheadM / 220.0f, 0.0f, 0.82f);
    const float roadX = center - cameraX;

    float r = 0.08f, g = 0.62f, b = 0.78f;
    if (finishGate) { r = 0.84f; g = 0.70f; b = 0.12f; }
    if (countdownGate) { r = 0.82f; g = 0.08f; b = 0.04f; }

    drawCube(roadX - 5.7f, 0.75f, z, 0.18f, 3.7f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX + 5.7f, 0.75f, z, 0.18f, 3.7f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX, 2.65f, z, 11.7f, 0.18f, 0.18f, yaw,
             0.18f * fog, 0.22f * fog, 0.24f * fog);
    drawCube(roadX, 2.35f, z - 0.12f, 4.8f, 0.55f, 0.10f, yaw,
             r * fog, g * fog, b * fog);
}

bool resolveOpponentCollision(Vehicle& car, RaceSession& race, float& cooldown) {
    if (cooldown > 0.0f || race.telemetry().phase != RacePhase::Racing) return false;
    const auto& s = car.telemetry();
    const auto& rt = race.telemetry();
    const float relM = rt.opponentProgressM - rt.playerProgressM;
    const float playerX = clampf(s.posX, -5.1f, 5.1f);
    if (std::fabs(relM) < 2.7f && std::fabs(rt.opponentLaneX - playerX) < 1.35f) {
        const float kick = (playerX <= rt.opponentLaneX) ? -0.75f : 0.75f;
        car.applyImpact(0.78f, kick);
        race.applyOpponentImpact(0.82f, 2.2f);
        cooldown = 0.65f;
        return true;
    }
    return false;
}

void drawHighway(const nr3ds::Telemetry& s,
                 const std::array<TrafficCar, kTrafficCount>& traffic,
                 const nr3ds::RaceTelemetry& race,
                 float nextGateM,
                 bool braking) {
    const float segLen = 10.0f;
    const float scroll = std::fmod(std::fabs(s.posY), segLen);

    const float worldCarX = clampf(s.posX, -5.1f, 5.1f);
    const float cameraX = worldCarX * 0.84f;
    const float carX = worldCarX - cameraX;

    for (int i = 0; i < 25; ++i) {
        const float z = -7.0f - float(i) * segLen + scroll;
        const float aheadM = std::max(0.0f, -z - 5.2f);
        const float center = roadCenterRelative(s.posY, aheadM);
        const float yaw = roadYawRelative(s.posY, aheadM);
        const float roadX = center - cameraX;
        const float fog = 1.0f - clampf(float(i) / 27.0f, 0.0f, 0.87f);

        drawCube(roadX, -1.35f, z, 12.0f, 0.12f, segLen + 0.25f, yaw,
                 0.085f * fog, 0.095f * fog, 0.12f * fog);
        drawCube(roadX - 6.25f, -0.72f, z, 0.20f, 0.70f, segLen, yaw,
                 0.25f * fog, 0.27f * fog, 0.31f * fog);
        drawCube(roadX + 6.25f, -0.72f, z, 0.20f, 0.70f, segLen, yaw,
                 0.25f * fog, 0.27f * fog, 0.31f * fog);

        if ((i & 1) == 0) {
            drawCube(roadX - 1.90f, -1.20f, z, 0.09f, 0.025f, 3.3f, yaw,
                     0.84f * fog, 0.82f * fog, 0.68f * fog);
            drawCube(roadX + 1.90f, -1.20f, z, 0.09f, 0.025f, 3.3f, yaw,
                     0.84f * fog, 0.82f * fog, 0.68f * fog);
        }

        if ((i % 3) == 1) {
            drawCube(roadX - 8.0f, 0.1f, z, 0.18f, 2.6f, 0.18f, yaw,
                     0.10f * fog, 0.14f * fog, 0.18f * fog);
            drawCube(roadX + 8.0f, 0.1f, z, 0.18f, 2.6f, 0.18f, yaw,
                     0.10f * fog, 0.14f * fog, 0.18f * fog);
            drawCube(roadX - 8.0f, 1.5f, z, 0.35f, 0.12f, 0.35f, yaw,
                     0.90f * fog, 0.66f * fog, 0.24f * fog);
            drawCube(roadX + 8.0f, 1.5f, z, 0.35f, 0.12f, 0.35f, yaw,
                     0.90f * fog, 0.66f * fog, 0.24f * fog);
        }

        if ((i % 4) == 0) {
            const float hL = 3.0f + float((i * 7) % 5) * 0.8f;
            const float hR = 2.5f + float((i * 5) % 6) * 0.75f;
            drawCube(roadX - 13.0f, -1.0f + hL * 0.5f, z - 3.0f,
                     4.0f, hL, 5.0f, yaw,
                     0.035f * fog, 0.055f * fog, 0.085f * fog);
            drawCube(roadX + 13.0f, -1.0f + hR * 0.5f, z + 1.5f,
                     4.5f, hR, 5.5f, yaw,
                     0.04f * fog, 0.05f * fog, 0.075f * fog);
        }

        if ((i % 9) == 5) {
            drawCube(roadX, 2.5f, z, 13.5f, 0.15f, 0.20f, yaw,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube(roadX - 5.8f, 0.7f, z, 0.16f, 3.5f, 0.16f, yaw,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube(roadX + 5.8f, 0.7f, z, 0.16f, 3.5f, 0.16f, yaw,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube(roadX - 1.8f, 2.25f, z - 0.12f, 2.5f, 0.65f, 0.12f, yaw,
                     0.08f * fog, 0.26f * fog, 0.20f * fog);
            drawCube(roadX + 2.2f, 2.25f, z - 0.12f, 2.8f, 0.65f, 0.12f, yaw,
                     0.08f * fog, 0.20f * fog, 0.30f * fog);
        }
    }

    for (const auto& t : traffic) {
        drawTrafficCar(t, cameraX, s.posY);
    }

    drawRaceOpponent(race, cameraX, s.posY);
    if (race.phase == RacePhase::Countdown) {
        drawRaceGate(16.0f, cameraX, s.posY, false, true);
    } else if (race.phase == RacePhase::Racing) {
        const bool finishGate = race.checkpointIndex >= int(RaceSession::kCheckpointCount);
        drawRaceGate(nextGateM, cameraX, s.posY, finishGate, false);
    }

    // Cheap geometry-only headlight pool.
    drawCube(carX, -1.17f, -11.5f, 4.7f, 0.018f, 9.5f, 0.0f,
             0.22f, 0.20f, 0.12f);

    const float roadYawHere = roadYawRelative(s.posY, 0.0f);
    const float carYaw = roadYawHere + clampf(-s.driftAngleDeg * 0.012f, -0.45f, 0.45f);
    drawCube(carX, -0.63f, -5.2f, 1.55f, 0.42f, 3.2f, carYaw,
             0.78f, 0.035f, 0.025f);
    drawCube(carX, -0.22f, -5.34f, 1.18f, 0.34f, 1.42f, carYaw,
             0.055f, 0.10f, 0.14f);

    drawCube(carX - 0.83f, -0.78f, -4.15f, 0.22f, 0.32f, 0.52f, carYaw,
             0.015f, 0.015f, 0.018f);
    drawCube(carX + 0.83f, -0.78f, -4.15f, 0.22f, 0.32f, 0.52f, carYaw,
             0.015f, 0.015f, 0.018f);
    drawCube(carX - 0.83f, -0.78f, -6.18f, 0.22f, 0.32f, 0.52f, carYaw,
             0.015f, 0.015f, 0.018f);
    drawCube(carX + 0.83f, -0.78f, -6.18f, 0.22f, 0.32f, 0.52f, carYaw,
             0.015f, 0.015f, 0.018f);

    const float brakeGlow = braking ? 1.0f : 0.68f;
    drawCube(carX - 0.47f, -0.48f, -3.57f, 0.24f, 0.12f, 0.07f, carYaw,
             brakeGlow, 0.025f, 0.01f);
    drawCube(carX + 0.47f, -0.48f, -3.57f, 0.24f, 0.12f, 0.07f, carYaw,
             brakeGlow, 0.025f, 0.01f);
}


void drawGarageScene(const GarageState& garage, float spin) {
    // Cheap native garage scene: floor, walls, fluorescent strips, cabinets and
    // the same primitive player car on a display pad. No textures/assets needed.
    drawCube(0.0f, -1.38f, -10.0f, 15.0f, 0.10f, 22.0f, 0.0f,
             0.055f, 0.060f, 0.070f);
    drawCube(0.0f, 3.2f, -20.5f, 15.0f, 9.0f, 0.20f, 0.0f,
             0.035f, 0.040f, 0.050f);
    drawCube(-7.3f, 2.0f, -11.0f, 0.20f, 7.0f, 20.0f, 0.0f,
             0.045f, 0.050f, 0.060f);
    drawCube( 7.3f, 2.0f, -11.0f, 0.20f, 7.0f, 20.0f, 0.0f,
             0.045f, 0.050f, 0.060f);

    // Ceiling light strips.
    for (int i = 0; i < 4; ++i) {
        const float z = -5.0f - float(i) * 5.0f;
        drawCube(-3.0f, 4.2f, z, 2.8f, 0.08f, 0.22f, 0.0f,
                 0.78f, 0.82f, 0.86f);
        drawCube( 3.0f, 4.2f, z, 2.8f, 0.08f, 0.22f, 0.0f,
                 0.78f, 0.82f, 0.86f);
    }

    // Tool cabinets / work benches.
    drawCube(-5.7f, -0.35f, -10.5f, 2.0f, 1.7f, 3.2f, 0.0f,
             0.18f, 0.055f, 0.045f);
    drawCube( 5.7f, -0.35f, -10.5f, 2.0f, 1.7f, 3.2f, 0.0f,
             0.06f, 0.10f, 0.16f);

    // Car display pad. Upgrade levels subtly change the accent rings so a build
    // has visible progression even before proper art/assets arrive.
    const float upgradeGlow = 0.08f + 0.035f * float(
        garage.engineLevel() + garage.turboLevel() + garage.tireLevel());
    drawCube(0.0f, -1.20f, -7.2f, 4.8f, 0.08f, 7.0f, 0.0f,
             0.10f + upgradeGlow, 0.10f, 0.12f + upgradeGlow * 0.5f);

    const float yaw = std::sin(spin * 0.45f) * 0.13f;
    drawCube(0.0f, -0.62f, -7.2f, 1.55f, 0.42f, 3.2f, yaw,
             0.78f, 0.035f, 0.025f);
    drawCube(0.0f, -0.21f, -7.35f, 1.18f, 0.34f, 1.42f, yaw,
             0.055f, 0.10f, 0.14f);
    drawCube(-0.83f, -0.78f, -6.15f, 0.22f, 0.32f, 0.52f, yaw,
             0.015f, 0.015f, 0.018f);
    drawCube( 0.83f, -0.78f, -6.15f, 0.22f, 0.32f, 0.52f, yaw,
             0.015f, 0.015f, 0.018f);
    drawCube(-0.83f, -0.78f, -8.18f, 0.22f, 0.32f, 0.52f, yaw,
             0.015f, 0.015f, 0.018f);
    drawCube( 0.83f, -0.78f, -8.18f, 0.22f, 0.32f, 0.52f, yaw,
             0.015f, 0.015f, 0.018f);
}

void printUpgradeLine(int row, bool selected, const char* name,
                      int level, int nextCost) {
    if (nextCost < 0) {
        std::printf("\x1b[%d;1H%c %-11s Lv%d  MAX          \x1b[K",
                    row, selected ? '>' : ' ', name, level);
    } else {
        std::printf("\x1b[%d;1H%c %-11s Lv%d -> %d  $%d  \x1b[K",
                    row, selected ? '>' : ' ', name, level, level + 1, nextCost);
    }
}

void drawGarageHud(const GarageState& garage, int selection, const char* status) {
    std::printf("\x1b[1;1HNR3DS v0.009 - GARAGE          \x1b[K");
    std::printf("\x1b[2;1HCash: $%d   W/L: %d/%d        \x1b[K",
                garage.cash(), garage.wins(), garage.losses());
    std::printf("\x1b[3;1HDpad U/D select  A buy          \x1b[K");
    std::printf("\x1b[4;1HDpad L/R tune   Y start race   \x1b[K");

    printUpgradeLine(6, selection == 0, "ENGINE", garage.engineLevel(),
                     garage.nextCost(UpgradeKind::Engine));
    printUpgradeLine(7, selection == 1, "TURBO", garage.turboLevel(),
                     garage.nextCost(UpgradeKind::Turbo));
    printUpgradeLine(8, selection == 2, "TIRES", garage.tireLevel(),
                     garage.nextCost(UpgradeKind::Tires));

    std::printf("\x1b[9;1H%c FINAL DRIVE   < %.2f >        \x1b[K",
                selection == 3 ? '>' : ' ', garage.finalDrive());
    for (int i = 0; i < 6; ++i) {
        std::printf("\x1b[%d;1H%c GEAR %d        < %.2f >        \x1b[K",
                    10 + i, selection == 4 + i ? '>' : ' ', i + 1,
                    garage.gearRatio(std::size_t(i)));
    }

    const auto cfg = garage.makeVehicleConfig();
    std::printf("\x1b[17;1HBuild: %.0fNm  +%.0fhp turbo      \x1b[K",
                cfg.baseTorqueNm, cfg.turboMaxExtraHp);
    std::printf("\x1b[18;1HGrip F/R: %.2f / %.2f            \x1b[K",
                cfg.frontGrip, cfg.rearGrip);
    std::printf("\x1b[20;1H%-36s\x1b[K", status ? status : "");
    std::printf("\x1b[22;1HSTART exits                      \x1b[K");
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
    const char* garageStatus = "Buy upgrades or tune gearing, then press Y.";
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

            garageSpin += dt;
            Mtx_PerspTilt(&gProjection, C3D_AngleFromDegrees(58.0f),
                          C3D_AspectRatioTop, 0.05f, 80.0f, false);
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            C3D_RenderTargetClear(top, C3D_CLEAR_ALL, 0x05070BFF, 0);
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
        if (std::fabs(car.telemetry().posX) > 5.1f && wallCooldown <= 0.0f) {
            ++wallHitCount;
            wallCooldown = 0.5f;
        }
        car.constrainLateral(-5.1f, 5.1f, 0.93f);

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
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
        C3D_FrameDrawOn(top);
        C3D_BindProgram(&gProgram);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocProjection, &gProjection);
        drawHighway(s, traffic, rt, race.nextGateDistanceM(), in.brake > 0.1f);
        C3D_FrameEnd(0);

        if (rt.phase == RacePhase::Countdown) {
            const int count = std::max(1, int(std::ceil(rt.countdown)));
            std::printf("\x1b[1;1HNR3DS v0.009 - STREET RACE    \x1b[K");
            std::printf("\x1b[5;1HRACE: GET READY  %d          \x1b[K", count);
        } else if (rt.phase == RacePhase::Racing) {
            std::printf("\x1b[1;1HNR3DS v0.009 - STREET RACE    \x1b[K");
            std::printf("\x1b[5;1HRACE: GO  CP %d/%d             \x1b[K",
                        rt.checkpointIndex, int(RaceSession::kCheckpointCount));
        } else {
            std::printf("\x1b[1;1HNR3DS v0.009 - STREET RACE    \x1b[K");
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
        std::printf("\x1b[15;1HCPU: %6.2f%% GPU: %6.2f%%\x1b[K",
                    C3D_GetProcessingTime() * 6.0f,
                    C3D_GetDrawingTime() * 6.0f);
    }

    sceneExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
