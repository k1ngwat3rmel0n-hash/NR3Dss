#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <array>
#include <algorithm>

#include "vshader_shbin.h"
#include "nr_road_source_t3x.h"
#include "nr_tunnel_source_t3x.h"
#include "nr_physics.hpp"
#include "nr_race.hpp"
#include "nr_garage.hpp"
#include "nr_world.hpp"
#include "nr_source_geometry.hpp"
#include "source_meshes.hpp"
#include "source_car.hpp"
#include "source_world_atlas.hpp"

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
struct TexVertex { float x, y, z, u, v; };

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

static const TexVertex kRoadTextureQuad[6] = {
    {-0.5f, 0.0f, -0.5f, 0.0f, 0.0f},
    { 0.5f, 0.0f, -0.5f, 1.0f, 0.0f},
    { 0.5f, 0.0f,  0.5f, 1.0f, 1.0f},
    { 0.5f, 0.0f,  0.5f, 1.0f, 1.0f},
    {-0.5f, 0.0f,  0.5f, 0.0f, 1.0f},
    {-0.5f, 0.0f, -0.5f, 0.0f, 0.0f},
};

static const TexVertex kWallTextureQuad[6] = {
    {0.0f, -0.5f, -0.5f, 0.0f, 1.0f},
    {0.0f,  0.5f, -0.5f, 0.0f, 0.0f},
    {0.0f,  0.5f,  0.5f, 1.0f, 0.0f},
    {0.0f,  0.5f,  0.5f, 1.0f, 0.0f},
    {0.0f, -0.5f,  0.5f, 1.0f, 1.0f},
    {0.0f, -0.5f, -0.5f, 0.0f, 1.0f},
};

DVLB_s* gShaderDvlb = nullptr;
shaderProgram_s gProgram{};
int gLocProjection = -1;
int gLocModelView = -1;
C3D_Mtx gProjection{};
void* gVbo = nullptr;
void* gSourceRoadVbo = nullptr;
void* gSourceTunnelRoofVbo = nullptr;
void* gLivisaCarVbo = nullptr;
void* gAtlasHighRoadVbo = nullptr;
void* gAtlasLowRoadVbo = nullptr;
void* gAtlasJunctionRoadVbo = nullptr;
void* gAtlasTunnelRoofVbo = nullptr;
void* gAtlasRoadLinesVbo = nullptr;
void* gAtlasSupportVbo = nullptr;
void* gAtlasOpenRoadVbo = nullptr;
void* gRoadTextureVbo = nullptr;
void* gWallTextureVbo = nullptr;
C3D_Tex gRoadTexture{};
C3D_Tex gTunnelTexture{};
bool gRoadTextureReady = false;
bool gTunnelTextureReady = false;
nr3ds::ExpresswayRoute gRoute;


// v0.015 audio plumbing. The supplied source soundtrack is stored as FSB5 and
// is catalogued under tools/music_import. Until offline FSB5 transcoding lands,
// run a tiny original PCM synth loop through NDSP to validate real 3DS audio.
constexpr int kAudioRate = 22050;
constexpr int kAudioLoopSeconds = 4;
constexpr int kAudioSamples = kAudioRate * kAudioLoopSeconds;
s16* gAudioBuffer = nullptr;
ndspWaveBuf gAudioWave{};
bool gAudioReady = false;


void audioTestInit() {
    const Result rc = ndspInit();
    if (R_FAILED(rc)) return;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, float(kAudioRate));
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    float mix[12]{};
    mix[0] = 0.23f;
    mix[1] = 0.23f;
    ndspChnSetMix(0, mix);

    gAudioBuffer = static_cast<s16*>(linearAlloc(kAudioSamples * sizeof(s16)));
    if (!gAudioBuffer) {
        ndspExit();
        return;
    }

    // Original four-second late-night synth test: kick/snare/hat + bass/arpeggio.
    // It exists only to prove the NDSP playback path and is not one of the supplied songs.
    constexpr float pi = 3.14159265358979323846f;
    u32 noise = 0x12345678u;
    for (int i = 0; i < kAudioSamples; ++i) {
        const float t = float(i) / float(kAudioRate);
        const float beat = std::fmod(t * (160.0f / 60.0f), 4.0f);
        const float beatFrac = beat - std::floor(beat);
        const int beatIndex = int(std::floor(beat));

        float v = 0.0f;
        // Side-chain-like bass pulse.
        const float bassEnv = std::exp(-beatFrac * 4.0f);
        const float bassHz = (beatIndex == 2) ? 65.41f : 55.0f;
        v += std::sin(2.0f * pi * bassHz * t) * 0.22f * bassEnv;

        // Kick on beats 0/2.
        if ((beatIndex & 1) == 0 && beatFrac < 0.20f) {
            const float env = std::exp(-beatFrac * 28.0f);
            const float hz = 72.0f - beatFrac * 170.0f;
            v += std::sin(2.0f * pi * hz * t) * 0.34f * env;
        }

        // Snare on beats 1/3, deterministic LFSR noise.
        noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
        const float n = (float(noise & 0xFFFFu) / 32767.5f) - 1.0f;
        if ((beatIndex & 1) == 1 && beatFrac < 0.16f)
            v += n * 0.16f * std::exp(-beatFrac * 22.0f);

        // Eighth-note hats.
        const float eighth = std::fmod(t * (160.0f / 60.0f) * 2.0f, 1.0f);
        if (eighth < 0.055f)
            v += n * 0.045f * std::exp(-eighth * 45.0f);

        // Small high arpeggio to make the audio test obviously musical.
        const int step = int(std::floor(t * 8.0f)) & 7;
        static constexpr float notes[8] = {220.0f, 261.63f, 329.63f, 392.0f,
                                           329.63f, 261.63f, 246.94f, 293.66f};
        v += std::sin(2.0f * pi * notes[step] * t) * 0.035f;

        v = v < -0.95f ? -0.95f : (v > 0.95f ? 0.95f : v);
        gAudioBuffer[i] = s16(v * 32767.0f);
    }

    std::memset(&gAudioWave, 0, sizeof(gAudioWave));
    gAudioWave.data_pcm16 = gAudioBuffer;
    gAudioWave.nsamples = kAudioSamples;
    gAudioWave.looping = true;
    DSP_FlushDataCache(gAudioBuffer, kAudioSamples * sizeof(s16));
    ndspChnWaveBufAdd(0, &gAudioWave);
    gAudioReady = true;
}

void audioTestExit() {
    if (gAudioReady) {
        ndspChnWaveBufClear(0);
        ndspExit();
        gAudioReady = false;
    }
    if (gAudioBuffer) {
        linearFree(gAudioBuffer);
        gAudioBuffer = nullptr;
    }
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * clampf(t, 0.0f, 1.0f);
}

float laneXFor(int laneIndex) {
    return float(laneIndex - 1) * kLaneWidth;
}

// Convert road-local lateral/forward offsets into the player camera frame.
// v0.016 positioned barriers/props with X-only offsets, so on bends their
// centers did not rotate around the curved road. That made walls cut straight
// across corners even though each piece had the correct yaw.
void roadLocalOffset(float roadX, float roadZ, float yaw,
                     float lateralM, float forwardM,
                     float& outX, float& outZ) {
    const float c = std::cos(yaw);
    const float sn = std::sin(yaw);
    outX = roadX + lateralM * c + forwardM * sn;
    outZ = roadZ - lateralM * sn + forwardM * c;
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

void setupColorPipeline() {
    C3D_BindProgram(&gProgram);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddFixed(attrInfo, 1);                // v1 = color
    AttrInfo_AddFixed(attrInfo, 2);                // v2 = unused UV
    C3D_FixedAttribSet(2, 0.0f, 0.0f, 0.0f, 0.0f);

    bindPositionVbo(gVbo, sizeof(Vertex));

    // Restore the fixed-color combiner after the textured road/tunnel pass.
    // v0.017 accidentally called setupColorPipeline() recursively here, so the
    // first expressway frame overflowed the stack and left the last garage frame
    // visible. Keep this function strictly non-recursive.
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both,
                  GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void setupTexturedPipeline(void* vbo, C3D_Tex* texture) {
    C3D_BindProgram(&gProgram);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddFixed(attrInfo, 1);                // v1 = fog/tint
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v2 = UV

    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    // Two streamed attributes: v0 then v2.
    BufInfo_Add(bufInfo, vbo, sizeof(TexVertex), 2, 0x20);

    C3D_TexBind(0, texture);
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both,
                  GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

bool loadTextureFromMem(C3D_Tex* tex, const void* data, size_t size) {
    Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, nullptr, false);
    if (!t3x) return false;
    Tex3DS_TextureFree(t3x);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);
    return true;
}

void drawTexturedQuad(float x, float y, float z,
                      float sx, float sy, float sz,
                      float yaw, float tint) {
    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, x, y, z, true);
    Mtx_RotateY(&modelView, yaw, true);
    Mtx_Scale(&modelView, sx, sy, sz);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocModelView, &modelView);
    setColor(tint, tint, tint, 1.0f);
    C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
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
    AttrInfo_AddFixed(attrInfo, 2);
    C3D_FixedAttribSet(2, 0.0f, 0.0f, 0.0f, 0.0f);
    setColor(1, 1, 1, 1);

    gVbo = linearAlloc(sizeof(kCube));
    std::memcpy(gVbo, kCube, sizeof(kCube));

    gRoadTextureVbo = linearAlloc(sizeof(kRoadTextureQuad));
    if (gRoadTextureVbo)
        std::memcpy(gRoadTextureVbo, kRoadTextureQuad, sizeof(kRoadTextureQuad));
    gWallTextureVbo = linearAlloc(sizeof(kWallTextureQuad));
    if (gWallTextureVbo)
        std::memcpy(gWallTextureVbo, kWallTextureQuad, sizeof(kWallTextureQuad));

    gSourceRoadVbo = linearAlloc(sizeof(kSourceRoadVerts));
    if (gSourceRoadVbo)
        std::memcpy(gSourceRoadVbo, kSourceRoadVerts, sizeof(kSourceRoadVerts));
    gSourceTunnelRoofVbo = linearAlloc(sizeof(kSourceTunnelRoofVerts));
    if (gSourceTunnelRoofVbo)
        std::memcpy(gSourceTunnelRoofVbo, kSourceTunnelRoofVerts, sizeof(kSourceTunnelRoofVerts));
    gLivisaCarVbo = linearAlloc(sizeof(kLivisaStockVerts));
    if (gLivisaCarVbo)
        std::memcpy(gLivisaCarVbo, kLivisaStockVerts, sizeof(kLivisaStockVerts));

    // v0.016 multi-scene source atlas. These are actual developer-authorized
    // Unity mesh triangles normalized offline into a common road-local frame.
    gAtlasHighRoadVbo = linearAlloc(sizeof(kAtlasHighRoadVerts));
    if (gAtlasHighRoadVbo) std::memcpy(gAtlasHighRoadVbo, kAtlasHighRoadVerts, sizeof(kAtlasHighRoadVerts));
    gAtlasLowRoadVbo = linearAlloc(sizeof(kAtlasLowRoadVerts));
    if (gAtlasLowRoadVbo) std::memcpy(gAtlasLowRoadVbo, kAtlasLowRoadVerts, sizeof(kAtlasLowRoadVerts));
    gAtlasJunctionRoadVbo = linearAlloc(sizeof(kAtlasJunctionRoadVerts));
    if (gAtlasJunctionRoadVbo) std::memcpy(gAtlasJunctionRoadVbo, kAtlasJunctionRoadVerts, sizeof(kAtlasJunctionRoadVerts));
    gAtlasTunnelRoofVbo = linearAlloc(sizeof(kAtlasTunnelRoofVerts));
    if (gAtlasTunnelRoofVbo) std::memcpy(gAtlasTunnelRoofVbo, kAtlasTunnelRoofVerts, sizeof(kAtlasTunnelRoofVerts));
    gAtlasRoadLinesVbo = linearAlloc(sizeof(kAtlasRoadLinesVerts));
    if (gAtlasRoadLinesVbo) std::memcpy(gAtlasRoadLinesVbo, kAtlasRoadLinesVerts, sizeof(kAtlasRoadLinesVerts));
    gAtlasSupportVbo = linearAlloc(sizeof(kAtlasSupportVerts));
    if (gAtlasSupportVbo) std::memcpy(gAtlasSupportVbo, kAtlasSupportVerts, sizeof(kAtlasSupportVerts));
    gAtlasOpenRoadVbo = linearAlloc(sizeof(kAtlasOpenRoadVerts));
    if (gAtlasOpenRoadVbo) std::memcpy(gAtlasOpenRoadVbo, kAtlasOpenRoadVerts, sizeof(kAtlasOpenRoadVerts));

    // v0.017: actual NIGHT-RUNNERS albedo crops, converted by tex3ds to tiny
    // ETC1 textures at build time.
    gRoadTextureReady = loadTextureFromMem(
        &gRoadTexture, nr_road_source_t3x, nr_road_source_t3x_size);
    gTunnelTextureReady = loadTextureFromMem(
        &gTunnelTexture, nr_tunnel_source_t3x, nr_tunnel_source_t3x_size);

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
    if (gRoadTextureReady) C3D_TexDelete(&gRoadTexture);
    if (gTunnelTextureReady) C3D_TexDelete(&gTunnelTexture);
    if (gWallTextureVbo) linearFree(gWallTextureVbo);
    if (gRoadTextureVbo) linearFree(gRoadTextureVbo);
    if (gAtlasOpenRoadVbo) linearFree(gAtlasOpenRoadVbo);
    if (gAtlasSupportVbo) linearFree(gAtlasSupportVbo);
    if (gAtlasRoadLinesVbo) linearFree(gAtlasRoadLinesVbo);
    if (gAtlasTunnelRoofVbo) linearFree(gAtlasTunnelRoofVbo);
    if (gAtlasJunctionRoadVbo) linearFree(gAtlasJunctionRoadVbo);
    if (gAtlasLowRoadVbo) linearFree(gAtlasLowRoadVbo);
    if (gAtlasHighRoadVbo) linearFree(gAtlasHighRoadVbo);
    if (gLivisaCarVbo) linearFree(gLivisaCarVbo);
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
    float x = 0.0f, laneZ = 0.0f;
    roadLocalOffset(center - cameraX, z, yaw, t.laneX, 0.0f, x, laneZ);
    const float y = -0.62f + elev * 0.34f;

    drawCube(x, y, laneZ, 1.45f, 0.38f, 2.85f, yaw,
             t.r * fog, t.g * fog, t.b * fog);
    drawCube(x, y + 0.37f, laneZ - 0.12f, 1.08f, 0.30f, 1.25f, yaw,
             0.05f * fog, 0.08f * fog, 0.11f * fog);

    drawCube(x - 0.44f * std::cos(yaw), y + 0.15f, laneZ + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
    drawCube(x + 0.44f * std::cos(yaw), y + 0.15f, laneZ + 1.47f, 0.20f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.02f * fog, 0.01f * fog);
}

void drawRaceOpponent(const nr3ds::RaceTelemetry& race,
                      float cameraX, float playerWorldM) {
    if (race.phase == RacePhase::Finished) return;
    const float relM = race.opponentProgressM - race.playerProgressM;
    if (relM < -4.5f || relM > 195.0f) return;

    const auto rf = gRoute.localFrame(playerWorldM, relM);
    const float center = rf.lateralM;
    const float yaw = rf.yawRad;
    const float z = -4.7f - rf.forwardM;
    const float elev = rf.elevationM;
    const float fog = 1.0f - clampf(std::max(relM, 0.0f) / 220.0f, 0.0f, 0.82f);
    float x = 0.0f, laneZ = 0.0f;
    roadLocalOffset(center - cameraX, z, yaw, race.opponentLaneX, 0.0f, x, laneZ);
    const float y = -0.60f + elev * 0.34f;

    drawCube(x, y, laneZ, 1.52f, 0.40f, 3.00f, yaw,
             0.78f * fog, 0.26f * fog, 0.055f * fog);
    drawCube(x, y + 0.38f, laneZ - 0.15f, 1.08f, 0.31f, 1.30f, yaw,
             0.055f * fog, 0.075f * fog, 0.095f * fog);
    drawCube(x - 0.46f * std::cos(yaw), y + 0.15f, laneZ + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
             1.0f * fog, 0.03f * fog, 0.01f * fog);
    drawCube(x + 0.46f * std::cos(yaw), y + 0.15f, laneZ + 1.55f, 0.21f, 0.10f, 0.06f, yaw,
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

    for (int side = -1; side <= 1; side += 2) {
        float x = 0.0f, sideZ = 0.0f;
        roadLocalOffset(roadX, z, yaw, float(side) * (roadHalf + 0.36f), 0.0f, x, sideZ);
        drawCube(x, roadY + 0.48f, sideZ,
                 0.18f, 0.72f, 8.05f, yaw,
                 0.27f * fog, 0.27f * fog, 0.27f * fog);
        drawCube(x, roadY + 1.56f, sideZ,
                 0.07f, 1.48f, 0.07f, yaw,
                 postR * fog, postG * fog, postB * fog);
        drawCube(x, roadY + 2.27f, sideZ,
                 0.07f, 0.07f, 8.0f, yaw,
                 postR * fog, postG * fog, postB * fog);
        drawCube(x, roadY + 1.58f, sideZ,
                 0.035f, 1.25f, 7.70f, yaw,
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

    // Actual compact support mesh from sharedassets2. It is cheap enough to
    // retain alongside the coarse safety proxy and gives the silhouette source detail.
    if (gAtlasSupportVbo) {
        const float sourceScale = roadHalf * 0.92f;
        drawSourceTriangles(gAtlasSupportVbo, kAtlasSupportVertsCount,
                            roadX, roadY - 5.1f, z,
                            sourceScale, sourceScale * 0.14f, sourceScale * 1.05f, yaw,
                            0.24f * fog, 0.25f * fog, 0.26f * fog);
    }

    // Reduced proxy of AREA_2_SUPPORTS2.001/.002: twin piers, cap beam and a
    // darker under-deck. The original source meshes are enormous spans; this
    // version repeats a short module that streams cheaply.
    float leftX = 0.0f, leftZ = 0.0f, rightX = 0.0f, rightZ = 0.0f;
    roadLocalOffset(roadX, z, yaw, -pierOffset, 0.0f, leftX, leftZ);
    roadLocalOffset(roadX, z, yaw,  pierOffset, 0.0f, rightX, rightZ);
    drawCube(leftX, roadY + h * 0.45f, leftZ,
             0.72f, h, 0.82f, yaw,
             0.17f * fog, 0.18f * fog, 0.19f * fog);
    drawCube(rightX, roadY + h * 0.45f, rightZ,
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
    float leftWallX = 0.0f, leftWallZ = 0.0f, rightWallX = 0.0f, rightWallZ = 0.0f;
    roadLocalOffset(roadX, z, yaw, -wallX, 0.0f, leftWallX, leftWallZ);
    roadLocalOffset(roadX, z, yaw,  wallX, 0.0f, rightWallX, rightWallZ);
    drawCube(leftWallX, roadY + 1.58f, leftWallZ,
             0.72f, 3.50f, 8.15f, yaw,
             0.50f * fog, 0.43f * fog, 0.23f * fog);
    drawCube(rightWallX, roadY + 1.58f, rightWallZ,
             0.72f, 3.50f, 8.15f, yaw,
             0.50f * fog, 0.43f * fog, 0.23f * fog);
    drawCube(roadX, roadY + 3.53f, z,
             roadHalf * 2.0f + 1.45f, 0.32f, 8.2f, yaw,
             0.39f * fog, 0.36f * fog, 0.25f * fog);

    // v0.016 uses a second tunnel family recovered from sharedassets19,
    // rather than repeating the single v0.014 test roof everywhere.
    if ((segmentIndex % 3) == 0) {
        const float sourceScale = roadHalf + 0.42f;
        drawSourceTriangles(gAtlasTunnelRoofVbo, kAtlasTunnelRoofVertsCount,
                            roadX, roadY + 3.20f, z,
                            sourceScale, sourceScale * 0.50f, sourceScale * 4.0f, yaw,
                            0.58f * fog, 0.51f * fog, 0.32f * fog);
    }

    // Ribbing is one of the strongest tunnel depth cues in the original.
    if ((segmentIndex & 1) == 0) {
        drawCube(leftWallX + 0.18f * std::cos(yaw), roadY + 1.72f, leftWallZ,
                 0.10f, 3.35f, 0.12f, yaw,
                 0.70f * fog, 0.60f * fog, 0.34f * fog);
        drawCube(rightWallX - 0.18f * std::cos(yaw), roadY + 1.72f, rightWallZ,
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
        drawCube(leftWallX + 0.55f * std::cos(yaw), roadY + 0.92f, leftWallZ - 1.0f,
                 0.11f, 1.25f, 1.15f, yaw,
                 0.15f * fog, 0.19f * fog, 0.16f * fog);
        drawCube(leftWallX + 0.50f * std::cos(yaw), roadY + 1.02f, leftWallZ - 1.0f,
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


void drawSourceTexturePass(float routeProgressM, float cameraX) {
    if ((!gRoadTextureReady && !gTunnelTextureReady) ||
        (!gRoadTextureVbo && !gWallTextureVbo)) {
        return;
    }

    constexpr float segLen = 8.0f;
    const int firstSeg = std::max(
        0, int(std::floor(std::max(routeProgressM, 0.0f) / segLen)) - 1);

    // First pass: source asphalt on the whole visible road. The underlying
    // procedural deck is retained for collision and fallback rendering.
    if (gRoadTextureReady && gRoadTextureVbo) {
        setupTexturedPipeline(gRoadTextureVbo, &gRoadTexture);
        for (int i = 0; i < 40; ++i) {
            const int segId = firstSeg + i;
            const float worldM = float(segId) * segLen;
            if (worldM < 0.0f || worldM > gRoute.totalLengthM()) continue;
            if (!gRoute.chunkActive(gRoute.chunkIndex(worldM), routeProgressM)) continue;

            const float aheadM = worldM - routeProgressM;
            const auto rf = gRoute.localFrame(routeProgressM, aheadM);
            if (rf.forwardM < -12.0f || rf.forwardM > 300.0f) continue;

            const auto& section = gRoute.sectionAt(worldM);
            const float z = -4.7f - rf.forwardM;
            const float roadX = rf.lateralM - cameraX;
            const float roadY = -1.34f + rf.elevationM * 0.34f;
            const float yaw = rf.yawRad;
            const float baseFog =
                1.0f - clampf(std::max(rf.forwardM, 0.0f) / 300.0f, 0.0f, 0.90f);
            const float horizonFade =
                clampf((300.0f - rf.forwardM) / 32.0f, 0.0f, 1.0f);
            const float fog = baseFog * horizonFade;
            if (fog <= 0.003f) continue;

            // Just above the procedural deck top surface. This is deliberately
            // a simple road-local quad for the first texture pass; source-mesh
            // UV preservation follows once exact scene transforms are applied.
            drawTexturedQuad(roadX, roadY + 0.066f, z,
                             section.roadWidthM - 0.10f, 1.0f, segLen + 0.16f,
                             yaw, fog);
        }
    }

    // Second pass: source tunnel concrete on both inner walls and ceiling.
    if (gTunnelTextureReady && gWallTextureVbo) {
        setupTexturedPipeline(gWallTextureVbo, &gTunnelTexture);
        for (int i = 0; i < 40; ++i) {
            const int segId = firstSeg + i;
            const float worldM = float(segId) * segLen;
            if (worldM < 0.0f || worldM > gRoute.totalLengthM()) continue;
            if (!gRoute.chunkActive(gRoute.chunkIndex(worldM), routeProgressM)) continue;

            const auto& section = gRoute.sectionAt(worldM);
            if (section.style != RoadStyle::Tunnel) continue;

            const float aheadM = worldM - routeProgressM;
            const auto rf = gRoute.localFrame(routeProgressM, aheadM);
            if (rf.forwardM < -12.0f || rf.forwardM > 300.0f) continue;

            const float z = -4.7f - rf.forwardM;
            const float roadX = rf.lateralM - cameraX;
            const float roadY = -1.34f + rf.elevationM * 0.34f;
            const float yaw = rf.yawRad;
            const float roadHalf = section.roadWidthM * 0.5f;
            const float baseFog =
                1.0f - clampf(std::max(rf.forwardM, 0.0f) / 300.0f, 0.0f, 0.90f);
            const float horizonFade =
                clampf((300.0f - rf.forwardM) / 32.0f, 0.0f, 1.0f);
            const float fog = baseFog * horizonFade;
            if (fog <= 0.003f) continue;

            float lx = 0.0f, lz = 0.0f, rx = 0.0f, rz = 0.0f;
            roadLocalOffset(roadX, z, yaw, -(roadHalf + 0.075f), 0.0f, lx, lz);
            roadLocalOffset(roadX, z, yaw,  (roadHalf + 0.075f), 0.0f, rx, rz);
            drawTexturedQuad(lx, roadY + 1.60f, lz,
                             1.0f, 3.30f, segLen + 0.18f, yaw, fog);
            drawTexturedQuad(rx, roadY + 1.60f, rz,
                             1.0f, 3.30f, segLen + 0.18f, yaw, fog);
        }

        // Ceiling uses the horizontal quad and the same real tunnel-concrete
        // texture. Keep it just below the procedural roof so it remains visible.
        if (gRoadTextureVbo) {
            setupTexturedPipeline(gRoadTextureVbo, &gTunnelTexture);
            for (int i = 0; i < 40; ++i) {
                const int segId = firstSeg + i;
                const float worldM = float(segId) * segLen;
                if (worldM < 0.0f || worldM > gRoute.totalLengthM()) continue;
                if (!gRoute.chunkActive(gRoute.chunkIndex(worldM), routeProgressM)) continue;
                const auto& section = gRoute.sectionAt(worldM);
                if (section.style != RoadStyle::Tunnel) continue;
                const auto rf = gRoute.localFrame(routeProgressM, worldM - routeProgressM);
                if (rf.forwardM < -12.0f || rf.forwardM > 300.0f) continue;

                const float z = -4.7f - rf.forwardM;
                const float roadX = rf.lateralM - cameraX;
                const float roadY = -1.34f + rf.elevationM * 0.34f;
                const float baseFog =
                    1.0f - clampf(std::max(rf.forwardM, 0.0f) / 300.0f, 0.0f, 0.90f);
                const float horizonFade =
                    clampf((300.0f - rf.forwardM) / 32.0f, 0.0f, 1.0f);
                const float fog = baseFog * horizonFade;
                if (fog <= 0.003f) continue;

                drawTexturedQuad(roadX, roadY + 3.355f, z,
                                 section.roadWidthM + 1.15f, 1.0f, segLen + 0.18f,
                                 rf.yawRad, fog * 0.88f);
            }
        }
    }

    setupColorPipeline();
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, gLocProjection, &gProjection);
}

void drawHighway(const nr3ds::Telemetry& s,
                 const std::array<TrafficCar, kTrafficCount>& traffic,
                 const nr3ds::RaceTelemetry& race,
                 float nextGateM,
                 bool braking,
                 float routeProgressM) {
    const float segLen = 8.0f;
    // IMPORTANT: segment identity is absolute. v0.013 regenerated all prop
    // patterns from loop index 0 whenever baseM crossed a 10 m boundary. That
    // made lamps, lane dashes, supports and tunnel ribs visibly "reset" at
    // speed. Keep a stable world segment ID and only slide the camera through it.
    const int firstSeg = std::max(0, int(std::floor(std::max(routeProgressM, 0.0f) / segLen)) - 1);

    const float worldCarX = clampf(s.posX, -kRoadLimit, kRoadLimit);
    const float cameraX = worldCarX * 0.78f;
    const float carX = worldCarX - cameraX;
    const RoadStyle styleNow = gRoute.styleAt(routeProgressM);

    // v0.017 first real source-texture pass. It runs once before the existing
    // color/proxy geometry, then restores the color pipeline.
    drawSourceTexturePass(routeProgressM, cameraX);

    for (int i = 0; i < 40; ++i) {
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
        float leftBarrierX = 0.0f, leftBarrierZ = 0.0f;
        float rightBarrierX = 0.0f, rightBarrierZ = 0.0f;
        roadLocalOffset(roadX, z, yaw, -(roadHalf + 0.16f), 0.0f, leftBarrierX, leftBarrierZ);
        roadLocalOffset(roadX, z, yaw,  (roadHalf + 0.16f), 0.0f, rightBarrierX, rightBarrierZ);
        drawCube(leftBarrierX, roadY + 0.58f, leftBarrierZ,
                 0.22f, 0.72f, segLen + 0.12f, yaw,
                 barrierR * fog, barrierG * fog, barrierB * fog);
        drawCube(rightBarrierX, roadY + 0.58f, rightBarrierZ,
                 0.22f, 0.72f, segLen + 0.12f, yaw,
                 barrierR * fog, barrierG * fog, barrierB * fog);

        // v0.016 source atlas: different recovered LOD families are selected
        // by road context instead of stamping one AREA_2 mesh across the entire map.
        // The procedural deck remains underneath as collision/visual insurance.
        const float sourceScale = roadHalf * 0.96f;
        // The old flat-colored HighRoad stamping is intentionally paused in
        // v0.017 so it cannot cover the new source asphalt. The source mesh
        // remains cataloged for the later UV-preserving mesh conversion.
        // The LowRoad/Junction/Open atlas meshes remain cataloged, but are not
        // stamped as free-standing road modules in this hotfix. Their Unity
        // scene transforms are required for correct placement; repeating them
        // road-locally produced the large dark slabs seen in v0.016.


        // Real source road-line geometry is drawn on the non-junction surface families.
        if ((style == RoadStyle::HighLevel || style == RoadStyle::Underpass) &&
            (segId % 3) == 1) {
            drawSourceTriangles(gAtlasRoadLinesVbo, kAtlasRoadLinesVertsCount,
                                roadX, roadY + 0.145f, z,
                                sourceScale, sourceScale, sourceScale * 1.35f, yaw,
                                0.92f * fog, 0.91f * fog, 0.76f * fog);
        }

        // Highly visible reflective lane markings. These are a cheap but strong
        // speed cue on the 3DS screen.
        if ((segId & 1) == 0) {
            const float lineGlow = (style == RoadStyle::Tunnel) ? 1.0f : 0.90f;
            float lineLX = 0.0f, lineLZ = 0.0f, lineRX = 0.0f, lineRZ = 0.0f;
            roadLocalOffset(roadX, z, yaw, -kLaneWidth * 0.5f, 0.0f, lineLX, lineLZ);
            roadLocalOffset(roadX, z, yaw,  kLaneWidth * 0.5f, 0.0f, lineRX, lineRZ);
            drawCube(lineLX, roadY + 0.14f, lineLZ,
                     0.075f, 0.018f, 3.45f, yaw,
                     lineGlow * fog, lineGlow * fog, 0.80f * fog);
            drawCube(lineRX, roadY + 0.14f, lineRZ,
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
                    float fx = 0.0f, fz = 0.0f;
                    roadLocalOffset(roadX, z, yaw, float(side) * (roadHalf + 0.42f), 0.0f, fx, fz);
                    drawCube(fx, roadY + 1.62f, fz - 2.7f,
                             0.08f, 1.65f, 0.08f, yaw,
                             0.58f * fog, 0.24f * fog, 0.055f * fog);
                    drawCube(fx, roadY + 1.62f, fz + 2.7f,
                             0.08f, 1.65f, 0.08f, yaw,
                             0.58f * fog, 0.24f * fog, 0.055f * fog);
                    drawCube(fx, roadY + 2.35f, fz,
                             0.08f, 0.08f, segLen, yaw,
                             0.55f * fog, 0.20f * fog, 0.045f * fog);
                }
            }
        }

        // Normal roadside lamps; much closer to the player than the old scene.
        if ((segId % 3) == 1 && style != RoadStyle::Tunnel && style != RoadStyle::Underpass) {
            for (int side = -1; side <= 1; side += 2) {
                float lx = 0.0f, lz = 0.0f;
                roadLocalOffset(roadX, z, yaw, float(side) * (roadHalf + 1.05f), 0.0f, lx, lz);
                drawCube(lx, roadY + 1.42f, lz, 0.12f, 3.15f, 0.12f, yaw,
                         0.12f * fog, 0.11f * fog, 0.09f * fog);
                drawCube(lx, roadY + 3.02f, lz, 0.32f, 0.10f, 0.30f, yaw,
                         0.98f * fog, 0.52f * fog, 0.12f * fog);
            }
        }

        if (style == RoadStyle::HighLevel) {
            // The recovered source route is explicitly the AREA_2,1 HIGH branch.
            // Keep the player on an exposed upper deck with close infrastructure
            // rather than drawing the old temporary overhead-road corridor.
            if ((segId % 4) == 1) {
                for (int side = -1; side <= 1; side += 2) {
                    float sx = 0.0f, sz = 0.0f;
                    roadLocalOffset(roadX, z, yaw, float(side) * (roadHalf + 1.4f), 0.0f, sx, sz);
                    drawCube(sx, roadY - 2.0f, sz, 0.55f, 4.2f, 0.55f, yaw,
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
                     0.20f * fog, 0.20f * fog, 0.19f * fog);
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

    // First real source-car pass. The body shell is developer-authorized Livisa
    // geometry converted offline from the customization bundle to a ~6.3k-triangle
    // road LOD. Wheels/glass/lights stay cheap procedural pieces for now.
    const float carYaw = clampf(-s.driftAngleDeg * 0.012f, -0.42f, 0.42f);
    const float carZ = -4.35f;
    if (gLivisaCarVbo) {
        drawSourceTriangles(gLivisaCarVbo, kLivisaStockVertsCount,
                            carX, -0.96f, carZ,
                            1.0f, 1.0f, 1.0f, carYaw,
                            0.78f, 0.030f, 0.022f);
    }
    // Dark glass volume restores material separation until texture/UV support lands.
    drawCube(carX, -0.28f, carZ - 0.10f, 1.18f, 0.30f, 1.18f, carYaw,
             0.035f, 0.060f, 0.080f);

    for (int side = -1; side <= 1; side += 2) {
        drawCube(carX + float(side) * 0.86f, -0.79f, carZ + 1.08f,
                 0.20f, 0.38f, 0.53f, carYaw,
                 0.012f, 0.012f, 0.015f);
        drawCube(carX + float(side) * 0.86f, -0.79f, carZ - 1.12f,
                 0.20f, 0.38f, 0.53f, carYaw,
                 0.012f, 0.012f, 0.015f);
    }

    const float brakeGlow = braking ? 1.0f : 0.66f;
    drawCube(carX - 0.49f, -0.51f, carZ + 1.91f,
             0.28f, 0.10f, 0.055f, carYaw,
             brakeGlow, 0.018f, 0.006f);
    drawCube(carX + 0.49f, -0.51f, carZ + 1.91f,
             0.28f, 0.10f, 0.055f, carYaw,
             brakeGlow, 0.018f, 0.006f);
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
    const float baseZ = -7.65f;
    // Source mesh faces -Z; the showroom camera views it from +Z, so rotate 180°.
    const float yaw = 3.14159265f + 0.34f + std::sin(spin * 0.70f) * 0.18f;
    const float tireAccent = 0.22f + 0.10f * float(garage.tireLevel());
    const float turboAccent = 0.22f + 0.10f * float(garage.turboLevel());
    const float engineAccent = 0.72f + 0.055f * float(garage.engineLevel());

    if (gLivisaCarVbo) {
        drawSourceTriangles(gLivisaCarVbo, kLivisaStockVertsCount,
                            baseX, -1.00f, baseZ,
                            1.0f, 1.0f, 1.0f, yaw,
                            engineAccent, 0.045f, 0.030f);
    }

    // Cheap glass/material separation and wheels remain independent so future
    // customization slots can replace them without rebuilding the body VBO.
    drawGaragePart(baseX, -1.00f, baseZ, 0.0f, 0.67f, -0.08f,
                   1.16f, 0.30f, 1.25f, yaw,
                   0.045f, 0.070f, 0.085f);
    const float wheelZ[2] = {-1.12f, 1.08f};
    for (int axle = 0; axle < 2; ++axle) {
        for (int side = -1; side <= 1; side += 2) {
            const float lx = float(side) * 0.87f;
            drawGaragePart(baseX, -1.00f, baseZ, lx, 0.22f, wheelZ[axle],
                           0.20f, 0.42f, 0.56f, yaw,
                           0.015f, 0.015f, 0.018f);
            drawGaragePart(baseX, -1.00f, baseZ, lx, 0.22f, wheelZ[axle],
                           0.215f, 0.23f, 0.24f, yaw,
                           tireAccent, tireAccent, tireAccent);
        }
    }
    // Front lamps are at local -Z on the recovered source shell.
    drawGaragePart(baseX, -1.00f, baseZ, -0.52f, 0.45f, -2.03f,
                   0.32f, 0.10f, 0.055f, yaw,
                   0.96f, 0.86f, 0.58f);
    drawGaragePart(baseX, -1.00f, baseZ,  0.52f, 0.45f, -2.03f,
                   0.32f, 0.10f, 0.055f, yaw,
                   0.96f, 0.86f, 0.58f);
    // Intercooler accent grows slightly with turbo level.
    drawGaragePart(baseX, -1.00f, baseZ, 0.0f, 0.20f, -2.05f,
                   0.52f, 0.12f, 0.035f, yaw,
                   0.10f, 0.13f + turboAccent * 0.20f, 0.16f + turboAccent * 0.28f);
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
    std::printf("\x1b[1;1HNR TUNING // v0.017 SOURCE TEX      \x1b[K");
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
    audioTestInit();
    GarageState garage;
    Vehicle car(garage.makeVehicleConfig());
    RaceSession race;
    auto traffic = makeTraffic();

    GameMode mode = GameMode::Garage;
    int garageSelection = 0;
    const char* garageStatus = "Tune the build, rotate the car, then press Y.";
    float garageSpin = 0.0f;
    // World travel is intentionally separate from RaceSession progress.
    // RaceSession freezes when either racer finishes; the road must not.
    float worldProgressM = 0.0f;

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
        worldProgressM = 0.0f;
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

        // Keep visual world travel alive after a race result. Previously the
        // renderer used rt.playerProgressM, but RaceSession stops updating that
        // value as soon as either racer finishes. That made the entire scenery
        // freeze while the speedometer still showed the car moving.
        if (race.telemetry().phase != RacePhase::Countdown) {
            worldProgressM += (car.telemetry().speedKph / 3.6f) * dt;
            worldProgressM = clampf(worldProgressM, 0.0f,
                                    gRoute.totalLengthM() - 0.001f);
        }

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
        drawHighway(s, traffic, rt, race.nextGateDistanceM(), in.brake > 0.1f, worldProgressM);
        C3D_FrameEnd(0);

        if (rt.phase == RacePhase::Countdown) {
            const int count = std::max(1, int(std::ceil(rt.countdown)));
            std::printf("\x1b[1;1HNR3DS v0.017.1 - TEXTURE HOTFIX    \x1b[K");
            std::printf("\x1b[5;1HRACE: GET READY  %d          \x1b[K", count);
        } else if (rt.phase == RacePhase::Racing) {
            std::printf("\x1b[1;1HNR3DS v0.017.1 - TEXTURE HOTFIX    \x1b[K");
            std::printf("\x1b[5;1HRACE: GO  CP %d/%d             \x1b[K",
                        rt.checkpointIndex, int(RaceSession::kCheckpointCount));
        } else {
            std::printf("\x1b[1;1HNR3DS v0.017.1 - TEXTURE HOTFIX    \x1b[K");
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
        const auto& currentSection = gRoute.sectionAt(worldProgressM);
        std::printf("\x1b[14;1HZone: %-12s chunks %d-%d\x1b[K",
                    nr3ds::ExpresswayRoute::styleName(currentSection.style),
                    gRoute.activeChunkFirst(worldProgressM),
                    gRoute.activeChunkLast(worldProgressM));
        std::printf("\x1b[15;1HSource: %-18s\x1b[K", currentSection.sourceName);
        std::printf("\x1b[16;1HGeo: source meshes + Livisa\x1b[K");
        std::printf("\x1b[17;1HTex: ROAD2 + TUNNEL GRUNGE      \x1b[K");
        std::printf("\x1b[18;1HAudio: %s\x1b[K", gAudioReady ? "NDSP test loop" : "DSP unavailable");
        std::printf("\x1b[19;1HCPU: %6.2f%% GPU: %6.2f%%\x1b[K",
                    C3D_GetProcessingTime() * 6.0f,
                    C3D_GetDrawingTime() * 6.0f);
    }

    audioTestExit();
    sceneExit();
    C3D_Fini();
    gfxExit();
    return 0;
}
