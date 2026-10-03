#pragma once

#include <array>
#include <cstddef>

namespace nr3ds {

enum class RacePhase {
    Countdown,
    Racing,
    Finished,
};

struct RaceTelemetry {
    RacePhase phase = RacePhase::Countdown;
    float countdown = 3.0f;
    float elapsed = 0.0f;
    float playerProgressM = 0.0f;
    float opponentProgressM = 6.0f;
    float opponentSpeedKph = 0.0f;
    float opponentLaneX = 2.8f;
    float gapM = 6.0f; // positive = rival ahead
    int checkpointIndex = 0;
    bool playerWon = false;
};

class RaceSession {
public:
    static constexpr float kCourseLengthM = 900.0f;
    static constexpr std::size_t kCheckpointCount = 3;
    static constexpr std::array<float, kCheckpointCount> kCheckpoints{
        225.0f, 450.0f, 675.0f
    };

    void reset();
    void update(float playerSpeedKph, float dt);
    void applyOpponentImpact(float speedRetention, float progressKickM);

    const RaceTelemetry& telemetry() const { return t_; }
    float nextGateDistanceM() const;

private:
    RaceTelemetry t_{};
    float opponentTargetLaneX_ = 2.8f;
    int laneStage_ = -1;
};

} // namespace nr3ds
