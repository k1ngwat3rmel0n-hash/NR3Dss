#include "nr_race.hpp"

#include <algorithm>
#include <cmath>

namespace nr3ds {
namespace {
inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

inline float moveToward(float current, float target, float maxDelta) {
    if (current < target) return std::min(current + maxDelta, target);
    if (current > target) return std::max(current - maxDelta, target);
    return current;
}
}

void RaceSession::reset() {
    t_ = RaceTelemetry{};
    opponentTargetLaneX_ = 2.8f;
    laneStage_ = -1;
}

void RaceSession::update(float playerSpeedKph, float dt) {
    dt = clampf(dt, 0.0f, 0.05f);
    playerSpeedKph = std::max(0.0f, playerSpeedKph);

    if (t_.phase == RacePhase::Countdown) {
        t_.countdown -= dt;
        if (t_.countdown <= 0.0f) {
            t_.countdown = 0.0f;
            t_.phase = RacePhase::Racing;
        }
        t_.gapM = t_.opponentProgressM - t_.playerProgressM;
        return;
    }

    if (t_.phase == RacePhase::Finished) return;

    t_.elapsed += dt;
    t_.playerProgressM += (playerSpeedKph / 3.6f) * dt;

    // A deliberately lightweight rival model. It has a normal target pace, then
    // gets a modest catch-up/slow-down adjustment based on the current gap. This
    // mirrors the role of NIGHT-RUNNERS' race catch-up logic without requiring a
    // second full Vehicle simulation on Old 3DS.
    const float gapBefore = t_.opponentProgressM - t_.playerProgressM;
    const float courseT = clampf(t_.opponentProgressM / kCourseLengthM, 0.0f, 1.0f);
    float desiredKph = 150.0f + 10.0f * courseT - gapBefore * 0.13f;
    desiredKph = clampf(desiredKph, 138.0f, 178.0f);

    // Launch progressively rather than teleporting to cruise speed.
    const float accelKphPerSec = (t_.opponentSpeedKph < 95.0f) ? 22.0f : 12.0f;
    const float decelKphPerSec = 10.0f;
    if (t_.opponentSpeedKph < desiredKph) {
        t_.opponentSpeedKph = moveToward(t_.opponentSpeedKph, desiredKph, accelKphPerSec * dt);
    } else {
        t_.opponentSpeedKph = moveToward(t_.opponentSpeedKph, desiredKph, decelKphPerSec * dt);
    }
    t_.opponentProgressM += (t_.opponentSpeedKph / 3.6f) * dt;

    while (t_.checkpointIndex < int(kCheckpointCount) &&
           t_.playerProgressM >= kCheckpoints[std::size_t(t_.checkpointIndex)]) {
        ++t_.checkpointIndex;
    }

    // Rival lane plan changes at coarse course segments. The movement itself is
    // smoothed so it reads as an intentional lane change instead of a teleport.
    const int stage = std::min(4, int(t_.opponentProgressM / 175.0f));
    if (stage != laneStage_) {
        static constexpr float kLaneTargets[5] = {2.8f, 0.0f, -2.8f, 0.0f, 2.8f};
        opponentTargetLaneX_ = kLaneTargets[stage];
        laneStage_ = stage;
    }
    t_.opponentLaneX = moveToward(t_.opponentLaneX, opponentTargetLaneX_, 1.35f * dt);
    t_.gapM = t_.opponentProgressM - t_.playerProgressM;

    const bool playerFinished = t_.playerProgressM >= kCourseLengthM;
    const bool opponentFinished = t_.opponentProgressM >= kCourseLengthM;
    if (playerFinished || opponentFinished) {
        t_.phase = RacePhase::Finished;
        t_.playerWon = playerFinished && !opponentFinished;
        if (playerFinished && opponentFinished) {
            t_.playerWon = t_.playerProgressM >= t_.opponentProgressM;
        }
    }
}

void RaceSession::applyOpponentImpact(float speedRetention, float progressKickM) {
    const float keep = clampf(speedRetention, 0.0f, 1.0f);
    t_.opponentSpeedKph *= keep;
    t_.opponentProgressM = std::max(0.0f, t_.opponentProgressM + progressKickM);
    t_.gapM = t_.opponentProgressM - t_.playerProgressM;
}

float RaceSession::nextGateDistanceM() const {
    if (t_.checkpointIndex < int(kCheckpointCount)) {
        return kCheckpoints[std::size_t(t_.checkpointIndex)] - t_.playerProgressM;
    }
    return kCourseLengthM - t_.playerProgressM;
}

} // namespace nr3ds
