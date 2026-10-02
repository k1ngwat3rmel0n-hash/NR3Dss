#include "nr_physics.hpp"

#include <cstdio>
#include <cstdlib>

using nr3ds::InputState;
using nr3ds::Vehicle;

int main(int argc, char** argv) {
    const char* outPath = (argc > 1) ? argv[1] : "nr3ds_sim.csv";
    FILE* f = std::fopen(outPath, "wb");
    if (!f) {
        std::perror("fopen");
        return 1;
    }

    std::fprintf(f, "time,speed_kph,rpm,gear,torque_nm,turbo,health,water_c,oil_c,tire_temp,rear_slip,drift_deg,x,y\n");

    Vehicle car;
    InputState in;
    constexpr float dt = 1.0f / 60.0f;
    constexpr int frames = 60 * 30;

    for (int frame = 0; frame < frames; ++frame) {
        const float t = frame * dt;

        // 0-12s: launch/accelerate straight.
        // 12-18s: sustained fast corner.
        // 18-22s: handbrake-assisted slide.
        // 22-30s: recover and continue.
        in = {};
        in.throttle = (t < 26.0f) ? 1.0f : 0.25f;
        if (t >= 12.0f && t < 18.0f) in.steer = 0.45f;
        if (t >= 18.0f && t < 22.0f) {
            in.steer = 0.65f;
            in.handbrake = 1.0f;
            in.throttle = 0.70f;
        }
        if (t >= 22.0f) in.steer = 0.18f;
        if (t >= 27.0f) in.brake = 0.35f;

        car.step(in, dt);
        const auto& s = car.telemetry();

        std::fprintf(f,
            "%.4f,%.3f,%.1f,%d,%.3f,%.4f,%.3f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f\n",
            t, s.speedKph, s.rpm, s.gear, s.engineTorqueNm, s.turboSpool,
            s.engineHealth, s.waterTempC, s.oilTempC, s.tireTemp, s.rearSlip,
            s.driftAngleDeg, s.posX, s.posY);
    }

    std::fclose(f);
    const auto& s = car.telemetry();
    std::printf("NR3DS portable physics smoke test complete.\n");
    std::printf("Final: %.1f km/h | %.0f rpm | gear %d | drift %.1f deg | pos=(%.1f, %.1f)\n",
        s.speedKph, s.rpm, s.gear, s.driftAngleDeg, s.posX, s.posY);
    std::printf("CSV: %s\n", outPath);
    return 0;
}
