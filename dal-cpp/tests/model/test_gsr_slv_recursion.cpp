#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/gsrslv.hpp>

using Dal::Date_;
using Dal::Handle_;
using Dal::Matrix_;
using Dal::Vector_;

namespace {
    constexpr double STEP_TIME = 45.0 / Dal::DAYS_PER_YEAR;

    Handle_<Dal::GSRSLVModelData_> RecursionData(int factors, double gScale = 1.0, double hScale = 1.0, bool singular = false) {
        const Date_ today(2026, 10, 2);
        const Handle_<Dal::GSRCurveData_> curve(
            new Dal::GSRCurveData_("curve", today, "USD", {today, today.AddDays(3650)}, {0.0, -0.3}, {}, Matrix_<>(0, 0)));
        Dal::MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = vol.hKnotDates_ = {today};
        vol.gValues_ = Matrix_<>(factors, 1);
        vol.hValues_ = Matrix_<>(factors, 1);
        vol.correlations_ = Matrix_<>(factors, factors, singular ? 1.0 : 0.0);
        for (int i = 0; i < factors; ++i) {
            vol.factorNames_.push_back("factor" + Dal::String::FromInt(i));
            vol.correlations_(i, i) = 1.0;
            vol.gValues_(i, 0) = gScale * 0.02 / (i + 1);
            vol.hValues_(i, 0) = hScale * (1.0 - 0.6 * i);
        }
        const Handle_<Dal::MultiFactorGSRModelData_> gaussian(
            new Dal::MultiFactorGSRModelData_("gaussian", curve, Handle_<Dal::MultiFactorGSRVolData_>(new Dal::MultiFactorGSRVolData_("vol", vol))));
        Dal::GSRSLVSettings_ settings;
        settings.maxStep_ = 1.0;
        return Handle_<Dal::GSRSLVModelData_>(new Dal::GSRSLVModelData_(
            "smile", gaussian, Handle_<Dal::GSRLeverageData_>(new Dal::GSRLeverageData_("leverage", {0.0}, {0.0}, Matrix_<>(1, 1, 1.1))), settings));
    }

    template <class T_> void PrepareContinuation(int factors, Dal::AAD::GSRSLV_<T_>* model) {
        Matrix_<> correlations(factors, factors, 0.0);
        for (int i = 0; i < factors; ++i)
            correlations(i, i) = 1.0;
        model->PrepareHybrid({0.0, STEP_TIME}, Vector_<Dal::AAD::SampleDef_>(2), correlations);
    }

    Vector_<size_t> ContinuationSlots(size_t factors) {
        Vector_<size_t> slots(factors + 2);
        for (size_t i = 0; i < slots.size(); ++i)
            slots[i] = factors + 3 - i;
        return slots;
    }

    Vector_<> ContinuationDrivers(size_t factors, double normal) {
        Vector_<> drivers(factors + 4);
        for (size_t i = 0; i < drivers.size(); ++i)
            drivers[i] = normal * (1.0 + 0.2 * i);
        return drivers;
    }

    Vector_<> ContinuationInputs(size_t factors) {
        Vector_<> state(factors + factors * factors + 5, 0.0);
        state[0] = 0.123;
        state[1] = -0.456;
        for (size_t i = 0; i < factors; ++i) {
            state[2 + i] = 0.01 * (i + 1);
            for (size_t j = 0; j < factors; ++j)
                state[2 + factors + i * factors + j] = 0.002 * (i + 1) - 0.001 * j;
        }
        state[state.size() - 3] = state[state.size() - 2] = 0.8;
        state.back() = 0.02;
        return state;
    }

    Vector_<> ContinuationPath(int factors, double normal, const Vector_<>& initial, size_t parameter = 0, double bump = 0.0) {
        Dal::AAD::GSRSLV_<> model(*RecursionData(factors));
        *model.Parameters()[parameter] += bump;
        PrepareContinuation(factors, &model);
        auto state = initial;
        model.EvolveHybrid(0, ContinuationDrivers(factors, normal), ContinuationSlots(factors), &state, 2);
        return state;
    }

    template <class T_> T_ ContinuationPayoff(const Vector_<T_>& state) {
        T_ total(0.0);
        for (size_t i = 2; i < state.size(); ++i)
            total += (i - 1) * state[i];
        return total;
    }

    Vector_<> ReferenceContinuation(int factors, double normal, const Vector_<>& initial) {
        const double dt = STEP_TIME, variance = initial[initial.size() - 3];
        const double scale = 1.1 * 1.1 * variance, diffusion = 1.1 * std::sqrt(variance);
        double shift = 0.0, yHH = 0.0, noiseH = 0.0, bridgeVariance = 0.0;
        const auto drivers = ContinuationDrivers(factors, normal);
        const auto slots = ContinuationSlots(factors);
        auto state = initial;
        for (int i = 0; i < factors; ++i) {
            const double g = 0.02 / (i + 1), h = 1.0 - 0.6 * i;
            shift += h * initial[2 + i];
            noiseH += h * g * drivers[slots[i]];
            bridgeVariance += h * h * g * g;
            double drift = 0.0;
            for (int j = 0; j < factors; ++j) {
                drift += initial[2 + factors + i * factors + j] * (1.0 - 0.6 * j);
                yHH += h * initial[2 + factors + i * factors + j] * (1.0 - 0.6 * j);
            }
            state[2 + i] += dt * drift + 0.5 * dt * dt * scale * g * g * h + diffusion * std::sqrt(dt) * g * drivers[slots[i]];
            for (int j = 0; j <= i; ++j) {
                state[2 + factors + i * factors + j] += i == j ? dt * scale * g * g : 0.0;
                state[2 + factors + j * factors + i] = state[2 + factors + i * factors + j];
            }
        }
        state.back() +=
            0.03 * dt + dt * shift + 0.5 * dt * dt * yHH + scale * bridgeVariance * dt * dt * dt / 6.0 +
            diffusion * (0.5 * dt * std::sqrt(dt) * noiseH + std::sqrt(bridgeVariance * dt * dt * dt / 12.0) * drivers[slots[factors + 1]]);
        state[state.size() - 2] += dt * (1.0 - variance) + 0.5 * std::sqrt(dt * variance) * drivers[slots[factors]];
        state[state.size() - 3] = std::max(0.0, state[state.size() - 2]);
        return state;
    }

    void CheckContinuationModelRisks(int factors, double normal, const Vector_<>& initial, const Dal::AAD::GSRSLV_<Dal::AAD::Number_>& model) {
        for (size_t parameter = 0; parameter < model.Parameters().size(); ++parameter) {
            const double expected = (ContinuationPayoff(ContinuationPath(factors, normal, initial, parameter, 1e-6)) -
                                     ContinuationPayoff(ContinuationPath(factors, normal, initial, parameter, -1e-6))) /
                                    2e-6;
            ASSERT_NEAR(Dal::AAD::Adjoint(*model.Parameters()[parameter]), expected, 1e-8 * std::max(1.0, std::abs(expected)))
                << model.ParameterLabels()[parameter];
        }
    }

    void CheckContinuationStateRisks(int factors, double normal, const Vector_<>& initial, const Vector_<Dal::AAD::Number_>& seeds) {
        for (size_t input = 0; input < seeds.size(); ++input) {
            auto upper = initial, lower = initial;
            upper[input] += 1e-6;
            lower[input] -= 1e-6;
            const double expected =
                (ContinuationPayoff(ContinuationPath(factors, normal, upper)) - ContinuationPayoff(ContinuationPath(factors, normal, lower))) / 2e-6;
            ASSERT_NEAR(Dal::AAD::Adjoint(seeds[input]), expected, 1e-8 * std::max(1.0, std::abs(expected))) << input;
        }
    }

    template <class T_> T_ TerminalStatePayoff(const typename Dal::AAD::GSRSLV_<T_>::State_& state) {
        T_ total = state.logNumeraire_ + 0.3 * state.variance_ + 0.7 * state.latentVariance_;
        for (size_t i = 0; i < state.x_.size(); ++i) {
            total += (i + 1) * state.x_[i];
            for (size_t j = 0; j < state.x_.size(); ++j)
                total += (i + j + 1) * state.y_(i, j);
        }
        return total;
    }

    Vector_<> TruncatedDrivers(int factors) {
        Vector_<> gaussian(2 * (factors + 2), 0.2);
        for (int step = 0; step < 2; ++step) {
            gaussian[step * (factors + 2) + factors] = -10.0;
            gaussian[step * (factors + 2) + factors + 1] = 0.0;
        }
        return gaussian;
    }

    double TruncatedPrice(const Dal::GSRSLVModelData_& data, const Vector_<>& gaussian, size_t parameter, double bump) {
        Dal::AAD::GSRSLV_<> model(data);
        const Vector_<> timeline{1.0, 2.0};
        const Vector_<Dal::AAD::SampleDef_> definitions(2);
        model.Allocate(timeline, definitions);
        *model.Parameters()[parameter] += bump;
        model.Init(timeline, definitions);
        return TerminalStatePayoff<double>(model.StateAfter(gaussian));
    }

    void CheckTruncatedRisks(const Dal::GSRSLVModelData_& data,
                             const Vector_<>& gaussian,
                             const Dal::AAD::GSRSLV_<Dal::AAD::Number_>& model,
                             double price) {
        for (size_t parameter = 0; parameter < model.Parameters().size(); ++parameter) {
            const auto bumped = [&](double bump) { return TruncatedPrice(data, gaussian, parameter, bump); };
            const double expected = Dal::AAD::Value(*model.Parameters()[parameter]) == 0.0 ? (-3.0 * price + 4.0 * bumped(1e-6) - bumped(2e-6)) / 2e-6
                                                                                           : (bumped(1e-6) - bumped(-1e-6)) / 2e-6;
            ASSERT_NEAR(Dal::AAD::Adjoint(*model.Parameters()[parameter]), expected, 1e-8 * std::max(1.0, std::abs(expected)))
                << model.ParameterLabels()[parameter];
        }
    }
} // namespace

TEST(GSRSLVTest, TestRecursionFitsNodeAndEdgeBudgets) {
    for (const int factors : {1, 3}) {
        const Dal::TapeGuard_ guard(Dal::AAD::Tape());
        Dal::AAD::GSRSLV_<Dal::AAD::Number_> model(*RecursionData(factors));
        const Vector_<> timeline{1.0};
        const Vector_<Dal::AAD::SampleDef_> definitions(1);
        model.Allocate(timeline, definitions);
        Dal::AAD::Rewind(*Dal::AAD::Tape());
        for (auto* parameter : model.Parameters())
            Dal::AAD::PutOnTape(*parameter);
        Dal::AAD::NewRecording(*Dal::AAD::Tape());
        model.Init(timeline, definitions);
        const size_t nodes = Dal::AAD::Tape()->nodes_.OccupiedSlots();
        const size_t edges = Dal::AAD::Tape()->ders_.OccupiedSlots();
        const auto state = model.StateAfter(Vector_<>(model.SimDim(), 0.2));
        ASSERT_GT(Dal::AAD::Value(state.variance_), 0.0);
        ASSERT_LE(Dal::AAD::Tape()->nodes_.OccupiedSlots() - nodes, factors == 1 ? 17U : 28U);
        ASSERT_LE(Dal::AAD::Tape()->ders_.OccupiedSlots() - edges, factors == 1 ? 37U : 92U);
    }
}

TEST(GSRSLVTest, TestRecursionPreservesActiveContinuationRisks) {
    for (const int factors : {1, 2, 3, 4}) {
        const Dal::TapeGuard_ guard(Dal::AAD::Tape());
        Dal::AAD::GSRSLV_<Dal::AAD::Number_> model(*RecursionData(factors));
        const auto initial = ContinuationInputs(factors);
        for (int recording = 0; recording < 2; ++recording) {
            Dal::AAD::Rewind(*Dal::AAD::Tape());
            for (auto* parameter : model.Parameters())
                Dal::AAD::PutOnTape(*parameter);
            Vector_<Dal::AAD::Number_> seeds(initial.begin(), initial.end());
            Dal::AAD::NewRecording(*Dal::AAD::Tape());
            PrepareContinuation(factors, &model);
            Dal::AAD::Mark(*Dal::AAD::Tape());
            for (const double normal : {0.2, -0.4}) {
                Dal::AAD::RewindToMark(*Dal::AAD::Tape());
                Dal::AAD::ZeroAdjoints(*Dal::AAD::Tape());
                auto state = seeds;
                model.EvolveHybrid(0, ContinuationDrivers(factors, normal), ContinuationSlots(factors), &state, 2);
                const auto expected = ReferenceContinuation(factors, normal, initial);
                for (size_t i = 0; i < state.size(); ++i)
                    ASSERT_NEAR(Dal::AAD::Value(state[i]), expected[i], 1e-12) << factors << ":" << i;
                const Dal::AAD::Number_ payoff = ContinuationPayoff(state);
                Dal::AAD::Adjoint(payoff) = 1.0;
                Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
                ASSERT_NO_FATAL_FAILURE(CheckContinuationModelRisks(factors, normal, initial, model));
                ASSERT_NO_FATAL_FAILURE(CheckContinuationStateRisks(factors, normal, initial, seeds));
            }
        }
    }
}

TEST(GSRSLVTest, TestRecursionPreservesZeroVarianceAndSingularRisks) {
    for (const int factors : {1, 3}) {
        for (const auto& scales : {std::pair{1.0, 1.0}, std::pair{0.0, 1.0}, std::pair{1.0, 0.0}}) {
            const Dal::TapeGuard_ guard(Dal::AAD::Tape());
            const auto data = RecursionData(factors, scales.first, scales.second, true);
            Dal::AAD::GSRSLV_<Dal::AAD::Number_> model(*data);
            const Vector_<> timeline{1.0, 2.0};
            const Vector_<Dal::AAD::SampleDef_> definitions(2);
            model.Allocate(timeline, definitions);
            Dal::AAD::Rewind(*Dal::AAD::Tape());
            for (auto* parameter : model.Parameters())
                Dal::AAD::PutOnTape(*parameter);
            Dal::AAD::NewRecording(*Dal::AAD::Tape());
            model.Init(timeline, definitions);
            const auto gaussian = TruncatedDrivers(factors);
            const auto state = model.StateAfter(gaussian);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.variance_), 0.0);
            ASSERT_DOUBLE_EQ(Dal::AAD::Value(state.latentVariance_), -3.0);
            const Dal::AAD::Number_ payoff = TerminalStatePayoff<Dal::AAD::Number_>(state);
            const double price = TruncatedPrice(*data, gaussian, 0, 0.0);
            ASSERT_NEAR(Dal::AAD::Value(payoff), price, 1e-12);
            Dal::AAD::Adjoint(payoff) = 1.0;
            Dal::AAD::PropagateToStart(*Dal::AAD::Tape());
            ASSERT_NO_FATAL_FAILURE(CheckTruncatedRisks(*data, gaussian, model, price));
        }
    }
}
