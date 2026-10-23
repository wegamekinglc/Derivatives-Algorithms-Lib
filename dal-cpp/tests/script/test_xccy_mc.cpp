#include <gtest/gtest.h>

#include "xccymcfixture.hpp"
#include "xccymcreference.hpp"
#include <dal/math/aad/recording.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/script/xccymc.hpp>
#include <dal/script/xccymc_internal.hpp>
#include <limits>

using namespace Dal;
using namespace Dal::TestXccyMc;

TEST(XccyMcTest, TestPhaseSelectsFrozenReservationWithoutCallOrderState) {
    CalibrationMcMapping_ mapping;
    mapping.dimension_ = 35;
    mapping.identity_ = "five-slot-mapping";
    mapping.seed_ = 1024;
    XccyMcSettings_ settings;
    settings.batches_ = 16;
    settings.pathsPerBatch_ = 16384;
    settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    const auto held = PrepareXccyMcReservation(settings, mapping);
    settings.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    const auto validation = PrepareXccyMcReservation(settings, mapping);
    ASSERT_EQ(held.ranges_.front().firstPath_, 1310720U);
    ASSERT_EQ(held.ranges_.back().pathEnd_, 1572864U);
    ASSERT_EQ(validation.ranges_.front().firstPath_, 262144U);
    ASSERT_EQ(validation.ranges_.back().pathEnd_, 524288U);
    ASSERT_EQ(validation.ranges_.front().firstCoordinate_, 262144U * 35);
    ASSERT_EQ(validation.ranges_.back().coordinateEnd_, 524288U * 35);
    ASSERT_NE(held.identity_, validation.identity_);
    ASSERT_EQ(validation.identity_, PrepareXccyMcReservation(settings, mapping).identity_);
}

TEST(XccyMcTest, TestUnsetPhaseAndCapacityFailBeforeAllocatingRanges) {
    CalibrationMcMapping_ mapping;
    mapping.dimension_ = 5;
    mapping.identity_ = "mapping";
    mapping.seed_ = 1024;
    XccyMcSettings_ settings;
    settings.batches_ = 2;
    settings.pathsPerBatch_ = 2;
    try {
        static_cast<void>(PrepareXccyMcReservation(settings, mapping));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/phase");
        ASSERT_EQ(error.Failure().diagnostic_.actual_, "_NOT_SET");
    }
    settings.phase_ = XccyMonteCarloPhase_::Value_::Fit;
    settings.pathsPerBatch_ = 131073;
    try {
        static_cast<void>(PrepareXccyMcReservation(settings, mapping));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        ASSERT_EQ(error.Failure().diagnostic_.field_, "/sampling");
    }
}

TEST(XccyMcTest, TestCheckedCoordinateOverflowSeedAndCountRefusal) {
    CalibrationMcMapping_ mapping;
    mapping.dimension_ = std::numeric_limits<size_t>::max();
    mapping.identity_ = "mapping";
    mapping.seed_ = 1024;
    XccyMcSettings_ settings;
    settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    settings.batches_ = settings.pathsPerBatch_ = 2;
    ASSERT_THROW(static_cast<void>(PrepareXccyMcReservation(settings, mapping)), CalibrationError_);
    mapping.dimension_ = 5;
    settings.seed_ = 7;
    ASSERT_THROW(static_cast<void>(PrepareXccyMcReservation(settings, mapping)), CalibrationError_);
    settings.seed_ = 1024;
    settings.batches_ = -1;
    ASSERT_THROW(static_cast<void>(PrepareXccyMcReservation(settings, mapping)), CalibrationError_);
    settings.batches_ = 2;
    settings.streamVersion_ = "DAL.MRG32-NORMAL-PATHS/1";
    ASSERT_THROW(static_cast<void>(PrepareXccyMcReservation(settings, mapping)), CalibrationError_);
}

TEST(XccyMcTest, TestZeroVolatilityTreeCompiledAndGaussianFastUseOriginalInstrument) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = ModelSpec(TestXccy::TODAY, true);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("mc", spec));
    const auto expected = EvaluateXccyGaussianMoments<double>(PrepareMoment(instrument, context), *model);
    XccyMcSettings_ settings;
    settings.phase_ = XccyMonteCarloPhase_::Value_::Validation;
    settings.batches_ = settings.pathsPerBatch_ = 2;
    settings.retainPaths_ = true;
    for (const bool compiled : {false, true}) {
        settings.compiled_ = compiled;
        const PreparedXccyMc_ prepared(instrument, "Price", context, model, settings);
        const auto actual = prepared.Evaluate();
        ASSERT_NEAR(actual.estimate_.mean_, expected.price_, 1e-10);
        ASSERT_NEAR(actual.coefficients_.a_, expected.a_, 1e-10);
        ASSERT_NEAR(actual.coefficients_.b_, expected.b_, 1e-10);
        ASSERT_NEAR(actual.estimate_.standardError_, 0., 1e-12);
        ASSERT_EQ(actual.estimate_.pathValues_.size(), 4U);
        for (const double price : actual.estimate_.pathValues_)
            ASSERT_NEAR(price, expected.price_, 1e-10);
    }
}

TEST(XccyMcTest, TestNativeAadSpotAndBatchGradientErrorsAgreeWithCrnDifferences) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = NonflatSpec();
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
    const auto context = Context(spec, Empty());
    const Handle_<XccyGaussianModelData_> model(new XccyGaussianModelData_("aad", spec));
    XccyMcSettings_ settings;
    settings.phase_ = XccyMonteCarloPhase_::Value_::Fit;
    settings.batches_ = 4;
    settings.pathsPerBatch_ = 8;
    settings.aad_ = true;
    settings.parameterLabels_ = {"fxspot:EUR/USD"};
    for (const bool compiled : {false, true}) {
        settings.compiled_ = compiled;
        const auto aad = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        ASSERT_EQ(aad.report_->Data().parameters_.size(), 1U);
        ASSERT_TRUE(aad.report_->Data().quoteJacobian_.has_value());
        ASSERT_EQ(aad.report_->Data().parameters_[0].id_, settings.parameterLabels_[0]);
        ASSERT_NEAR((*aad.report_->Data().quoteJacobian_)(0, 0), aad.estimate_.gradient_[0], 1e-12);
        auto valueSettings = settings;
        valueSettings.aad_ = false;
        valueSettings.parameterLabels_.clear();
        const PreparedXccyMc_ plain(instrument, "Price", context, model, valueSettings);
        for (double h : {1e-5, 5e-6}) {
            auto plus = spec, minus = spec;
            plus.market_.fxSpot_ += h;
            minus.market_.fxSpot_ -= h;
            const auto up = plain.Evaluate(Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("up", plus)));
            const auto down = plain.Evaluate(Handle_<XccyGaussianModelData_>(new XccyGaussianModelData_("down", minus)));
            ASSERT_EQ(up.reservation_.identity_, aad.reservation_.identity_);
            ASSERT_NEAR(aad.estimate_.gradient_[0], (up.estimate_.mean_ - down.estimate_.mean_) / (2. * h), 1e-8);
            Vector_<> fd;
            double mean = 0., squared = 0.;
            for (size_t i = 0; i < aad.estimate_.batches_.size(); ++i) {
                fd.push_back((up.estimate_.batches_[i].value_ - down.estimate_.batches_[i].value_) / (2. * h));
                mean += fd.back() / 4.;
                ASSERT_NEAR(fd.back(), aad.estimate_.batches_[i].derivatives_[0], 1e-8);
            }
            for (const double value : fd)
                squared += (value - mean) * (value - mean) / 12.;
            ASSERT_NEAR(aad.estimate_.gradientStandardError_[0], std::sqrt(squared), 1e-8);
        }
        ASSERT_GT(aad.estimate_.peakTapeNodes_, 0U);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0U);
    }
}

TEST(XccyMcTest, TestLedgerRejectsOverlapMixedMappingAndMutationWhileCrnIsExplicit) {
    CalibrationMcMapping_ mapping;
    mapping.dimension_ = 35;
    mapping.seed_ = 1024;
    mapping.identity_ = "five-slots";
    auto settings = Settings();
    const auto validation = PrepareXccyMcReservation(settings, mapping);
    ASSERT_THROW(RequireXccyMcIndependent(validation, validation), CalibrationError_);
    ASSERT_NO_THROW(RequireXccyMcIndependent(validation, validation, true));
    settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
    const auto held = PrepareXccyMcReservation(settings, mapping);
    ASSERT_NO_THROW(RequireXccyMcIndependent(validation, held));
    auto stale = held;
    stale.dimension_ += 5;
    ASSERT_THROW(RequireXccyMcIndependent(validation, stale), CalibrationError_);
    stale = held;
    stale.ranges_[0].firstCoordinate_ += 1;
    ASSERT_THROW(RequireXccyMcIndependent(validation, stale), CalibrationError_);
    mapping.identity_ = "different-grid";
    const auto mixed = PrepareXccyMcReservation(settings, mapping);
    ASSERT_THROW(RequireXccyMcIndependent(validation, mixed), CalibrationError_);
    ASSERT_EQ(ParseXccyMonteCarloPhase("HeldOut"), XccyMonteCarloPhase_::Value_::HeldOut);
    for (const String_ invalid : {"", "heldout", "Unknown"})
        ASSERT_THROW(static_cast<void>(ParseXccyMonteCarloPhase(invalid)), CalibrationError_);
    settings.pathsPerBatch_ = 65536;
    settings.batches_ = 16;
    ASSERT_EQ(PrepareXccyMcReservation(settings, mapping).ranges_.back().pathEnd_, 2359296U);
    settings.pathsPerBatch_++;
    ASSERT_THROW(static_cast<void>(PrepareXccyMcReservation(settings, mapping)), CalibrationError_);
}

TEST(XccyMcTest, TestExactCoordinateTraceAndIndependentCashflowsCatchExtraNormal) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = NonflatSpec();
    const auto context = Context(spec, Empty());
    const auto model = Data(spec);
    auto settings = Settings(2, 3);
    settings.retainPaths_ = true;
    for (const auto mode : XccyNotionalModeListAll()) {
        const auto instrument = Instrument(mode);
        const auto actual = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        const Script::PreparedXccyScript_ source(instrument, context, model);
        const Script::XccyScriptPathProvider_<> provider(source);
        ASSERT_EQ(actual.mapping_.normalCoordinates_.size(), provider.Model().SimDim());
        ASSERT_EQ(actual.mapping_.dimension_ % 5, 0U);
        ASSERT_EQ(actual.mapping_.numeraire_, "dal.domestic-collateral-bank/1");
        ASSERT_EQ(actual.consumedCoordinates_, 6 * provider.Model().SimDim());
        auto rng = New(RNGType_("MRG32"), 1024, provider.Model().SimDim(), true);
        Vector_<> normals(provider.Model().SimDim());
        size_t path = 0;
        for (size_t batch = 0; batch < actual.reservation_.ranges_.size(); ++batch) {
            const auto& range = actual.reservation_.ranges_[batch];
            rng->SkipNormalTo(range.firstPath_);
            for (size_t p = range.firstPath_; p < range.pathEnd_; ++p) {
                rng->FillNormal(&normals);
                if (p == range.firstPath_) {
                    ASSERT_EQ(normals, actual.firstNormals_[batch]);
                }
                const auto states = provider.Model().GenerateStates(normals);
                const CashflowReference_ independent(spec, states, Empty(), 64);
                const double expected = Sum(independent.Payments(mode, false, true));
                ASSERT_NEAR(actual.estimate_.pathValues_[path++], expected, 1e-10);
            }
            rng->SkipNormalTo(range.firstPath_);
            rng->FillNormal(&normals);
            rng->FillNormal(&normals);
            ASSERT_NE(normals, actual.firstNormals_[batch]);
        }
        settings.compiled_ = false;
        const auto tree = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        ASSERT_EQ(tree.reservation_.identity_, actual.reservation_.identity_);
        ASSERT_EQ(tree.estimate_.pathValues_, actual.estimate_.pathValues_);
        settings.compiled_ = true;
    }
}

TEST(XccyMcTest, TestAllNativeCurveBasisAndVolPartialsThroughSnapshotMap) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = NonflatSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, true);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 3);
    static_cast<void>(XccyGaussianMomentNativeMap(*model, &settings.parameterLabels_));
    settings.aad_ = true;
    const auto aad = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    const auto labels = settings.parameterLabels_;
    ASSERT_NEAR(aad.estimate_.mean_, NONFLAT_PRICE, 1e-10);
    ASSERT_EQ(labels.size(), NONFLAT_NATIVE.size());
    for (size_t j = 0; j < labels.size(); ++j) {
        ASSERT_EQ(labels[j], String_(NONFLAT_NATIVE[j].label_));
        ASSERT_NEAR(aad.estimate_.gradient_[j], NONFLAT_NATIVE[j].full_, 1e-8);
        ASSERT_NEAR(aad.estimate_.gradient_[j], NONFLAT_NATIVE[j].half_, 1e-8);
    }
#if LDBL_MANT_DIG > DBL_MANT_DIG
    settings.aad_ = false;
    settings.parameterLabels_.clear();
    settings.retainPaths_ = true;
    const PreparedXccyMc_ plain(instrument, "Price", context, model, settings);
    for (size_t j = 0; j < labels.size(); ++j) {
        SCOPED_TRACE(labels[j]);
        for (const double h : {1e-3 * Scale(labels[j]), 5e-4 * Scale(labels[j])}) {
            const auto f = [&](double shift) {
                const auto candidate = Bump(*model, labels[j], shift);
                const auto value = plain.Evaluate(candidate);
                const Script::PreparedXccyScript_ source(instrument, context, candidate);
                const Script::XccyScriptPathProvider_<> layout(source);
                const auto wide = NewXccyGaussianModel<long double>(*candidate);
                wide->Prepare(layout.Model().Grid());
                auto rng = New(RNGType_("MRG32"), settings.seed_, value.mapping_.dimension_, true);
                Vector_<> normals(value.mapping_.dimension_);
                long double sum = 0.L;
                size_t path = 0;
                for (const auto& range : value.reservation_.ranges_) {
                    rng->SkipNormalTo(range.firstPath_);
                    for (size_t p = range.firstPath_; p < range.pathEnd_; ++p) {
                        rng->FillNormal(&normals);
                        const auto price = WidePrice(*wide, wide->GenerateStates(normals));
                        REQUIRE(std::abs(price - value.estimate_.pathValues_[path++]) < 1e-10L, "wide reference matches original cashflows");
                        sum += price;
                    }
                }
                return sum / path;
            };
            const long double fd = (8.L * (f(h) - f(-h)) - (f(2. * h) - f(-2. * h))) / (12.L * h);
            ASSERT_NEAR(aad.estimate_.gradient_[j], static_cast<double>(fd), 1e-8);
        }
    }
#endif
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
}

TEST(XccyMcTest, TestParUsesOverallMeansAndFullPairedCovarianceForGradientSe) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = ModelSpec();
    const auto admitted = PrepareMoment(Instrument(), Context(spec, Empty()), "BasisSpread");
    XccyMcResult_ actual;
    actual.estimate_.parameterLabels_ = {"test-native-axis"};
    const std::array<std::array<double, 4>, 4> rows{{{{2., -4., .3, .02}}, {{3., -5., -.2, .3}}, {{1., -3., .7, -.1}}, {{4., -6., -.4, .2}}}};
    std::array<double, 4> means{};
    for (const auto& row : rows) {
        CalibrationMcBatchValue_ batch;
        batch.coefficients_ = CalibrationAffineCoefficients_{row[1], row[0], {row[3]}, {row[2]}};
        actual.estimate_.batches_.push_back(batch);
        for (size_t j = 0; j < 4; ++j)
            means[j] += row[j] / 4.;
    }
    XccyMcInternal::Summarize(&actual, admitted, .001);
    const double b = means[0], a = means[1], db = means[2], da = means[3];
    ASSERT_NEAR(actual.estimate_.mean_, -b / a, 1e-12);
    ASSERT_NEAR(actual.estimate_.gradient_[0], -db / a + b * da / (a * a), 1e-12);
    const std::array<double, 4> weights{da / (a * a), db / (a * a) - 2. * b * da / (a * a * a), -1. / a, b / (a * a)};
    double squared = 0., meanOfRatios = 0.;
    for (const auto& row : rows) {
        double influence = 0.;
        for (size_t j = 0; j < 4; ++j)
            influence += weights[j] * (row[j] - means[j]);
        squared += influence * influence / 12.;
        meanOfRatios += -row[0] / row[1] / 4.;
    }
    ASSERT_NEAR(actual.estimate_.gradientStandardError_[0], std::sqrt(squared), 1e-12);
    ASSERT_GT(std::abs(meanOfRatios - actual.estimate_.mean_), 1e-3);
    ASSERT_GT(std::abs(actual.estimate_.standardError_ - actual.estimate_.gradientStandardError_[0]), 1e-3);
    ASSERT_EQ(actual.coefficientCovariance_.Rows(), 4);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double covariance = 0.;
            for (const auto& row : rows)
                covariance += (row[i] - means[i]) * (row[j] - means[j]) / 3.;
            ASSERT_NEAR(actual.coefficientCovariance_(i, j), covariance, 1e-12);
        }
}

TEST(XccyMcTest, TestExpiredZeroDimensionAndParFailureKeepOwnedDiagnostics) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = ModelSpec(Date_(2027, 11, 9));
    const auto model = Data(spec);
    const auto instrument = Instrument();
    const auto context = Context(spec, Empty());
    const auto settings = Settings(2, 2);
    const auto result = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
    ASSERT_EQ(result.mapping_.dimension_, 0U);
    ASSERT_EQ(result.consumedCoordinates_, 0U);
    ASSERT_EQ(result.estimate_.mean_, 0.);
    ASSERT_EQ(result.reservation_.ranges_[0].firstCoordinate_, 0U);
    ASSERT_EQ(result.reservation_.ranges_[0].coordinateEnd_, 0U);
    ASSERT_EQ(result.reservation_.phase_, settings.phase_);
    ASSERT_THROW(RequireXccyMcIndependent(result.reservation_, result.reservation_), CalibrationError_);
    Handle_<CalibrationReport_> report;
    Handle_<Storable_> candidate;
    try {
        static_cast<void>(PreparedXccyMc_(instrument, "BasisSpread", context, model, settings).Evaluate());
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationAnnuity");
        ASSERT_EQ(error.Failure().diagnostic_.instrumentId_, instrument->Id());
        ASSERT_EQ(error.Failure().diagnostic_.measureId_, "BasisSpread");
        report = error.Failure().report_;
        candidate = error.Failure().candidate_;
    }
    ASSERT_TRUE(report);
    ASSERT_TRUE(candidate);
    ASSERT_EQ(report->Errors()[0].code_, "InvalidCalibrationAnnuity");
}

TEST(XccyMcTest, TestCombinedGradientBudgetIsSeparateFromParityPrecisionAndPriceSe) {
    XccyMcResult_ value;
    value.estimate_.validation_ = true;
    value.estimate_.batches_ = Vector_<CalibrationMcBatchValue_>(2);
    value.estimate_.parameterLabels_ = {"fxspot:EUR/USD"};
    value.estimate_.gradient_ = {.125};
    value.estimate_.gradientStandardError_ = {.05};
    value.estimate_.standardError_ = 1e-5;
    CalibrationMcReference_ reference;
    reference.gradient_ = {0.};
    reference.gradientStandardError_ = reference.finiteDifferenceError_ = {0.};
    CalibrationMcPrecision_ precision;
    precision.errorScale_ = .01;
    precision.parameterScales_ = {.01};
    precision.epsilonGradient_ = {1e-8};
    const auto result = ValidateXccyMc(value, reference, precision);
    ASSERT_TRUE(result.validation_.parityPassed_);
    ASSERT_TRUE(result.validation_.precisionPassed_);
    ASSERT_TRUE(result.validation_.valueErrorPassed_);
    ASSERT_TRUE(result.validation_.gradientParityPassed_);
    ASSERT_TRUE(result.validation_.gradientPrecisionPassed_);
    ASSERT_FALSE(result.gradientCombinedPassed_);
    ASSERT_FALSE(result.validation_.numericalValidationPassed_);
    ASSERT_TRUE(result.validation_.budgetExhausted_);
}

TEST(XccyMcTest, TestSourceRebindingOrientationAndSpreadLegFlowThroughBothCoefficients) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 3);
    settings.retainPaths_ = true;
    for (const auto mode : XccyNotionalModeListAll())
        for (const bool foreign : {false, true}) {
            const auto instrument = Instrument(mode, foreign);
            const auto price = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
            const auto reverse = PreparedXccyMc_(Instrument(mode, foreign, false), "Price", context, model, settings).Evaluate();
            const auto rebound = PreparedXccyMc_(Rebind(instrument, .002, 220.), "Price", context, model, settings).Evaluate();
            const auto par = PreparedXccyMc_(instrument, "BasisSpread", context, model, settings).Evaluate();
            ASSERT_NEAR(reverse.estimate_.mean_, -price.estimate_.mean_, 1e-10);
            ASSERT_NEAR(rebound.estimate_.mean_, 2. * (price.coefficients_.b_ + .002 * price.coefficients_.a_), 1e-10);
            ASSERT_NEAR(par.estimate_.mean_, -price.coefficients_.b_ / price.coefficients_.a_, 1e-12);
            ASSERT_TRUE(par.estimate_.affine_.has_value());
            ASSERT_TRUE(par.estimate_.pathValues_.empty());
            ASSERT_EQ(par.pathCoefficients_.size(), 6U);
            ASSERT_EQ(price.reservation_.identity_, reverse.reservation_.identity_);
            for (size_t p = 0; p < price.pathCoefficients_.size(); ++p)
                ASSERT_NEAR(reverse.estimate_.pathValues_[p], -price.estimate_.pathValues_[p], 1e-10);
        }
}

TEST(XccyMcTest, TestAadRankLabelsGeometryAndCallerTapeRefusalsAreOwned) {
    using namespace TestXccyMoments;
    using namespace TestXccyExecution;
    const auto spec = ModelSpec();
    const auto model = Data(spec);
    const auto instrument = Instrument();
    const auto context = Context(spec, Empty());
    auto settings = Settings(2, 2);
    settings.aad_ = true;
    settings.parameterLabels_ = {"a:USD"};
    ASSERT_THROW(static_cast<void>(PreparedXccyMc_(instrument, "Price", context, model, settings)), CalibrationError_);
    settings.parameterLabels_ = {"fxspot:EUR/USD", "fxspot:EUR/USD"};
    ASSERT_THROW(static_cast<void>(PreparedXccyMc_(instrument, "Price", context, model, settings)), CalibrationError_);
    settings.parameterLabels_ = {"fxspot:EUR/USD"};
    ASSERT_THROW(static_cast<void>(PreparedXccyMc_(instrument, "Price", context, Data(ModelSpec(TestXccy::TODAY, true)), settings)),
                 CalibrationError_);
    const PreparedXccyMc_ prepared(instrument, "Price", context, model, settings);
    auto wrong = spec;
    wrong.dynamics_.bucketEndDates_[0] = wrong.dynamics_.bucketEndDates_[0].AddDays(1);
    try {
        static_cast<void>(prepared.Evaluate(Data(wrong)));
        FAIL();
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationContextMismatch");
        ASSERT_FALSE(error.Failure().candidate_);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), 0);
    }
    {
        const auto reset = AAD::SetNumResultsForAAD(true, 2);
        ASSERT_THROW(static_cast<void>(prepared.Evaluate()), CalibrationError_);
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 2U);
    }
    {
        AAD::RecordingScope_ caller;
        caller.StartRecording();
        AAD::Number_ input(2.);
        AAD::Number_ output = input * input;
        const auto nodes = AAD::Tape()->nodes_.Size();
        ASSERT_THROW(static_cast<void>(prepared.Evaluate()), CalibrationError_);
        ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
        caller.FinishRecording();
        caller.ClearAdjoints();
        Adjoint(output) = 1.;
        caller.Reverse();
        ASSERT_NEAR(Adjoint(input), 4., 1e-12);
    }
    auto small = spec;
    small.dynamics_.domesticSigma_[0] = 1e-8;
    ASSERT_TRUE(std::isfinite(PreparedXccyMc_(instrument, "Price", context, Data(small), settings).Evaluate().estimate_.gradient_[0]));
}
