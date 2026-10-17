//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/platform.hpp>

#include <dal-cpp/benchmarks/rate_risk_perf/quoteriskbenchfixtures.hpp>
#include <dal-public/src/calibrationrisk.hpp>

#include "jointxccyquoteriskfixtures.hpp"

namespace {
    class FlatIVS_ final : public Dal::AAD::IVS_ {
        double vol_;

    public:
        explicit FlatIVS_(double vol = 0.2) : IVS_(100.0, 0.05, 0.02), vol_(vol) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };

    Dal::DupireRiskInputs_ Inputs() {
        return {{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
    }

    void AssertSameMatrix(const Dal::Matrix_<>& actual, const Dal::Matrix_<>& expected) {
        ASSERT_EQ(actual.Rows(), expected.Rows());
        ASSERT_EQ(actual.Cols(), expected.Cols());
        for (int row = 0; row < actual.Rows(); ++row)
            for (int column = 0; column < actual.Cols(); ++column)
                ASSERT_EQ(actual(row, column), expected(row, column));
    }

    template <class C_> void AssertFailure(C_ operation, const std::string& reason) {
        try {
            operation();
            FAIL() << "Expected " << reason;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(reason), std::string::npos) << error.what();
        }
    }

    Dal::RateQuoteRiskProvenance_ Captured(const Dal::JointMultiCurveCalibrationSpec_& spec,
                                           const Dal::JointMultiCurveCalibrationResult_& calibrated,
                                           const Dal::JointMultiCurveCalibrationOptions_& options,
                                           const Dal::RatePricingMarket_& market,
                                           Dal::RateQuoteRiskProvenanceConfig_ config) {
        config.retainCalibrationRecord_ = true;
        return Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, config);
    }

    Dal::Matrix_<> PricedGradient(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                                  const Dal::RatePricingMarket_& market,
                                  const Dal::RateQuoteRiskProvenance_& provenance) {
        Dal::Vector_<Dal::String_> keys;
        const auto& ranges = provenance.Axis().parameterRanges_;
        for (const auto& range : ranges)
            keys.push_back(provenance.ComponentKeyByParameterBlock().at(range.blockKey_));
        const auto cells = provenance.Kind() == "JOINT_MULTI_CURVE"
                               ? Dal::RateCashflowPricingInternal::JointNodeSensitivitiesBatch(trades, market, keys)
                               : Dal::RateTradeNodeSensitivitiesBatch(trades, market, keys);
        REQUIRE(cells.size() == trades.size() * ranges.size(), "Incomplete independent joint gradients");
        Dal::Matrix_<> gradient(static_cast<int>(provenance.Axis().parameters_.size()), 1, 0.0);
        for (size_t index = 0; index < cells.size(); ++index) {
            const auto block = index % ranges.size();
            const auto& range = ranges[block];
            const auto& cell = cells[index];
            REQUIRE(cell.componentKey_ == keys[block], "Independent gradient component order differs");
            if (!cell.result_.eligible_) {
                REQUIRE(cell.result_.reason_ == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT", "Independent gradient failed: " + cell.result_.reason_);
                continue;
            }
            REQUIRE(cell.result_.gradient_.size() == static_cast<size_t>(range.size_), "Independent joint gradient width differs");
            for (int ordinal = 0; ordinal < range.size_; ++ordinal)
                gradient(range.offset_ + ordinal, 0) += cell.result_.gradient_[ordinal];
        }
        return gradient;
    }

    void AssertLegacyMapping(const Dal::Vector_<Dal::RateTradeDefinition_>& trades,
                             const Dal::RatePricingMarket_& market,
                             const Dal::RateQuoteRiskProvenance_& provenance) {
        const auto boundary = Dal::NewCalibrationPullback(provenance);
        const auto parameters = Dal::NewCalibrationParameterAdjoints(boundary, PricedGradient(trades, market, provenance));
        const auto result = Dal::PullbackCalibration(boundary, parameters);
        const auto legacy = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
        ASSERT_TRUE(legacy.provenanceFailures_.empty());
        ASSERT_EQ(legacy.buckets_.size(), provenance.Axis().quotes_.size());
        ASSERT_EQ(boundary.Domain(), "RATE_CURVE");
        ASSERT_EQ(result.Method(), "RetainedCurveEffectiveInverse");
        ASSERT_EQ(result.Unit(), "DECIMAL_QUOTE");
        ASSERT_EQ(result.Boundary(), "FrozenCalibrationEffectiveInverse");
        ASSERT_EQ(result.TotalAdjoints().Rows(), static_cast<int>(legacy.buckets_.size()));
        ASSERT_EQ(result.TotalAdjoints().Cols(), 1);
        for (int row = 0; row < result.TotalAdjoints().Rows(); ++row) {
            ASSERT_EQ(result.TotalAdjoints()(row, 0), legacy.buckets_[row].dPvDDecimalQuote_);
            ASSERT_EQ(result.TotalAdjoints()(row, 0) * 1e-4, legacy.buckets_[row].dv01_);
            ASSERT_EQ(result.CalibrationAdjoints()(row, 0), result.TotalAdjoints()(row, 0));
            ASSERT_EQ(result.DirectAdjoints()(row, 0), 0.0);
        }
    }

    auto SmallCurve() {
        const auto spec = JointQuoteRiskFixtures::Spec();
        const auto options = JointXccyQuoteRiskFixtures::Options();
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        return Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2)));
    }

    template <class M_, class B_>
    void AssertNativeProvider(const M_& materials, const Dal::RateRiskPerf::QuoteRiskBenchmarkCase_& example, B_ build) {
        auto config = materials.config_;
        config.retainCalibrationRecord_ = true;
        const auto provenance = build(materials.spec_, *materials.calibration_, materials.options_, materials.market_, config);
        ASSERT_TRUE(provenance.Available()) << provenance.Reason();
        ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping(example.trades_, materials.market_, provenance));
        ASSERT_EQ(Dal::NewCalibrationPullback(provenance).ParameterCols(), 1);
    }

    void AssertCurveMismatch(const Dal::CalibrationPullback_& target, const Dal::CalibrationPullback_& changed) {
        ASSERT_FALSE(target.Matches(changed));
        const auto seeds = Dal::NewCalibrationParameterAdjoints(changed, Dal::Matrix_<>(changed.ParameterRows(), changed.ParameterCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(target, seeds), Dal::Exception_);
        const auto good = Dal::NewCalibrationParameterAdjoints(target, Dal::Matrix_<>(target.ParameterRows(), target.ParameterCols(), 0.0));
        const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(changed, Dal::Matrix_<>(changed.QuoteRows(), changed.QuoteCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(target, good, direct), Dal::Exception_);
    }

    double CurveObjective(const Dal::JointMultiCurveCalibrationSpec_& spec,
                          const Dal::JointMultiCurveCalibrationOptions_& options,
                          const Dal::Matrix_<>& seeds,
                          const Dal::Vector_<>& direction,
                          double step) {
        auto shifted = spec;
        int quote = 0;
        for (int block = 0; block < static_cast<int>(shifted.curves_.size()); ++block)
            for (auto& instrument : shifted.curves_[block].instruments_) {
                instrument = JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, block,
                                                                instrument->MarketRate() + step * direction[quote++]);
            }
        const auto calibrated = Dal::CalibrateJointMultiCurve(shifted, options);
        const auto market = JointQuoteRiskFixtures::Market(shifted, calibrated);
        double value = 0.0;
        int parameter = 0;
        for (int block = 0; block < static_cast<int>(shifted.curves_.size()); ++block) {
            const auto inspected = Dal::InspectCurveParameters(*market.curveComponents_.at(JointQuoteRiskFixtures::BlockKey(block)), spec.today_);
            for (double coordinate : inspected.passiveParameters_)
                value += coordinate * seeds(parameter++, 0);
        }
        REQUIRE(parameter == seeds.Rows(), "Independent calibration coordinate count differs");
        return value;
    }

    void AssertDirectionalCalibration(const Dal::JointMultiCurveCalibrationSpec_& spec, const Dal::JointMultiCurveCalibrationOptions_& options) {
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        const auto boundary = Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2)));
        ASSERT_EQ(boundary.ParameterRows(), boundary.QuoteRows());
        Dal::Matrix_<> seeds(boundary.ParameterRows(), 1);
        Dal::Vector_<> direction(boundary.QuoteRows());
        for (int row = 0; row < seeds.Rows(); ++row) {
            seeds(row, 0) = std::sin(0.3 + 0.7 * row);
            direction[row] = std::cos(0.4 + 0.6 * row);
        }
        const auto result = Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(boundary, seeds));
        double adjoint = 0.0;
        for (int row = 0; row < result.TotalAdjoints().Rows(); ++row)
            adjoint += result.TotalAdjoints()(row, 0) * direction[row];
        bool previousPass = false;
        bool adjacentPass = false;
        for (double step : {2e-6, 1e-6, 5e-7}) {
            const double oracle =
                (CurveObjective(spec, options, seeds, direction, step) - CurveObjective(spec, options, seeds, direction, -step)) / (2.0 * step);
            const double error = std::abs(adjoint - oracle);
            const bool pass = error <= 1e-6 || error <= 1e-6 * std::max(std::abs(adjoint), std::abs(oracle));
            adjacentPass = adjacentPass || (previousPass && pass);
            previousPass = pass;
            std::cout << "CommonCurveCalibrationOracle," << options.jacobianMode_.String() << ',' << spec.curves_[1].baseLayeredOverDiscount_ << ','
                      << std::setprecision(17) << step << ',' << adjoint << ',' << oracle << ',' << error << ',' << pass << '\n';
        }
        ASSERT_TRUE(adjacentPass);
    }
} // namespace

TEST(CalibrationRiskTest, TestCommonDupireResultMatchesTypedPullbackAndOwnsSeeds) {
    const FlatIVS_ ivs;
    const auto snapshot = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto boundary = Dal::NewCalibrationPullback(snapshot);
    Dal::Matrix_<> parameterMatrix(snapshot.Surface()->vols_.Rows(), snapshot.Surface()->vols_.Cols(), -0.25);
    Dal::Matrix_<> directMatrix(3, 2, 0.125);
    const auto reference = Dal::PullbackDupireCalibration(snapshot, Dal::DupireParameterAdjoints_{snapshot, parameterMatrix},
                                                          Dal::DupireDirectQuoteAdjoints_{snapshot, directMatrix});
    const auto parameters = Dal::NewCalibrationParameterAdjoints(boundary, parameterMatrix);
    const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(boundary, directMatrix);
    parameterMatrix.Fill(100.0);
    directMatrix.Fill(200.0);
    const auto result = Dal::PullbackCalibration(boundary, parameters, direct);
    ASSERT_EQ(boundary.Domain(), "DUPIRE");
    ASSERT_EQ(boundary.ParameterRows(), snapshot.Surface()->vols_.Rows());
    ASSERT_EQ(boundary.ParameterCols(), snapshot.Surface()->vols_.Cols());
    ASSERT_EQ(boundary.QuoteRows(), 3);
    ASSERT_EQ(boundary.QuoteCols(), 2);
    ASSERT_TRUE(std::get<Dal::DupireCalibrationSnapshot_>(boundary.Source()).Matches(snapshot));
    ASSERT_TRUE(result.Calibration().Matches(boundary));
    ASSERT_EQ(result.Method(), reference.Method());
    ASSERT_EQ(result.Unit(), reference.Unit());
    ASSERT_EQ(result.Boundary(), reference.Boundary());
    AssertSameMatrix(result.CalibrationAdjoints(), reference.CalibrationAdjoints());
    AssertSameMatrix(result.DirectAdjoints(), reference.DirectAdjoints());
    AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints());
    static_assert(std::is_const_v<std::remove_reference_t<decltype(result.TotalAdjoints())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(parameters.Adjoints())>>);
}

TEST(CalibrationRiskTest, TestDupireDirectQuotesPermitEqualQuotesUnderDifferentBases) {
    const FlatIVS_ original, changed(0.25);
    const auto snapshot = Dal::CalibrateDupireWithRisk(original, Inputs());
    const auto other = Dal::CalibrateDupireWithRisk(changed, Inputs());
    const auto boundary = Dal::NewCalibrationPullback(snapshot);
    const auto different = Dal::NewCalibrationPullback(other);
    ASSERT_FALSE(boundary.Matches(different));
    const Dal::Matrix_<> nodes(boundary.ParameterRows(), boundary.ParameterCols(), 0.25);
    const Dal::Matrix_<> quotes(3, 2, -0.125);
    const auto reference = Dal::PullbackDupireCalibration(snapshot, {snapshot, nodes}, Dal::DupireDirectQuoteAdjoints_{other, quotes});
    const auto result = Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(boundary, nodes),
                                                 Dal::NewCalibrationDirectQuoteAdjoints(different, quotes));
    AssertSameMatrix(result.CalibrationAdjoints(), reference.CalibrationAdjoints());
    AssertSameMatrix(result.DirectAdjoints(), reference.DirectAdjoints());
    AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints());
    ASSERT_THROW(Dal::PullbackCalibration(boundary, Dal::NewCalibrationParameterAdjoints(different, nodes)), Dal::Exception_);
}

TEST(CalibrationRiskTest, TestCurveCommonResultMatchesLegacyAcrossModesAndRepresentations) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (const auto parameterization :
             {Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD,
              Dal::CurveParameterization_::Value_::ZERO_RATE, Dal::CurveParameterization_::Value_::LOG_DISCOUNT})
            for (bool layered : {false, true}) {
                const auto spec = JointQuoteRiskFixtures::Spec(5, 2, parameterization, layered);
                const auto options = JointXccyQuoteRiskFixtures::Options(mode);
                const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
                const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
                const auto provenance = Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2));
                ASSERT_NO_FATAL_FAILURE(
                    AssertLegacyMapping({JointQuoteRiskFixtures::Irs(spec), JointQuoteRiskFixtures::Irs(spec, 1, -250000.0)}, market, provenance));
            }
}

TEST(CalibrationRiskTest, TestSingleJointXccyAndStagedSourcesMatchActualLegacyPortfolios) {
    using namespace Dal::RateRiskPerf;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (int quotes : {8, 16}) {
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeSingleCurveProvenanceMaterials(quotes, mode), MakeSingleCurveQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildSingleCurveQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeJointXccyProvenanceMaterials(quotes, mode), MakeJointXccyQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildJointXccyQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
            ASSERT_NO_FATAL_FAILURE(
                AssertNativeProvider(MakeStagedXccyProvenanceMaterials(quotes, mode), MakeStagedXccyBasisQuoteRiskCase(quotes, mode, 3),
                                     [](const auto& spec, const auto& result, const auto& options, const auto& market, const auto& config) {
                                         return Dal::BuildStagedXccyBasisQuoteRiskProvenance(spec, result, options, market, config);
                                     }));
        }
}

TEST(CalibrationRiskTest, TestCurveMixedRepresentationsAndActualPvCurrenciesRemainSeparate) {
    auto spec = JointQuoteRiskFixtures::Spec(8, 3);
    spec.curves_[1].parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    spec.curves_[2].parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto provenance = Captured(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(3));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointQuoteRiskFixtures::Irs(spec, 1), JointQuoteRiskFixtures::Irs(spec, 2)}, market, provenance));
    const auto currencySpec = JointQuoteRiskFixtures::Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
    const auto currencyCalibrated = Dal::CalibrateJointMultiCurve(currencySpec, options);
    const auto currencyMarket = JointXccyQuoteRiskFixtures::Market(currencySpec, currencyCalibrated, true);
    const auto currencyProvenance = Captured(currencySpec, currencyCalibrated, options, currencyMarket, JointQuoteRiskFixtures::Config(2));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointQuoteRiskFixtures::Irs(currencySpec, 1)}, currencyMarket, currencyProvenance));
    ASSERT_NO_FATAL_FAILURE(AssertLegacyMapping({JointXccyQuoteRiskFixtures::Trade(currencySpec)}, currencyMarket, currencyProvenance));
    const auto combined = Dal::AggregateRatePortfolioQuoteRisk(
        {JointQuoteRiskFixtures::Irs(currencySpec, 1), JointXccyQuoteRiskFixtures::Trade(currencySpec)}, currencyMarket, {currencyProvenance});
    ASSERT_EQ(combined.pvByActualPvCcy_.size(), 2);
    ASSERT_EQ(combined.buckets_.size(), 2 * currencyProvenance.Axis().quotes_.size());
}

TEST(CalibrationRiskTest, TestCurveSourceIdentityIncludesIdCaseBindingsQuotesAndSettings) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    const auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    auto config = JointQuoteRiskFixtures::Config(2);
    const auto original = Captured(spec, calibrated, options, market, config);
    const auto boundary = Dal::NewCalibrationPullback(original);
    const auto equivalent = Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, config));
    ASSERT_TRUE(boundary.Matches(equivalent));
    config.calibrationId_ = "GENERIC-JOINT";
    const auto caseId = Captured(spec, calibrated, options, market, config);
    ASSERT_EQ(caseId.State().fingerprint_, original.State().fingerprint_);
    ASSERT_EQ(caseId.CalibrationRecord(), original.CalibrationRecord());
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(caseId)));
    config = JointQuoteRiskFixtures::Config(2);
    config.componentKeyByParameterBlock_["curve:0"] = "renamed";
    market.curveComponents_["renamed"] = market.curveComponents_.at("curve:0");
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(Captured(spec, calibrated, options, market, config))));
    auto renamedSpec = spec;
    renamedSpec.curves_[0].curveName_ = "Repeated_Name";
    const auto renamed = Dal::CalibrateJointMultiCurve(renamedSpec, options);
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(
        boundary, Dal::NewCalibrationPullback(Captured(renamedSpec, renamed, options, JointQuoteRiskFixtures::Market(renamedSpec, renamed),
                                                       JointQuoteRiskFixtures::Config(2)))));
    auto changedSpec = spec;
    const auto instrument = spec.curves_[0].instruments_[0];
    changedSpec.curves_[0].instruments_[0] =
        JointQuoteRiskFixtures::Instrument(spec.today_, instrument->TimeSpan().second, 0, instrument->MarketRate() + 1e-5);
    const auto changed = Dal::CalibrateJointMultiCurve(changedSpec, options);
    ASSERT_NO_FATAL_FAILURE(AssertCurveMismatch(
        boundary, Dal::NewCalibrationPullback(Captured(changedSpec, changed, options, JointQuoteRiskFixtures::Market(changedSpec, changed),
                                                       JointQuoteRiskFixtures::Config(2)))));
    auto changedOptions = options;
    changedOptions.jacobianMode_ = Dal::CurveJacobianMode_::Value_::BUMPED;
    const auto bumped = Dal::CalibrateJointMultiCurve(spec, changedOptions);
    ASSERT_NO_FATAL_FAILURE(
        AssertCurveMismatch(boundary, Dal::NewCalibrationPullback(Captured(spec, bumped, changedOptions, JointQuoteRiskFixtures::Market(spec, bumped),
                                                                           JointQuoteRiskFixtures::Config(2)))));
}

TEST(CalibrationRiskTest, TestCurveMappingRequiresAvailableCapturedContent) {
    const auto spec = JointQuoteRiskFixtures::Spec();
    auto options = JointXccyQuoteRiskFixtures::Options();
    const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
    const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
    const auto defaultSource = Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, JointQuoteRiskFixtures::Config(2));
    ASSERT_TRUE(defaultSource.Available());
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { Dal::NewCalibrationPullback(defaultSource); }, "QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED"));
    options.computeEffJacobianInverse_ = false;
    const auto unavailable = Dal::CalibrateJointMultiCurve(spec, options);
    const auto source = Captured(spec, unavailable, options, JointQuoteRiskFixtures::Market(spec, unavailable), JointQuoteRiskFixtures::Config(2));
    ASSERT_FALSE(source.Available());
    ASSERT_NO_FATAL_FAILURE(AssertFailure([&] { Dal::NewCalibrationPullback(source); }, std::string(source.Reason().c_str())));
}

TEST(CalibrationRiskTest, TestSeedShapeNonFiniteAndMixedDomainFailuresRecover) {
    const auto curve = SmallCurve();
    const FlatIVS_ ivs;
    const auto dupire = Dal::NewCalibrationPullback(Dal::CalibrateDupireWithRisk(ivs, Inputs()));
    ASSERT_FALSE(curve.Matches(dupire));
    for (const auto& boundary : {curve, dupire}) {
        const auto good = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols(), 0.25));
        const auto before = Dal::PullbackCalibration(boundary, good);
        for (const auto& invalid : {Dal::Matrix_<>(), Dal::Matrix_<>(0, std::numeric_limits<int>::max()),
                                    Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols() + 1)})
            ASSERT_THROW(Dal::NewCalibrationParameterAdjoints(boundary, invalid), Dal::Exception_);
        for (double invalid :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
            Dal::Matrix_<> parameters(boundary.ParameterRows(), boundary.ParameterCols(), 0.0);
            Dal::Matrix_<> direct(boundary.QuoteRows(), boundary.QuoteCols(), 0.0);
            parameters(0, 0) = direct(0, 0) = invalid;
            ASSERT_THROW(Dal::NewCalibrationParameterAdjoints(boundary, parameters), Dal::Exception_);
            ASSERT_THROW(Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct), Dal::Exception_);
        }
        const auto other = boundary.Matches(curve) ? dupire : curve;
        const auto mixed = Dal::NewCalibrationDirectQuoteAdjoints(other, Dal::Matrix_<>(other.QuoteRows(), other.QuoteCols(), 0.0));
        ASSERT_THROW(Dal::PullbackCalibration(boundary, good, mixed), Dal::Exception_);
        ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, good).TotalAdjoints(), before.TotalAdjoints()));
    }
}

TEST(CalibrationRiskTest, TestCurveDirectContributionsZeroSeedsAndOverflowRecover) {
    const auto boundary = SmallCurve();
    const auto zero = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, 0.0));
    Dal::Matrix_<> direct(boundary.QuoteRows(), 1, -0.125);
    const auto directOnly = Dal::PullbackCalibration(boundary, zero, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct));
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(directOnly.TotalAdjoints(), direct));
    for (double value : directOnly.CalibrationAdjoints())
        ASSERT_EQ(value, 0.0);
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, -0.25));
    const auto calibration = Dal::PullbackCalibration(boundary, seeds);
    const auto combined = Dal::PullbackCalibration(boundary, seeds, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct));
    for (int row = 0; row < direct.Rows(); ++row) {
        ASSERT_EQ(combined.CalibrationAdjoints()(row, 0), calibration.TotalAdjoints()(row, 0));
        ASSERT_EQ(combined.DirectAdjoints()(row, 0), direct(row, 0));
        ASSERT_EQ(combined.TotalAdjoints()(row, 0), calibration.TotalAdjoints()(row, 0) + direct(row, 0));
    }
    const auto extreme = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, 1e300));
    const auto finite = Dal::PullbackCalibration(boundary, extreme);
    for (int row = 0; row < direct.Rows(); ++row)
        direct(row, 0) = std::copysign(std::numeric_limits<double>::max(), finite.TotalAdjoints()(row, 0));
    ASSERT_THROW(Dal::PullbackCalibration(boundary, extreme, Dal::NewCalibrationDirectQuoteAdjoints(boundary, direct)), Dal::Exception_);
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, seeds).TotalAdjoints(), calibration.TotalAdjoints()));
}

TEST(CalibrationRiskTest, TestPassiveCurveMappingPreservesUnrelatedActiveRecording) {
    const auto boundary = SmallCurve();
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), 1, -0.25));
    const auto reference = Dal::PullbackCalibration(boundary, seeds);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    const auto nodes = Dal::AAD::Tape()->nodes_.Size();
    const auto result = Dal::PullbackCalibration(boundary, seeds);
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(result.TotalAdjoints(), reference.TotalAdjoints()));
    ASSERT_TRUE(result.Calibration().Matches(boundary));
    ASSERT_EQ(result.Unit(), "DECIMAL_QUOTE");
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
    recording.FinishRecording();
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(CalibrationRiskTest, TestDupireNestedRejectionPreservesRecordingAndRecovers) {
    const FlatIVS_ ivs;
    const auto boundary = Dal::NewCalibrationPullback(Dal::CalibrateDupireWithRisk(ivs, Inputs()));
    const auto seeds = Dal::NewCalibrationParameterAdjoints(boundary, Dal::Matrix_<>(boundary.ParameterRows(), boundary.ParameterCols(), 0.25));
    const auto reference = Dal::PullbackCalibration(boundary, seeds);
    {
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Dal::AAD::Number_ output = input * input;
        recording.FinishRecording();
        const auto nodes = Dal::AAD::Tape()->nodes_.Size();
        ASSERT_THROW(Dal::PullbackCalibration(boundary, seeds), Dal::Exception_);
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
        Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
        recording.Reverse();
        ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
        recording.Close();
    }
    ASSERT_NO_FATAL_FAILURE(AssertSameMatrix(Dal::PullbackCalibration(boundary, seeds).TotalAdjoints(), reference.TotalAdjoints()));
}

TEST(CalibrationRiskTest, TestSmoothSquareCurveDirectionsMatchIndependentRecalibration) {
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED})
        for (bool layered : {false, true})
            ASSERT_NO_FATAL_FAILURE(
                AssertDirectionalCalibration(JointQuoteRiskFixtures::Spec(5, 2, Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, layered),
                                             JointXccyQuoteRiskFixtures::Options(mode)));
}
