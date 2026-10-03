#include "nr_physics.hpp"
#include "nr_race.hpp"
#include "nr_garage.hpp"
#include "nr_world.hpp"

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

    {
        RaceSession race;
        constexpr float dt = 1.0f / 60.0f;
        for (int i = 0; i < 190; ++i) race.update(0.0f, dt);
        assert(race.telemetry().phase == RacePhase::Racing);
        assert(race.telemetry().elapsed > 0.0f);
    }

    {
        RaceSession race;
        constexpr float dt = 1.0f / 60.0f;
        // Clear the countdown, then verify a fast player can complete the race.
        for (int i = 0; i < 181; ++i) race.update(0.0f, dt);
        for (int i = 0; i < 60 * 30 && race.telemetry().phase != RacePhase::Finished; ++i) {
            race.update(220.0f, dt);
        }
        assert(race.telemetry().phase == RacePhase::Finished);
        assert(race.telemetry().playerProgressM >= RaceSession::kCourseLengthM);
        assert(race.telemetry().playerWon);
    }


    {
        GarageState garage;
        assert(garage.cash() == 1100);
        const auto base = garage.makeVehicleConfig();
        assert(std::fabs(base.baseTorqueNm - 320.0f) < 0.01f);
        assert(std::fabs(base.finalDrive - 4.10f) < 0.01f);

        assert(garage.purchase(UpgradeKind::Engine));
        assert(garage.engineLevel() == 1);
        assert(garage.cash() == 400);
        const auto upgraded = garage.makeVehicleConfig();
        assert(upgraded.baseTorqueNm > base.baseTorqueNm);
        assert(!garage.purchase(UpgradeKind::Turbo)); // not enough cash yet

        garage.rewardRace(true);
        assert(garage.cash() == 1400);
        assert(garage.wins() == 1);
        assert(garage.purchase(UpgradeKind::Turbo));
        assert(garage.turboLevel() == 1);

        const float oldFinal = garage.finalDrive();
        garage.adjustFinalDrive(0.10f);
        assert(garage.finalDrive() > oldFinal);

        const float oldFirst = garage.gearRatio(0);
        garage.adjustGearRatio(0, 0.05f);
        assert(garage.gearRatio(0) >= oldFirst);
    }


    {
        ExpresswayRoute route;
        assert(route.totalLengthM() > 1200.0f);
        assert(route.styleAt(40.0f) == RoadStyle::Open);
        assert(route.styleAt(170.0f) == RoadStyle::SodiumFence);
        assert(route.styleAt(700.0f) == RoadStyle::Tunnel);
        assert(route.styleAt(860.0f) == RoadStyle::Junction);
        assert(route.activeChunkFirst(400.0f) <= route.chunkIndex(400.0f));
        assert(route.activeChunkLast(400.0f) >= route.chunkIndex(650.0f));
        const float c0 = route.centerAt(0.0f);
        const float c1 = route.centerAt(900.0f);
        assert(std::fabs(c1 - c0) > 0.2f);
    }

    std::puts("All NR3DS core/race/garage/world tests passed.");
    return 0;
}
