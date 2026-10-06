//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/platform/platform.hpp>

#include "../math/aad/models/flat_ivs.hpp"
#include "dupireriskinputs.hpp"

using Dal::CalibrateDupireWithRisk;
using Dal::DupireRiskInputs_;
using Dal::Matrix_;
using Dal::Vector_;
using Dal::Test::SmallRiskInputs;

namespace {
    class CallbackIVS_ final : public Dal::AAD::IVS_ {
        std::function<double(double, double)> volatility_;

    public:
        explicit CallbackIVS_(std::function<double(double, double)> volatility) : IVS_(100.0, 0.05, 0.02), volatility_(std::move(volatility)) {}
        [[nodiscard]] double ImpliedVol(double strike, double maturity) const override { return volatility_(strike, maturity); }
    };

    double LegacyQuoteObjective(const Dal::AAD::IVS_& ivs, const DupireRiskInputs_& inputs, const Matrix_<>& seeds) {
        Dal::AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column)
                quotes.Bump(row, column, inputs.quoteSpreads_(row, column));
        const auto numeric =
            Dal::AAD::DupireCalib(ivs, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
        double objective = 0.0;
        for (int row = 0; row < seeds.Rows(); ++row)
            for (int column = 0; column < seeds.Cols(); ++column)
                objective += seeds(row, column) * numeric.lVols_(row, column);
        return objective;
    }

    constexpr std::array<double, 3> QUOTE_STEPS{2e-4, 1e-4, 5e-5};

    std::array<double, 3>
    QuoteBucketDifferences(const Dal::AAD::IVS_& ivs, const DupireRiskInputs_& inputs, const Matrix_<>& seeds, int row, int column) {
        std::array<double, 3> differences;
        for (size_t step = 0; step < QUOTE_STEPS.size(); ++step) {
            auto plus = inputs;
            auto minus = inputs;
            plus.quoteSpreads_(row, column) += QUOTE_STEPS[step];
            minus.quoteSpreads_(row, column) -= QUOTE_STEPS[step];
            differences[step] = (LegacyQuoteObjective(ivs, plus, seeds) - LegacyQuoteObjective(ivs, minus, seeds)) / (2.0 * QUOTE_STEPS[step]);
        }
        return differences;
    }

    std::array<double, 3> QuoteDirectionDifferences(const Dal::AAD::IVS_& ivs, const DupireRiskInputs_& inputs, const Matrix_<>& seeds) {
        std::array<double, 3> differences;
        for (size_t step = 0; step < QUOTE_STEPS.size(); ++step) {
            auto plus = inputs;
            auto minus = inputs;
            for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
                for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                    const double delta = QUOTE_STEPS[step] * std::cos(0.7 * row + 0.4 * column);
                    plus.quoteSpreads_(row, column) += delta;
                    minus.quoteSpreads_(row, column) -= delta;
                }
            differences[step] = (LegacyQuoteObjective(ivs, plus, seeds) - LegacyQuoteObjective(ivs, minus, seeds)) / (2.0 * QUOTE_STEPS[step]);
        }
        return differences;
    }

    bool AdjacentQuoteStepsAgree(double adjoint, const std::array<double, 3>& differences, const std::string& coordinate) {
        bool previous = false;
        bool adjacent = false;
        for (size_t step = 0; step < differences.size(); ++step) {
            const double error = std::abs(differences[step] - adjoint);
            const double tolerance = 3e-4 + 1e-3 * std::abs(differences[step]);
            const bool pass = std::isfinite(differences[step]) && error <= tolerance;
            std::ostringstream row;
            row << std::setprecision(17) << "DupireOracle coordinate=" << coordinate << " step=" << QUOTE_STEPS[step] << " aad=" << adjoint
                << " fd=" << differences[step] << " error=" << error << " tolerance=" << tolerance << " pass=" << pass;
            std::cout << row.str() << '\n';
            adjacent = adjacent || (previous && pass);
            previous = pass;
        }
        return adjacent;
    }
} // namespace

TEST(DupireRiskTest, TestReplayWithNonalignedQuoteAxesKeepsScalarCalibration) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    auto inputs = SmallRiskInputs();
    inputs.quoteStrikes_ = {75.0, 105.0, 135.0};
    inputs.quoteMaturities_ = {0.4, 1.2};
    const auto snapshot = CalibrateDupireWithRisk(ivs, inputs);
    const Dal::DupireParameterAdjoints_ seeds{snapshot, Matrix_<>(9, 2, 0.5)};
    const auto result = Dal::PullbackDupireCalibration(snapshot, seeds);
    double direction = 0.0;
    for (int row = 0; row < result.TotalAdjoints().Rows(); ++row)
        for (int column = 0; column < result.TotalAdjoints().Cols(); ++column)
            direction += std::cos(0.7 * row + 0.4 * column) * result.TotalAdjoints()(row, column);
    ASSERT_TRUE(AdjacentQuoteStepsAgree(direction, QuoteDirectionDifferences(ivs, inputs, seeds.adjoints_), "nonaligned-direction"));
}

TEST(DupireRiskTest, TestNonalignedReplayKeepsIndividualNodeQuoteAdjoints) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    auto inputs = SmallRiskInputs();
    inputs.quoteStrikes_ = {75.0, 105.0, 135.0};
    inputs.quoteMaturities_ = {0.4, 1.2};
    const auto snapshot = CalibrateDupireWithRisk(ivs, inputs);
    const auto original = snapshot.Surface()->vols_;
    for (int node : {0, 4, 8})
        for (int maturity = 0; maturity < original.Cols(); ++maturity) {
            Dal::DupireParameterAdjoints_ seeds{snapshot, Matrix_<>(original.Rows(), original.Cols(), 0.0)};
            seeds.adjoints_(node, maturity) = -0.5;
            const auto result = Dal::PullbackDupireCalibration(snapshot, seeds);
            for (int row = 0; row < result.TotalAdjoints().Rows(); ++row)
                for (int column = 0; column < result.TotalAdjoints().Cols(); ++column)
                    ASSERT_TRUE(AdjacentQuoteStepsAgree(result.TotalAdjoints()(row, column),
                                                        QuoteBucketDifferences(ivs, inputs, seeds.adjoints_, row, column), "individual-node"));
        }
    ASSERT_TRUE(std::equal(original.begin(), original.end(), snapshot.Surface()->vols_.begin()));
}

TEST(DupireRiskTest, TestRejectsUnrepresentableGridSpacingBeforeSampling) {
    int samples = 0;
    const CallbackIVS_ ivs([&samples](double, double) {
        ++samples;
        return 0.2;
    });
    auto inputs = SmallRiskInputs();
    const double low = 1e20;
    const double high = std::nextafter(std::nextafter(low, INFINITY), INFINITY);
    inputs.inclusionSpots_ = {low, high};
    inputs.maxSpotSpacing_ = 1.0;
    try {
        static_cast<void>(CalibrateDupireWithRisk(ivs, inputs));
        FAIL() << "unrepresentable spacing accepted";
    } catch (const Dal::Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("coordinate precision"), std::string::npos);
    }
    ASSERT_EQ(samples, 0);
}

TEST(DupireRiskTest, TestFlatParallelQuotePullbackAddsBoundaryAliasSeeds) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = CalibrateDupireWithRisk(ivs, SmallRiskInputs());
    const auto& surface = *snapshot.Surface();
    Matrix_<> seeds(surface.vols_.Rows(), surface.vols_.Cols(), 0.0);
    seeds(0, 0) = 2.0;
    seeds(1, 0) = -0.5;
    seeds(seeds.Rows() - 1, 0) = -0.75;
    seeds(seeds.Rows() - 2, 0) = 1.0;
    seeds(seeds.Rows() / 2, 1) = 1.25;
    const auto risk = Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, seeds});
    ASSERT_EQ(risk.CalibrationAdjoints().Rows(), 3);
    ASSERT_EQ(risk.CalibrationAdjoints().Cols(), 2);
    const double parallel = std::accumulate(risk.CalibrationAdjoints().begin(), risk.CalibrationAdjoints().end(), 0.0);
    ASSERT_NEAR(parallel, 3.0, 3e-5 * 5.5);
    ASSERT_EQ(risk.Method(), "NativeAADCalibrationVJP");
    ASSERT_EQ(risk.Unit(), "decimal-vol");
    for (double direct : risk.DirectAdjoints())
        ASSERT_EQ(direct, 0.0);
    seeds.Fill(0.0);
    const auto zero = Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, seeds});
    for (double total : zero.TotalAdjoints())
        ASSERT_EQ(total, 0.0);
    ASSERT_EQ(std::accumulate(risk.CalibrationAdjoints().begin(), risk.CalibrationAdjoints().end(), 0.0), parallel);
}

TEST(DupireRiskTest, TestEveryQuoteBucketAndDirectionMatchIndependentCalibrationBumps) {
    const Dal::AAD::FlatIVS_ flat(100.0, 0.05, 0.02, 0.2);
    const Dal::AAD::MertonIVS_ merton(100.0, 0.2, 0.08, -0.1, 0.15);
    const std::array<const Dal::AAD::IVS_*, 2> bases{&flat, &merton};
    for (size_t base = 0; base < bases.size(); ++base) {
        auto inputs = SmallRiskInputs();
        inputs.quoteStrikes_ = {75.0, 105.0, 135.0};
        inputs.quoteMaturities_ = {0.4, 1.2};
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column)
                inputs.quoteSpreads_(row, column) = 0.001 + 0.00003 * row + 0.00004 * column;
        const auto snapshot = CalibrateDupireWithRisk(*bases[base], inputs);
        Matrix_<> seeds(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols());
        for (int row = 0; row < seeds.Rows(); ++row)
            for (int column = 0; column < seeds.Cols(); ++column)
                seeds(row, column) = 0.1 * std::sin(0.6 * row + 0.3 * column);
        const auto risk = Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, seeds});
        double directionalAdjoint = 0.0;
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                const auto differences = QuoteBucketDifferences(*bases[base], inputs, seeds, row, column);
                const std::string coordinate = std::to_string(base) + ":" + std::to_string(row) + ":" + std::to_string(column);
                ASSERT_TRUE(AdjacentQuoteStepsAgree(risk.TotalAdjoints()(row, column), differences, coordinate));
                directionalAdjoint += risk.TotalAdjoints()(row, column) * std::cos(0.7 * row + 0.4 * column);
            }
        const auto differences = QuoteDirectionDifferences(*bases[base], inputs, seeds);
        ASSERT_TRUE(AdjacentQuoteStepsAgree(directionalAdjoint, differences, std::to_string(base) + ":direction"));
    }
}

TEST(DupireRiskTest, TestDirectQuotesAddExactlyOnceWithSeparateAuditContributions) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const auto snapshot = CalibrateDupireWithRisk(ivs, SmallRiskInputs());
    const Dal::DupireParameterAdjoints_ seeds{snapshot, Matrix_<>(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), 0.25)};
    const auto calibration = Dal::PullbackDupireCalibration(snapshot, seeds);
    Matrix_<> direct(3, 2);
    for (int row = 0; row < direct.Rows(); ++row)
        for (int column = 0; column < direct.Cols(); ++column)
            direct(row, column) = 0.25 * row - 0.125 * column;
    const Dal::AAD::FlatIVS_ anotherBase(100.0, 0.05, 0.02, 0.25);
    const auto sameQuotes = CalibrateDupireWithRisk(anotherBase, SmallRiskInputs());
    ASSERT_FALSE(snapshot.Matches(sameQuotes));
    const auto combined = Dal::PullbackDupireCalibration(snapshot, seeds, Dal::DupireDirectQuoteAdjoints_{sameQuotes, direct});
    for (int row = 0; row < direct.Rows(); ++row)
        for (int column = 0; column < direct.Cols(); ++column) {
            ASSERT_EQ(combined.CalibrationAdjoints()(row, column), calibration.CalibrationAdjoints()(row, column));
            ASSERT_EQ(combined.DirectAdjoints()(row, column), direct(row, column));
            ASSERT_EQ(combined.TotalAdjoints()(row, column), calibration.CalibrationAdjoints()(row, column) + direct(row, column));
        }
}

TEST(DupireRiskTest, TestSnapshotSurvivesOriginalIVSMutationAndDestruction) {
    int calls = 0;
    double volatility = 0.2;
    Dal::String_ name = "original";
    auto inputs = SmallRiskInputs();
    const auto expected = inputs;
    auto ivs = std::make_unique<CallbackIVS_>([&](double, double) {
        ++calls;
        inputs.quoteSpreads_(0, 0) = -100.0;
        name = "changed";
        return volatility;
    });
    const auto snapshot = CalibrateDupireWithRisk(*ivs, inputs, name);
    ASSERT_GT(calls, 0);
    ASSERT_EQ(snapshot.Inputs().quoteSpreads_(0, 0), 0.0);
    ASSERT_EQ(snapshot.Surface()->Name(), "original");
    const Dal::DupireParameterAdjoints_ seeds{snapshot, Matrix_<>(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), 1.0)};
    const auto before = Dal::PullbackDupireCalibration(snapshot, seeds);
    const int sampled = calls;
    volatility = 0.7;
    ivs.reset();
    const auto after = Dal::PullbackDupireCalibration(snapshot, seeds);
    ASSERT_EQ(calls, sampled);
    ASSERT_TRUE(std::equal(before.TotalAdjoints().begin(), before.TotalAdjoints().end(), after.TotalAdjoints().begin()));
    const Dal::AAD::FlatIVS_ equivalent(100.0, 0.05, 0.02, 0.2);
    ASSERT_TRUE(snapshot.Matches(CalibrateDupireWithRisk(equivalent, expected, "different name")));
}

TEST(DupireRiskTest, TestMismatchedOrInvalidSeedsRejectAndValidRequestsRecover) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const auto inputs = SmallRiskInputs();
    const auto snapshot = CalibrateDupireWithRisk(ivs, inputs);
    const Dal::DupireParameterAdjoints_ valid{snapshot, Matrix_<>(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), -0.2)};
    const auto before = Dal::PullbackDupireCalibration(snapshot, valid);
    auto changed = inputs;
    changed.quoteStrikes_ = {75.0, 100.0, 120.0};
    const auto different = CalibrateDupireWithRisk(ivs, changed);
    ASSERT_FALSE(snapshot.Matches(different));
    ASSERT_THROW(Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{different, valid.adjoints_}), Dal::Exception_);
    ASSERT_THROW(Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, Matrix_<>(1, 1, 0.0)}), Dal::Exception_);
    auto bad = valid;
    bad.adjoints_(0, 0) = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(Dal::PullbackDupireCalibration(snapshot, bad), Dal::Exception_);
    ASSERT_THROW(Dal::PullbackDupireCalibration(snapshot, valid, Dal::DupireDirectQuoteAdjoints_{different, Matrix_<>(3, 2, 0.0)}), Dal::Exception_);
    const auto after = Dal::PullbackDupireCalibration(snapshot, valid);
    ASSERT_TRUE(std::equal(before.TotalAdjoints().begin(), before.TotalAdjoints().end(), after.TotalAdjoints().begin()));
    {
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Dal::AAD::Number_ output = input * input;
        recording.FinishRecording();
        ASSERT_THROW(Dal::PullbackDupireCalibration(snapshot, valid), Dal::Exception_);
        Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
        recording.Reverse();
        ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
        recording.Close();
    }
    ASSERT_TRUE(std::equal(before.TotalAdjoints().begin(), before.TotalAdjoints().end(),
                           Dal::PullbackDupireCalibration(snapshot, valid).TotalAdjoints().begin()));
}

TEST(DupireRiskTest, TestInvalidQuoteAndCalibrationDomainsNeverPublishSnapshots) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    auto inputs = SmallRiskInputs();
    inputs.quoteSpreads_ = Matrix_<>(2, 3, 0.0);
    ASSERT_THROW(CalibrateDupireWithRisk(ivs, inputs), Dal::Exception_);
    inputs = SmallRiskInputs();
    inputs.quoteSpreads_(0, 0) = std::numeric_limits<double>::infinity();
    ASSERT_THROW(CalibrateDupireWithRisk(ivs, inputs), Dal::Exception_);
    inputs = SmallRiskInputs();
    inputs.quoteSpreads_.Fill(-0.3);
    ASSERT_THROW(CalibrateDupireWithRisk(ivs, inputs), Dal::Exception_);
    inputs = SmallRiskInputs();
    inputs.quoteStrikes_ = {120.0, 100.0, 80.0};
    ASSERT_THROW(CalibrateDupireWithRisk(ivs, inputs), Dal::Exception_);
    const CallbackIVS_ calendar([](double, double maturity) { return 0.2 * std::pow(maturity, -0.75); });
    ASSERT_THROW(CalibrateDupireWithRisk(calendar, SmallRiskInputs()), Dal::Exception_);
    const CallbackIVS_ nonfinite([](double, double) { return std::numeric_limits<double>::quiet_NaN(); });
    ASSERT_THROW(CalibrateDupireWithRisk(nonfinite, SmallRiskInputs()), Dal::Exception_);
    ASSERT_NO_THROW(CalibrateDupireWithRisk(ivs, SmallRiskInputs()));
}

TEST(DupireRiskTest, TestNegativeDiscreteCurvatureIsDiagnosedWithoutClipping) {
    const Dal::AAD::FlatIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    auto inputs = SmallRiskInputs();
    inputs.quoteSpreads_(1, 0) = 0.01;
    inputs.quoteSpreads_(1, 1) = 0.01;
    try {
        static_cast<void>(CalibrateDupireWithRisk(ivs, inputs));
        FAIL() << "negative discrete curvature accepted";
    } catch (const Dal::Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("curvature"), std::string::npos);
    }
}
