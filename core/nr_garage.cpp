#include "nr_garage.hpp"

#include <algorithm>

namespace nr3ds {
namespace {
constexpr std::array<int, GarageState::kMaxUpgradeLevel> kEngineCosts{700, 1400, 2500};
constexpr std::array<int, GarageState::kMaxUpgradeLevel> kTurboCosts{650, 1350, 2300};
constexpr std::array<int, GarageState::kMaxUpgradeLevel> kTireCosts{450, 900, 1600};

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}
}

GarageState::GarageState() {
    gearRatios_ = Vehicle::makeDefaultConfig().gearRatios;
}

int GarageState::levelFor(UpgradeKind kind) const {
    switch (kind) {
        case UpgradeKind::Engine: return engineLevel_;
        case UpgradeKind::Turbo: return turboLevel_;
        case UpgradeKind::Tires: return tireLevel_;
    }
    return 0;
}

int& GarageState::levelForMutable(UpgradeKind kind) {
    switch (kind) {
        case UpgradeKind::Engine: return engineLevel_;
        case UpgradeKind::Turbo: return turboLevel_;
        case UpgradeKind::Tires: return tireLevel_;
    }
    return engineLevel_;
}

int GarageState::nextCost(UpgradeKind kind) const {
    const int level = levelFor(kind);
    if (level >= kMaxUpgradeLevel) return -1;
    switch (kind) {
        case UpgradeKind::Engine: return kEngineCosts[std::size_t(level)];
        case UpgradeKind::Turbo: return kTurboCosts[std::size_t(level)];
        case UpgradeKind::Tires: return kTireCosts[std::size_t(level)];
    }
    return -1;
}

bool GarageState::purchase(UpgradeKind kind) {
    const int cost = nextCost(kind);
    if (cost < 0 || cash_ < cost) return false;
    cash_ -= cost;
    ++levelForMutable(kind);
    return true;
}

void GarageState::rewardRace(bool won) {
    if (won) {
        cash_ += 1000;
        ++wins_;
    } else {
        cash_ += 300;
        ++losses_;
    }
}

float GarageState::gearRatio(std::size_t gearIndex) const {
    if (gearIndex >= gearRatios_.size()) return 0.0f;
    return gearRatios_[gearIndex];
}

void GarageState::adjustFinalDrive(float delta) {
    finalDrive_ = clampf(finalDrive_ + delta, 3.20f, 5.00f);
}

void GarageState::adjustGearRatio(std::size_t gearIndex, float delta) {
    if (gearIndex >= gearRatios_.size()) return;

    float minValue = 0.55f;
    float maxValue = 4.20f;
    if (gearIndex + 1 < gearRatios_.size()) {
        minValue = gearRatios_[gearIndex + 1] + 0.08f;
    }
    if (gearIndex > 0) {
        maxValue = gearRatios_[gearIndex - 1] - 0.08f;
    }
    gearRatios_[gearIndex] = clampf(gearRatios_[gearIndex] + delta, minValue, maxValue);
}

VehicleConfig GarageState::makeVehicleConfig() const {
    VehicleConfig cfg = Vehicle::makeDefaultConfig();
    cfg.baseTorqueNm += 30.0f * float(engineLevel_);
    cfg.engineStrengthHp += 45.0f * float(engineLevel_);
    cfg.coolingRate += 10.0f * float(engineLevel_);

    cfg.turboMaxExtraHp += 25.0f * float(turboLevel_);
    cfg.turboLevel = std::min(3, cfg.turboLevel + turboLevel_);

    const float tireBonus = 0.045f * float(tireLevel_);
    cfg.frontGrip += tireBonus;
    cfg.rearGrip += tireBonus;

    cfg.finalDrive = finalDrive_;
    cfg.gearRatios = gearRatios_;
    return cfg;
}

} // namespace nr3ds
