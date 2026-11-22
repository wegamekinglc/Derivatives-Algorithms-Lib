#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curvescriptevaluation_internal.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/matrix/linearsolveaccuracy.hpp>

#include "compoundedoiscalibrationfixture.hpp"

using namespace Dal;
using namespace Dal::CompoundedOisCalibrationTest;

namespace {
    CurveInstrumentCalibrationOptions_ Options() {
        CurveInstrumentCalibrationOptions_ result;
        result.jacobianMode_ = CalibrationDerivative_("AAD");
        return result;
    }
    ScriptCurveQuoteRiskAccuracy_ Accuracy(bool pvOnly = false) { return {{1e-12, 1e-12}, 1e-10, 1e-12, 1e-12, pvOnly}; }
    Vector_<> PassiveParameters(const String_& role) {
        return role == "Discount" ? Vector_<>{-.0002, -.02} : CompoundedOisTest::BackgroundParameters();
    }
    CurveInstrumentCalibrationSpec_ RiskSpec(const String_& mode, const String_& role, bool mixed = false, bool zeroRate = false) {
        auto spec = Spec(mode, role, mixed, zeroRate);
        if (role == "Discount") {
            // Separate coupon forwards enough to make 1bp quote bumps local in the discount coordinates.
            const auto background = PassiveParameters(role);
            spec.market_.overnightCurves_["USD.SOFR"].reset(
                new DiscountLogDF_("passive", "USD", {Date_(2024, 3, 1), Date_(2024, 3, 5), Date_(2024, 3, 20)}, {0., background[0], background[1]},
                                   DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
            const auto row = spec.observations_[1];
            const double value = Oracle(spec.compoundedOis_[1]->Spec(), role, Parameters(zeroRate), zeroRate, spec.valuation_, background)[1];
            spec.observations_[1] =
                NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(),
                                          NewCalibrationMarketQuote(row->Quote()->Id(), value, row->Quote()->Unit()), row->ErrorScale());
        }
        return spec;
    }
    CurveInstrumentCalibrationSpec_ Bumped(CurveInstrumentCalibrationSpec_ spec, int column, double bump) {
        const auto row = spec.observations_[column];
        const auto quote = NewCalibrationMarketQuote(row->Quote()->Id(), row->Quote()->Value() + bump, row->Quote()->Unit());
        spec.observations_[column] = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), quote, row->ErrorScale());
        return spec;
    }
    void ReplaceTable(CurveInstrumentCalibrationSpec_* spec, size_t index, const CompoundedOisScriptSpec_& contract) {
        const auto id = spec->compoundedOis_[index]->Spec().instrumentId_;
        const auto tables = PrepareCompoundedOis(contract, spec->valuation_);
        spec->compoundedOis_[index] = tables;
        for (auto& row : spec->observations_)
            if (row->Instrument()->Id() == id)
                row =
                    NewCalibrationObservation(row->Id(), NewCompoundedOisScriptInstrument(tables), row->MeasureId(), row->Quote(), row->ErrorScale());
    }
    void RebuildValuation(CurveInstrumentCalibrationSpec_* spec) {
        for (size_t i = 0; i < spec->compoundedOis_.size(); ++i)
            ReplaceTable(spec, i, spec->compoundedOis_[i]->Spec());
    }
    CurveInstrumentCalibrationSpec_ NegativeParRateSpec() {
        auto spec = RiskSpec("None", "Shared");
        const auto quotes = Measures(spec, "Shared", {.0003, .002}, false);
        for (int i = 0; i < 2; ++i)
            spec = Bumped(spec, i, quotes[i] - spec.observations_[i]->Quote()->Value());
        auto contract = Contract(false, "Lookback");
        contract.instrumentId_ = "registered-negative";
        ReplaceTable(&spec, 2, contract);
        return spec;
    }
    double FrozenParRate(const PreparedCompoundedOis_& tables, const Vector_<>& sharedLogDiscounts) {
        const auto& s = tables.Spec();
        const auto logDf = [&](const Date_& date) { return CompoundedOisTest::LogDiscount(date, sharedLogDiscounts); };
        Vector_<> products(s.coupons_.size(), 1.);
        for (const auto& row : tables.ObservationSchedule()) {
            if (!row.included_)
                continue;
            const double rate = row.historicalRequired_ ? tables.Valuation().fixings_->Find(row.fixingIndex_, row.fixingTime_).value()
                                                        : std::expm1(logDf(row.rateStart_) - logDf(row.rateEnd_)) / row.rateAccrual_;
            products[row.couponRow_] *= 1. + rate * row.compoundingAccrual_;
        }
        double floating = 0., annuity = 0.;
        for (size_t i = 0; i < products.size(); ++i)
            if (tables.CouponPayments()[i].included_)
                floating += (products[i] - 1.) * std::exp(logDf(s.coupons_[i].payment_));
        for (size_t i = 0; i < s.fixedLeg_.size(); ++i)
            if (tables.FixedPayments()[i].included_)
                annuity += s.fixedLeg_[i].accrual_ * std::exp(logDf(s.fixedLeg_[i].payment_));
        // Both legs are per unit notional; reversing the trade changes both signs.
        return floating / annuity;
    }
    CompoundedOisScriptSpec_ ParRateContract(const String_& mode) {
        if (mode == "Legacy")
            return CompoundedOisTest::Contract();
        auto contract = Contract(false, mode);
        // Give the registered trade a different maturity and fixed payment date from both calibration instruments.
        const auto start = contract.coupons_.front().start_;
        const Date_ end(2024, 3, 11);
        contract.coupons_.back().end_ = end;
        contract.fixedLeg_ = {{start, end, Date_(2024, 3, 15), (end - start) / 365.}};
        return contract;
    }
    void AssertParRateBumps(const CurveInstrumentCalibrationSpec_& spec, const CompoundedOisParRateRiskResult_& base, const String_& mode) {
        const auto& tables = spec.compoundedOis_.back();
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        for (size_t j = 0; j < spec.observations_.size(); ++j) {
            const auto& id = spec.observations_[j]->Quote()->Id();
            ASSERT_EQ(base.QuoteAxis()[j].id_, id);
            const double analytic = base.SensitivityBpPerBp()[j];
            for (const double h : {1e-4, 5e-5}) {
                SCOPED_TRACE(::testing::Message() << mode << " " << id << " h=" << h);
                std::ostringstream row;
                row << std::setprecision(14) << "ParRate FD " << mode << " " << id << " h=" << h << " analytic=" << analytic;
                std::array<double, 2> rates{};
                for (size_t side = 0; side < rates.size(); ++side) {
                    const double bump = side == 0 ? -h : h;
                    SCOPED_TRACE(bump);
                    const auto bumped = Bumped(spec, static_cast<int>(j), bump);
                    const auto fit = CalibrateCurveInstruments(bumped, Options());
                    ASSERT_TRUE(fit.fitWithinTolerance_);
                    ASSERT_EQ(fit.solveMode_, CurveSolveMode_("EXACT"));
                    rates[side] = FrozenParRate(*tables, fit.parameters_);

                    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(true));
                    ASSERT_TRUE(snapshot.RiskAvailable());
                    const auto& numerics = snapshot.Numerics();
                    ASSERT_EQ(numerics.rawResiduals_.size(), spec.observations_.size());
                    double residual = 0.;
                    for (const auto r : numerics.rawResiduals_) {
                        ASSERT_LE(std::abs(r), 1e-12);
                        residual = std::max(residual, std::abs(r));
                    }
                    ASSERT_TRUE(numerics.reciprocalCondition_);
                    ASSERT_GE(*numerics.reciprocalCondition_, 1e-10);
                    const auto risk = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, bumped);
                    ASSERT_TRUE(risk.Available());
                    ASSERT_NEAR(risk.ParRate(), rates[side], 1e-12);
                    ASSERT_EQ(risk.TransposeBackwardErrors().size(), 1u);
                    const double backward = risk.TransposeBackwardErrors()[0];
                    ASSERT_LE(backward, 1e-12);
                    row << (side == 0 ? " minus" : " plus") << "(residual,rcond,backward)=" << residual << "," << *numerics.reciprocalCondition_
                        << "," << backward;
                }
                const double reference = (rates[1] - rates[0]) / (2. * h);
                const double error = std::abs(analytic - reference);
                ASSERT_LE(error, 1e-7 + 1e-5 * std::abs(reference));
                row << " reference=" << reference << " abs_error=" << error;
                std::cout << row.str() << '\n';
            }
        }
    }
    void AssertParRateRejected(const CompoundedOisParRateResolution_& result, const String_& code) {
        ASSERT_FALSE(result.Available());
        ASSERT_TRUE(result.quotes_.empty());
        ASSERT_EQ(result.diagnostics_.size(), 1u);
        ASSERT_EQ(result.diagnostics_.front().code_, code);
    }
    void AssertParRateUnavailable(const CompoundedOisParRateRiskResult_& result, const String_& code) {
        ASSERT_FALSE(result.Available());
        ASSERT_EQ(result.Diagnostics().size(), 1u);
        ASSERT_EQ(result.Diagnostics()[0].code_, code);
        ASSERT_EQ(result.Diagnostics()[0].measureId_, "ParRate");
        ASSERT_THROW(static_cast<void>(result.ParRate()), Exception_);
        ASSERT_THROW(static_cast<void>(result.NativeGradient()), Exception_);
        ASSERT_THROW(static_cast<void>(result.SensitivityBpPerBp()), Exception_);
        ASSERT_THROW(static_cast<void>(result.TransposeBackwardErrors()), Exception_);
    }
    void AssertUnserializableParRateContext(bool pvOnly) {
        auto spec = RiskSpec("None", "Overnight");
        const auto nodes = CompoundedOisTest::BackgroundParameters();
        const auto background = std::make_shared<DiscountLogDF_>("passive", "USD", spec.geometry_.knotDates_, Vector_<>{0., nodes[0], nodes[1]},
                                                                 DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
        spec.market_.discountCurves_[CollateralType_("OIS")] = Handle_<DiscountCurve_>(background);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), Accuracy(pvOnly));
        const auto& tables = spec.compoundedOis_.back();
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        const auto good = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
        ASSERT_TRUE(good.Available());
        ASSERT_EQ(good.SnapshotIdentity(), snapshot.Identity());

        const Vector_<> bump{std::numeric_limits<double>::infinity(), 0.};
        background->ApplyDX(bump.begin(), 1.);
        const auto bad = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
        ASSERT_NO_FATAL_FAILURE(AssertParRateUnavailable(bad, "ScriptCurveQuoteRiskUnavailable"));
        ASSERT_EQ(bad.SnapshotIdentity(), good.SnapshotIdentity());
        const auto& diagnostic = bad.Diagnostics().front();
        ASSERT_EQ(diagnostic.field_, "/context");
        ASSERT_EQ(diagnostic.stage_, CalibrationStage_::Value_::Validate);
        ASSERT_EQ(diagnostic.instrumentId_, instrument->Id());
        ASSERT_TRUE(diagnostic.actual_);
        ASSERT_NE(diagnostic.actual_->find("ARCHIVE_NUMBER_NON_FINITE"), String_::npos);
        ASSERT_THROW(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec), Exception_);
    }
    void AssertUnavailable(const ScriptCurveQuoteRiskResult_& result) {
        ASSERT_FALSE(result.Available());
        ASSERT_FALSE(result.Diagnostics().empty());
        ASSERT_THROW(static_cast<void>(result.PV()), Exception_);
        ASSERT_THROW(static_cast<void>(result.RawSensitivity()), Exception_);
        ASSERT_THROW(static_cast<void>(result.DV01()), Exception_);
    }
    void AssertRoot(const CurveInstrumentCalibrationResult_& fit) {
        ASSERT_TRUE(fit.fitWithinTolerance_);
        ASSERT_EQ(fit.solveMode_, CurveSolveMode_("EXACT"));
        const auto values = PreparedCurveInstruments_(*fit.resolvedSpec_, Options()).Evaluate(fit.parameters_);
        SquareMatrix_<> matrix(2, 0.);
        Matrix_<> rhs(2, 2, 0.);
        for (int i = 0; i < 2; ++i) {
            ASSERT_LE(std::abs(values.rawResiduals_[i]), 1e-12);
            rhs(i, i) = 1.;
            for (int j = 0; j < 2; ++j)
                matrix(i, j) = values.quoteJacobian_(i, j);
        }
        const CheckedLinearSolve_ checked(matrix, rhs, {1e-12, 1e-12});
        ASSERT_GE(checked.Diagnostics().reciprocalConditionInfinity_, 1e-10);
    }
    double DirectPrice(const Handle_<CalibrationInstrument_>& instrument,
                       const Handle_<PreparedCompoundedOis_>& tables,
                       const CurveInstrumentCalibrationResult_& fit) {
        CurveScriptEvaluationRequest_ request;
        request.geometry_ = fit.resolvedSpec_->geometry_;
        request.market_ = fit.resolvedSpec_->market_;
        request.valuation_ = fit.resolvedSpec_->valuation_;
        request.compoundedOis_ = tables;
        request.parameters_ = fit.parameters_;
        return EvaluateCurveScriptInstrument(instrument, request).values_[0];
    }
    void AssertFd(const CurveInstrumentCalibrationSpec_& spec, const ScriptCurveQuoteRiskResult_& risk, const String_& role, bool zeroRate) {
        const auto& tables = spec.compoundedOis_[2];
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        const double notional = tables->Spec().notional_;
        Vector_<> previous;
        double maxError = 0., maxStability = 0.;
        for (const double h : {1e-4, 5e-5}) {
            Vector_<> fd(2);
            for (int j = 0; j < 2; ++j) {
                const auto plus = CalibrateCurveInstruments(Bumped(spec, j, h), Options());
                const auto minus = CalibrateCurveInstruments(Bumped(spec, j, -h), Options());
                ASSERT_NO_FATAL_FAILURE(AssertRoot(plus));
                ASSERT_NO_FATAL_FAILURE(AssertRoot(minus));
                const double up = DirectPrice(instrument, tables, plus), down = DirectPrice(instrument, tables, minus);
                ASSERT_NEAR(up, Oracle(tables->Spec(), role, plus.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                            1e-10 * notional);
                ASSERT_NEAR(down, Oracle(tables->Spec(), role, minus.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                            1e-10 * notional);
                fd[j] = (up - down) / (2. * h);
                const double tolerance = 1e-7 + 1e-5 * std::abs(fd[j] / notional);
                const double dvTolerance = 1e-11 + 1e-5 * std::abs(fd[j] * 1e-4 / notional);
                ASSERT_NEAR(risk.RawSensitivity()[j] / notional, fd[j] / notional, tolerance);
                ASSERT_NEAR(risk.DV01()[j] / notional, fd[j] * 1e-4 / notional, dvTolerance);
                maxError = std::max(maxError, std::abs(risk.RawSensitivity()[j] - fd[j]) / notional / tolerance);
                if (!previous.empty()) {
                    ASSERT_NEAR(fd[j] / notional, previous[j] / notional, tolerance);
                    maxStability = std::max(maxStability, std::abs(fd[j] - previous[j]) / notional / tolerance);
                }
            }
            previous = fd;
        }
        const auto mode = tables->Spec().observationConvention_ ? tables->Spec().observationConvention_->mode_.String() : "Legacy";
        std::cout << "OIS quote-FD " << tables->RuleId() << " mode=" << mode << ' ' << role << " zero_rate=" << zeroRate
                  << " today=" << spec.valuation_.todayFixingPolicy_.String() << " N=" << notional << " error/limit=" << maxError
                  << " stability/limit=" << maxStability << '\n';
    }
    void AssertModes(const CurveInstrumentCalibrationSpec_& spec, const String_& role, bool zeroRate = false) {
        const auto fit = CalibrateCurveInstruments(spec, Options());
        ASSERT_NO_FATAL_FAILURE(AssertRoot(fit));
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
        const auto full = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(true));
        ASSERT_TRUE(full.RiskAvailable());
        ASSERT_TRUE(full.MappingAvailable());
        ASSERT_TRUE(lean.RiskAvailable());
        ASSERT_FALSE(lean.MappingAvailable());
        ASSERT_FALSE(lean.ParameterQuoteMapping(spec).Available());
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, full, spec);
        const auto pvOnly = ValueCalibrationInstrumentWithQuoteRisk(instrument, lean, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_TRUE(pvOnly.Available());
        ASSERT_EQ(risk.Currency(), Ccy_("USD"));
        ASSERT_DOUBLE_EQ(risk.PV(), pvOnly.PV());
        ASSERT_NEAR(risk.PV(), Oracle(spec.compoundedOis_[2]->Spec(), role, fit.parameters_, zeroRate, spec.valuation_, PassiveParameters(role))[0],
                    1e-8);
        const auto mapping = full.ParameterQuoteMapping(spec);
        for (int j = 0; j < 2; ++j) {
            ASSERT_DOUBLE_EQ(risk.RawSensitivity()[j], pvOnly.RawSensitivity()[j]);
            ASSERT_DOUBLE_EQ(risk.DV01()[j], risk.RawSensitivity()[j] * 1e-4);
            const auto& m = mapping.ParameterResponse();
            ASSERT_NEAR(risk.RawSensitivity()[j], risk.NativeGradient()[0] * m(0, j) + risk.NativeGradient()[1] * m(1, j), 1e-8);
            ASSERT_LE(risk.Numerics().transposeBackwardErrors_[0], 1e-12);
            ASSERT_LE(mapping.Numerics().responseBackwardErrors_[j], 1e-12);
        }
        ASSERT_NO_FATAL_FAILURE(AssertFd(spec, risk, role, zeroRate));
    }
} // namespace

TEST(CompoundedOisQuoteRiskTest, TestFrozenParRateUsesLegacyHistoricalAndProjectedFixings) {
    const auto tables = PrepareCompoundedOis(CompoundedOisTest::Contract(), CompoundedOisTest::Valuation());
    const auto theta = Parameters();
    ASSERT_NEAR(FrozenParRate(*tables, theta), CompoundedOisTest::Oracle(theta, theta)[1], 1e-12);
}

TEST(CompoundedOisQuoteRiskTest, TestParRateBucketsMatchFrozenCashflowsAfterCompleteRecalibration) {
    for (const String_ mode : {"None", "Lookback", "ObservationShift", "Lockout", "Legacy"}) {
        SCOPED_TRACE(mode);
        auto spec = RiskSpec("None", "Shared");
        auto contract = ParRateContract(mode);
        contract.instrumentId_ = "registered-par";
        ReplaceTable(&spec, 2, contract);
        const auto& tables = spec.compoundedOis_.back();
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        const auto fit = CalibrateCurveInstruments(spec, Options());
        const auto full = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy()).ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
        const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(true)).ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
        ASSERT_TRUE(full.Available());
        ASSERT_TRUE(lean.Available());
        ASSERT_NEAR(full.ParRate(), FrozenParRate(*tables, fit.parameters_), 1e-12);
        ASSERT_DOUBLE_EQ(lean.ParRate(), full.ParRate());
        ASSERT_EQ(full.TransposeBackwardErrors().size(), 1u);
        ASSERT_EQ(lean.TransposeBackwardErrors().size(), 1u);
        ASSERT_LE(full.TransposeBackwardErrors()[0], 1e-12);
        ASSERT_LE(lean.TransposeBackwardErrors()[0], 1e-12);
        std::ostringstream row;
        row << std::setprecision(14) << "ParRate base " << mode << " rate=" << full.ParRate()
            << " full_backward=" << full.TransposeBackwardErrors()[0] << " pv_only_backward=" << lean.TransposeBackwardErrors()[0];
        std::cout << row.str() << '\n';
        ASSERT_EQ(full.QuoteAxis().size(), spec.observations_.size());
        ASSERT_EQ(lean.QuoteAxis().size(), full.QuoteAxis().size());
        ASSERT_EQ(full.SensitivityBpPerBp().size(), full.QuoteAxis().size());
        ASSERT_EQ(lean.SensitivityBpPerBp(), full.SensitivityBpPerBp());
        for (size_t j = 0; j < full.QuoteAxis().size(); ++j) {
            ASSERT_EQ(lean.QuoteAxis()[j].id_, full.QuoteAxis()[j].id_);
            ASSERT_EQ(lean.QuoteAxis()[j].unit_.Token(), "Rate");
            ASSERT_EQ(full.QuoteAxis()[j].unit_.Token(), "Rate");
        }
        ASSERT_NO_FATAL_FAILURE(AssertParRateBumps(spec, full, mode));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestLockoutParRateAccumulatesAnchorRiskForEitherDirectionAndNotional) {
    auto spec = RiskSpec("None", "Shared");
    auto contract = Contract(true, "Lockout");
    contract.instrumentId_ = "lockout-anchor";
    ReplaceTable(&spec, 2, contract);
    auto receive = contract, doubled = contract;
    receive.instrumentId_ = "receive";
    receive.direction_ = "ReceiveFixed";
    doubled.instrumentId_ = "doubled";
    doubled.notional_ *= 2.;
    for (const auto& variant : {receive, doubled})
        spec.compoundedOis_.push_back(PrepareCompoundedOis(variant, spec.valuation_));

    const auto& rows = spec.compoundedOis_[2]->ObservationSchedule();
    ASSERT_EQ(rows.size(), 2u);
    ASSERT_EQ(rows[1].reusedFromRow_, 0u);
    const double first = 4. / 360., tail = 1. / 360., fixed = 5. / 365.;
    ASSERT_DOUBLE_EQ(rows[0].compoundingAccrual_, first);
    ASSERT_DOUBLE_EQ(rows[1].compoundingAccrual_, tail);
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const double x = fit.parameters_[0];
    const double anchor = std::expm1(-x) / first;
    const double dRateDAnchor = (first * (1. + tail * anchor) + tail * (1. + first * anchor)) / fixed;
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto value = [&](size_t i) {
        const auto& tables = spec.compoundedOis_[i];
        return snapshot.ValueCompoundedOisParRateWithQuoteRisk(NewCompoundedOisScriptInstrument(tables), tables, spec);
    };
    const auto base = value(2);
    ASSERT_TRUE(base.Available());
    ASSERT_EQ(base.SensitivityBpPerBp().size(), spec.observations_.size());
    ASSERT_EQ(base.NativeGradient().size(), 2u);
    ASSERT_NEAR(base.NativeGradient()[0], dRateDAnchor * (-std::exp(-x) / first), 1e-10);
    ASSERT_NEAR(base.NativeGradient()[1], 0., 1e-12);
    for (const size_t i : {3, 4}) {
        const auto risk = value(i);
        ASSERT_TRUE(risk.Available());
        ASSERT_DOUBLE_EQ(risk.ParRate(), base.ParRate());
        ASSERT_EQ(risk.SensitivityBpPerBp(), base.SensitivityBpPerBp());
    }
}

TEST(CompoundedOisQuoteRiskTest, TestVersionTwoFullAndPvOnlyUseFrozenNativeCapability) {
    const auto spec = Spec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_.back());
    for (const bool pvOnly : {false, true}) {
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
        ASSERT_TRUE(snapshot.RiskAvailable());
        ASSERT_EQ(snapshot.MappingAvailable(), !pvOnly);
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_EQ(risk.RawSensitivity().size(), 2u);
        ASSERT_DOUBLE_EQ(risk.DV01()[0], risk.RawSensitivity()[0] * 1e-4);
        ASSERT_EQ(risk.Numerics().factorizationCount_, 1);
    }
}

TEST(CompoundedOisQuoteRiskTest, TestAllLegacyToolsRetainCompleteQuoteRecalibration) {
    for (const auto* role : {"Overnight", "Discount", "Shared"}) {
        auto spec = RiskSpec("None", role);
        for (size_t i = 0; i < 3; ++i) {
            auto legacy = Contract(i == 0, "None", true);
            legacy.instrumentId_ = spec.compoundedOis_[i]->Spec().instrumentId_;
            legacy.coupon_ = spec.compoundedOis_[i]->Spec().coupon_;
            ReplaceTable(&spec, i, legacy);
        }
        ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestQuoteSelectionSignedDv01AndActualNotionalScaling) {
    auto spec = RiskSpec("Lockout", "Shared");
    const auto primary = spec.compoundedOis_[2]->Spec();
    for (const auto* id : {"double-pv", "receive-pv"}) {
        auto variant = primary;
        variant.instrumentId_ = id;
        variant.notional_ *= String_(id) == "double-pv" ? 2. : 1.;
        variant.direction_ = String_(id) == "receive-pv" ? "ReceiveFixed" : "PayFixed";
        spec.compoundedOis_.push_back(PrepareCompoundedOis(variant, spec.valuation_));
    }
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto base = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]), snapshot, spec);
    ASSERT_TRUE(base.Available());
    for (const size_t i : {3, 4}) {
        const double scale = i == 3 ? 2. : -1.;
        const auto risk = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[i]), snapshot, spec);
        ASSERT_TRUE(risk.Available());
        ASSERT_NEAR(risk.PV(), scale * base.PV(), 1e-10);
        for (int j = 0; j < 2; ++j) {
            ASSERT_NEAR(risk.RawSensitivity()[j], scale * base.RawSensitivity()[j], 1e-10);
            ASSERT_NEAR(risk.DV01()[j], scale * base.DV01()[j], 1e-14);
        }
    }
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const auto reordered = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"long", "short"}});
    const auto subset = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{"long"}});
    ASSERT_TRUE(reordered.Available());
    ASSERT_TRUE(subset.Available());
    ASSERT_EQ(reordered.Axes().quotes_[0].ordinal_, 1);
    ASSERT_EQ(reordered.Axes().quotes_[1].ordinal_, 0);
    ASSERT_EQ(subset.Axes().quotes_[0].ordinal_, 1);
    ASSERT_EQ(subset.RawSensitivity().size(), 1u);
    ASSERT_DOUBLE_EQ(reordered.RawSensitivity()[0], base.RawSensitivity()[1]);
    ASSERT_DOUBLE_EQ(reordered.RawSensitivity()[1], base.RawSensitivity()[0]);
    ASSERT_DOUBLE_EQ(subset.RawSensitivity()[0], base.RawSensitivity()[1]);
}

TEST(CompoundedOisQuoteRiskTest, TestNegativeParRateUsesOneTransposeRhsForSelectedBuckets) {
    const auto spec = NegativeParRateSpec();
    const auto& tables = spec.compoundedOis_[2];
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    const int beforeFit = CurveCalibrationInvocationCount();
    const auto fit = CalibrateCurveInstruments(spec, Options());
    ASSERT_EQ(CurveCalibrationInvocationCount() - beforeFit, 1);
    const auto& theta = fit.parameters_;
    const auto rate = [&](const Vector_<>& x) { return Oracle(tables->Spec(), "Shared", x, false, spec.valuation_)[1]; };
    const auto full = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto mapping = full.ParameterQuoteMapping(spec);
    const auto& response = mapping.ParameterResponse();
    for (const auto& snapshot : {full, NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(true))}) {
        ASSERT_TRUE(snapshot.RiskAvailable());
        for (const Vector_<String_>& ids : {Vector_<String_>{"long"}, {"long", "short"}}) {
            const int before = CurveCalibrationInvocationCount();
            const auto result = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec, {ids});
            ASSERT_TRUE(result.Available());
            ASSERT_EQ(result.SnapshotIdentity(), snapshot.Identity());
            ASSERT_LT(result.ParRate(), 0.);
            ASSERT_NEAR(result.ParRate(), rate({.0003, .002}), 1e-11);
            ASSERT_EQ(result.QuoteAxis().size(), ids.size());
            ASSERT_EQ(result.SensitivityBpPerBp().size(), ids.size());
            ASSERT_EQ(result.NativeGradient().size(), theta.size());
            ASSERT_EQ(result.TransposeBackwardErrors().size(), 1u);
            ASSERT_LE(result.TransposeBackwardErrors()[0], 1e-12);
            ASSERT_EQ(CurveCalibrationInvocationCount() - before, 0);
            for (size_t j = 0; j < theta.size(); ++j) {
                auto up = theta, down = theta;
                up[j] += 1e-7;
                down[j] -= 1e-7;
                ASSERT_NEAR(result.NativeGradient()[j], (rate(up) - rate(down)) / 2e-7, 1e-6);
            }
            for (size_t j = 0; j < ids.size(); ++j) {
                ASSERT_EQ(result.QuoteAxis()[j].id_, ids[j]);
                const int column = ids[j] == "long" ? 1 : 0;
                ASSERT_EQ(result.QuoteAxis()[j].ordinal_, column);
                ASSERT_NEAR(result.SensitivityBpPerBp()[j],
                            result.NativeGradient()[0] * response(0, column) + result.NativeGradient()[1] * response(1, column), 1e-12);
                std::ostringstream row;
                row << std::setprecision(15) << "par-rate pv_only=" << snapshot.Accuracy().pvOnly_ << " buckets=" << ids.size()
                    << " rate=" << result.ParRate() << " quote=" << ids[j] << " bp/bp=" << result.SensitivityBpPerBp()[j]
                    << " transpose_rhs=" << result.TransposeBackwardErrors().size()
                    << " recalibrations=" << CurveCalibrationInvocationCount() - before;
                std::cout << row.str() << '\n';
            }
        }
    }
}

TEST(CompoundedOisQuoteRiskTest, TestFullParRateOwnsUnserializableContextDiagnostic) {
    ASSERT_NO_FATAL_FAILURE(AssertUnserializableParRateContext(false));
}

TEST(CompoundedOisQuoteRiskTest, TestPvOnlyParRateOwnsUnserializableContextDiagnostic) {
    ASSERT_NO_FATAL_FAILURE(AssertUnserializableParRateContext(true));
}

TEST(CompoundedOisQuoteRiskTest, TestParRateRejectsSettledAndInsufficientAnnuitiesAndRestoresTape) {
    auto spec = RiskSpec("None", "Shared");
    auto contract = Contract(true, "None");
    contract.instrumentId_ = "high-floor";
    contract.minAbsAnnuity_ = 100.;
    spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
    contract.instrumentId_ = "all-settled";
    contract.coupons_ = {{"past", Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29)}};
    contract.fixedLeg_ = {{Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29), 1. / 365.}};
    spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), Accuracy(true));
    ASSERT_TRUE(snapshot.RiskAvailable());
    const auto mode = AAD::SetNumResultsForAAD(true, 3);
    for (size_t i : {3, 4}) {
        const auto& tables = spec.compoundedOis_[i];
        const auto result = snapshot.ValueCompoundedOisParRateWithQuoteRisk(NewCompoundedOisScriptInstrument(tables), tables, spec);
        ASSERT_NO_FATAL_FAILURE(AssertParRateUnavailable(result, "InvalidCalibrationAnnuity"));
        const auto& diagnostic = result.Diagnostics()[0];
        ASSERT_EQ(diagnostic.instrumentId_, tables->Spec().instrumentId_);
        ASSERT_EQ(diagnostic.field_, "/annuity");
        ASSERT_TRUE(diagnostic.actual_ && !diagnostic.actual_->empty());
        const double annuity = std::stod(diagnostic.actual_->c_str());
        if (i == 3) {
            ASSERT_LT(annuity, 0.);
            ASSERT_LT(std::abs(annuity), contract.minAbsAnnuity_);
        } else
            ASSERT_DOUBLE_EQ(annuity, 0.);
        ASSERT_TRUE(AAD::Tape()->multi_);
        ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
        ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
    }
    const auto& live = spec.compoundedOis_[2];
    ASSERT_TRUE(snapshot.ValueCompoundedOisParRateWithQuoteRisk(NewCompoundedOisScriptInstrument(live), live, spec).Available());
    ASSERT_TRUE(AAD::Tape()->multi_);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3u);
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
}

TEST(CompoundedOisQuoteRiskTest, TestParRateRejectsExcessTransposeErrorAtEvaluation) {
    const auto spec = NegativeParRateSpec();
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    auto accuracy = Accuracy(true);
    const auto good = NewScriptCurveQuoteRiskSnapshot(fit, accuracy).ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
    ASSERT_TRUE(good.Available());
    const double error = good.TransposeBackwardErrors().front();
    ASSERT_GT(error, 0.);
    accuracy.responseBackwardErrorLimit_ = error / 2.;
    const auto strict = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
    ASSERT_TRUE(strict.RiskAvailable());
    const auto result = strict.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
    ASSERT_NO_FATAL_FAILURE(AssertParRateUnavailable(result, "ScriptCurveQuoteRiskResponseAccuracy"));
    ASSERT_EQ(result.Diagnostics()[0].field_, "/par_rate/transpose_error");
    ASSERT_EQ(result.Diagnostics()[0].stage_, CalibrationStage_::Value_::Evaluate);
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, 0u);
}

TEST(CompoundedOisQuoteRiskTest, TestParRateRejectsNestedRecordingAndPreservesOuterDerivative) {
    const auto spec = NegativeParRateSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), Accuracy());
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    AAD::RecordingScope_ outer;
    AAD::Number_ x;
    outer.RegisterInput(x, 3.);
    outer.StartRecording();
    const auto before = AAD::MeasureTape(*AAD::Tape()).nodes_;
    const auto result = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
    ASSERT_NO_FATAL_FAILURE(AssertParRateUnavailable(result, "ScriptCurveQuoteRiskNestedRecording"));
    ASSERT_EQ(result.Diagnostics()[0].field_, "/par_rate/recording");
    ASSERT_EQ(AAD::MeasureTape(*AAD::Tape()).nodes_, before);
    AAD::Number_ square = x * x;
    outer.FinishRecording();
    AAD::Adjoint(square) = 1.;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(AAD::Adjoint(x), 6.);
    outer.Close();
    ASSERT_EQ(result.Diagnostics()[0].stage_, CalibrationStage_::Value_::Evaluate);
}

TEST(CompoundedOisQuoteRiskTest, TestParRateRejectsAnnuityWithOppositeSign) {
    const auto spec = NegativeParRateSpec();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), Accuracy());
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    const auto resolved = snapshot.ResolveCompoundedOisParRate(instrument, tables, spec);
    ASSERT_TRUE(resolved.Available());
    ASSERT_EQ(resolved.plan_->Measure().annuitySign_, -1);
    // An admitted PayFixed contract has positive discounts and negative fixed-leg coefficients.
    try {
        static_cast<void>(CurveScriptInternal::Measure(*resolved.plan_, CurveScriptInternal::Affine_<double>{1., .03}));
        FAIL() << "Accepted a positive PayFixed annuity";
    } catch (const CalibrationError_& error) {
        const auto& d = error.Failure().diagnostic_;
        ASSERT_EQ(d.code_, "InvalidCalibrationAnnuity");
        ASSERT_EQ(d.field_, "/annuity");
        ASSERT_EQ(d.actual_, "1");
    }
    auto source = instrument->Definition()->Spec();
    for (auto& measure : source.measures_)
        if (measure.id_ == "ParRate")
            measure.annuitySign_ = 1;
    const auto changed = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    ASSERT_NO_FATAL_FAILURE(
        AssertParRateUnavailable(snapshot.ValueCompoundedOisParRateWithQuoteRisk(changed, tables, spec), "CompoundedOisSourceMismatch"));
}

TEST(CompoundedOisQuoteRiskTest, TestResolveRegisteredParRateOnBothSnapshotModes) {
    auto spec = RiskSpec("ObservationShift", "Shared", true);
    auto receiving = spec.compoundedOis_.back()->Spec();
    receiving.direction_ = "ReceiveFixed";
    receiving.minAbsAnnuity_ = .002;
    spec.compoundedOis_.back() = PrepareCompoundedOis(receiving, spec.valuation_);
    const auto fit = CalibrateCurveInstruments(spec, Options());
    for (const auto& tables : {spec.compoundedOis_.front(), spec.compoundedOis_.back()}) {
        const auto instrument = NewCompoundedOisScriptInstrument(tables);
        String_ planIdentity;
        for (const bool pvOnly : {false, true}) {
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto result = snapshot.ResolveCompoundedOisParRate(instrument, tables, spec);
            ASSERT_TRUE(result.Available());
            ASSERT_TRUE(result.diagnostics_.empty());
            const auto& plan = *result.plan_;
            ASSERT_EQ(plan.Measure().id_, "ParRate");
            ASSERT_EQ(plan.Measure().kind_, CalibrationMeasureKind_::Value_::ParAffineRate);
            ASSERT_EQ(plan.Measure().annuitySign_, tables->Spec().direction_ == "PayFixed" ? -1 : 1);
            ASSERT_EQ(plan.Measure().minAbsAnnuity_, tables->Spec().minAbsAnnuity_);
            ASSERT_EQ(plan.Capability().ruleId_, tables->RuleId());
            ASSERT_EQ(plan.Instrument()->InstrumentIdentity(), instrument->InstrumentIdentity());
            if (pvOnly)
                ASSERT_EQ(plan.PlanIdentity(), planIdentity);
            planIdentity = plan.PlanIdentity();
            ASSERT_EQ(result.quotes_.size(), 2u);
            ASSERT_EQ(result.quotes_[0].id_, "short");
            ASSERT_EQ(result.quotes_[0].ordinal_, 0);
            ASSERT_EQ(result.quotes_[1].id_, "long");
            ASSERT_EQ(result.quotes_[1].ordinal_, 1);
        }
    }
}

TEST(CompoundedOisQuoteRiskTest, TestParRateChecksTableIdentityWithColdAndWarmPreparationCache) {
    const auto spec = RiskSpec("Lookback", "Shared");
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    auto other = tables->Spec();
    other.coupons_.front().couponId_ = "renamed-coupon";
    const auto different = PrepareCompoundedOis(other, spec.valuation_);
    const auto sameScript = NewCompoundedOisScriptInstrument(different);
    ASSERT_EQ(sameScript->InstrumentIdentity(), instrument->InstrumentIdentity());
    ASSERT_NE(different->ObservationScheduleIdentity(), tables->ObservationScheduleIdentity());
    auto options = Options();
    options.preparationCache_ = std::make_shared<CalibrationPreparationCache_>();
    String_ planIdentity;
    for (const auto* cache : {"cold", "warm"}) {
        SCOPED_TRACE(cache);
        const auto fit = CalibrateCurveInstruments(spec, options);
        for (const bool pvOnly : {false, true}) {
            SCOPED_TRACE(pvOnly);
            const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto accepted = snapshot.ResolveCompoundedOisParRate(instrument, tables, spec);
            ASSERT_TRUE(accepted.Available());
            ASSERT_EQ(accepted.plan_->Measure().id_, "ParRate");
            ASSERT_EQ(accepted.plan_->Measure().kind_, CalibrationMeasureKind_::Value_::ParAffineRate);
            if (!planIdentity.empty())
                ASSERT_EQ(accepted.plan_->PlanIdentity(), planIdentity);
            planIdentity = accepted.plan_->PlanIdentity();
            const auto risk = snapshot.ValueCompoundedOisParRateWithQuoteRisk(instrument, tables, spec);
            ASSERT_TRUE(risk.Available());
            ASSERT_EQ(risk.SnapshotIdentity(), snapshot.Identity());
            ASSERT_NO_FATAL_FAILURE(AssertParRateUnavailable(snapshot.ValueCompoundedOisParRateWithQuoteRisk(sameScript, different, spec),
                                                             "CompoundedOisSourceMismatch"));
            ASSERT_NO_FATAL_FAILURE(
                AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(sameScript, different, spec), "CompoundedOisSourceMismatch"));
        }
    }
}

TEST(CompoundedOisQuoteRiskTest, TestParRateSelectsSnapshotQuotesAndRejectsInvalidRequests) {
    const auto spec = RiskSpec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    auto unknown = tables->Spec();
    unknown.instrumentId_ = "unregistered";
    const auto other = PrepareCompoundedOis(unknown, spec.valuation_);
    auto source = instrument->Definition()->Spec();
    source.source_.rows_.back().text_ += " ";
    const auto differentSource = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    const auto rebound = BindCalibrationInstrumentParameters(instrument, {{{"K", .045, CalibrationValueUnit_::Parse("Rate")}}, {}});
    auto stale = spec;
    auto values = stale.valuation_.fixings_->Values();
    values["USD-SOFR"][DateTime_(Date_(2024, 2, 29))] += .001;
    stale.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
    for (const bool pvOnly : {false, true}) {
        SCOPED_TRACE(pvOnly);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
        const auto resolve = [&](const CompoundedOisParRateRequest_& request) {
            return snapshot.ResolveCompoundedOisParRate(instrument, tables, spec, request);
        };
        const auto reversed = resolve({{"long", "short"}});
        ASSERT_TRUE(reversed.Available());
        ASSERT_EQ(reversed.quotes_.size(), 2u);
        ASSERT_EQ(reversed.quotes_[0].id_, "long");
        ASSERT_EQ(reversed.quotes_[0].ordinal_, 1);
        ASSERT_EQ(reversed.quotes_[1].id_, "short");
        ASSERT_EQ(reversed.quotes_[1].ordinal_, 0);
        const auto subset = resolve({{"long"}});
        ASSERT_TRUE(subset.Available());
        ASSERT_EQ(subset.quotes_.size(), 1u);
        ASSERT_EQ(subset.quotes_.front().ordinal_, 1);
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{"long", "long"}}), "ScriptCurveQuoteRiskDuplicateSelection"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{"short", "LONG"}}), "ScriptCurveQuoteRiskUnknownSelection"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(resolve({{}, true}), "ScriptCurveQuoteRiskLiveQuoteUnsupported"));
        ASSERT_NO_FATAL_FAILURE(AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(NewCompoundedOisScriptInstrument(other), other, spec),
                                                      "CompoundedOisUnregisteredInstrument"));
        for (const auto& changed : {differentSource, rebound})
            ASSERT_NO_FATAL_FAILURE(
                AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(changed, tables, spec), "CompoundedOisSourceMismatch"));
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(snapshot.ResolveCompoundedOisParRate(instrument, tables, stale), "ScriptCurveQuoteRiskIdentityMismatch"));
        const auto price = ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, {{}, "ParRate"});
        ASSERT_FALSE(price.Available());
        ASSERT_EQ(price.Diagnostics().size(), 1u);
        ASSERT_EQ(price.Diagnostics().front().code_, "ScriptCurveQuoteRiskUnsupportedMeasure");
    }
}

TEST(CompoundedOisQuoteRiskTest, TestFixedContractRequestsAndIdentityMismatchesStayUnavailable) {
    const auto spec = RiskSpec("ObservationShift", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    for (const ScriptCurveQuoteRiskRequest_ request :
         {ScriptCurveQuoteRiskRequest_{{"unknown"}}, {{"short", "short"}}, {{"SHORT"}}, {{}, "ParRate"}, {{}, "Price", true}})
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec, request)));
    auto unregistered = spec.compoundedOis_[2]->Spec();
    unregistered.instrumentId_ = "unknown-pv";
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(
        NewCompoundedOisScriptInstrument(PrepareCompoundedOis(unregistered, spec.valuation_)), snapshot, spec)));
    auto changed = spec.compoundedOis_[2]->Spec();
    changed.coupon_ += .001;
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(
        ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(PrepareCompoundedOis(changed, spec.valuation_)), snapshot, spec)));
    auto source = instrument->Definition()->Spec();
    source.source_.rows_.back().text_ += " ";
    const auto changedSource = NewScriptCalibrationInstrument({instrument->Id(), NewScriptInstrumentDefinition(source), {}, instrument->Pillar()});
    ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(changedSource, snapshot, spec)));
    const auto clone =
        ValueCalibrationInstrumentWithQuoteRisk(LoadCalibrationInstrumentJson(DumpCalibrationInstrumentJson(instrument)), snapshot, spec);
    ASSERT_TRUE(clone.Available());
}

TEST(CompoundedOisQuoteRiskTest, TestEveryFrozenOisContextDependencyIsChecked) {
    const auto spec = RiskSpec("Lookback", "Discount");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
    const Vector_<std::function<void(CurveInstrumentCalibrationSpec_*)>> changes{
        [](auto* s) {
            s->market_.overnightCurves_["USD.SOFR"].reset(new DiscountLogDF_("changed", "USD", {Date_(2024, 3, 1), Date_(2024, 3, 20)}, {0., -.03},
                                                                             DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR")));
        },
        [](auto* s) { s->market_.routes_.back().useTargetCurve_ = true; },
        [](auto* s) { s->valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical"); },
        [](auto* s) {
            auto values = s->valuation_.fixings_->Values();
            values["USD-SOFR"][DateTime_(Date_(2024, 2, 29))] = .04;
            s->valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        },
        [](auto* s) {
            auto c = s->compoundedOis_[2]->Spec();
            c.calendar_ = HolidayCenterData_("other-calendar", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
            ReplaceTable(s, 2, c);
        },
        [](auto* s) { s->compoundedOis_.pop_back(); },
        [](auto* s) { *s = Bumped(*s, 0, 1e-4); }};
    for (const auto& change : changes) {
        auto context = spec;
        change(&context);
        ASSERT_FALSE(snapshot.ValidateContext(context).Passed());
        ASSERT_FALSE(snapshot.ParameterQuoteMapping(context).Available());
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, context)));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestMissingTablesCannotEnterEitherRiskEndpoint) {
    const auto original = RiskSpec("ObservationShift", "Discount");
    auto spec = original;
    spec.observations_ = {spec.observations_[0]};
    spec.geometry_.knotDates_ = {Date_(2024, 3, 1), Date_(2024, 3, 20)};
    spec.solver_.initialGuessPerNode_ = {-.002};
    spec.compoundedOis_.clear();
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(spec, Options()), {{1e-12}, 1e-10, 1e-12});
    ASSERT_TRUE(snapshot.RiskAvailable());
    for (const bool legacy : {false, true}) {
        auto contract = Contract(false, "None", legacy);
        const auto instrument = NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_));
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec)));
    }
    auto fit = CalibrateCurveInstruments(original, Options());
    auto wrong = *fit.resolvedSpec_;
    wrong.compoundedOis_.clear();
    fit.resolvedSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(wrong);
    const auto rejected = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(rejected.RiskAvailable());
    ASSERT_FALSE(rejected.Diagnostics().empty());
}

TEST(CompoundedOisQuoteRiskTest, TestSnapshotAndResultsOwnAllInputsBeyondTheirOwners) {
    for (const bool pvOnly : {false, true}) {
        std::optional<ScriptCurveQuoteRiskSnapshot_> snapshot;
        std::optional<ScriptCurveQuoteRiskResult_> available, unavailable;
        {
            auto spec = RiskSpec("ObservationShift", "Discount");
            auto curve = std::make_shared<DiscountLogDF_>("passive", "USD", spec.geometry_.knotDates_, Vector_<>{0., -.0002, -.02},
                                                          DayBasis_("ACT_365F"), LogDfScheme_("LOG_LINEAR"));
            spec.market_.overnightCurves_["USD.SOFR"] = Handle_<DiscountCurve_>(curve);
            auto quote = std::make_shared<CalibrationMarketQuote_>(*spec.observations_[1]->Quote());
            const auto row = spec.observations_[1];
            spec.observations_[1] =
                NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), Handle_<CalibrationMarketQuote_>(quote), row->ErrorScale());
            const auto fit = CalibrateCurveInstruments(spec, Options());
            snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy(pvOnly));
            const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[2]);
            available = ValueCalibrationInstrumentWithQuoteRisk(instrument, *snapshot, spec);
            ASSERT_TRUE(available->Available());
            auto contract = spec.compoundedOis_[2]->Spec();
            contract.instrumentId_ = "unregistered";
            unavailable = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(PrepareCompoundedOis(contract, spec.valuation_)),
                                                                  *snapshot, spec);
            const Vector_<> delta{.01, .01};
            curve->ApplyDX(delta.begin(), 1.);
            *quote = *NewCalibrationMarketQuote(quote->Id(), quote->Value() + .01, quote->Unit());
            spec.compoundedOis_.clear();
            const auto again = ValueCalibrationInstrumentWithQuoteRisk(instrument, *snapshot, *snapshot->ResolvedSpec());
            ASSERT_TRUE(again.Available());
            ASSERT_DOUBLE_EQ(again.PV(), available->PV());
            ASSERT_EQ(again.RawSensitivity(), available->RawSensitivity());
        }
        ASSERT_TRUE(snapshot->RiskAvailable());
        ASSERT_EQ(snapshot->MappingAvailable(), !pvOnly);
        snapshot.reset();
        ASSERT_TRUE(std::isfinite(available->PV()));
        ASSERT_EQ(available->RawSensitivity().size(), 2u);
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(*unavailable));
        ASSERT_EQ(unavailable->Diagnostics()[0].instrumentId_, std::optional<String_>("unregistered"));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestUnobservablePwcAndPwlfRemainUnavailable) {
    for (const auto* parameterization : {"PIECEWISE_CONSTANT_FWD", "PIECEWISE_LINEAR_FWD"}) {
        auto spec = RiskSpec("ObservationShift", "Overnight");
        spec.geometry_.parameterization_ = CurveParameterization_(parameterization);
        spec.geometry_.knotDates_ = {Date_(2024, 3, 5), Date_(2024, 3, 20)};
        spec.solver_.initialGuessPerNode_.clear();
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[1]);
        const PreparedCurveInstruments_ initial(spec, Options());
        const Vector_<> theta(initial.Describe().freeParameters_.size(), .02);
        const double quote = initial.Evaluate(theta, false).modelMeasures_[1];
        spec.observations_.clear();
        for (size_t i = 0; i < theta.size(); ++i) {
            const auto id = String::FromInt(static_cast<int>(i));
            spec.observations_.push_back(NewCalibrationObservation(
                "o" + id, instrument, "ParRate", NewCalibrationMarketQuote("q" + id, quote, CalibrationValueUnit_::Parse("Rate")), 1e-13));
        }
        spec.solver_.initialGuessPerNode_ = theta;
        const auto fit = CalibrateCurveInstruments(spec, Options());
        ASSERT_TRUE(fit.fitWithinTolerance_);
        auto accuracy = Accuracy();
        accuracy.residualAbsoluteLimits_ = Vector_<>(theta.size(), 1e-12);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        ASSERT_FALSE(snapshot.RiskAvailable());
        ASSERT_FALSE(snapshot.MappingAvailable());
        ASSERT_FALSE(snapshot.Diagnostics().empty());
        ASSERT_NO_FATAL_FAILURE(AssertUnavailable(ValueCalibrationInstrumentWithQuoteRisk(instrument, snapshot, spec)));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestRootConditionAndApproximateModeCannotUseFitSuccessAlone) {
    auto spec = RiskSpec("None", "Shared");
    const auto fit = CalibrateCurveInstruments(spec, Options());
    auto strict = Accuracy();
    strict.minReciprocalCondition_ = 1.;
    const auto conditioned = NewScriptCurveQuoteRiskSnapshot(fit, strict);
    ASSERT_FALSE(conditioned.RiskAvailable());
    ASSERT_EQ(conditioned.Diagnostics()[0].code_, "ScriptCurveQuoteRiskConditionRejected");
    for (auto& row : spec.observations_)
        row = NewCalibrationObservation(row->Id(), row->Instrument(), row->MeasureId(), row->Quote(), .1);
    const auto looseFit = CalibrateCurveInstruments(spec, Options());
    ASSERT_TRUE(looseFit.fitWithinTolerance_);
    const auto& tables = spec.compoundedOis_.back();
    const auto instrument = NewCompoundedOisScriptInstrument(tables);
    for (const bool pvOnly : {false, true}) {
        const auto loose = NewScriptCurveQuoteRiskSnapshot(looseFit, Accuracy(pvOnly));
        ASSERT_FALSE(loose.RiskAvailable());
        ASSERT_EQ(loose.Diagnostics()[0].code_, "ScriptCurveQuoteRiskRootResidual");
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(loose.ResolveCompoundedOisParRate(instrument, tables, spec), "ScriptCurveQuoteRiskRootResidual"));
    }
    spec.solver_.solveMode_ = CurveSolveMode_("APPROXIMATE");
    spec.solver_.fitWeight_ = 1.;
    spec.solver_.fitNormTolerance_ = .01;
    const auto approximateFit = CalibrateCurveInstruments(spec, Options());
    for (const bool pvOnly : {false, true}) {
        const auto approximate = NewScriptCurveQuoteRiskSnapshot(approximateFit, Accuracy(pvOnly));
        ASSERT_FALSE(approximate.RiskAvailable());
        ASSERT_EQ(approximate.Diagnostics()[0].code_, "ScriptCurveQuoteRiskUnsupportedMode");
        ASSERT_NO_FATAL_FAILURE(
            AssertParRateRejected(approximate.ResolveCompoundedOisParRate(instrument, tables, spec), "ScriptCurveQuoteRiskUnsupportedMode"));
    }
}

TEST(CompoundedOisQuoteRiskTest, TestTodayHistoryAndModelFutureIsolationUseFrozenFixings) {
    for (const auto& mode : CompoundedOisObservationModeListAll()) {
        auto spec = RiskSpec(mode.String(), "Discount");
        spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
        RebuildValuation(&spec);
        const double value =
            Oracle(spec.compoundedOis_[1]->Spec(), "Discount", Parameters(), false, spec.valuation_, PassiveParameters("Discount"))[1];
        spec = Bumped(spec, 1, value - spec.observations_[1]->Quote()->Value());
        ASSERT_NO_FATAL_FAILURE(AssertModes(spec, "Discount"));
    }
    auto original = RiskSpec("ObservationShift", "Shared");
    const auto before = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(original, Options()), Accuracy());
    const auto pv = NewCompoundedOisScriptInstrument(original.compoundedOis_[2]);
    const auto first = ValueCalibrationInstrumentWithQuoteRisk(pv, before, original);
    auto values = original.valuation_.fixings_->Values();
    values["USD-SOFR"][DateTime_(Date_(2024, 3, 1))] = -.5;
    values["USD-SOFR"][DateTime_(Date_(2024, 3, 5))] = -.4;
    original.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
    RebuildValuation(&original);
    const auto after = NewScriptCurveQuoteRiskSnapshot(CalibrateCurveInstruments(original, Options()), Accuracy());
    const auto second = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(original.compoundedOis_[2]), after, original);
    ASSERT_TRUE(first.Available());
    ASSERT_TRUE(second.Available());
    ASSERT_NE(before.ContextIdentity(), after.ContextIdentity());
    ASSERT_DOUBLE_EQ(first.PV(), second.PV());
    ASSERT_EQ(first.RawSensitivity(), second.RawSensitivity());
}

TEST(CompoundedOisQuoteRiskTest, TestHistoricalRatesRetainDiscountRiskAndSettledPriceHasZeroRisk) {
    for (const auto* role : {"Overnight", "Discount", "Shared"}) {
        auto spec = RiskSpec("ObservationShift", role);
        auto values = spec.valuation_.fixings_->Values();
        values["USD-SOFR"][DateTime_(Date_(2024, 2, 27))] = .02;
        values["USD-SOFR"][DateTime_(Date_(2024, 2, 28))] = .021;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_(values));
        RebuildValuation(&spec);
        auto contract = Contract(true, "None");
        contract.instrumentId_ = "history-pv";
        contract.coupons_ = {{"past-accrual", Date_(2024, 2, 27), Date_(2024, 2, 29), Date_(2024, 3, 6)}};
        contract.fixedLeg_ = {{Date_(2024, 2, 27), Date_(2024, 2, 29), Date_(2024, 3, 6), 2. / 365.}};
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        contract.instrumentId_ = "settled-pv";
        contract.coupons_ = {{"no-history-query", Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29)}};
        contract.fixedLeg_ = {{Date_(2024, 2, 26), Date_(2024, 2, 27), Date_(2024, 2, 29), 1. / 365.}};
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        contract = spec.compoundedOis_[3]->Spec();
        contract.instrumentId_ = "today-pv";
        contract.coupons_[0].payment_ = contract.fixedLeg_[0].payment_ = Date_(2024, 3, 1);
        spec.compoundedOis_.push_back(PrepareCompoundedOis(contract, spec.valuation_));
        const auto fit = CalibrateCurveInstruments(spec, Options());
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
        const auto historical = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[3]), snapshot, spec);
        const auto settled = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[4]), snapshot, spec);
        const auto today = ValueCalibrationInstrumentWithQuoteRisk(NewCompoundedOisScriptInstrument(spec.compoundedOis_[5]), snapshot, spec);
        ASSERT_TRUE(historical.Available());
        ASSERT_TRUE(settled.Available());
        ASSERT_TRUE(today.Available());
        ASSERT_DOUBLE_EQ(settled.PV(), 0.);
        ASSERT_EQ(settled.RawSensitivity(), (Vector_<>{0., 0.}));
        ASSERT_EQ(settled.DV01(), (Vector_<>{0., 0.}));
        const auto discount = String_(role) == "Overnight" ? PassiveParameters(role) : fit.parameters_;
        const double amount = 100. * ((1. + .02 / 360.) * (1. + .021 / 360.) - 1. - .02 * 2. / 365.);
        ASSERT_NEAR(historical.PV(), amount * std::exp(LogDiscount(Date_(2024, 3, 6), discount)), 1e-10);
        ASSERT_NEAR(today.PV(), amount, 1e-10);
        ASSERT_EQ(today.RawSensitivity(), (Vector_<>{0., 0.}));
        const double targetDiscount = String_(role) == "Overnight" ? 0. : 1.;
        ASSERT_NEAR(historical.NativeGradient()[0], targetDiscount * historical.PV() * 14. / 15., 1e-12);
        ASSERT_NEAR(historical.NativeGradient()[1], targetDiscount * historical.PV() / 15., 1e-12);
        const auto instrument = NewCompoundedOisScriptInstrument(spec.compoundedOis_[3]);
        for (const double h : {1e-4, 5e-5})
            for (int j = 0; j < 2; ++j) {
                const auto plus = CalibrateCurveInstruments(Bumped(spec, j, h), Options());
                const auto minus = CalibrateCurveInstruments(Bumped(spec, j, -h), Options());
                ASSERT_NO_FATAL_FAILURE(AssertRoot(plus));
                ASSERT_NO_FATAL_FAILURE(AssertRoot(minus));
                const double fd =
                    (DirectPrice(instrument, spec.compoundedOis_[3], plus) - DirectPrice(instrument, spec.compoundedOis_[3], minus)) / (2. * h);
                ASSERT_NEAR(historical.RawSensitivity()[j] / 100., fd / 100., 1e-7 + 1e-5 * std::abs(fd / 100.));
                ASSERT_NEAR(historical.DV01()[j] / 100., fd * 1e-4 / 100., 1e-11 + 1e-5 * std::abs(fd * 1e-4 / 100.));
            }
        const double magnitude = std::abs(historical.RawSensitivity()[0]) + std::abs(historical.RawSensitivity()[1]);
        ASSERT_EQ(magnitude == 0., String_(role) == "Overnight");
    }
}

TEST(CompoundedOisQuoteRiskTest, TestHistoricalLockoutAnchorCannotSupplyMissingOnTargetRank) {
    auto spec = RiskSpec("Lockout", "Overnight");
    spec.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("RequireHistorical");
    RebuildValuation(&spec);
    for (size_t i = 0; i < 2; ++i) {
        const double value = Oracle(spec.compoundedOis_[i]->Spec(), "Overnight", Parameters(), false, spec.valuation_)[1];
        spec = Bumped(spec, static_cast<int>(i), value - spec.observations_[i]->Quote()->Value());
    }
    spec.solver_.initialGuessPerNode_ = Parameters();
    const auto fit = CalibrateCurveInstruments(spec, Options());
    ASSERT_TRUE(fit.fitWithinTolerance_);
    const auto snapshot = NewScriptCurveQuoteRiskSnapshot(fit, Accuracy());
    ASSERT_FALSE(snapshot.RiskAvailable());
    ASSERT_FALSE(snapshot.MappingAvailable());
    ASSERT_EQ(snapshot.Diagnostics()[0].code_, "ScriptCurveQuoteRiskJacobianRejected");
}

TEST(CompoundedOisQuoteRiskTest, TestAllModesThreeTargetsAndZeroRateMatchCompleteQuoteRecalibration) {
    for (const bool zeroRate : {false, true})
        for (const auto& mode : CompoundedOisObservationModeListAll())
            for (const auto* role : {"Overnight", "Discount", "Shared"}) {
                SCOPED_TRACE(mode.String());
                SCOPED_TRACE(role);
                ASSERT_NO_FATAL_FAILURE(AssertModes(RiskSpec(mode.String(), role, false, zeroRate), role, zeroRate));
            }
}

TEST(CompoundedOisQuoteRiskTest, TestMixedLegacyToolsAndLegacyPvMatchCompleteQuoteRecalibration) {
    for (const auto& mode : CompoundedOisObservationModeListAll())
        for (const auto* role : {"Overnight", "Discount", "Shared"}) {
            auto spec = RiskSpec(mode.String(), role, true);
            ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
            auto pv = Contract(false, "None", true);
            pv.instrumentId_ = "registered-pv";
            pv.coupon_ = .031;
            spec.compoundedOis_[2] = PrepareCompoundedOis(pv, spec.valuation_);
            ASSERT_NO_FATAL_FAILURE(AssertModes(spec, role));
        }
}
