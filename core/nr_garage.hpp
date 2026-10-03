#pragma once

#include "nr_physics.hpp"

#include <array>
#include <cstddef>

namespace nr3ds {

enum class UpgradeKind {
    Engine,
    Turbo,
    Tires,
};

class GarageState {
public:
    static constexpr int kMaxUpgradeLevel = 3;

    GarageState();

    int cash() const { return cash_; }
    int wins() const { return wins_; }
    int losses() const { return losses_; }
    int engineLevel() const { return engineLevel_; }
    int turboLevel() const { return turboLevel_; }
    int tireLevel() const { return tireLevel_; }
    float finalDrive() const { return finalDrive_; }
    float gearRatio(std::size_t gearIndex) const;

    int nextCost(UpgradeKind kind) const;
    bool purchase(UpgradeKind kind);
    void rewardRace(bool won);

    void adjustFinalDrive(float delta);
    void adjustGearRatio(std::size_t gearIndex, float delta);

    VehicleConfig makeVehicleConfig() const;

private:
    int cash_ = 1100;
    int wins_ = 0;
    int losses_ = 0;
    int engineLevel_ = 0;
    int turboLevel_ = 0;
    int tireLevel_ = 0;
    float finalDrive_ = 4.10f;
    std::array<float, kMaxGears> gearRatios_{};

    int levelFor(UpgradeKind kind) const;
    int& levelForMutable(UpgradeKind kind);
};

} // namespace nr3ds
