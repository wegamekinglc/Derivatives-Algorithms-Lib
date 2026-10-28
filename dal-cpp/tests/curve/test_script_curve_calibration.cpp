#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/curve/aadjacobian.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/yczerorate.hpp>

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 21), MATURITY(2027, 10, 21);
    double NativeInitial(const CurveInstrumentCalibrationSpec_& spec);
    double MixedNormalOracle(const CurveInstrumentCalibrationSpec_& spec) {
        class Function_ final : public Underdetermined::Function_ {
            const CurveInstrumentCalibrationSpec_& spec_;
            const PreparedCurveInstruments_& plan_;
            double initial_;

        public:
            explicit Function_(const CurveInstrumentCalibrationSpec_& spec, const PreparedCurveInstruments_& plan)
                : spec_(spec), plan_(plan), initial_(NativeInitial(spec)) {}
            Vector_<> F(const Vector_<>& z) const override {
                const double theta = initial_ + spec_.solver_.parameterScales_[0] * z[0];
                return plan_.Residuals({theta});
            }
            void Gradient(const Vector_<>& z, const Vector_<>&, Matrix_<>* jacobian) const override {
                jacobian->Resize(2, static_cast<int>(z.size()));
                auto up = z, down = z;
                for (int j = 0; j < jacobian->Cols(); ++j) {
                    up[j] += 1e-6;
                    down[j] -= 1e-6;
                    const auto plus = F(up), minus = F(down);
                    for (int i = 0; i < 2; ++i)
                        (*jacobian)(i, j) = (plus[i] - minus[i]) / (up[j] - down[j]);
                    up[j] = down[j] = z[j];
                }
            }
        };
        const PreparedCurveInstruments_ plan(spec);
        Function_ function(spec, plan);
        Sparse::TriDiagonal_ weights(1);
        weights.Set(0, 0, spec.solver_.parameterScales_[0] * spec.solver_.parameterScales_[0] * spec.solver_.smoothingWeight_);
        Dictionary_ dictionary;
        dictionary.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.solver_.maxEvaluations_)));
        dictionary.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.solver_.maxRestarts_)));
        const auto z =
            Underdetermined::Approximate(function, {0.}, {spec.observations_[0]->ErrorScale(), spec.observations_[1]->ErrorScale()},
                                         *spec.solver_.fitNormTolerance_, weights, UnderdeterminedControls_(dictionary), spec.solver_.fitWeight_);
        return NativeInitial(spec) + spec.solver_.parameterScales_[0] * z[0];
    }
    CurveInstrumentCalibrationSpec_ DepositSpec() {
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = TODAY;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.geometry_.knotDates_ = {TODAY, MATURITY};
        spec.valuation_.evaluationDate_ = TODAY;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        spec.market_.routes_ = {{"USD.OIS"}};
        const auto instrument =
            NewDepositScriptInstrument(Deposit_(TODAY, MATURITY, .025, DayBasis_("ACT_360")), {"deposit", Ccy_("USD"), "USD.OIS", 100.});
        const auto quote = NewCalibrationMarketQuote("q", .035, CalibrationValueUnit_::Parse("Rate"));
        spec.observations_ = {NewCalibrationObservation("o", instrument, "ParRate", quote, 1e-10)};
        return spec;
    }
    void AssertInvalidObjectiveMetadata(const CalibrationReport_& report, bool weight, double value) {
        const auto expected = std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
        if (weight) {
            ASSERT_EQ(report.Data().summary_.fitWeight_, expected);
            ASSERT_EQ(report.Data().objective_.lambda_, expected);
        } else {
            ASSERT_EQ(report.Data().objective_.parameterScales_, std::isfinite(value) ? Vector_<>({value}) : Vector_<>());
            ASSERT_EQ(report.Data().parameters_[0].scale_, expected);
        }
    }
    struct LegacySolveResult_ {
        Vector_<> coordinates_, residuals_;
        int evaluations_ = 0, gradients_ = 0;
        bool fitWithinTolerance_ = false;
    };
    Vector_<> NativeInitialVector(const CurveInstrumentCalibrationSpec_& spec) {
        CurveCalibrationSpec_ geometry;
        geometry.today_ = spec.geometry_.today_;
        geometry.ccy_ = spec.geometry_.ccy_;
        geometry.curveName_ = spec.geometry_.curveName_;
        geometry.knotDates_ = spec.geometry_.knotDates_;
        geometry.parameterization_ = spec.geometry_.parameterization_;
        geometry.logDfScheme_ = spec.geometry_.logDfScheme_;
        geometry.baseCurve_ = spec.geometry_.baseCurve_;
        geometry.targetCollateral_ = spec.geometry_.targetCollateral_;
        geometry.targetTenor_ = spec.geometry_.targetTenor_;
        geometry.calibrateDiscountCurve_ = spec.geometry_.calibrateDiscountCurve_;
        geometry.liborBasis_ = spec.geometry_.liborBasis_;
        geometry.initialGuess_ = spec.solver_.initialGuess_;
        geometry.initialGuessPerNode_ = spec.solver_.initialGuessPerNode_;
        return ResolveCurveCalibrationInitialGuess(geometry);
    }
    double NativeInitial(const CurveInstrumentCalibrationSpec_& spec) { return NativeInitialVector(spec)[0]; }
    class LegacyDepositFunction_ final : public Underdetermined::Function_ {
        const CurveInstrumentCalibrationSpec_& spec_;
        const PreparedCurveInstruments_& plan_;
        Vector_<> initial_;
        int* evaluations_;
        int* gradients_;

        [[nodiscard]] double Scale(size_t j) const { return spec_.solver_.parameterScales_.empty() ? 1. : spec_.solver_.parameterScales_[j]; }

    public:
        explicit LegacyDepositFunction_(const CurveInstrumentCalibrationSpec_& spec,
                                        const PreparedCurveInstruments_& plan,
                                        int* evaluations,
                                        int* gradients)
            : spec_(spec), plan_(plan), initial_(NativeInitialVector(spec)), evaluations_(evaluations), gradients_(gradients) {}
        [[nodiscard]] Vector_<> Residuals(const Vector_<>& z) const {
            auto native = initial_;
            for (size_t j = 0; j < native.size(); ++j)
                native[j] += Scale(j) * z[j];
            return plan_.Residuals(native);
        }
        [[nodiscard]] double BumpSize() const override { return 1e-6; }
        Vector_<> F(const Vector_<>& z) const override {
            ++*evaluations_;
            return Residuals(z);
        }
        std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>&, const Vector_<>&) const override {
            ++*gradients_;
            return nullptr;
        }
        void Gradient(const Vector_<>& z, const Vector_<>&, Matrix_<>* jacobian) const override {
            jacobian->Resize(static_cast<int>(plan_.Axes().observations_.size()), static_cast<int>(z.size()));
            auto up = z, down = z;
            for (int j = 0; j < jacobian->Cols(); ++j) {
                up[j] += 1e-6;
                down[j] -= 1e-6;
                const auto plus = F(up), minus = F(down);
                for (int i = 0; i < jacobian->Rows(); ++i)
                    (*jacobian)(i, j) = (plus[i] - minus[i]) / (up[j] - down[j]);
                up[j] = down[j] = z[j];
            }
        }
    };
    bool LegacyFitAccepted(const CurveInstrumentCalibrationSpec_& spec, const Vector_<>& residuals) {
        double norm = 0.;
        bool exact = true;
        for (size_t i = 0; i < residuals.size(); ++i) {
            const double scaled = residuals[i] / spec.observations_[i]->ErrorScale();
            exact = exact && std::abs(scaled) <= 1.;
            norm = std::hypot(norm, scaled);
        }
        return spec.solver_.solveMode_ == CurveSolveMode_::Value_::EXACT ? exact : norm <= *spec.solver_.fitNormTolerance_;
    }
    LegacySolveResult_ SolveLegacyDeposit(const CurveInstrumentCalibrationSpec_& spec) {
        LegacySolveResult_ result;
        const PreparedCurveInstruments_ plan(spec);
        LegacyDepositFunction_ function(spec, plan, &result.evaluations_, &result.gradients_);
        Vector_<Date_> dates;
        for (const auto& parameter : plan.Describe().freeParameters_)
            dates.push_back(parameter.date_);
        auto weights = BuildCurveCalibrationWeights(dates, 1, spec.solver_.smoothingWeight_);
        Vector_<> scales;
        for (const auto& row : spec.observations_)
            scales.push_back(row->ErrorScale());
        result.coordinates_ =
            RunCurveSolver(function, Vector_<>(plan.Describe().freeParameters_.size(), 0.), scales,
                           spec.solver_.solveMode_ == CurveSolveMode_::Value_::EXACT, spec.solver_.fitNormTolerance_.value_or(1e-6), *weights,
                           spec.solver_.maxEvaluations_, spec.solver_.maxRestarts_, nullptr, nullptr, spec.solver_.fitWeight_);
        result.residuals_ = function.Residuals(result.coordinates_);
        result.fitWithinTolerance_ = LegacyFitAccepted(spec, result.residuals_);
        return result;
    }
} // namespace

TEST(ScriptCurveCalibrationTest, TestLegacySolverDrivesTypedCalibration) {
    for (const bool approximate : {false, true}) {
        auto spec = DepositSpec();
        spec.solver_.parameterScales_ = {.3};
        if (approximate) {
            spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
            spec.solver_.fitWeight_ = .7;
            spec.solver_.fitNormTolerance_ = .01;
        }
        const auto expected = SolveLegacyDeposit(spec);
        const auto actual = CalibrateCurveInstruments(spec);
        ASSERT_TRUE(actual.fitWithinTolerance_) << "approximate=" << approximate;
        ASSERT_EQ(actual.coordinates_.size(), 1u) << "approximate=" << approximate;
        ASSERT_EQ(actual.rawResiduals_.size(), 1u) << "approximate=" << approximate;
        ASSERT_NEAR(actual.coordinates_[0], expected.coordinates_[0], 1e-12) << "approximate=" << approximate;
        ASSERT_NEAR(actual.rawResiduals_[0], expected.residuals_[0], 1e-12) << "approximate=" << approximate;
        ASSERT_EQ(actual.evaluations_, expected.evaluations_) << "approximate=" << approximate;
        ASSERT_EQ(actual.gradients_, expected.gradients_) << "approximate=" << approximate;
    }
}

TEST(ScriptCurveCalibrationTest, TestFiniteDifferenceFitMatchesDepositOracleAcrossCoordinateScales) {
    const double expected = -std::log(1. + .035 * (365. / 360.));
    for (const auto mode : {CurveSolveMode_::Value_::EXACT, CurveSolveMode_::Value_::APPROXIMATE}) {
        for (const double scale : {1., .3, .001, .0001, 10.}) {
            SCOPED_TRACE(scale);
            auto spec = DepositSpec();
            spec.solver_.parameterScales_ = {scale};
            spec.solver_.solveMode_ = mode;
            spec.solver_.fitWeight_ = .7;
            spec.solver_.fitNormTolerance_ = .01;
            const auto fit = CalibrateCurveInstruments(spec);
            ASSERT_TRUE(fit.fitWithinTolerance_);
            ASSERT_NEAR(fit.parameters_[0], expected, 1e-10);
            ASSERT_LE(std::abs(fit.rawResiduals_[0]), 1e-10);
        }
    }
}

TEST(ScriptCurveCalibrationTest, TestCaseSensitiveRoutesKeepDistinctPreparationContexts) {
    auto spec = DepositSpec();
    auto definition = spec.observations_[0]->Instrument()->Definition()->Spec();
    definition.header_.discountRoute_ = "usd.ois";
    const auto instrument = NewScriptCalibrationInstrument({"lower-route", NewScriptInstrumentDefinition(definition), {}, MATURITY});
    spec.market_.routes_.push_back({"usd.ois"});
    spec.observations_.push_back(NewCalibrationObservation("lower-observation", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-10));
    const auto result = PreparedCurveInstruments_(spec).Evaluate({-.04}, false);
    const auto& plans = result.report_->Data().plans_;
    ASSERT_EQ(plans.size(), 2u);
    ASSERT_NE(plans[0].contextIdentity_, plans[1].contextIdentity_);
    ASSERT_DOUBLE_EQ(result.modelMeasures_[0], result.modelMeasures_[1]);
}

TEST(ScriptCurveCalibrationTest, TestReportOwnsEvaluateAndFitAfterInputsAreDestroyed) {
    for (const bool calibrate : {false, true}) {
        Handle_<CalibrationReport_> report;
        double value = 0., residual = 0., parameter = 0., jacobian = 0.;
        int evaluations = 0, gradients = 0;
        {
            const auto spec = DepositSpec();
            CurveInstrumentCalibrationOptions_ options;
            options.computeQuoteJacobian_ = true;
            options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
            const PreparedCurveInstruments_ plan(spec, options);
            const auto result = calibrate ? CalibrateCurveInstruments(spec, options) : plan.Evaluate({-.04});
            report = result.report_;
            value = result.modelMeasures_[0];
            residual = result.rawResiduals_[0];
            parameter = result.parameters_[0];
            jacobian = result.quoteJacobian_(0, 0);
            evaluations = result.evaluations_;
            gradients = result.gradients_;
        }
        ASSERT_TRUE(report);
        const auto& data = report->Data();
        ASSERT_EQ(data.observations_.size(), 1u);
        ASSERT_EQ(data.parameters_.size(), 1u);
        ASSERT_EQ(data.observations_[0].modelValue_, std::optional<double>(value));
        ASSERT_EQ(data.observations_[0].rawResidual_, std::optional<double>(residual));
        ASSERT_EQ(data.observations_[0].derivativeMethod_, "AAD");
        ASSERT_EQ(data.observations_[0].backend_, "FastDeterministic");
        ASSERT_EQ(data.parameters_[0].id_, "LOG_DISCOUNT_FACTOR:2027-10-21");
        ASSERT_EQ(data.parameters_[0].value_, std::optional<double>(parameter));
        ASSERT_TRUE(data.quoteJacobian_);
        ASSERT_DOUBLE_EQ((*data.quoteJacobian_)(0, 0), jacobian);
        ASSERT_EQ(data.summary_.evaluations_, std::optional<int>(evaluations));
        ASSERT_EQ(data.summary_.jacobianEvaluations_, std::optional<int>(gradients));
        ASSERT_EQ(data.plans_.size(), 1u);
        ASSERT_EQ(data.summary_.compilerIdentity_, data.plans_[0].compilerIdentity_);
        ASSERT_EQ(data.summary_.contextIdentity_, data.plans_[0].contextIdentity_);
        ASSERT_EQ(report->Axes().quoteIndex_, Vector_<int>({0}));
        ASSERT_FALSE(report->QuoteRiskAvailable());
    }
}

TEST(ScriptCurveCalibrationTest, TestCopiedFailureReportOwnsLastCompleteCandidate) {
    std::optional<CalibrationError_> retained;
    {
        auto spec = DepositSpec();
        spec.solver_.maxEvaluations_ = 1;
        try {
            static_cast<void>(CalibrateCurveInstruments(spec));
        } catch (const CalibrationError_& error) {
            retained = error;
        }
    }
    ASSERT_TRUE(retained);
    const auto& failure = retained->Failure();
    const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(failure.candidate_.get());
    ASSERT_TRUE(candidate);
    const auto& result = candidate->Result();
    ASSERT_TRUE(result.curve_);
    ASSERT_EQ(failure.report_->Data().summary_.status_, "Failed");
    ASSERT_EQ(failure.report_->Data().summary_.evaluations_, std::optional<int>(result.evaluations_));
    ASSERT_EQ(failure.report_->Data().observations_[0].modelValue_, std::optional<double>(result.modelMeasures_[0]));
    ASSERT_EQ(failure.report_->Data().parameters_[0].value_, std::optional<double>(result.parameters_[0]));
    ASSERT_FALSE(failure.report_->Data().quoteJacobian_);
    ASSERT_EQ(failure.report_->Errors()[0].code_, failure.diagnostic_.code_);
}

TEST(ScriptCurveCalibrationTest, TestInvalidObjectiveReportRetainsPrimaryFailureAndOwnedAxes) {
    for (bool weight : {false, true}) {
        for (double value : {0., std::numeric_limits<double>::quiet_NaN()}) {
            std::optional<CalibrationError_> retained;
            {
                auto spec = DepositSpec();
                if (weight) {
                    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
                    spec.solver_.fitWeight_ = value;
                    spec.solver_.fitNormTolerance_ = .1;
                } else {
                    spec.solver_.parameterScales_ = {value};
                }
                try {
                    static_cast<void>(CalibrateCurveInstruments(spec));
                } catch (const CalibrationError_& error) {
                    retained = error;
                }
            }
            ASSERT_TRUE(retained);
            const auto& failure = retained->Failure();
            const auto& d = failure.diagnostic_;
            ASSERT_EQ(d.code_, "InvalidCalibrationObjective");
            ASSERT_EQ(d.field_, std::optional<String_>(weight ? "/solver/fit_weight" : "/solver/parameter_scales"));
            ASSERT_NE(d.message_.find(weight ? "lambda" : "scales"), String_::npos);
            ASSERT_EQ(d.observationId_, std::optional<String_>("o"));
            ASSERT_EQ(d.instrumentId_, std::optional<String_>("deposit"));
            ASSERT_EQ(d.quoteId_, std::optional<String_>("q"));
            ASSERT_EQ(d.measureId_, std::optional<String_>("ParRate"));
            ASSERT_TRUE(failure.candidate_);
            ASSERT_TRUE(failure.report_);
            const auto& report = *failure.report_;
            ASSERT_EQ(report.Errors().size(), 1);
            ASSERT_EQ(report.Errors()[0].code_, d.code_);
            ASSERT_EQ(report.Errors()[0].field_, d.field_);
            ASSERT_EQ(report.Errors()[0].message_, d.message_);
            ASSERT_EQ(report.Axes().observations_.size(), 1);
            ASSERT_EQ(report.Axes().quotes_.size(), 1);
            ASSERT_EQ(report.Axes().quoteIndex_, Vector_<int>({0}));
            ASSERT_DOUBLE_EQ(report.Axes().quoteMap_(0, 0), 1.);
            ASSERT_EQ(report.Data().observations_[0].instrument_->Id(), "deposit");
            ASSERT_EQ(report.Data().summary_.evaluations_, std::optional<int>(0));
            ASSERT_EQ(report.Data().summary_.jacobianEvaluations_, std::optional<int>(0));
            AssertInvalidObjectiveMetadata(report, weight, value);
        }
    }
}

TEST(ScriptCurveCalibrationTest, TestNativeCoordinatesAndRawJacobianAgainstCashflowOracle) {
    const auto spec = DepositSpec();
    const PreparedCurveInstruments_ plan(spec);
    const auto value = plan.Evaluate({-.04});
    const double alpha = 365. / 360.;
    ASSERT_NEAR(value.modelMeasures_[0], std::expm1(.04) / alpha, 1e-12);
    ASSERT_NEAR(value.quoteJacobian_(0, 0), -std::exp(.04) / alpha, 1e-8);
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_NEAR(fit.parameters_[0], -std::log1p(.035 * alpha), 1e-10);
    ASSERT_EQ(fit.axes_.quoteIndex_, Vector_<int>({0}));
    ASSERT_EQ(fit.freeParameters_[0].component_, CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);
}

TEST(ScriptCurveCalibrationTest, TestRawResidualsMatchFullEvaluation) {
    auto spec = DepositSpec();
    const auto row = spec.observations_[0];
    const auto price = NewCalibrationMarketQuote("price", 1., CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_.push_back(NewCalibrationObservation("price-row", row->Instrument(), "Price", price, 1e-10));
    const PreparedCurveInstruments_ plan(spec);
    const auto expected = plan.Evaluate({-.04}, false);
    ASSERT_EQ(plan.Residuals({-.04}), expected.rawResiduals_);
}

TEST(ScriptCurveCalibrationTest, TestUnresolvedNativeDifferenceCannotPublishZeroRisk) {
    for (bool calibrate : {false, true}) {
        std::optional<CalibrationError_> saved;
        {
            auto spec = DepositSpec();
            spec.solver_.initialGuessPerNode_ = {-.04};
            const auto quote = NewCalibrationMarketQuote("q", std::expm1(.04) / (365. / 360.), CalibrationValueUnit_::Parse("Rate"));
            spec.observations_ = {NewCalibrationObservation("o", spec.observations_[0]->Instrument(), "ParRate", quote, 1e-10)};
            CurveInstrumentCalibrationOptions_ options;
            options.differenceStep_ = 1e-20;
            options.computeQuoteJacobian_ = true;
            try {
                if (calibrate)
                    static_cast<void>(CalibrateCurveInstruments(spec, options));
                else
                    static_cast<void>(PreparedCurveInstruments_(spec, options).Evaluate({-.04}));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
        }
        ASSERT_TRUE(saved) << "calibrate=" << calibrate;
        const auto& f = saved->Failure();
        ASSERT_EQ(f.diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(f.diagnostic_.field_, std::optional<String_>("/options/difference_step"));
        ASSERT_EQ(f.diagnostic_.parameterKey_, std::optional<String_>("LOG_DISCOUNT_FACTOR:2027-10-21"));
        ASSERT_EQ(f.report_->Axes().observations_[0]->Id(), "o");
        ASSERT_EQ(f.report_->Axes().quotes_[0]->Id(), "q");
        ASSERT_EQ(f.report_->Errors().size(), 1);
        ASSERT_TRUE(f.candidate_);
    }
    Matrix_<> legacy;
    CentralDifferenceJacobian(Vector_<>{-.04}, 1, 1e-20, [](const auto& x) { return x; }, &legacy);
    ASSERT_DOUBLE_EQ(legacy(0, 0), 0.);
}

TEST(ScriptCurveCalibrationTest, TestNativeDifferenceUsesActualRepresentableSpacing) {
    auto spec = DepositSpec();
    const auto instrument =
        NewDepositScriptInstrument(Deposit_(TODAY, MATURITY, .025, DayBasis_("ACT_360")), {"small-annuity", Ccy_("USD"), "USD.OIS", 100., 1e-300});
    spec.observations_ = {NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), 1.)};
    CurveInstrumentCalibrationOptions_ options;
    options.differenceStep_ = 1e-12;
    const auto value = PreparedCurveInstruments_(spec, options).Evaluate({-600.});
    const double independent = -std::exp(600.) / (365. / 360.);
    ASSERT_NEAR(value.quoteJacobian_(0, 0) / independent, 1., 1e-3);
}

TEST(ScriptCurveCalibrationTest, TestNonuniformCoordinateScalesPreserveMetricUnderLegacyBudget) {
    for (const auto mode : {CurveSolveMode_::Value_::EXACT, CurveSolveMode_::Value_::APPROXIMATE}) {
        SCOPED_TRACE(mode == CurveSolveMode_::Value_::EXACT ? "EXACT" : "APPROXIMATE");
        auto spec = DepositSpec();
        const Date_ second(2028, 10, 23);
        spec.geometry_.knotDates_.push_back(second);
        spec.solver_.parameterScales_ = {.1, .3};
        spec.solver_.smoothingWeight_ = .1;
        spec.solver_.solveMode_ = mode;
        spec.solver_.fitWeight_ = 1.;
        spec.solver_.fitNormTolerance_ = .1;
        const auto instrument =
            NewDepositScriptInstrument(Deposit_(TODAY, second, .025, DayBasis_("ACT_360")), {"second", Ccy_("USD"), "USD.OIS", 100.});
        const auto quote = NewCalibrationMarketQuote("q2", .04, CalibrationValueUnit_::Parse("Rate"));
        spec.observations_.push_back(NewCalibrationObservation("o2", instrument, "ParRate", quote, 1e-10));
        const auto checkMetric = [](const CurveInstrumentCalibrationResult_& result) {
            ASSERT_EQ(result.parameterScales_, (Vector_<>{.1, .3}));
            ASSERT_DOUBLE_EQ(result.nativeWeights_(0, 0), .2);
            ASSERT_DOUBLE_EQ(result.nativeWeights_(0, 1), -.1);
            ASSERT_DOUBLE_EQ(result.nativeWeights_(1, 1), .2);
            ASSERT_DOUBLE_EQ(result.weights_(0, 1), result.weights_(1, 0));
            ASSERT_NEAR(result.weights_(0, 0), .002, 1e-18);
            ASSERT_NEAR(result.weights_(0, 1), -.003, 1e-18);
            ASSERT_NEAR(result.weights_(1, 1), .018, 1e-17);
        };
        LegacySolveResult_ expected;
        bool converged = false;
        try {
            expected = SolveLegacyDeposit(spec);
            converged = expected.fitWithinTolerance_;
        } catch (const Underdetermined::ConvergenceError_&) {
        }
        if (converged) {
            const auto actual = CalibrateCurveInstruments(spec);
            ASSERT_TRUE(actual.fitWithinTolerance_);
            ASSERT_EQ(actual.coordinates_.size(), expected.coordinates_.size());
            ASSERT_EQ(actual.rawResiduals_.size(), expected.residuals_.size());
            for (size_t i = 0; i < actual.coordinates_.size(); ++i)
                ASSERT_NEAR(actual.coordinates_[i], expected.coordinates_[i], 1e-12);
            for (size_t i = 0; i < actual.rawResiduals_.size(); ++i)
                ASSERT_NEAR(actual.rawResiduals_[i], expected.residuals_[i], 1e-12);
            ASSERT_EQ(actual.evaluations_, expected.evaluations_);
            ASSERT_EQ(actual.gradients_, expected.gradients_);
            checkMetric(actual);
        } else {
            std::optional<CalibrationError_> saved;
            try {
                static_cast<void>(CalibrateCurveInstruments(spec));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
            ASSERT_TRUE(saved);
            ASSERT_EQ(saved->Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
            const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(saved->Failure().candidate_.get());
            ASSERT_NE(candidate, nullptr);
            ASSERT_FALSE(candidate->Result().fitWithinTolerance_);
            checkMetric(candidate->Result());
        }
    }
}

namespace {
    template <class T_> void AssertForwardFailure() {
        std::optional<CalibrationError_> saved;
        const Date_ far(2126, 10, 21);
        const long double accrual = (far - TODAY) / 365.L;
        const double independent = static_cast<double>(std::exp(714.L - std::log(accrual)) - 1.L / accrual);
        ASSERT_TRUE(std::isfinite(independent));
        ASSERT_GT(independent, 1e308);
        {
            TapeGuard_ guard(AAD::Tape());
            CurveCalibrationSpec_ geometry;
            geometry.today_ = TODAY;
            geometry.ccy_ = "USD";
            geometry.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
            geometry.knotDates_ = {TODAY, far};
            geometry.calibrateDiscountCurve_ = false;
            geometry.targetTenor_ = PeriodLength_("3M");
            const CandidateCurveProvider_<T_> provider(geometry, {{"projection", true, CollateralType_("OIS"), PeriodLength_("3M")}}, {T_(-714.)});
            try {
                static_cast<void>(provider.Forward("projection", TODAY, far, DayBasis::Act365F()));
            } catch (const CalibrationError_& error) {
                saved = error;
            }
        }
        ASSERT_TRUE(saved);
        const auto& f = saved->Failure();
        ASSERT_EQ(f.diagnostic_.code_, "CalibrationNumericalFailure");
        ASSERT_EQ(f.diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(f.diagnostic_.field_, std::optional<String_>("/routes/projection/forward"));
        ASSERT_TRUE(f.report_);
        ASSERT_EQ(f.report_->Stage(), CalibrationStage_::Value_::Evaluate);
        ASSERT_EQ(f.report_->Errors().size(), 1);
    }
} // namespace

TEST(ScriptCurveCalibrationTest, TestForwardCannotReturnInfiniteIntermediate) {
    AssertForwardFailure<double>();
    AssertForwardFailure<AAD::Number_>();
}

TEST(ScriptCurveCalibrationTest, TestEvaluateFailureOwnsAxesAndNativeLayout) {
    std::optional<CalibrationError_> retained;
    {
        const PreparedCurveInstruments_ plan(DepositSpec());
        try {
            static_cast<void>(plan.Evaluate({std::numeric_limits<double>::quiet_NaN()}));
        } catch (const CalibrationError_& error) {
            retained = error;
        }
    }
    ASSERT_TRUE(retained);
    const auto& failure = retained->Failure();
    ASSERT_EQ(failure.diagnostic_.stage_, CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(failure.diagnostic_.observationId_, std::optional<String_>("o"));
    ASSERT_EQ(failure.diagnostic_.parameterKey_, std::optional<String_>("LOG_DISCOUNT_FACTOR:2027-10-21"));
    ASSERT_EQ(failure.report_->Axes().quoteIndex_, Vector_<int>({0}));
    const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(failure.candidate_.get());
    ASSERT_NE(candidate, nullptr);
    ASSERT_EQ(candidate->Result().freeParameters_.size(), 1u);
    ASSERT_EQ(candidate->Result().freeParameters_[0].date_, MATURITY);
    ASSERT_TRUE(candidate->Result().parameters_.empty());
}

TEST(ScriptCurveCalibrationTest, TestFourNativeLayoutsAndAadAgainstIndependentIntegral) {
    const Date_ second(2028, 10, 21);
    const double t2 = (second - TODAY) / 365.;
    const Vector_<CurveParameterization_> modes = {CurveParameterization_::Value_::LOG_DISCOUNT, CurveParameterization_::Value_::ZERO_RATE,
                                                   CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                                   CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD};
    const Vector_<Vector_<>> parameters = {{-.02, -.06}, {.02, .06 / t2}, {.02, .04}, {.02, .03, .04, .05}};
    for (size_t m = 0; m < modes.size(); ++m) {
        auto spec = DepositSpec();
        spec.geometry_.parameterization_ = modes[m];
        spec.geometry_.knotDates_ = m == 0 ? Vector_<Date_>({TODAY, MATURITY, second}) : Vector_<Date_>({MATURITY, second});
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto aad = PreparedCurveInstruments_(spec, options).Evaluate(parameters[m]);
        const auto fd = PreparedCurveInstruments_(spec).Evaluate(parameters[m]);
        ASSERT_EQ(aad.quoteJacobian_.Cols(), static_cast<int>(parameters[m].size()));
        const double alpha = 365. / 360.;
        ASSERT_NEAR(aad.modelMeasures_[0], std::expm1(.02) / alpha, 1e-12);
        for (int j = 0; j < aad.quoteJacobian_.Cols(); ++j)
            ASSERT_NEAR(aad.quoteJacobian_(0, j), fd.quoteJacobian_(0, j), 1e-8);
        ASSERT_NEAR(aad.quoteJacobian_(0, 0), (m == 0 ? -1. : 1.) * std::exp(.02) / alpha, 1e-10);
        if (m == 3) {
            ASSERT_EQ(aad.freeParameters_[0].component_, CurveFreeParameterComponent_::Value_::LEFT_FORWARD);
            ASSERT_EQ(aad.freeParameters_[1].component_, CurveFreeParameterComponent_::Value_::RIGHT_FORWARD);
            ASSERT_EQ(aad.freeParameters_[2].component_, CurveFreeParameterComponent_::Value_::LEFT_FORWARD);
            ASSERT_NEAR(aad.quoteJacobian_(0, 1), 0., 1e-12);
        }
    }
}

TEST(ScriptCurveCalibrationTest, TestExplicitBaseDiscountProjectionAndBackgroundRoutes) {
    CurveCalibrationSpec_ geometry;
    geometry.today_ = TODAY;
    geometry.ccy_ = "USD";
    geometry.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    geometry.knotDates_ = {MATURITY};
    geometry.baseCurve_.reset(
        NewDiscountZeroRate("base", "USD", TODAY, {MATURITY}, {.02}, DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR")).release());
    geometry.calibrateDiscountCurve_ = false;
    geometry.targetTenor_ = PeriodLength_("3M");
    geometry.discountCurves_[CollateralType_("OIS")] = geometry.baseCurve_;
    geometry.forwardCurves_[PeriodLength_("6M")] = geometry.baseCurve_;
    const Vector_<CurveScriptRoute_> routes = {
        {"discount"}, {"three", true, CollateralType_("OIS"), PeriodLength_("3M")}, {"six", true, CollateralType_("OIS"), PeriodLength_("6M")}};
    const CandidateCurveProvider_<double> provider(geometry, routes, {.01});
    ASSERT_NEAR(provider.Discount("discount", TODAY, MATURITY), std::exp(-.02), 1e-12);
    ASSERT_NEAR(provider.Forward("three", TODAY, MATURITY, DayBasis::Act365F()), std::expm1(.03), 1e-12);
    ASSERT_NEAR(provider.Forward("six", TODAY, MATURITY, DayBasis::Act365F()), std::expm1(.02), 1e-12);
    ASSERT_THROW(static_cast<void>(provider.Discount("three", TODAY, MATURITY)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(provider.Discount("missing", TODAY, MATURITY)), CalibrationError_);
    ASSERT_THROW(static_cast<void>(provider.Discount("discount", Date_(2025, 10, 21), MATURITY)), CalibrationError_);
    auto* tape = AAD::Tape();
    TapeGuard_ guard(tape);
    auto parameters = RegisterCurveParameters({.01});
    guard.recording_.StartRecording();
    const CandidateCurveProvider_<AAD::Number_> active(geometry, routes, parameters);
    Vector_<AAD::Number_> values = {active.Forward("three", TODAY, MATURITY, DayBasis::Act365F()),
                                    active.Forward("six", TODAY, MATURITY, DayBasis::Act365F()), active.Discount("discount", TODAY, MATURITY)};
    guard.recording_.FinishRecording();
    const auto jacobian = HarvestCurveJacobian(*tape, parameters, values);
    ASSERT_NEAR(jacobian(0, 0), std::exp(.03), 1e-10);
    ASSERT_DOUBLE_EQ(jacobian(1, 0), 0.);
    ASSERT_DOUBLE_EQ(jacobian(2, 0), 0.);
    guard.Close();
}

TEST(ScriptCurveCalibrationTest, TestInitialBoundaryAndDuplicateQuoteL2BudgetFailure) {
    auto spec = DepositSpec();
    spec.solver_.initialGuessPerNode_ = {-.04};
    const double residual = PreparedCurveInstruments_(spec).Evaluate({-.04}, false).rawResiduals_[0];
    const auto row = spec.observations_[0];
    spec.observations_[0] = NewCalibrationObservation("o", row->Instrument(), "ParRate", row->Quote(), residual);
    spec.solver_.maxEvaluations_ = 1;
    const auto boundary = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(boundary.fitWithinTolerance_);
    ASSERT_DOUBLE_EQ(boundary.maxAbsScaledResidual_, 1.);
    ASSERT_EQ(boundary.evaluations_, 1);
    ASSERT_EQ(boundary.gradients_, 0);
    ASSERT_EQ(boundary.derivativeMethod_, CalibrationDerivative_::Value_::None);
    spec.observations_ = {NewCalibrationObservation("a", row->Instrument(), "ParRate", row->Quote(), residual / .8),
                          NewCalibrationObservation("b", row->Instrument(), "ParRate", row->Quote(), residual / .8)};
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 10.;
    spec.solver_.fitNormTolerance_ = 1.1;
    try {
        static_cast<void>(CalibrateCurveInstruments(spec));
        FAIL() << "L2 fit must fail despite RMS and each row being below eta";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(error.Failure().candidate_.get());
        ASSERT_NE(candidate, nullptr);
        const auto& result = candidate->Result();
        ASSERT_EQ(result.axes_.quoteIndex_, Vector_<int>({0, 0}));
        ASSERT_EQ(result.axes_.quoteMap_.Rows(), 2);
        ASSERT_EQ(result.axes_.quoteMap_.Cols(), 1);
        ASSERT_NEAR(result.scaledNorm_, std::sqrt(2.) * .8, 1e-12);
        ASSERT_DOUBLE_EQ(*result.fitWeight_, 10.);
        ASSERT_DOUBLE_EQ(*result.fitNormTolerance_, 1.1);
        ASSERT_EQ(result.evaluations_, 1);
        ASSERT_FALSE(result.fitWithinTolerance_);
    }
}

TEST(ScriptCurveCalibrationTest, TestMixedPriceRateUsesSharedApproximateSolver) {
    auto spec = DepositSpec();
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 1e6;
    spec.solver_.fitNormTolerance_ = .01;
    const auto rateRow = spec.observations_[0];
    for (double multiplier : {1., 10000.}) {
        const auto instrument =
            BindCalibrationInstrumentParameters(rateRow->Instrument(), {{{"N", 100. * multiplier, CalibrationValueUnit_::Parse("Price:USD")}}, {}});
        const double pv = -100. + 100. * (1. + .025 * 365. / 360.) / (1. + .035 * 365. / 360.);
        const auto price = NewCalibrationMarketQuote("pv", pv * multiplier, CalibrationValueUnit_::Parse("Price:USD"));
        spec.observations_ = {NewCalibrationObservation("r", instrument, "ParRate", rateRow->Quote(), 1e-3),
                              NewCalibrationObservation("p", instrument, "Price", price, .1 * multiplier)};
        spec.solver_.parameterScales_ = {multiplier == 1. ? 1. : .25};
        const auto result = CalibrateCurveInstruments(spec);
        ASSERT_TRUE(result.fitWithinTolerance_);
        ASSERT_EQ(result.axes_.quoteIndex_, Vector_<int>({0, 1}));
        ASSERT_DOUBLE_EQ(result.nativeWeights_(0, 0), 1.);
        const double scale = result.parameterScales_[0];
        ASSERT_DOUBLE_EQ(result.weights_(0, 0), scale * scale);
        ASSERT_DOUBLE_EQ(*result.fitWeight_, 1e6);
        ASSERT_NEAR(result.parameters_[0], MixedNormalOracle(spec), 1e-10);
    }
}

TEST(ScriptCurveCalibrationTest, TestPreparationWrapsNativeGeometryErrors) {
    auto spec = DepositSpec();
    spec.geometry_.knotDates_.push_back(TODAY);
    try {
        const PreparedCurveInstruments_ plan(spec);
        FAIL() << "Nonmonotonic geometry must fail with an owned calibration report";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Prepare);
        ASSERT_EQ(error.Failure().report_->Axes().observations_[0]->Id(), "o");
    }
}

TEST(ScriptCurveCalibrationTest, TestLegacyImporterKeepsUnfittedLastPointBehavior) {
    CurveCalibrationSpec_ old;
    old.today_ = TODAY;
    old.ccy_ = "USD";
    old.knotDates_ = {MATURITY};
    old.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    old.instruments_ = {Handle_<YCInstrument_>(new Deposit_(TODAY, MATURITY, .035, DayBasis::Act365F()))};
    old.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    old.maxEvaluations_ = 1;
    const auto imported = ImportLegacyCurveObjective(old);
    const auto direct = CalibrateYieldCurve(old);
    const auto compatible = CalibrateLegacyCurve(imported);
    ASSERT_TRUE(imported.fitWeight_);
    ASSERT_DOUBLE_EQ(*imported.fitWeight_, 1e-4);
    ASSERT_DOUBLE_EQ(imported.fitNormTolerance_, old.fitTolerance_);
    ASSERT_DOUBLE_EQ((*compatible.curve_)(TODAY, MATURITY), (*direct.curve_)(TODAY, MATURITY));
    ASSERT_DOUBLE_EQ(compatible.diagnostics_.modelRates_[0], direct.diagnostics_.modelRates_[0]);
    ASSERT_GT(compatible.diagnostics_.maxAbsResidual_, old.tolerance_);
    auto spec = DepositSpec();
    spec.solver_.maxEvaluations_ = 1;
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = imported.fitWeight_;
    spec.solver_.fitNormTolerance_ = imported.fitNormTolerance_;
    ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(spec)), CalibrationError_);
}

TEST(ScriptCurveCalibrationTest, TestMissingRoutesObjectiveAndSnapshotAreExplicitFailures) {
    auto spec = DepositSpec();
    {
        auto missing = spec;
        missing.valuation_.fixings_.reset();
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(missing)), CalibrationError_);
    }
    {
        auto missing = spec;
        missing.market_.routes_[0].id_ = "different";
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(missing)), CalibrationError_);
    }
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    try {
        static_cast<void>(CalibrateCurveInstruments(spec));
        FAIL() << "No implicit mixed-unit fit weight";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "InvalidCalibrationObjective");
        ASSERT_EQ(error.Failure().diagnostic_.field_, std::optional<String_>("/solver/fit_weight"));
    }
}

TEST(ScriptCurveCalibrationTest, TestNumericalTrialFailureRetainsInitialValidatedCandidate) {
    auto spec = DepositSpec();
    CurveInstrumentCalibrationOptions_ options;
    options.differenceStep_ = 1e300;
    std::optional<CalibrationError_> retained;
    try {
        static_cast<void>(CalibrateCurveInstruments(spec, options));
    } catch (const CalibrationError_& error) {
        retained = error;
    }
    ASSERT_TRUE(retained);
    ASSERT_EQ(retained->Failure().diagnostic_.stage_, CalibrationStage_::Value_::Solve);
    const auto* candidate = dynamic_cast<const CurveInstrumentCalibrationCandidate_*>(retained->Failure().candidate_.get());
    ASSERT_NE(candidate, nullptr);
    ASSERT_EQ(candidate->Result().parameters_, Vector_<>({-.05}));
    ASSERT_EQ(candidate->Result().axes_.quoteIndex_, Vector_<int>({0}));
    ASSERT_EQ(candidate->Result().evaluations_, 2);
    ASSERT_EQ(candidate->Result().gradients_, 1);
    ASSERT_FALSE(candidate->Result().fitWithinTolerance_);
}

TEST(ScriptCurveCalibrationTest, TestPlanIdentityIncludesNativeGeometryAndTodayPolicy) {
    auto spec = DepositSpec();
    spec.geometry_.knotDates_.push_back(Date_(2028, 10, 21));
    const PreparedCurveInstruments_ plan(spec);
    const auto value = plan.Evaluate({-.04, -.08}, false), moved = plan.Evaluate({-.03, -.08}, false);
    ASSERT_EQ(value.planIdentity_, moved.planIdentity_);
    ASSERT_NE(value.candidateIdentity_, moved.candidateIdentity_);
    spec.geometry_.logDfScheme_ = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;
    const auto cubic = PreparedCurveInstruments_(spec).Evaluate({-.04, -.08}, false);
    ASSERT_NE(cubic.planIdentity_, value.planIdentity_);
    ASSERT_NEAR(cubic.modelMeasures_[0], value.modelMeasures_[0], 1e-12);
    spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
    const auto historical = PreparedCurveInstruments_(spec).Evaluate({-.04, -.08}, false);
    ASSERT_NE(historical.planIdentity_, cubic.planIdentity_);
    ASSERT_NEAR(historical.modelMeasures_[0], cubic.modelMeasures_[0], 1e-12);
}

TEST(ScriptCurveCalibrationTest, TestLegacyReturnSurvivesUnrepresentableNewObjectiveMapping) {
    CurveCalibrationSpec_ native;
    native.today_ = TODAY;
    native.ccy_ = "USD";
    native.knotDates_ = {MATURITY};
    native.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    native.instruments_ = {Handle_<YCInstrument_>(new Deposit_(TODAY, MATURITY, .035, DayBasis::Act365F()))};
    native.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    native.fitTolerance_ = 1e200;
    const auto imported = ImportLegacyCurveObjective(native);
    ASSERT_FALSE(imported.fitWeight_);
    const auto result = CalibrateLegacyCurve(imported);
    ASSERT_DOUBLE_EQ((*result.curve_)(TODAY, MATURITY), std::exp(-native.initialGuess_));
}

TEST(ScriptCurveCalibrationTest, TestDiscountBindingCannotChangeContractCollateral) {
    auto spec = DepositSpec();
    spec.market_.routes_[0].collateral_ = CollateralType_("GC");
    spec.market_.discountCurves_[CollateralType_("GC")].reset(
        NewDiscountZeroRate("other", "USD", TODAY, {MATURITY}, {.02}, DayBasis::Act365F(), LogDfScheme_("LOG_LINEAR")).release());
    ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(spec)), CalibrationError_);
}

TEST(ScriptCurveCalibrationTest, TestPriceAndRatePwlPartialsInInterleavedOrder) {
    auto spec = DepositSpec();
    const Date_ second(2028, 10, 23);
    const double elapsed = (second - MATURITY) / 365., alpha = (second - TODAY) / 360.;
    spec.geometry_.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    spec.geometry_.knotDates_ = {MATURITY, second};
    const auto instrument =
        NewDepositScriptInstrument(Deposit_(TODAY, second, .025, DayBasis_("ACT_360")), {"two-year", Ccy_("USD"), "USD.OIS", 100.});
    spec.observations_ = {
        NewCalibrationObservation("r", instrument, "ParRate", NewCalibrationMarketQuote("r", .035, CalibrationValueUnit_::Parse("Rate")), 1e-8),
        NewCalibrationObservation("p", instrument, "Price", NewCalibrationMarketQuote("p", 0., CalibrationValueUnit_::Parse("Price:USD")), 1e-4)};
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    const auto value = PreparedCurveInstruments_(spec, options).Evaluate({.02, .03, .04, .05});
    const double integral = .02 + .5 * (.03 + .04) * elapsed, df = std::exp(-integral);
    ASSERT_NEAR(value.modelMeasures_[0], std::expm1(integral) / alpha, 1e-12);
    ASSERT_NEAR(value.modelMeasures_[1], -100. + 100. * (1. + .025 * alpha) * df, 1e-12);
    const Vector_<> exposures = {1., .5 * elapsed, .5 * elapsed, 0.};
    for (int j = 0; j < 4; ++j) {
        ASSERT_NEAR(value.quoteJacobian_(0, j), std::exp(integral) * exposures[j] / alpha, 1e-10);
        ASSERT_NEAR(value.quoteJacobian_(1, j), -100. * (1. + .025 * alpha) * df * exposures[j], 1e-10);
    }
    options.evaluation_ = CalibrationEvaluation_::Value_::GenericDeterministic;
    const auto generic = PreparedCurveInstruments_(spec, options).Evaluate({.02, .03, .04, .05});
    ASSERT_EQ(generic.capabilities_[0].backend_, CalibrationBackend_::Value_::GenericDeterministic);
    ASSERT_EQ(generic.modelMeasures_, value.modelMeasures_);
    ASSERT_EQ(generic.quoteJacobian_(1, 1), value.quoteJacobian_(1, 1));
}

TEST(ScriptCurveCalibrationTest, TestAllLogSchemesAtInteriorDatesMatchFdAad) {
    const Date_ middle(2028, 4, 21), second(2028, 10, 21), third(2029, 10, 21);
    for (const auto& mode : {CurveParameterization_("LOG_DISCOUNT"), CurveParameterization_("ZERO_RATE")}) {
        for (const auto& scheme : LogDfSchemeListAll()) {
            auto spec = DepositSpec();
            spec.geometry_.parameterization_ = mode;
            spec.geometry_.logDfScheme_ = scheme;
            spec.geometry_.knotDates_ = mode == CurveParameterization_::Value_::LOG_DISCOUNT ? Vector_<Date_>({TODAY, MATURITY, second, third})
                                                                                             : Vector_<Date_>({MATURITY, second, third});
            const auto instrument =
                NewDepositScriptInstrument(Deposit_(TODAY, middle, .025, DayBasis_("ACT_360")), {"interior", Ccy_("USD"), "USD.OIS", 100.});
            spec.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", spec.observations_[0]->Quote(), 1e-8);
            CurveInstrumentCalibrationOptions_ options;
            options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
            const Vector_<> theta =
                mode == CurveParameterization_::Value_::LOG_DISCOUNT ? Vector_<>({-.02, -.045, -.08}) : Vector_<>({.02, .0225, .026});
            const auto aad = PreparedCurveInstruments_(spec, options).Evaluate(theta), fd = PreparedCurveInstruments_(spec).Evaluate(theta);
            ASSERT_NEAR(aad.modelMeasures_[0], fd.modelMeasures_[0], 1e-12);
            for (int j = 0; j < 3; ++j)
                ASSERT_NEAR(aad.quoteJacobian_(0, j), fd.quoteJacobian_(0, j), 1e-8);
        }
    }
}

TEST(ScriptCurveCalibrationTest, TestObservationOrderAndPillarKnotPlanning) {
    auto spec = DepositSpec();
    const Date_ second(2028, 10, 23);
    const auto instrument =
        NewDepositScriptInstrument(Deposit_(TODAY, second, .025, DayBasis_("ACT_360")), {"two-year", Ccy_("USD"), "USD.OIS", 100.});
    const auto quote = NewCalibrationMarketQuote("two-year-q", .04, CalibrationValueUnit_::Parse("Rate"));
    spec.observations_ = {NewCalibrationObservation("first", instrument, "ParRate", quote, 1e-10), spec.observations_[0]};
    spec.geometry_.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
    spec.geometry_.knotDates_ = {TODAY};
    const auto fit = CalibrateCurveInstruments(spec);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_EQ(fit.axes_.observations_[0]->Id(), "first");
    ASSERT_EQ(fit.axes_.quotes_[0]->Id(), "two-year-q");
    ASSERT_EQ(fit.freeParameters_[0].date_, MATURITY);
    ASSERT_EQ(fit.freeParameters_[1].date_, second);
    ASSERT_NEAR(fit.parameters_[0], -std::log1p(.035 * 365. / 360.), 1e-10);
    ASSERT_NEAR(fit.parameters_[1], -std::log1p(.04 * (second - TODAY) / 360.), 1e-10);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(0, 0), 2.);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(0, 1), -1.);
    ASSERT_DOUBLE_EQ(fit.nativeWeights_(1, 1), 2.);
    spec.geometry_.knotPolicy_ = CurveKnotPolicy_::Value_::AUGMENTED;
    const auto augmented = PreparedCurveInstruments_(spec).Describe();
    ASSERT_EQ(augmented.definition_.nodeDates_, fit.definition_.nodeDates_);
}

TEST(ScriptCurveCalibrationTest, TestOrderedStageInheritanceAndProjectionInitialFit) {
    MultiCurveInstrumentCalibrationSpec_ bundle;
    bundle.stages_ = {DepositSpec(), DepositSpec()};
    bundle.stages_[1].geometry_.calibrateDiscountCurve_ = false;
    bundle.stages_[1].geometry_.targetTenor_ = PeriodLength_("3M");
    bundle.stages_[1].solver_.initialGuessPerNode_ = {0.};
    const auto fit = CalibrateCurveInstruments(bundle);
    ASSERT_EQ(fit.stages_.size(), 2u);
    ASSERT_TRUE(fit.stages_[0].fitWithinTolerance_);
    ASSERT_TRUE(fit.stages_[1].fitWithinTolerance_);
    ASSERT_EQ(fit.stages_[1].gradients_, 0);
    ASSERT_EQ(fit.stages_[1].resolvedSpec_->geometry_.baseCurve_, fit.stages_[0].curve_);
    ASSERT_EQ(fit.stages_[1].resolvedSpec_->market_.discountCurves_.at(CollateralType_("OIS")), fit.stages_[0].curve_);
    ASSERT_DOUBLE_EQ((*fit.forwardCurves_.at(PeriodLength_("3M")))(TODAY, MATURITY), (*fit.stages_[0].curve_)(TODAY, MATURITY));
}

TEST(ScriptCurveCalibrationTest, TestOrderedStagesRejectDifferentCurrencyOrValuation) {
    MultiCurveInstrumentCalibrationSpec_ bundle;
    bundle.stages_ = {DepositSpec(), DepositSpec()};
    {
        auto& stage = bundle.stages_[1];
        stage.geometry_.ccy_ = "EUR";
        stage.market_.routes_ = {{"EUR.OIS"}};
        const auto instrument =
            NewDepositScriptInstrument(Deposit_(TODAY, MATURITY, .025, DayBasis_("ACT_360")), {"eur-deposit", Ccy_("EUR"), "EUR.OIS", 100.});
        stage.observations_[0] = NewCalibrationObservation("eur", instrument, "ParRate", stage.observations_[0]->Quote(), 1e-10);
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(bundle)), CalibrationError_);
    }
    {
        auto& stage = bundle.stages_[1];
        stage = DepositSpec();
        stage.geometry_.today_ = TODAY.AddDays(1);
        stage.geometry_.knotDates_ = {TODAY.AddDays(1), MATURITY.AddDays(1)};
        stage.valuation_.evaluationDate_ = stage.geometry_.today_;
        const auto instrument = NewDepositScriptInstrument(Deposit_(TODAY.AddDays(1), MATURITY.AddDays(1), .025, DayBasis_("ACT_360")),
                                                           {"later-deposit", Ccy_("USD"), "USD.OIS", 100.});
        stage.observations_[0] = NewCalibrationObservation("later", instrument, "ParRate", stage.observations_[0]->Quote(), 1e-10);
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(bundle)), CalibrationError_);
    }
}

TEST(ScriptCurveCalibrationTest, TestDuplicateQuoteFitsAndConflictingRowsStayUnfitted) {
    auto spec = DepositSpec();
    const auto row = spec.observations_[0];
    spec.observations_.push_back(NewCalibrationObservation("second", row->Instrument(), "ParRate", row->Quote(), 1e-10));
    CurveInstrumentCalibrationOptions_ options;
    options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
    options.computeQuoteJacobian_ = true;
    const auto exact = CalibrateCurveInstruments(spec, options);
    ASSERT_TRUE(exact.fitWithinTolerance_);
    ASSERT_EQ(exact.axes_.quoteIndex_, Vector_<int>({0, 0}));
    ASSERT_NEAR(exact.parameters_[0], -std::log1p(.035 * 365. / 360.), 1e-10);
    spec.solver_.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;
    spec.solver_.fitWeight_ = 1e6;
    spec.solver_.fitNormTolerance_ = 1.;
    const auto fit = CalibrateCurveInstruments(spec, options);
    ASSERT_EQ(fit.axes_.quoteIndex_, Vector_<int>({0, 0}));
    ASSERT_EQ(fit.quoteJacobian_.Rows(), 2);
    ASSERT_EQ(fit.quoteJacobian_.Cols(), 1);
    ASSERT_NEAR(fit.quoteJacobian_(0, 0), -std::exp(-fit.parameters_[0]) / (365. / 360.), 1e-10);
    ASSERT_DOUBLE_EQ(fit.quoteJacobian_(0, 0), fit.quoteJacobian_(1, 0));
    ASSERT_EQ(fit.diagnosticGradients_, 1);
    ASSERT_FALSE(fit.report_->QuoteRiskAvailable());
    const auto other = NewCalibrationMarketQuote("other", .045, CalibrationValueUnit_::Parse("Rate"));
    spec.observations_[1] = NewCalibrationObservation("second", row->Instrument(), "ParRate", other, 1e-10);
    spec.solver_.maxRestarts_ = 2;
    ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(spec, options)), CalibrationError_);
    spec.observations_[1] = spec.observations_[0];
    ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(spec, options)), CalibrationError_);
}

TEST(ScriptCurveCalibrationTest, TestUnderflowingMetricCannotPublishSuccessfulInitialFit) {
    auto spec = DepositSpec();
    const auto row = spec.observations_[0];
    const double target = std::expm1(.05) / (365. / 360.);
    spec.observations_[0] = NewCalibrationObservation("o", row->Instrument(), "ParRate",
                                                      NewCalibrationMarketQuote("q", target, CalibrationValueUnit_::Parse("Rate")), 1e-8);
    spec.solver_.parameterScales_ = {1e-200};
    ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(spec)), CalibrationError_);
}

TEST(ScriptCurveCalibrationTest, TestInvalidAnchorsPillarsAndDerivativeHaveOwnedErrors) {
    {
        auto spec = DepositSpec();
        spec.geometry_.knotDates_ = {MATURITY};
        ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(spec)), CalibrationError_);
    }
    {
        auto spec = DepositSpec();
        spec.geometry_.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
        ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(spec)), CalibrationError_);
    }
    {
        auto spec = DepositSpec();
        const auto row = spec.observations_[0];
        const auto instrument = NewScriptCalibrationInstrument({"early-pillar", row->Instrument()->Definition(), {}, TODAY});
        spec.observations_[0] = NewCalibrationObservation("o", instrument, "ParRate", row->Quote(), 1e-8);
        ASSERT_THROW(static_cast<void>(PreparedCurveInstruments_(spec)), CalibrationError_);
    }
    {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_::Value_::Analytic;
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(DepositSpec(), options)), CalibrationError_);
        options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
        ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(DepositSpec(), options)), CalibrationError_);
    }
}

TEST(ScriptCurveCalibrationTest, TestFitReportRetainsModelValueWhenRawResidualLosesDigits) {
    auto spec = DepositSpec();
    const auto row = spec.observations_[0];
    const auto quote = NewCalibrationMarketQuote("huge-price", 1e16, CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_ = {NewCalibrationObservation("p", row->Instrument(), "Price", quote, 2e16)};
    spec.solver_.maxEvaluations_ = 1;
    const auto fit = CalibrateCurveInstruments(spec);
    const double model = -100. + 100. * (1. + .025 * 365. / 360.) * std::exp(-.05);
    ASSERT_TRUE(fit.fitWithinTolerance_);
    ASSERT_NEAR(fit.modelMeasures_[0], model, 1e-12);
    ASSERT_DOUBLE_EQ(fit.rawResiduals_[0], model - 1e16);
    ASSERT_EQ(fit.evaluations_, 1);
}

TEST(ScriptCurveCalibrationTest, TestUnrepresentableRawNormCannotPublishSuccessfulFit) {
    auto spec = DepositSpec();
    const auto row = spec.observations_[0];
    const auto quote = NewCalibrationMarketQuote("large", -1e308, CalibrationValueUnit_::Parse("Price:USD"));
    spec.observations_ = {NewCalibrationObservation("a", row->Instrument(), "Price", quote, 1e308),
                          NewCalibrationObservation("b", row->Instrument(), "Price", quote, 1e308),
                          NewCalibrationObservation("c", row->Instrument(), "Price", quote, 1e308),
                          NewCalibrationObservation("d", row->Instrument(), "Price", quote, 1e308)};
    ASSERT_THROW(static_cast<void>(CalibrateCurveInstruments(spec)), CalibrationError_);
}
