#include "nr_physics.hpp"

#include <algorithm>
#include <cmath>

namespace nr3ds {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kG = 9.80665f;

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}
inline float clamp01(float v) { return clampf(v, 0.0f, 1.0f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * clamp01(t); }
inline float signf(float v) { return (v > 0.0f) - (v < 0.0f); }
inline float deg2rad(float d) { return d * (kPi / 180.0f); }
inline float rad2deg(float r) { return r * (180.0f / kPi); }
}

float Curve16::sample(float t) const {
    t = clamp01(t);
    const float f = t * float(kCurveSamples - 1);
    const std::size_t i = static_cast<std::size_t>(f);
    if (i >= kCurveSamples - 1) return v.back();
    const float frac = f - float(i);
    return v[i] + (v[i + 1] - v[i]) * frac;
}

VehicleConfig Vehicle::makeDefaultConfig() {
    VehicleConfig c;

    // Placeholder serialized curves. The shape is intentionally cheap and smooth;
    // the exact NIGHT-RUNNERS keyframes still need extraction from Unity assets.
    c.torqueCurve.v = {
        0.52f, 0.62f, 0.73f, 0.84f,
        0.92f, 0.98f, 1.00f, 0.99f,
        0.96f, 0.93f, 0.89f, 0.84f,
        0.78f, 0.70f, 0.60f, 0.48f
    };

    c.turboCurve.v = {
        0.00f, 0.00f, 0.02f, 0.05f,
        0.11f, 0.20f, 0.33f, 0.48f,
        0.63f, 0.76f, 0.86f, 0.93f,
        0.97f, 0.99f, 1.00f, 1.00f
    };

    c.tireTempGripCurve.v = {
        0.78f, 0.82f, 0.86f, 0.90f,
        0.94f, 0.98f, 1.00f, 1.01f,
        1.01f, 1.00f, 0.99f, 0.97f,
        0.94f, 0.90f, 0.86f, 0.80f
    };

    return c;
}

Vehicle::Vehicle(const VehicleConfig& cfg) : cfg_(cfg) { reset(); }

void Vehicle::setConfig(const VehicleConfig& cfg) {
    cfg_ = cfg;
    reset();
}

void Vehicle::applyImpact(float speedRetention, float lateralKick) {
    const float keep = clampf(speedRetention, 0.0f, 1.0f);
    t_.speedMps *= keep;
    t_.speedKph = t_.speedMps * 3.6f;
    lateralVelocity_ += lateralKick;
    yawRate_ += lateralKick * 0.08f;
    t_.rearSlip = std::max(t_.rearSlip, 0.45f);
}

void Vehicle::constrainLateral(float minX, float maxX, float speedRetention) {
    if (minX > maxX) std::swap(minX, maxX);
    if (t_.posX < minX) {
        t_.posX = minX;
        lateralVelocity_ = std::fabs(lateralVelocity_) * 0.20f;
        yawRate_ *= -0.25f;
        applyImpact(speedRetention, 0.75f);
    } else if (t_.posX > maxX) {
        t_.posX = maxX;
        lateralVelocity_ = -std::fabs(lateralVelocity_) * 0.20f;
        yawRate_ *= -0.25f;
        applyImpact(speedRetention, -0.75f);
    }
}

void Vehicle::reset() {
    t_ = Telemetry{};
    t_.rpm = cfg_.idleRpm;
    t_.gear = 1;
    t_.engineHealth = 100.0f;
    t_.waterTempC = 90.0f;
    t_.oilTempC = 95.0f;
    t_.tireTemp = 0.25f;
    t_.frontSideGrip = cfg_.frontGrip;
    t_.rearSideGrip = cfg_.rearGrip;
    engineInertia_ = 1.0f;
    inductionLerp_ = 0.0f;
    heatSoak_ = 1.0f;
    clutchBlend_ = 1.0f;
    lastSpeedMps_ = 0.0f;
    lateralVelocity_ = 0.0f;
    yawRate_ = 0.0f;
    filteredSteer_ = 0.0f;
    gearIndex_ = 0;
}

void Vehicle::updateTransmission(const InputState& in, float dt) {
    (void)dt;
    const int maxGear = int(std::max<std::size_t>(1, cfg_.gearCount)) - 1;
    if (in.shiftUp && gearIndex_ < maxGear) ++gearIndex_;
    if (in.shiftDown && gearIndex_ > 0) --gearIndex_;

    // Lightweight auto-shift fallback. Explicit button presses still win.
    if (!in.shiftUp && !in.shiftDown) {
        if (t_.rpm > cfg_.maxRpm * 0.96f && gearIndex_ < maxGear) ++gearIndex_;
        else if (t_.rpm < cfg_.maxRpm * 0.38f && gearIndex_ > 0 && t_.speedKph > 25.0f) --gearIndex_;
    }
    t_.gear = gearIndex_ + 1;
}

void Vehicle::updateForcedInduction(const InputState& in, float dt) {
    const float gas = clamp01(in.throttle);
    const float rpmN = clamp01(t_.rpm / cfg_.maxRpm);

    // Recovered NIGHT-RUNNERS behavior: spool tracks normalized RPM under throttle,
    // and falls at roughly 5 units/second off-throttle.
    if (t_.rpm > cfg_.maxRpm * 0.05f && gas >= 0.5f) {
        inductionLerp_ = std::max(inductionLerp_, rpmN);
        inductionLerp_ = clamp01(inductionLerp_);
    } else if (gas <= 0.001f) {
        inductionLerp_ = std::max(0.0f, inductionLerp_ - dt * 5.0f);
    } else {
        inductionLerp_ = std::max(0.0f, inductionLerp_ - dt * 1.2f);
    }

    t_.turboSpool = cfg_.turboCurve.sample(inductionLerp_);
}

void Vehicle::updateEngine(const InputState& in, float dt) {
    const float gas = clamp01(in.throttle);
    const float clutch = clamp01(in.clutch);
    const float ratio = cfg_.gearRatios[std::size_t(gearIndex_)] * cfg_.finalDrive;
    const float wheelRps = (cfg_.wheelRadiusM > 0.001f)
        ? t_.speedMps / (2.0f * kPi * cfg_.wheelRadiusM)
        : 0.0f;
    const float coupledRpm = std::max(cfg_.idleRpm, wheelRps * 60.0f * ratio);

    // Recovered qualitative EngineInertia behavior: free-rev quickly with clutch,
    // react differently off throttle, and soften with slip.
    const float rpmN = clamp01(t_.rpm / cfg_.maxRpm);
    if (clutch > 0.95f && gas > 0.95f) {
        engineInertia_ = lerp(4.0f, 10.0f, rpmN);
    } else if (gas < 0.01f) {
        engineInertia_ = (t_.rearSlip > 0.25f) ? 8.0f : 4.0f;
    } else if (t_.rearSlip > 0.25f) {
        engineInertia_ = lerp(8.0f, 3.0f, rpmN);
    } else {
        const float slipT = clamp01(t_.rearSlip / 2.0f);
        engineInertia_ = 7.0f * lerp(2.0f, 1.0f, rpmN) * lerp(1.0f, 0.2f, slipT);
    }

    const float freeRevTarget = lerp(cfg_.idleRpm, cfg_.maxRpm * 1.02f, gas);
    clutchBlend_ = 1.0f - clutch;
    const float rpmTarget = lerp(freeRevTarget, coupledRpm, clutchBlend_);
    t_.rpm += (rpmTarget - t_.rpm) * clamp01(engineInertia_ * dt);
    t_.rpm = clampf(t_.rpm, cfg_.idleRpm * 0.85f, cfg_.maxRpm * 1.03f);

    // Thermal/damage structure mirrors the recovered enginePowerRealTime logic.
    const float extraHp = cfg_.turboMaxExtraHp * t_.turboSpool;
    const float overloadHp = (cfg_.baseTorqueNm * 0.00134102f * t_.rpm) + extraHp;

    const float heatRate =
        lerp(4.0f, 1.0f, clamp01(cfg_.coolingRate / std::max(overloadHp, 1.0f))) +
        lerp(0.0f, 2.0f, clamp01((t_.speedKph - 200.0f) / 200.0f));

    const float cooling = lerp(0.35f, 2.2f, clamp01(t_.speedKph / 160.0f));
    t_.waterTempC += (gas * heatRate - cooling) * dt;
    t_.oilTempC += (gas * heatRate * 0.75f - cooling * 0.45f) * dt;
    t_.waterTempC = clampf(t_.waterTempC, 70.0f, 155.0f);
    t_.oilTempC = clampf(t_.oilTempC, 75.0f, 165.0f);

    if (gas >= 0.999f && t_.waterTempC > 130.0f) {
        const float damageRate = lerp(0.01f, 0.03f, (t_.waterTempC - 130.0f) / 10.0f);
        t_.engineHealth -= damageRate * dt;
    }
    if (gas >= 0.999f && t_.oilTempC > 130.0f) {
        const float damageRate = lerp(0.01f, 0.03f, (t_.oilTempC - 130.0f) / 10.0f);
        t_.engineHealth -= damageRate * dt;
    }
    t_.engineHealth = clampf(t_.engineHealth, 0.0f, 100.0f);

    const float healthPower = (t_.engineHealth < 20.0f)
        ? lerp(0.60f, 1.0f, t_.engineHealth / 20.0f)
        : 1.0f;

    heatSoak_ = lerp(1.0f, 0.5f,
        clamp01((t_.waterTempC + t_.oilTempC - 250.0f) / 30.0f));

    const float torqueN = cfg_.torqueCurve.sample(clamp01(t_.rpm / cfg_.maxRpm));
    const float turboTorqueNm = (extraHp * 7121.0f) / std::max(t_.rpm, 1000.0f);

    // 0.60 final multiplier is present in the recovered PC method.
    t_.engineTorqueNm = (cfg_.baseTorqueNm * torqueN + turboTorqueNm)
        * healthPower * heatSoak_ * 0.60f;
}

void Vehicle::updateTiresAndGrip(const InputState& in, float dt) {
    const float brake = clamp01(in.brake);
    const float hb = clamp01(in.handbrake);

    // Estimate demanded longitudinal force versus a simple traction envelope.
    const float ratio = cfg_.gearRatios[std::size_t(gearIndex_)] * cfg_.finalDrive;
    const float driveForce = (t_.engineTorqueNm * ratio * 0.86f) / std::max(cfg_.wheelRadiusM, 0.05f);
    const float normalRear = cfg_.massKg * kG * 0.52f;
    const float rearLimit = normalRear * cfg_.rearGrip;
    t_.rearSlip = clamp01((std::fabs(driveForce) - rearLimit * 0.72f) / std::max(rearLimit, 1.0f));
    if (hb > 0.1f) t_.rearSlip = std::max(t_.rearSlip, 0.75f * hb);

    const bool sliding = t_.rearSlip > 0.18f || std::fabs(t_.driftAngleDeg) > 7.0f;

    // Recovered handbrake timer: growth slows as vehicle speed approaches 150 km/h.
    if (hb >= 0.999f) {
        if (t_.handbrakeTimer <= 1.0f) {
            const float speedT = clamp01(t_.speedKph / 150.0f);
            t_.handbrakeTimer += dt * lerp(1.0f, 0.1f, speedT);
            t_.handbrakeTimer = clamp01(t_.handbrakeTimer);
        }
    } else {
        t_.handbrakeTimer = 0.0f;
    }

    // Recovered thermal structure, with the ambiguous PC expression preserved in spirit.
    if (sliding) {
        const float tempSpeed = clamp01(t_.rearSlip);
        const float heatingRate = lerp(0.05f, 0.10f, tempSpeed);
        t_.tireTemp += dt * heatingRate;
    } else if (t_.tireTemp > 0.0f) {
        t_.tireTemp -= dt * 0.01f;
    }
    t_.tireTemp = clamp01(t_.tireTemp);
    const float tempGrip = cfg_.tireTempGripCurve.sample(t_.tireTemp);

    // Power and gear-dependent grip trend reconstructed from TireModel.
    const float powerT = clamp01((t_.engineTorqueNm - 80.0f) / 420.0f);
    const float gearT = (cfg_.gearCount > 1)
        ? float(gearIndex_) / float(cfg_.gearCount - 1)
        : 0.0f;
    const float speedCross = clamp01(t_.speedKph / 120.0f);

    float frontSide = cfg_.frontGrip * tempGrip;
    float rearSide = cfg_.rearGrip * tempGrip;

    rearSide *= lerp(1.04f, 0.88f, powerT);
    rearSide *= lerp(0.92f, 1.05f, gearT);
    frontSide *= lerp(1.02f, 0.96f, speedCross);

    if (hb > 0.01f) {
        // Directly based on a recovered handbrake override branch.
        rearSide *= lerp(0.20f, 0.75f, t_.handbrakeTimer);
        frontSide *= 1.50f;
    }

    // Braking shifts usable lateral grip forward in this lightweight model.
    frontSide *= lerp(1.0f, 1.08f, brake);
    rearSide *= lerp(1.0f, 0.92f, brake);

    t_.frontSideGrip = clampf(frontSide, 0.20f, 2.5f);
    t_.rearSideGrip = clampf(rearSide, 0.15f, 2.5f);
}

void Vehicle::integrateChassis(const InputState& in, float dt) {
    const float throttle = clamp01(in.throttle);
    const float brake = clamp01(in.brake);
    const float steerInput = clampf(in.steer, -1.0f, 1.0f);

    const float ratio = cfg_.gearRatios[std::size_t(gearIndex_)] * cfg_.finalDrive;
    float driveForce = (t_.engineTorqueNm * ratio * 0.86f) / std::max(cfg_.wheelRadiusM, 0.05f);
    driveForce *= throttle * (1.0f - clamp01(in.clutch));

    const float normalRear = cfg_.massKg * kG * 0.52f;
    const float tractionLimit = normalRear * t_.rearSideGrip * 1.05f;
    driveForce = clampf(driveForce, -tractionLimit, tractionLimit);

    const float drag = 0.5f * 1.225f * cfg_.dragCdA * t_.speedMps * t_.speedMps;
    const float rolling = cfg_.massKg * kG * cfg_.rollingResistance;
    const float braking = brake * cfg_.brakeForceN + clamp01(in.handbrake) * cfg_.brakeForceN * 0.35f;

    float net = driveForce - drag - rolling - braking;
    if (t_.speedMps <= 0.01f && net < 0.0f) net = 0.0f;

    t_.speedMps = std::max(0.0f, t_.speedMps + (net / cfg_.massKg) * dt);
    t_.speedKph = t_.speedMps * 3.6f;

    // High-speed steering precision layer. At highway speed the center of the
    // Circle Pad becomes much finer, the maximum road-wheel angle drops, and
    // steering changes are rate-limited. During a real slide/handbrake event we
    // deliberately relax the filtering so countersteer stays responsive.
    const float precisionT = clamp01((t_.speedKph - 55.0f) / 165.0f);
    const float driftGain = clamp01(t_.rearSlip * 1.35f + clamp01(in.handbrake) * 0.55f);
    const float driftRelease = clamp01(driftGain * 1.6f);

    float deadzone = lerp(cfg_.steerDeadzoneLow, cfg_.steerDeadzoneHigh, precisionT);
    const float rawAbs = std::fabs(steerInput);
    float normalized = 0.0f;
    if (rawAbs > deadzone) {
        normalized = (rawAbs - deadzone) / std::max(1.0f - deadzone, 0.001f);
    }

    const float exponent = lerp(1.0f, cfg_.steerCurveHighSpeed, precisionT * (1.0f - 0.65f * driftRelease));
    float shapedSteer = signf(steerInput) * std::pow(clamp01(normalized), exponent);
    shapedSteer = lerp(shapedSteer, steerInput, 0.55f * driftRelease);

    float steerRate = lerp(cfg_.steerRateLow, cfg_.steerRateHigh, precisionT);
    steerRate = lerp(steerRate, cfg_.steerRateLow * 1.15f, driftRelease);
    const float maxStep = steerRate * dt;
    filteredSteer_ += clampf(shapedSteer - filteredSteer_, -maxStep, maxStep);
    filteredSteer_ = clampf(filteredSteer_, -1.0f, 1.0f);
    t_.steerFiltered = filteredSteer_;

    const float speedSteerT = clamp01(t_.speedKph / 190.0f);
    float maxSteerDeg = lerp(cfg_.maxSteerDegLow, cfg_.maxSteerDegHigh, speedSteerT);
    maxSteerDeg = lerp(maxSteerDeg, std::max(maxSteerDeg, 11.0f), 0.55f * driftRelease);
    const float steerAngle = deg2rad(maxSteerDeg * filteredSteer_);

    // Bicycle model baseline, softened by front grip and with rear-slip drift contribution.
    float desiredYaw = 0.0f;
    if (std::fabs(steerAngle) > 0.0001f && cfg_.wheelbaseM > 0.1f) {
        desiredYaw = (t_.speedMps / cfg_.wheelbaseM) * std::tan(steerAngle);
    }
    desiredYaw *= clampf(t_.frontSideGrip, 0.3f, 1.4f);

    const float steerDirection = signf(filteredSteer_);
    const float driftYaw = steerDirection * driftGain * lerp(0.2f, 1.6f, clamp01(t_.speedKph / 100.0f));

    const float targetYaw = desiredYaw + driftYaw;
    yawRate_ += (targetYaw - yawRate_) * clamp01(dt * lerp(8.0f, 3.0f, driftGain));

    // Lateral velocity creates a visible drift angle while remaining stable on Old 3DS-class CPU.
    const float lateralTarget = steerDirection * t_.speedMps * driftGain * 0.34f;
    lateralVelocity_ += (lateralTarget - lateralVelocity_) * clamp01(dt * lerp(7.0f, 2.5f, driftGain));

    t_.headingRad += yawRate_ * dt;
    const float fwdX = std::sin(t_.headingRad);
    const float fwdY = std::cos(t_.headingRad);
    const float rightX = std::cos(t_.headingRad);
    const float rightY = -std::sin(t_.headingRad);

    t_.posX += (fwdX * t_.speedMps + rightX * lateralVelocity_) * dt;
    t_.posY += (fwdY * t_.speedMps + rightY * lateralVelocity_) * dt;

    t_.driftAngleDeg = rad2deg(std::atan2(lateralVelocity_, std::max(t_.speedMps, 0.25f)));
    lastSpeedMps_ = t_.speedMps;
}

void Vehicle::step(const InputState& in, float dt) {
    dt = clampf(dt, 0.0005f, 0.05f);
    updateTransmission(in, dt);
    updateForcedInduction(in, dt);
    updateEngine(in, dt);
    updateTiresAndGrip(in, dt);
    integrateChassis(in, dt);
}

} // namespace nr3ds
