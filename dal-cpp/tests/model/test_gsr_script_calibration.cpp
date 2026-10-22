#include <gtest/gtest.h>

#include <dal/math/aad/recording.hpp>
#include <dal/model/gsrbondoption.hpp>
#include <dal/model/gsrscriptcalibration.hpp>
#include <dal/time/dateutils.hpp>

using namespace Dal;

namespace {
    CalibrationPreparationContext_ FitContext(double first = 0.012, double second = 0.016) {
        CalibrationPreparationContext_ c;
        const Date_ v(2026, 10, 21), k(2027, 10, 21);
        c.currency_ = Ccy_("USD");
        c.discountRoute_ = "USD.OIS";
        c.valuation_.evaluationDate_ = v;
        c.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        c.curve_.reset(new GSRCurveData_("curve", v, "USD", {v, k, Date_(2028, 10, 21), Date_(2029, 10, 21)}, {0., -.04, -.08, -.12}, {}, {}));
        Matrix_<> g(1, 2);
        g(0, 0) = first;
        g(0, 1) = second;
        Handle_<MultiFactorGSRVolData_> vol(new MultiFactorGSRVolData_("vol", {"level"}, {v, k}, g, {v}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
        c.model_.reset(new MultiFactorGSRModelData_("model", c.curve_, vol));
        return c;
    }
    GSRScriptCalibrationSettings_ FitSettings() {
        GSRScriptCalibrationSettings_ s;
        s.sampling_.allowMonteCarloCalibration_ = true;
        s.sampling_.fitBatches_ = 2;
        s.sampling_.fitPathsPerBatch_ = 64;
        s.sampling_.validationBatches_ = 4;
        s.sampling_.validationPathsPerBatch_ = 128;
        s.sampling_.heldOutBatches_ = 4;
        s.sampling_.heldOutPathsPerBatch_ = 128;
        s.sampling_.maxPathsPerView_ = 1152;
        s.optimizer_.gradientTolerance_ = 1e-10;
        return s;
    }
    Vector_<GSRCalibrationParameter_> FitParameters() { return {{0, 1, .001, .06}, {0, 0, .001, .06}}; }
    Vector_<Handle_<CalibrationObservation_>> FitRows(const GSRScriptCalibrationSettings_& settings, double scale = .1) {
        const auto targetContext = FitContext(.02, .025);
        Vector_<Handle_<CalibrationObservation_>> rows;
        for (int i = 0; i < 2; ++i) {
            const String_ id = i == 0 ? "short" : "long";
            const auto instrument = NewBondOptionScriptInstrument(
                BondOption_{Date_(2027 + i, 10, 21), Date_(2028 + i, 10, 21), .97, OptionType_("CALL")}, {id, Ccy_("USD"), "USD.OIS", 100.});
            auto mc = settings.sampling_;
            mc.aad_ = false;
            const auto target = PrepareCalibrationMc(instrument, "Price", targetContext, mc).Evaluate(instrument, targetContext, false).mean_;
            rows.push_back(NewCalibrationObservation(id, instrument, "Price",
                                                     NewCalibrationMarketQuote(id, target, CalibrationValueUnit_::Parse("Price:USD")), scale));
        }
        return rows;
    }
} // namespace

TEST(GSRScriptCalibrationTest, TestSelectedGRecoveryAndFullRawJacobian) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_NEAR(result.parameters_[0], .025, 1e-8);
    ASSERT_NEAR(result.parameters_[1], .02, 1e-8);
    ASSERT_EQ(result.quoteJacobian_.Rows(), 2);
    ASSERT_EQ(result.quoteJacobian_.Cols(), 2);
    ASSERT_EQ(result.jacobianRank_, 2);
    ASSERT_EQ(result.parameterLabels_[0], "g:level:2027-10-21");
    ASSERT_DOUBLE_EQ(result.quoteJacobian_(0, 0), 0.);
    ASSERT_GT(result.quoteJacobian_(0, 1), 1.);
    ASSERT_EQ(result.derivativeMethod_, CalibrationDerivative_::Value_::AAD);
    ASSERT_GT(result.costs_.aadRows_, 0);
    ASSERT_FALSE(result.quoteRiskAvailable_);
    ASSERT_EQ(result.fit_[0].ranges_[0].firstPath_, 0);
    ASSERT_EQ(result.validation_[0].ranges_[0].firstPath_, 128);
}

TEST(GSRScriptCalibrationTest, TestNonRootRowsMatchIndependentCrnDifferences) {
    auto settings = FitSettings();
    settings.optimizer_.priorWeight_ = 1e8;
    settings.optimizer_.smoothingWeight_ = 2.;
    const auto rows = FitRows(settings);
    const auto parameters = FitParameters();
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, parameters, settings);
    ASSERT_GT(std::abs(result.residuals_[0]), .01);
    ASSERT_EQ(result.penaltyResiduals_.size(), 3);
    ASSERT_NEAR(result.objective_, result.dataObjective_ + result.penaltyObjective_, 1e-14);
    for (size_t j = 0; j < parameters.size(); ++j) {
        Vector_<> differences;
        for (double h : {1e-6, 5e-7}) {
            Vector_<> displacement(parameters.size(), 0.);
            displacement[j] = h;
            const auto up = FitContext(result.parameters_[1] + displacement[1], result.parameters_[0] + displacement[0]);
            const auto down = FitContext(result.parameters_[1] - displacement[1], result.parameters_[0] - displacement[0]);
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& instrument = rows[i]->Instrument();
                const double high = PrepareCalibrationMc(instrument, "Price", up, settings.sampling_).Evaluate(instrument, up, false).mean_;
                const double low = PrepareCalibrationMc(instrument, "Price", down, settings.sampling_).Evaluate(instrument, down, false).mean_;
                const double fd = (high - low) / (2. * h);
                ASSERT_NEAR(result.quoteJacobian_(i, j), fd, 2e-6);
                differences.push_back(fd);
            }
        }
        for (size_t i = 0; i < rows.size(); ++i)
            ASSERT_NEAR(differences[i], differences[i + rows.size()], 2e-6);
    }
}

TEST(GSRScriptCalibrationTest, TestBasketDifferencesAndScaleDoNotOverwriteRawRows) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto result = CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings);
    const auto aad = CalibrateGSRScriptPrices(FitContext(), FitRows(FitSettings()), FitParameters(), FitSettings());
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_EQ(result.costs_.aadRows_, 0);
    ASSERT_EQ(result.derivativeMethod_, CalibrationDerivative_::Value_::FiniteDifference);
    ASSERT_EQ(result.costs_.doubleRows_, result.costs_.basketEvaluations_ * 2);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(result.quoteJacobian_(i, j), aad.quoteJacobian_(i, j), 2e-4);
    settings.optimizer_.parameterScale_ = .02;
    const auto scaled = CalibrateGSRScriptPrices(FitContext(.02, .025), FitRows(settings, .2), FitParameters(), settings);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            ASSERT_NEAR(scaled.quoteJacobian_(i, j), result.quoteJacobian_(i, j), 2e-4);
}

TEST(GSRScriptCalibrationTest, TestFitNumericsAndHeldOutFailIndependently) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings, 1e-5);
    const auto badHeld = NewCalibrationObservation("held", rows[0]->Instrument(), "Price",
                                                   NewCalibrationMarketQuote("held", 1000., CalibrationValueUnit_::Parse("Price:USD")), .1);
    const auto result = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings, {badHeld});
    ASSERT_TRUE(result.converged_);
    ASSERT_TRUE(result.fitWithinTolerance_);
    ASSERT_FALSE(result.numericalValidationPassed_);
    ASSERT_TRUE(result.heldOutRequested_);
    ASSERT_FALSE(result.heldOutPassed_);
    ASSERT_EQ(result.heldOut_[0].ranges_[0].firstPath_, 640);
    ASSERT_EQ(result.heldOut_[0].pathsPerView_, 512);
    ASSERT_NEAR(result.modelPrices_[0], rows[0]->Quote()->Value(), 1e-12);
    ASSERT_NE(result.modelPrices_[0], result.validation_[0].mean_);
    ASSERT_EQ(result.costs_.paths_, (result.costs_.doubleRows_ + result.costs_.aadRows_ - 3) * 128 + 1536);
}

TEST(GSRScriptCalibrationTest, TestBoundsRankAndOptimizerStatusAreSeparateFromFit) {
    auto settings = FitSettings();
    auto parameters = FitParameters();
    parameters[0].upper_ = .018;
    parameters[1].upper_ = .015;
    const auto rows = FitRows(settings, 1e-5);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, parameters, settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.activeBounds_[0]);
    ASSERT_TRUE(result.activeBounds_[1]);
    const auto rank = CalibrateGSRScriptPrices(FitContext(), {rows[0]}, FitParameters(), settings);
    ASSERT_EQ(rank.jacobianRank_, 1);
    ASSERT_TRUE(std::isinf(rank.jacobianConditionEstimate_));
    settings.optimizer_.maxIterations_ = 1;
    const auto limited = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_FALSE(limited.converged_);
    ASSERT_EQ(limited.terminationReason_, "MaxIterations");
}

TEST(GSRScriptCalibrationTest, TestBudgetFailureOwnsObservationAxes) {
    auto settings = FitSettings();
    settings.maxBasketEvaluations_ = 1;
    Handle_<CalibrationReport_> report;
    try {
        static_cast<void>(CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings));
        FAIL() << "expected frozen basket budget failure";
    } catch (const CalibrationError_& error) {
        ASSERT_EQ(error.Failure().diagnostic_.code_, "CalibrationBudgetExceeded");
        ASSERT_EQ(error.Failure().diagnostic_.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_TRUE(error.Failure().candidate_);
        report = error.Failure().report_;
    }
    ASSERT_TRUE(report);
    ASSERT_EQ(report->Axes().observations_.size(), 2);
}

TEST(GSRScriptCalibrationTest, TestNegativeVanillaTargetIsRejected) {
    const auto settings = FitSettings();
    const auto original = FitRows(settings)[0];
    const auto row = NewCalibrationObservation("negative", original->Instrument(), "Price",
                                               NewCalibrationMarketQuote("negative", -1., CalibrationValueUnit_::Parse("Price:USD")), .1);
    ASSERT_THROW(static_cast<void>(CalibrateGSRScriptPrices(FitContext(), {row}, FitParameters(), settings)), CalibrationError_);
}

TEST(GSRScriptCalibrationTest, TestNoDescentReturnsCandidateWithIndependentFlags) {
    auto settings = FitSettings();
    settings.optimizer_.priorWeight_ = 1e8;
    settings.optimizer_.gradientTolerance_ = settings.optimizer_.stepTolerance_ = 1e-30;
    const auto result = CalibrateGSRScriptPrices(FitContext(), FitRows(settings, 1e-5), FitParameters(), settings);
    ASSERT_FALSE(result.converged_);
    ASSERT_EQ(result.terminationReason_, "NoDescent");
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.model_);
}

TEST(GSRScriptCalibrationTest, TestFrozenNumericalBudgetCanPassWhileFitFails) {
    auto settings = FitSettings();
    settings.sampling_.fitBatches_ = 8;
    settings.sampling_.fitPathsPerBatch_ = 8192;
    settings.sampling_.validationBatches_ = settings.sampling_.heldOutBatches_ = 16;
    settings.sampling_.validationPathsPerBatch_ = settings.sampling_.heldOutPathsPerBatch_ = 16384;
    settings.sampling_.maxPathsPerView_ = 589824;
    const auto rows = FitRows(settings);
    const auto result = CalibrateGSRScriptPrices(FitContext(), {rows[0]}, {{0, 0, .001, .015}}, settings);
    ASSERT_TRUE(result.converged_);
    ASSERT_FALSE(result.fitWithinTolerance_);
    ASSERT_TRUE(result.numericalValidationPassed_);
    ASSERT_FALSE(result.heldOutRequested_);
    ASSERT_FALSE(result.heldOutPassed_);
    ASSERT_EQ(result.fit_[0].pathsPerView_, 65536);
    ASSERT_EQ(result.validation_[0].pathsPerView_, 262144);
    const double volatility = .02 * (366. / 365.);
    const double d1 = std::log(std::exp(-.04) / .97) / volatility + .5 * volatility;
    const auto cdf = [](double x) { return .5 * std::erfc(-x / std::sqrt(2.)); };
    const double oracle = 100. * (std::exp(-.08) * cdf(d1) - .97 * std::exp(-.04) * cdf(d1 - volatility));
    const auto held = NewCalibrationObservation("oracle-held", rows[0]->Instrument(), "Price",
                                                NewCalibrationMarketQuote("oracle-held", oracle, CalibrationValueUnit_::Parse("Price:USD")), .1);
    const auto root = CalibrateGSRScriptPrices(FitContext(.02, .025), {rows[0]}, {{0, 0, .001, .06}}, settings, {held});
    ASSERT_TRUE(root.fitWithinTolerance_);
    ASSERT_TRUE(root.numericalValidationPassed_);
    ASSERT_TRUE(root.heldOutPassed_);
    const auto& value = root.heldOut_[0];
    ASSERT_LE(std::abs(value.mean_ - oracle) + 3. * value.standardError_, .1);
}

TEST(GSRScriptCalibrationTest, TestFeasibleFiniteDifferenceColumnsAreReported) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto rows = FitRows(settings, 1e-5);
    const auto result = CalibrateGSRScriptPrices(FitContext(), rows, {{0, 1, .001, .018}, {0, 0, .001, .015}}, settings);
    ASSERT_EQ(result.oneSidedColumns_, (Vector_<bool>{true, true}));
    ASSERT_TRUE(result.activeBounds_[0]);
    ASSERT_TRUE(result.activeBounds_[1]);
    ASSERT_TRUE(std::isfinite(result.quoteJacobian_(0, 1)));
}

TEST(GSRScriptCalibrationTest, TestStepStalledAndActualRowBatchCounts) {
    auto settings = FitSettings();
    const auto rows = FitRows(settings);
    settings.optimizer_.stepTolerance_ = 100.;
    const auto stalled = CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings);
    ASSERT_FALSE(stalled.converged_);
    ASSERT_EQ(stalled.terminationReason_, "StepStalled");
    const auto root = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings);
    ASSERT_EQ(root.costs_.basketEvaluations_, 5);
    ASSERT_EQ(root.costs_.jacobians_, 2);
    ASSERT_EQ(root.costs_.doubleRows_, 2);
    ASSERT_EQ(root.costs_.aadRows_, 8);
    ASSERT_EQ(root.costs_.batchRuns_, 24);
    ASSERT_EQ(root.costs_.paths_, 2048);
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    const auto fd = CalibrateGSRScriptPrices(FitContext(.02, .025), rows, FitParameters(), settings);
    ASSERT_EQ(fd.costs_.basketEvaluations_, 11);
    ASSERT_EQ(fd.costs_.aadRows_, 0);
    ASSERT_EQ(fd.costs_.doubleRows_, 22);
    ASSERT_EQ(fd.costs_.batchRuns_, 48);
    ASSERT_EQ(fd.costs_.paths_, 3584);
}

TEST(GSRScriptCalibrationTest, TestNestedTapeFailurePreservesCallerAndOwnsReport) {
    const auto settings = FitSettings();
    const auto rows = FitRows(settings);
    {
        AAD::RecordingScope_ recording;
        AAD::Number_ x;
        recording.RegisterInput(x, 2.);
        recording.StartRecording();
        ASSERT_THROW(static_cast<void>(CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings)), CalibrationError_);
        AAD::Number_ y = x * x;
        recording.FinishRecording();
        recording.ClearAdjoints();
        Adjoint(y) = 1.;
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Adjoint(x), 4.);
    }
    ASSERT_TRUE(CalibrateGSRScriptPrices(FitContext(), rows, FitParameters(), settings).fitWithinTolerance_);
    ASSERT_FALSE(AAD::LastRecordingCleanupFailure());
}

TEST(GSRScriptCalibrationTest, TestNormalizedBoundsStayFeasibleInNativeUnits) {
    const auto settings = FitSettings();
    const auto source = FitRows(settings)[0];
    for (bool upper : {false, true}) {
        const double target = upper ? source->Quote()->Value() + 1. : 0.;
        const auto row = NewCalibrationObservation("bound", source->Instrument(), "Price",
                                                   NewCalibrationMarketQuote("bound", target, CalibrationValueUnit_::Parse("Price:USD")), .1);
        const GSRCalibrationParameter_ parameter{0, 0, upper ? .001 : .027, upper ? .031 : .06};
        const auto result = CalibrateGSRScriptPrices(FitContext(.03), {row}, {parameter}, settings);
        ASSERT_TRUE(result.activeBounds_[0]);
        ASSERT_GE(result.parameters_[0], parameter.lower_);
        ASSERT_LE(result.parameters_[0], parameter.upper_);
        ASSERT_DOUBLE_EQ(result.model_->vol_->gValues_(0, 0), result.parameters_[0]);
    }
}

TEST(GSRScriptCalibrationTest, TestUnresolvedNativeStepNamesParameterAndStage) {
    auto settings = FitSettings();
    settings.derivativeMethod_ = CalibrationDerivative_::Value_::FiniteDifference;
    settings.optimizer_.finiteDifferenceStep_ = 1e-20;
    try {
        static_cast<void>(CalibrateGSRScriptPrices(FitContext(), FitRows(settings), FitParameters(), settings));
        FAIL() << "expected unrepresentable native step";
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.code_, "DerivativeUnavailable");
        ASSERT_EQ(d.stage_, CalibrationStage_::Value_::Solve);
        ASSERT_TRUE(d.parameterKey_);
        ASSERT_EQ(*d.parameterKey_, "g:level:2027-10-21");
        ASSERT_TRUE(error.Failure().candidate_);
        ASSERT_EQ(error.Failure().report_->Axes().observations_.size(), 2);
    }
}
