#include "nr_physics.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace nr3ds;

static void runFor(Vehicle& car, InputState in, float seconds) {
    const float dt = 1.0f / 60.0f;
    const int n = static_cast<int>(seconds / dt);
    for (int i = 0; i < n; ++i) car.step(in, dt);
}

int main() {
    {
        Vehicle car;
        InputState in{};
        in.throttle = 1.0f;
        runFor(car, in, 8.0f);
        assert(car.telemetry().speedKph > 20.0f);
        assert(car.telemetry().rpm >= 800.0f);
    }

    {
        Vehicle car;
        InputState in{};
        in.throttle = 1.0f;
        runFor(car, in, 5.0f);
        in.handbrake = 1.0f;
        in.steer = 0.7f;
        runFor(car, in, 0.5f);
        assert(car.telemetry().handbrakeTimer > 0.0f);
        assert(car.telemetry().rearSlip > 0.1f);
    }

    {
        Vehicle car;
        const float startHealth = car.telemetry().engineHealth;
        InputState in{};
        in.throttle = 1.0f;
        runFor(car, in, 10.0f);
        assert(car.telemetry().engineHealth <= startHealth);
    }

    std::puts("All NR3DS core tests passed.");
    return 0;
}
