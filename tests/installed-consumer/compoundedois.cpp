#include <cmath>
#include <iomanip>
#include <iostream>

#ifdef DAL_TEST_PUBLIC_OIS
#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/scriptcurvequoterisk.hpp>
#else
#include <dal/curve/curvescript.hpp>
#include <dal/curve/curvescriptevaluation.hpp>
#include <dal/curve/scriptcurvequoterisk.hpp>
#endif
#include <dal/platform/initall.hpp>

namespace {
    using namespace Dal;
    CompoundedOisScriptSpec_ Contract(const String_& mode) {
        const Date_ start(2024, 3, 5), end(2024, 3, 8), pay(2024, 3, 12);
        CompoundedOisScriptSpec_ trade;
        trade.instrumentId_ = "installed-" + mode;
        trade.direction_ = "PayFixed";
        trade.notional_ = 100.;
        trade.coupon_ = .02;
        trade.currency_ = trade.collateralCurrency_ = Ccy_("USD");
        trade.collateralType_ = CollateralType_("OIS");
        trade.discountRoute_ = "USD.OIS";
        trade.projectionRoute_ = "USD.SOFR";
        trade.fixingIndex_ = "USD-SOFR";
        trade.observationBasis_ = DayBasis_("ACT_360");
        trade.fixedBasis_ = DayBasis_("ACT_365F");
        trade.calendar_ = HolidayCenterData_("explicit", {Date_(2024, 3, 4)}, {Date_(2024, 3, 9)});
        trade.ruleId_ = "dal.compounded-ois-cashflows/2";
        trade.observationConvention_ = CompoundedOisObservationConvention_{
            CompoundedOisObservationMode_(mode), mode == "Lookback" || mode == "ObservationShift" ? 1 : 0, mode == "Lockout" ? 1 : 0};
        trade.fixedLeg_ = {{start, end, pay, 3. / 365.}};
        trade.coupons_ = {{"coupon", start, end, pay}};
        return trade;
    }
    CurveInstrumentCalibrationSpec_ BaseContext() {
        const Date_ today(2024, 3, 1);
        CurveInstrumentCalibrationSpec_ spec;
        spec.geometry_.today_ = today;
        spec.geometry_.ccy_ = "USD";
        spec.geometry_.knotDates_ = {today, Date_(2024, 3, 21)};
        spec.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        spec.geometry_.calibrateDiscountCurve_ = false;
        spec.geometry_.targetOvernightRoute_ = "USD.SOFR";
        spec.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        spec.valuation_.evaluationDate_ = today;
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_());
        return spec;
    }
    CurveInstrumentCalibrationSpec_ Context(const String_& mode) {
        auto spec = BaseContext();
        const auto prepared = PrepareCompoundedOis(Contract(mode), spec.valuation_);
        const auto loaded = LoadPreparedCompoundedOisJson(DumpPreparedCompoundedOisJson(prepared));
        REQUIRE(loaded->PreparationIdentity() == prepared->PreparationIdentity(), "Installed /2 archive identity");
        spec.compoundedOis_ = {loaded};
        spec.observations_ = {NewCalibrationObservation("par", NewCompoundedOisScriptInstrument(loaded), "ParRate",
                                                        NewCalibrationMarketQuote("q", .025, CalibrationValueUnit_::Parse("Rate")), 1e-10)};
        return spec;
    }
    CurveInstrumentCalibrationResult_ Fit(const CurveInstrumentCalibrationSpec_& spec) {
        CurveInstrumentCalibrationOptions_ options;
        options.jacobianMode_ = CalibrationDerivative_("AAD");
        auto result = CalibrateCurveInstruments(spec, options);
        REQUIRE(result.fitWithinTolerance_ && std::abs(result.rawResiduals_[0]) <= 1e-12, "Installed EXACT root");
        return result;
    }
    CurveScriptEvaluationResult_ Price(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationResult_& fit) {
        CurveScriptEvaluationRequest_ request;
        request.geometry_ = spec.geometry_;
        request.market_ = spec.market_;
        request.valuation_ = spec.valuation_;
        request.parameters_ = fit.parameters_;
        request.compoundedOis_ = spec.compoundedOis_[0];
        return EvaluateCurveScriptInstrument(spec.observations_[0]->Instrument(), request);
    }
    CurveInstrumentCalibrationSpec_ Bumped(CurveInstrumentCalibrationSpec_ spec, double step) {
        const auto& row = *spec.observations_[0];
        spec.observations_[0] =
            NewCalibrationObservation(row.Id(), row.Instrument(), row.MeasureId(),
                                      NewCalibrationMarketQuote("q", row.Quote()->Value() + step, row.Quote()->Unit()), row.ErrorScale());
        return spec;
    }
    struct Owned_ {
        ScriptCurveQuoteRiskResult_ risk_, rejected_;
        CurveScriptEvaluationResult_ evaluation_;
    };
#ifdef DAL_TEST_PUBLIC_OIS
    CompoundedOisParRateRiskResult_ LookbackRisk(bool highFloor, bool pvOnly) {
        auto spec = BaseContext();
        const Date_ start(2024, 3, 1), middle(2024, 3, 6), end(2024, 3, 12), pay(2024, 3, 13);
        spec.geometry_.knotDates_ = {start, Date_(2024, 3, 5), Date_(2024, 3, 20)};
        spec.valuation_.fixings_.reset(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 2, 29)), -.012}}}}));
        auto trade = Contract("None");
        trade.fixedLeg_ = {{start, middle, middle, 5. / 365.}};
        trade.coupons_ = {{"first", start, middle, middle}};
        for (const String_ id : {"short", "long"}) {
            if (id == "long") {
                trade.fixedLeg_.push_back({middle, end, pay, 6. / 365.});
                trade.coupons_.push_back({"second", middle, end, pay});
            }
            trade.instrumentId_ = id;
            const auto tables = PrepareCompoundedOis(trade, spec.valuation_);
            spec.compoundedOis_.push_back(tables);
            const auto quote =
                NewCalibrationMarketQuote(id, id == "short" ? .030179570014802 : .0362870928259698, CalibrationValueUnit_::Parse("Rate"));
            spec.observations_.push_back(NewCalibrationObservation(id, NewCompoundedOisScriptInstrument(tables), "ParRate", quote, 1e-13));
        }
        trade.instrumentId_ = "registered-par";
        trade.observationConvention_ = Contract("Lookback").observationConvention_;
        trade.coupons_.back().end_ = Date_(2024, 3, 11);
        trade.fixedLeg_ = {{start, Date_(2024, 3, 11), Date_(2024, 3, 15), 10. / 365.}};
        if (highFloor)
            trade.minAbsAnnuity_ = 100.;
        const auto tables = PrepareCompoundedOis(trade, spec.valuation_);
        spec.compoundedOis_.push_back(tables);
        const auto snapshot = NewScriptCurveQuoteRiskSnapshot(Fit(spec), {{1e-12, 1e-12}, 1e-10, 1e-12, 1e-12, pvOnly});
        return ValueCompoundedOisParRateWithQuoteRisk(NewCompoundedOisScriptInstrument(tables), tables, snapshot, spec, {{"long", "short"}});
    }
    void VerifyParRate() {
        for (const bool pvOnly : {false, true}) {
            const auto result = LookbackRisk(false, pvOnly);
            REQUIRE(result.Available() && !result.SnapshotIdentity().empty(), "Installed owning ParRate result");
            const auto summary = ScriptCurveQuoteRiskTable(result, "Summary"), buckets = ScriptCurveQuoteRiskTable(result, "Sensitivity");
            REQUIRE(Cell::ToString(summary(3, 1)) == result.SnapshotIdentity(), "Installed snapshot identity");
            REQUIRE(std::abs(Cell::ToDouble(summary(2, 1)) - .01856212179263) < 1e-12, "Installed decimal par rate");
            REQUIRE(buckets.Rows() == 3 && buckets.Cols() == 3 && Cell::ToString(buckets(0, 2)) == "sensitivity_bp_per_bp", "Installed bucket units");
            REQUIRE(Cell::ToString(buckets(1, 1)) == "long" && Cell::ToString(buckets(2, 1)) == "short", "Installed selected quote order");
            REQUIRE(Cell::ToDouble(buckets(1, 0)) == 1 && Cell::ToDouble(buckets(2, 0)) == 0, "Installed snapshot ordinals");
            REQUIRE(std::abs(Cell::ToDouble(buckets(1, 2)) - .87138316352446) < 1e-10 &&
                        std::abs(Cell::ToDouble(buckets(2, 2)) + .27115795540455) < 1e-10,
                    "Installed signed Lookback buckets");
            std::cout << std::setprecision(14) << "Lookback pv_only=" << pvOnly << " rate=" << result.ParRate()
                      << " long=" << result.SensitivityBpPerBp()[0] << " short=" << result.SensitivityBpPerBp()[1]
                      << " snapshot=" << result.SnapshotIdentity() << '\n';
        }
        const auto failure = LookbackRisk(true, false);
        REQUIRE(!failure.Available() && !failure.SnapshotIdentity().empty(), "Installed owning unavailable result");
        const auto summary = ScriptCurveQuoteRiskTable(failure, "Summary"), buckets = ScriptCurveQuoteRiskTable(failure, "Sensitivity");
        const auto errors = ScriptCurveQuoteRiskTable(failure, "Diagnostics");
        REQUIRE(Cell::IsEmpty(summary(2, 1)) && buckets.Rows() == 3 && Cell::IsEmpty(buckets(1, 2)) && Cell::IsEmpty(buckets(2, 2)),
                "Installed unavailable rate and slopes must be blank");
        REQUIRE(Cell::ToString(errors(1, 0)) == "InvalidCalibrationAnnuity" && Cell::ToString(errors(1, 3)) == "/annuity",
                "Installed annuity diagnostic");
        std::cout << "High floor: " << failure.Diagnostics().front().code_ << " " << *failure.Diagnostics().front().field_ << '\n';
    }
    void VerifyAudit(const Handle_<PreparedCompoundedOis_>& tables, const CurveScriptEvaluationResult_& evaluation) {
        const auto prepared = CompoundedOisObservationScheduleTable(tables);
        const auto evaluated = CurveScriptEvaluationTable(evaluation, "observation_schedule");
        REQUIRE(prepared.Rows() == 4 && prepared.Cols() == 22 && evaluated.Cols() == 26, "Installed public audit shape");
        for (int i = 0; i < prepared.Rows(); ++i)
            for (int j = 0; j < prepared.Cols(); ++j)
                REQUIRE(prepared(i, j) == evaluated(i, j), "Installed audit prefix");
    }
#endif
    Owned_ Run(const String_& mode) {
        const auto spec = Context(mode);
        const auto fit = Fit(spec);
        const auto& instrument = spec.observations_[0]->Instrument();
        ScriptCurveQuoteRiskAccuracy_ accuracy{{1e-12}, 1e-10, 1e-12};
        const auto full = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        REQUIRE(full.RiskAvailable() && full.MappingAvailable(), "Installed full mapping");
        const auto result = ValueCalibrationInstrumentWithQuoteRisk(instrument, full, spec);
        accuracy.pvOnly_ = true;
        const auto lean = NewScriptCurveQuoteRiskSnapshot(fit, accuracy);
        const auto pvOnly = ValueCalibrationInstrumentWithQuoteRisk(instrument, lean, spec);
        REQUIRE(lean.RiskAvailable() && !lean.MappingAvailable() && result.Available() && pvOnly.Available(), "Installed PV-only risk");
        REQUIRE(result.RawSensitivity() == pvOnly.RawSensitivity(), "Installed full/PV-only parity");
        for (const double step : {1e-4, 5e-5}) {
            const auto up = Bumped(spec, step), down = Bumped(spec, -step);
            const double reference = (Price(up, Fit(up)).values_[0] - Price(down, Fit(down)).values_[0]) / (2 * step);
            REQUIRE(std::abs(result.RawSensitivity()[0] - reference) / 100. <= 1e-7 + 1e-5 * std::abs(reference / 100.), "Installed quote FD");
        }
        REQUIRE(result.DV01()[0] == result.RawSensitivity()[0] * 1e-4, "Installed signed DV01 units");
        ScriptCurveQuoteRiskRequest_ request;
        request.liveQuotePayoff_ = true;
        const auto rejected = ValueCalibrationInstrumentWithQuoteRisk(instrument, lean, spec, request);
        REQUIRE(!rejected.Available() && rejected.Diagnostics()[0].code_ == "ScriptCurveQuoteRiskLiveQuoteUnsupported", "Installed rejection");
        const auto evaluation = Price(spec, fit);
#ifdef DAL_TEST_PUBLIC_OIS
        VerifyAudit(spec.compoundedOis_[0], evaluation);
#endif
        return {result, rejected, evaluation};
    }
} // namespace

int main() {
    try {
        Dal::RegisterAll_::Init();
#ifdef DAL_TEST_PUBLIC_OIS
        VerifyParRate();
#endif
        for (const Dal::String_ mode : {"None", "Lookback", "ObservationShift", "Lockout"}) {
            const auto owned = Run(mode);
            REQUIRE(owned.evaluation_.observationSchedule_.size() == 3 && owned.evaluation_.observationMode_ == mode, "Installed owning audit");
            const auto& row = owned.evaluation_.observationSchedule_[0];
            REQUIRE(row.row_ == 0 && row.sourceRow_ == 4 && row.statement_ == 1, "Installed audit coordinate bases");
            std::cout << mode << " PV=" << owned.risk_.PV() << " raw=" << owned.risk_.RawSensitivity()[0] << " DV01=" << owned.risk_.DV01()[0] << " "
                      << owned.rejected_.Diagnostics()[0].code_ << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
