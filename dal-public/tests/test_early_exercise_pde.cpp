#include <gtest/gtest.h>

#include <cmath>
#include <future>

#include <dal-public/src/earlyexercisepde.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>

using namespace Dal;

namespace {
    EarlyExercisePdeRequest_ Small() {
        EarlyExercisePdeRequest_ r;
        r.contract_ = {Date_(2026, 10, 18), Date_(2026, 10, 19), 210.0, OptionType_("PUT"), EarlyExerciseStyle_("AMERICAN"), {}};
        r.market_ = {100.0, 0.3, {{r.contract_.maturityDate_, 0.1, 0.04}}};
        r.settings_.gridPoints_ = 5;
        r.settings_.upper_ = 400.0;
        r.settings_.ordinarySteps_ = 1;
        r.settings_.rannacherIntervals_ = 0;
        return r;
    }
} // namespace

TEST(PublicEarlyExercisePdeTest, TestSmallLcpAgainstIndependentActiveSetEnumeration) {
    const auto result = EvaluateEarlyExercisePde(Small());
    ASSERT_TRUE(result.IsSuccess());
    double a[3][3] = {}, b[3] = {110.0, 10.0, 0.0};
    const double g[3] = {110.0, 10.0, 0.0}, dt = 1.0 / 365.0;
    for (int i = 0; i < 3; ++i) {
        const double node = i + 1, diffusion = 0.045 * node * node, drift = 0.03 * node;
        a[i][i] = 1.0 + dt * (2.0 * diffusion + 0.1);
        if (i > 0)
            a[i][i - 1] = -dt * (diffusion - drift);
        else
            b[i] += dt * (diffusion - drift) * 210.0;
        if (i < 2)
            a[i][i + 1] = -dt * (diffusion + drift);
    }
    int admissible = 0;
    for (int mask = 0; mask < 8; ++mask) {
        double system[3][4] = {};
        for (int i = 0; i < 3; ++i) {
            const bool active = (mask & (1 << i)) != 0;
            for (int j = 0; j < 3; ++j)
                system[i][j] = active ? double(i == j) : a[i][j];
            system[i][3] = active ? g[i] : b[i];
        }
        for (int i = 0; i < 3; ++i)
            for (int row = i + 1; row < 3; ++row) {
                const double factor = system[row][i] / system[i][i];
                for (int j = i; j <= 3; ++j)
                    system[row][j] -= factor * system[i][j];
            }
        double u[3] = {};
        for (int i = 2; i >= 0; --i) {
            u[i] = system[i][3];
            for (int j = i + 1; j < 3; ++j)
                u[i] -= system[i][j] * u[j];
            u[i] /= system[i][i];
        }
        bool valid = true;
        for (int i = 0; i < 3; ++i) {
            double w = -b[i];
            for (int j = 0; j < 3; ++j)
                w += a[i][j] * u[j];
            valid = valid && u[i] >= g[i] - 1e-10 && w >= -1e-10 && std::abs(std::min(u[i] - g[i], w)) <= 1e-10;
        }
        if (!valid)
            continue;
        ++admissible;
        for (int i = 0; i < 3; ++i) {
            ASSERT_NEAR(result.Values()[i + 1], u[i], 3e-8);
            double w = -b[i];
            for (int j = 0; j < 3; ++j)
                w += a[i][j] * result.Values()[j + 1];
            ASSERT_LE(std::abs(std::min(result.Values()[i + 1] - g[i], w / a[i][i])), result.Execution().epsilon_);
        }
    }
    ASSERT_EQ(admissible, 1);
    ASSERT_EQ(result.ExerciseLevels().size(), 2);
    ASSERT_GT(result.ExerciseLevels().front().regionCount_, 0);
    ASSERT_EQ(result.ExerciseLevels().back().regionCount_, 0);
    const auto& region = result.ExerciseRegions()[result.ExerciseLevels().front().regionOffset_];
    ASSERT_EQ(region.firstNode_, 0);
    ASSERT_EQ(region.lastNode_, 1);
    ASSERT_TRUE(region.leftClipped_);
    ASSERT_FALSE(region.rightClipped_);
    ASSERT_DOUBLE_EQ(result.Grid()[region.lastNode_], 100.0);
    ASSERT_DOUBLE_EQ(result.Grid()[region.lastNode_ + 1], 200.0);
}

TEST(PublicEarlyExercisePdeTest, TestCallerTapeAndConcurrentRequestsRemainIndependent) {
    using namespace AAD;
    Clear(*Tape());
    auto mode = SetNumResultsForAAD(true, 2);
    RecordingScope_ scope;
    Number_ x;
    scope.RegisterInput(x, 2.0);
    scope.StartRecording();
    Number_ square = x * x;
    scope.FinishRecording();
    scope.ClearAdjoints();
    NativeOperations_::SetSeed(square, 3.0, 1);
    const auto nodes = Tape()->nodes_.OccupiedSlots();
    const auto request = Small();
    const auto first = EvaluateEarlyExercisePde(request);
    auto future = std::async(std::launch::async, [request] { return EvaluateEarlyExercisePde(request); });
    const auto copy = future.get();
    ASSERT_DOUBLE_EQ(first.Price(), copy.Price());
    ASSERT_EQ(Tape()->nodes_.OccupiedSlots(), nodes);
    ASSERT_EQ(Tape()->numAdj_, 2);
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(square, 1), 3.0);
    scope.Reverse();
    ASSERT_DOUBLE_EQ(NativeOperations_::ReadAdjoint(x, 1), 12.0);
    scope.Close();
}
