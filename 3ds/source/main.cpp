#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <cmath>

#include "nr_physics.hpp"

using nr3ds::InputState;
using nr3ds::Vehicle;

namespace {
constexpr float kTopW = 400.0f;
constexpr float kTopH = 240.0f;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void drawRoad(const nr3ds::Telemetry& s) {
    const u32 sky = C2D_Color32(4, 6, 12, 255);
    const u32 road = C2D_Color32(22, 24, 30, 255);
    const u32 shoulder = C2D_Color32(56, 58, 64, 255);
    const u32 lane = C2D_Color32(224, 224, 205, 255);
    const u32 red = C2D_Color32(220, 35, 25, 255);
    const u32 glass = C2D_Color32(40, 90, 130, 255);

    C2D_DrawRectSolid(0, 0, 0, kTopW, kTopH, sky);
    C2D_DrawRectSolid(65, 0, 0, 270, kTopH, shoulder);
    C2D_DrawRectSolid(72, 0, 0, 256, kTopH, road);

    const float scroll = std::fmod(s.posY * 3.0f, 44.0f);
    for (int i = -1; i < 7; ++i) {
        const float y = float(i) * 44.0f + scroll;
        C2D_DrawRectSolid(155, y, 0, 4, 24, lane);
        C2D_DrawRectSolid(241, y, 0, 4, 24, lane);
    }

    const float carX = 200.0f + clampf(s.posX * 1.5f, -110.0f, 110.0f);
    const float carY = 178.0f;
    C2D_DrawRectSolid(carX - 9, carY - 15, 0, 18, 30, red);
    C2D_DrawRectSolid(carX - 6, carY - 10, 0, 12, 8, glass);
    C2D_DrawRectSolid(carX - 7, carY + 12, 0, 5, 2, C2D_Color32(255, 50, 30, 255));
    C2D_DrawRectSolid(carX + 2, carY + 12, 0, 5, 2, C2D_Color32(255, 50, 30, 255));
}
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    consoleInit(GFX_BOTTOM, nullptr);

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    Vehicle car;

    std::printf("NR3DS v0.001 - Old 3DS prototype\n");
    std::printf("A throttle | B brake | X handbrake\n");
    std::printf("L/R shift | Circle Pad steer | START exit\n");

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

        car.step(in, 1.0f / 60.0f);
        const auto& s = car.telemetry();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);
        drawRoad(s);
        C3D_FrameEnd(0);

        std::printf("\x1b[5;1HSpeed: %6.1f km/h   \x1b[K", s.speedKph);
        std::printf("\x1b[6;1HRPM:   %6.0f  Gear: %d \x1b[K", s.rpm, s.gear);
        std::printf("\x1b[7;1HTurbo: %5.2f  Slip: %5.2f\x1b[K", s.turboSpool, s.rearSlip);
        std::printf("\x1b[8;1HDrift: %6.1f deg       \x1b[K", s.driftAngleDeg);
        std::printf("\x1b[9;1HTire:  %5.2f  HB: %5.2f\x1b[K", s.tireTemp, s.handbrakeTimer);
        std::printf("\x1b[11;1HCPU: %6.2f%% GPU: %6.2f%%\x1b[K",
                    C3D_GetProcessingTime() * 6.0f, C3D_GetDrawingTime() * 6.0f);
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
