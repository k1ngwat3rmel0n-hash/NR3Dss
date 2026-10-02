#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nr3ds {

constexpr std::size_t kMaxGears = 6;
constexpr std::size_t kCurveSamples = 16;

struct Curve16 {
    std::array<float, kCurveSamples> v{};
    float sample(float t) const;
};

struct InputState {
    float throttle = 0.0f;   // 0..1
    float brake = 0.0f;      // 0..1
    float steer = 0.0f;      // -1..1
    float clutch = 0.0f;     // 0 engaged, 1 disengaged
    float handbrake = 0.0f;  // 0..1
    bool shiftUp = false;
    bool shiftDown = false;
};

struct VehicleConfig {
    float massKg = 1320.0f;
    float wheelRadiusM = 0.315f;
    float wheelbaseM = 2.52f;
    float finalDrive = 4.10f;
    std::array<float, kMaxGears> gearRatios{3.20f, 2.10f, 1.55f, 1.22f, 1.00f, 0.84f};
    std::size_t gearCount = 6;

    float idleRpm = 950.0f;
    float maxRpm = 8000.0f;
    float baseTorqueNm = 320.0f;
    float engineStrengthHp = 420.0f;
    float coolingRate = 260.0f;

    float dragCdA = 0.72f;
    float rollingResistance = 0.015f;
    float brakeForceN = 10500.0f;

    float frontGrip = 1.08f;
    float rearGrip = 1.02f;
    float maxSteerDegLow = 32.0f;
    float maxSteerDegHigh = 4.8f;
    float steerDeadzoneLow = 0.035f;
    float steerDeadzoneHigh = 0.095f;
    float steerCurveHighSpeed = 2.15f;
    float steerRateLow = 10.0f;
    float steerRateHigh = 3.2f;

    float turboMaxExtraHp = 105.0f;
    int turboLevel = 2;

    Curve16 torqueCurve;
    Curve16 turboCurve;
    Curve16 tireTempGripCurve;
};

struct Telemetry {
    float speedMps = 0.0f;
    float speedKph = 0.0f;
    float rpm = 0.0f;
    int gear = 1;

    float engineTorqueNm = 0.0f;
    float turboSpool = 0.0f;
    float engineHealth = 100.0f;
    float waterTempC = 90.0f;
    float oilTempC = 95.0f;

    float tireTemp = 0.25f;
    float frontSideGrip = 1.0f;
    float rearSideGrip = 1.0f;
    float rearSlip = 0.0f;
    float driftAngleDeg = 0.0f;
    float handbrakeTimer = 0.0f;
    float steerFiltered = 0.0f;

    float posX = 0.0f;
    float posY = 0.0f;
    float headingRad = 0.0f;
};

class Vehicle {
public:
    explicit Vehicle(const VehicleConfig& cfg = makeDefaultConfig());

    void reset();
    void step(const InputState& in, float dt);

    const Telemetry& telemetry() const { return t_; }
    const VehicleConfig& config() const { return cfg_; }
    void setConfig(const VehicleConfig& cfg);
    void applyImpact(float speedRetention, float lateralKick);
    void constrainLateral(float minX, float maxX, float speedRetention);

    static VehicleConfig makeDefaultConfig();

private:
    VehicleConfig cfg_{};
    Telemetry t_{};

    float engineInertia_ = 1.0f;
    float inductionLerp_ = 0.0f;
    float heatSoak_ = 1.0f;
    float clutchBlend_ = 1.0f;
    float lastSpeedMps_ = 0.0f;
    float lateralVelocity_ = 0.0f;
    float yawRate_ = 0.0f;
    float filteredSteer_ = 0.0f;

    int gearIndex_ = 0; // 0-based forward gear

    void updateTransmission(const InputState& in, float dt);
    void updateForcedInduction(const InputState& in, float dt);
    void updateEngine(const InputState& in, float dt);
    void updateTiresAndGrip(const InputState& in, float dt);
    void integrateChassis(const InputState& in, float dt);
};

} // namespace nr3ds
