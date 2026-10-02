#include <3ds.h>
#include <citro3d.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <array>

#include "vshader_shbin.h"
#include "nr_physics.hpp"

using nr3ds::InputState;
using nr3ds::Vehicle;

namespace {

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

constexpr u32 CLEAR_COLOR = 0x02040AFF;
constexpr int kCubeVerts = 36;
constexpr int kTrafficCount = 6;

struct Vertex { float x, y, z; };

struct TrafficCar {
    float laneX;
    float distanceM;
    float speedKph;
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
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void sceneExit() {
    if (gVbo) linearFree(gVbo);
    shaderProgramFree(&gProgram);
    if (gShaderDvlb) DVLB_Free(gShaderDvlb);
}

void updateProjection(float speedKph) {
    const float speedT = clampf(speedKph / 220.0f, 0.0f, 1.0f);
    const float fov = lerpf(60.0f, 67.0f, speedT);
    Mtx_PerspTilt(&gProjection, C3D_AngleFromDegrees(fov),
                  C3D_AspectRatioTop, 0.05f, 300.0f, false);
}

std::array<TrafficCar, kTrafficCount> makeTraffic() {
    return {{
        {-3.8f,  28.0f,  88.0f, 0.10f, 0.22f, 0.62f},
        { 0.0f,  46.0f, 104.0f, 0.50f, 0.50f, 0.54f},
        { 3.8f,  68.0f,  76.0f, 0.08f, 0.50f, 0.22f},
        {-3.8f,  92.0f, 118.0f, 0.62f, 0.10f, 0.10f},
        { 0.0f, 125.0f,  96.0f, 0.46f, 0.18f, 0.62f},
        { 3.8f, 158.0f, 112.0f, 0.68f, 0.52f, 0.12f},
    }};
}

void updateTraffic(std::array<TrafficCar, kTrafficCount>& traffic,
                   const nr3ds::Telemetry& s,
                   float dt) {
    for (int i = 0; i < kTrafficCount; ++i) {
        auto& t = traffic[std::size_t(i)];
        const float relMps = (t.speedKph - s.speedKph) / 3.6f;
        t.distanceM += relMps * dt;

        if (t.distanceM < -12.0f) {
            t.distanceM = 95.0f + float((i * 29) % 85);
        } else if (t.distanceM > 205.0f) {
            t.distanceM = 55.0f + float((i * 31) % 110);
        }
    }
}

void drawTrafficCar(const TrafficCar& t, float cameraX) {
    if (t.distanceM < -2.0f || t.distanceM > 190.0f) return;

    const float z = -5.2f - t.distanceM;
    const float fog = 1.0f - clampf(t.distanceM / 220.0f, 0.0f, 0.82f);
    const float x = t.laneX - cameraX;

    drawCube(x, -0.66f, z, 1.45f, 0.38f, 2.85f, 0.0f,
             t.r * fog, t.g * fog, t.b * fog);
    drawCube(x, -0.29f, z - 0.12f, 1.08f, 0.30f, 1.25f, 0.0f,
             0.05f * fog, 0.08f * fog, 0.11f * fog);

    drawCube(x - 0.44f, -0.51f, z + 1.47f, 0.20f, 0.10f, 0.06f, 0.0f,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
    drawCube(x + 0.44f, -0.51f, z + 1.47f, 0.20f, 0.10f, 0.06f, 0.0f,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
}

void drawHighway(const nr3ds::Telemetry& s,
                 const std::array<TrafficCar, kTrafficCount>& traffic,
                 bool braking) {
    const float segLen = 10.0f;
    const float scroll = std::fmod(std::fabs(s.posY), segLen);

    const float worldCarX = clampf(s.posX, -5.0f, 5.0f);
    const float cameraX = worldCarX * 0.84f;
    const float carX = worldCarX - cameraX;

    for (int i = 0; i < 24; ++i) {
        const float z = -7.0f - float(i) * segLen + scroll;
        const float fog = 1.0f - clampf(float(i) / 26.0f, 0.0f, 0.86f);

        drawCube(-cameraX, -1.35f, z, 12.0f, 0.12f, segLen + 0.15f, 0.0f,
                 0.085f * fog, 0.095f * fog, 0.12f * fog);
        drawCube(-6.25f - cameraX, -0.72f, z, 0.20f, 0.70f, segLen, 0.0f,
                 0.25f * fog, 0.27f * fog, 0.31f * fog);
        drawCube( 6.25f - cameraX, -0.72f, z, 0.20f, 0.70f, segLen, 0.0f,
                 0.25f * fog, 0.27f * fog, 0.31f * fog);

        if ((i & 1) == 0) {
            drawCube(-2.0f - cameraX, -1.20f, z, 0.09f, 0.025f, 3.3f, 0.0f,
                     0.84f * fog, 0.82f * fog, 0.68f * fog);
            drawCube( 2.0f - cameraX, -1.20f, z, 0.09f, 0.025f, 3.3f, 0.0f,
                     0.84f * fog, 0.82f * fog, 0.68f * fog);
        }

        if ((i % 3) == 1) {
            drawCube(-8.0f - cameraX, 0.1f, z, 0.18f, 2.6f, 0.18f, 0.0f,
                     0.10f * fog, 0.14f * fog, 0.18f * fog);
            drawCube( 8.0f - cameraX, 0.1f, z, 0.18f, 2.6f, 0.18f, 0.0f,
                     0.10f * fog, 0.14f * fog, 0.18f * fog);
            drawCube(-8.0f - cameraX, 1.5f, z, 0.35f, 0.12f, 0.35f, 0.0f,
                     0.90f * fog, 0.66f * fog, 0.24f * fog);
            drawCube( 8.0f - cameraX, 1.5f, z, 0.35f, 0.12f, 0.35f, 0.0f,
                     0.90f * fog, 0.66f * fog, 0.24f * fog);
        }

        if ((i % 4) == 0) {
            const float hL = 3.0f + float((i * 7) % 5) * 0.8f;
            const float hR = 2.5f + float((i * 5) % 6) * 0.75f;
            drawCube(-13.0f - cameraX, -1.0f + hL * 0.5f, z - 3.0f,
                     4.0f, hL, 5.0f, 0.0f,
                     0.035f * fog, 0.055f * fog, 0.085f * fog);
            drawCube( 13.0f - cameraX, -1.0f + hR * 0.5f, z + 1.5f,
                     4.5f, hR, 5.5f, 0.0f,
                     0.04f * fog, 0.05f * fog, 0.075f * fog);
        }

        if ((i % 9) == 5) {
            drawCube(-cameraX, 2.5f, z, 13.5f, 0.15f, 0.20f, 0.0f,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube(-5.8f - cameraX, 0.7f, z, 0.16f, 3.5f, 0.16f, 0.0f,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube( 5.8f - cameraX, 0.7f, z, 0.16f, 3.5f, 0.16f, 0.0f,
                     0.12f * fog, 0.16f * fog, 0.18f * fog);
            drawCube(-1.8f - cameraX, 2.25f, z - 0.12f, 2.5f, 0.65f, 0.12f, 0.0f,
                     0.08f * fog, 0.26f * fog, 0.20f * fog);
            drawCube( 2.2f - cameraX, 2.25f, z - 0.12f, 2.8f, 0.65f, 0.12f, 0.0f,
                     0.08f * fog, 0.20f * fog, 0.30f * fog);
        }
    }

    for (const auto& t : traffic) {
        drawTrafficCar(t, cameraX);
    }

    // Cheap headlight pool on the pavement. It is intentionally geometry-only.
    drawCube(carX, -1.17f, -11.5f, 4.7f, 0.018f, 9.5f, 0.0f,
             0.22f, 0.20f, 0.12f);

    const float carYaw = clampf(-s.driftAngleDeg * 0.012f, -0.45f, 0.45f);
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
    Vehicle car;
    auto traffic = makeTraffic();

    std::printf("NR3DS v0.004 - traffic test\n");
    std::printf("A gas | B brake | X handbrake\n");
    std::printf("L/R shift | Circle Pad steer\n");
    std::printf("START exit\n");

    while (aptMainLoop()) {
        hidScanInput();
        const u32 down = hidKeysDown();
        const u32 held = hidKeysHeld();
        if (down & KEY_START) break;

        circlePosition cp{};
        hidCircleRead(&cp);

        InputState in{};
        in.throttle = (held & KEY_A) ? 1.0f : 0.0f;
        in.brake = (held & KEY_B) ? 1.0f : 0.0f;
        in.handbrake = (held & KEY_X) ? 1.0f : 0.0f;
        in.steer = clampf(float(cp.dx) / 156.0f, -1.0f, 1.0f);
        in.shiftDown = (down & KEY_L) != 0;
        in.shiftUp = (down & KEY_R) != 0;

        constexpr float dt = 1.0f / 60.0f;
        car.step(in, dt);
        const auto& s = car.telemetry();
        updateTraffic(traffic, s, dt);
        updateProjection(s.speedKph);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_RenderTargetClear(top, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
        C3D_FrameDrawOn(top);
        C3D_BindProgram(&gProgram);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocProjection, &gProjection);
        drawHighway(s, traffic, in.brake > 0.1f);
        C3D_FrameEnd(0);

        std::printf("\x1b[6;1HSpeed: %6.1f km/h   \x1b[K", s.speedKph);
        std::printf("\x1b[7;1HRPM:   %6.0f  Gear: %d \x1b[K", s.rpm, s.gear);
        std::printf("\x1b[8;1HTurbo: %5.2f  Slip: %5.2f\x1b[K", s.turboSpool, s.rearSlip);
        std::printf("\x1b[9;1HDrift: %6.1f deg       \x1b[K", s.driftAngleDeg);
        std::printf("\x1b[10;1HTire:  %5.2f  HB: %5.2f\x1b[K", s.tireTemp, s.handbrakeTimer);
        std::printf("\x1b[11;1HTraffic: %d cars      \x1b[K", kTrafficCount);
        std::printf("\x1b[13;1HCPU: %6.2f%% GPU: %6.2f%%\x1b[K",
                    C3D_GetProcessingTime() * 6.0f,
                    C3D_GetDrawingTime() * 6.0f);
    }

    sceneExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
