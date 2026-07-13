#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/xccycalibration.hpp>
#include <dal/curve/xccyjointcalibration.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/platform/initall.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

namespace {
    constexpr int kWarmups = 3;
    constexpr int kRepeats = 10;
    constexpr int kPrecomputeBatch = 2000;
    constexpr int kPriceBatch = 2000;
    constexpr int kBasketPasses = 250;
    constexpr double kFxSpot = 1.10;

    using SwapHandle_ = Handle_<CrossCurrencySwap_>;
    using RateHandle_ = Handle_<CrossCurrencySwap_::Rate_>;

    struct Fixture_ {
        Date_ today_;
        DateTime_ valuationTime_;
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        Vector_<Date_> basisKnots_;
        CrossCurrencyConvention_ convention_;
        Handle_<MarketFixingSnapshot_> fixings_;
    };

    struct PricingCase_ {
        std::string label_;
        SwapHandle_ swap_;
        RateHandle_ rate_;
        std::vector<RateHandle_> basket_;
        CrossCurrencyMarket_ market_;
    };

    struct JointCurrencyFixture_ {
        JointCurrencyCurveSpec_ spec_;
        Handle_<CurveBlock_> market_;
        RateIndexConvention_ index_;
    };

    Bench::Result_ Normalize(const Bench::Result_& raw, const std::string& name, int64_t divisor) {
        REQUIRE(divisor > 0, "XCCY benchmark normalization divisor must be positive");
        return Bench::Result_{name, raw.medianNs / divisor, raw.minNs / divisor, raw.maxNs / divisor, raw.repeats};
    }

    Handle_<DiscountCurve_> MakeFlatCurve(const String_& name, const String_& ccy, const Date_& today, double rate) {
        const Vector_<Date_> knots = {
            Date::AddMonths(today, 6),  Date::AddMonths(today, 12),  Date::AddMonths(today, 24),
            Date::AddMonths(today, 60), Date::AddMonths(today, 120), Date::AddMonths(today, 240),
        };
        const Vector_<> values(knots.size(), rate);
        return Handle_<DiscountCurve_>(NewDiscountPWLF(name, ccy, PiecewiseLinear_(knots, values, values)));
    }

    Handle_<CurveBlock_> MakeBlock(const String_& name, const String_& ccy, const Date_& today, double rate) {
        return Handle_<CurveBlock_>(new CurveBlock_(MakeFlatCurve(name, ccy, today, rate)));
    }

    Handle_<CurveBlock_> MakePwcBlock(const String_& name, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<>& parameters) {
        const Handle_<DiscountCurve_> curve(NewDiscountPWC(name, ccy.String(), PiecewiseConstant_(knots, parameters)));
        return Handle_<CurveBlock_>(new CurveBlock_(curve));
    }

    RateIndexConvention_ MakeIndex(bool useProjectionCurve = true) {
        RateIndexConvention_ result;
        result.spotLag_ = 0;
        result.fixingLag_ = 0;
        result.useProjectionCurve_ = useProjectionCurve;
        result.forecastTenor_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Holidays::None();
        result.accrualHolidays_ = Holidays::None();
        result.collateral_ = CollateralType_(CollateralType_::Value_::OIS);
        return result;
    }

    RateLegConvention_ MakeLeg() {
        RateLegConvention_ result;
        result.paymentLag_ = 0;
        result.paymentFrequency_ = PeriodLength_("3M");
        result.dayBasis_ = DayBasis_("ACT_365F");
        result.businessDayConvention_ = BizDayConvention_("Unadjusted");
        result.paymentConvention_ = BizDayConvention_("Unadjusted");
        result.accrualHolidays_ = Holidays::None();
        result.paymentHolidays_ = Holidays::None();
        return result;
    }

    Fixture_ MakeFixture() {
        Fixture_ result;
        result.today_ = Date_(2024, 1, 15);
        // Midnight keeps same-day coupon/reset fixings forward-looking for the future-start cases,
        // matching the legacy benchmark's date-only valuation context.
        result.valuationTime_ = DateTime_(result.today_);
        result.domesticBlock_ = MakeBlock("xccy_perf_usd", "USD", result.today_, 0.02);
        result.foreignBlock_ = MakeBlock("xccy_perf_eur", "EUR", result.today_, 0.01);
        result.basisKnots_ = {
            Date::AddMonths(result.today_, 6),  Date::AddMonths(result.today_, 12),  Date::AddMonths(result.today_, 24),
            Date::AddMonths(result.today_, 60), Date::AddMonths(result.today_, 120),
        };
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = MakeIndex();
        result.convention_.domesticLeg_ = MakeLeg();
        result.convention_.foreignIndex_ = MakeIndex();
        result.convention_.foreignLeg_ = MakeLeg();
        result.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        return result;
    }

    CrossCurrencySwapConfig_ MakeConfig(const Fixture_& fixture, XccyNotionalMode_ mode) {
        CrossCurrencySwapConfig_ result;
        result.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.convention_ = fixture.convention_;
        result.notionalMode_ = mode;
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingConvention_ = BizDayConvention_("Unadjusted");
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = {"USD-XCCY-PERF-3M", 11, 0};
        result.foreignRateFixing_ = {"EUR-XCCY-PERF-3M", 11, 0};
        return result;
    }

    CrossCurrencyMarket_ MakeMarket(const Fixture_& fixture, double basisRate) {
        // Keep the legacy fixed/basis-only benchmark context byte-for-byte comparable with the
        // original xccy_perf baseline. Reset-aware cases are future-looking at this date-only
        // valuation; only the started MTM case below needs an explicit intraday snapshot.
        CrossCurrencyMarket_ result(fixture.domesticBlock_, fixture.foreignBlock_, kFxSpot);
        if (basisRate != 0.0) {
            const Vector_<> values(fixture.basisKnots_.size(), basisRate);
            result.SetBasisCurve(Handle_<DiscountCurve_>(NewDiscountPWC("xccy_perf_basis", "USD", PiecewiseConstant_(fixture.basisKnots_, values))));
        }
        return result;
    }

    SwapHandle_ MakeFixedSwap(const Fixture_& fixture, int maturityMonths, double marketRate = 0.0) {
        return SwapHandle_(new CrossCurrencySwap_(fixture.today_, fixture.today_, Date::AddMonths(fixture.today_, maturityMonths), marketRate,
                                                  CurrencyPair_(Ccy_("USD"), Ccy_("EUR")), 110.0, 100.0, fixture.convention_));
    }

    SwapHandle_ MakeResetSwap(const Fixture_& fixture, int maturityMonths, XccyNotionalMode_ mode, double marketRate = 0.0) {
        return SwapHandle_(new CrossCurrencySwap_(fixture.today_, fixture.today_, Date::AddMonths(fixture.today_, maturityMonths), marketRate,
                                                  MakeConfig(fixture, mode)));
    }

    std::vector<SwapHandle_> MakePricingBasket(const Fixture_& fixture, XccyNotionalMode_ mode) {
        std::vector<SwapHandle_> result;
        result.reserve(10);
        for (int year = 1; year <= 10; ++year) {
            result.push_back(mode == XccyNotionalMode_::Value_::FIXED ? MakeFixedSwap(fixture, 12 * year) : MakeResetSwap(fixture, 12 * year, mode));
        }
        return result;
    }

    std::vector<RateHandle_> PrecomputeAll(const std::vector<SwapHandle_>& instruments) {
        std::vector<RateHandle_> result;
        result.reserve(instruments.size());
        for (const auto& instrument : instruments)
            result.push_back(instrument->Precompute());
        return result;
    }

    PricingCase_ MakePricingCase(const Fixture_& fixture, const std::string& label, XccyNotionalMode_ mode) {
        PricingCase_ result{label, mode == XccyNotionalMode_::Value_::FIXED ? MakeFixedSwap(fixture, 120) : MakeResetSwap(fixture, 120, mode),
                            RateHandle_(), std::vector<RateHandle_>(), MakeMarket(fixture, 0.0020)};
        result.rate_ = result.swap_->Precompute();
        result.basket_ = PrecomputeAll(MakePricingBasket(fixture, mode));
        return result;
    }

    PricingCase_ MakeInProgressMtmCase(const Fixture_& fixture) {
        const DateTime_ valuationTime(fixture.today_, 12, 0);
        const Date_ start = Date::AddMonths(fixture.today_, -3);
        const Date_ maturity = Date::AddMonths(start, 120);
        const CrossCurrencySwapConfig_ config = MakeConfig(fixture, XccyNotionalMode_::Value_::MARK_TO_MARKET);
        const SwapHandle_ swap(new CrossCurrencySwap_(start, start, maturity, 0.0, config));
        const XccyCashflowPlan_ plan = BuildXccyCashflowPlan(start, maturity, config);

        MarketFixingSnapshot_::values_t values;
        for (const auto& request : RequiredHistoricalFixings(plan, valuationTime)) {
            if (request.indexName_ == config.domesticRateFixing_.indexName_)
                values[request.indexName_][request.fixingTime_] = 0.04;
            else if (request.indexName_ == config.foreignRateFixing_.indexName_)
                values[request.indexName_][request.fixingTime_] = 0.03;
            else
                values[request.indexName_][request.fixingTime_] = 1.20;
        }
        const Handle_<MarketFixingSnapshot_> fixings(new MarketFixingSnapshot_(values));
        CrossCurrencyMarket_ market(fixture.domesticBlock_, fixture.foreignBlock_, kFxSpot, valuationTime, Ccy_("USD"), fixings);
        const Vector_<> basisValues(fixture.basisKnots_.size(), 0.0020);
        market.SetBasisCurve(
            Handle_<DiscountCurve_>(NewDiscountPWC("xccy_perf_started_basis", "USD", PiecewiseConstant_(fixture.basisKnots_, basisValues))));
        return PricingCase_{"in-progress MTM", swap, swap->Precompute(), std::vector<RateHandle_>(), market};
    }

    CrossCurrencyCalibrationSpec_ MakeCalibrationSpec(const Fixture_& fixture) {
        const CrossCurrencyMarket_ quoteMarket = MakeMarket(fixture, 0.0020);
        const Vector_<int> maturities = {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 108, 120};

        CrossCurrencyCalibrationSpec_ result;
        result.today_ = fixture.today_;
        result.basisPair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        result.domesticCurveBlock_ = fixture.domesticBlock_;
        result.foreignCurveBlock_ = fixture.foreignBlock_;
        result.fxSpot_ = kFxSpot;
        result.knotDates_ = fixture.basisKnots_;
        result.tolerance_ = 1.0e-8;
        result.fitTolerance_ = 1.0e-7;
        result.initialGuess_ = 0.0;
        result.instruments_.reserve(maturities.size());
        for (const int maturity : maturities) {
            const SwapHandle_ prototype = MakeFixedSwap(fixture, maturity);
            const double quote = (*prototype->Precompute())(quoteMarket);
            result.instruments_.push_back(MakeFixedSwap(fixture, maturity, quote));
        }
        return result;
    }

    Handle_<YCInstrument_> QuotedDeposit(const Date_& today, const Date_& maturity, const RateIndexConvention_& index, const CurveBlock_& market) {
        const Handle_<YCInstrument_> prototype(new Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Handle_<YieldCurve_>()))(market);
        return Handle_<YCInstrument_>(new Deposit_(today, today, maturity, quote, index));
    }

    JointCurrencyFixture_ MakeJointCurrency(
        const Date_& today, const Ccy_& ccy, const Vector_<Date_>& knots, const Vector_<Date_>& maturities, const Vector_<>& parameters) {
        JointCurrencyFixture_ result;
        result.index_ = MakeIndex(false);
        result.market_ = MakePwcBlock(String_("xccy_perf_true_") + ccy.String(), ccy, knots, parameters);

        JointCurveDeclaration_ declaration;
        declaration.curveName_ = String_("xccy_perf_") + ccy.String() + "_ois";
        declaration.knotDates_ = knots;
        declaration.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
        declaration.calibrateDiscountCurve_ = true;
        declaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        declaration.smoothingWeight_ = 1.0;
        for (const auto& maturity : maturities)
            declaration.instruments_.push_back(QuotedDeposit(today, maturity, result.index_, *result.market_));

        result.spec_.ccy_ = ccy;
        result.spec_.liborBasis_ = DayBasis_("ACT_365F");
        result.spec_.curves_ = {declaration};
        return result;
    }

    CrossCurrencySwapConfig_ MakeJointXccyConfig(const CurrencyPair_& pair,
                                                 const RateIndexConvention_& domesticIndex,
                                                 const RateIndexConvention_& foreignIndex,
                                                 XccyNotionalMode_ mode) {
        CrossCurrencySwapConfig_ result;
        result.pair_ = pair;
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.notionalMode_ = mode;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = domesticIndex;
        result.convention_.foreignIndex_ = foreignIndex;
        result.convention_.domesticLeg_ = MakeLeg();
        result.convention_.foreignLeg_ = MakeLeg();
        result.fxReset_.fixingLag_ = 0;
        result.fxReset_.fixingHolidays_ = Holidays::None();
        result.fxReset_.fixingConvention_ = BizDayConvention_("Unadjusted");
        result.fxReset_.fixingHour_ = 11;
        result.fxReset_.fixingMinute_ = 0;
        result.domesticRateFixing_ = {"USD-XCCY-JOINT-PERF", 11, 0};
        result.foreignRateFixing_ = {"EUR-XCCY-JOINT-PERF", 11, 0};
        return result;
    }

    JointXccyCalibrationSpec_ MakeJointCalibrationSpec(const Fixture_& fixture) {
        const Vector_<Date_> knots = {
            Date::AddMonths(fixture.today_, 6),  Date::AddMonths(fixture.today_, 18),  Date::AddMonths(fixture.today_, 36),
            Date::AddMonths(fixture.today_, 60), Date::AddMonths(fixture.today_, 120),
        };
        const Vector_<Date_> ycMaturities = {
            Date::AddMonths(fixture.today_, 12), Date::AddMonths(fixture.today_, 24),  Date::AddMonths(fixture.today_, 48),
            Date::AddMonths(fixture.today_, 72), Date::AddMonths(fixture.today_, 120),
        };
        const JointCurrencyFixture_ domestic =
            MakeJointCurrency(fixture.today_, Ccy_("USD"), knots, ycMaturities, {0.015, 0.016, 0.017, 0.018, 0.019});
        const JointCurrencyFixture_ foreign =
            MakeJointCurrency(fixture.today_, Ccy_("EUR"), knots, ycMaturities, {0.010, 0.011, 0.012, 0.013, 0.014});
        const Vector_<> basisParameters = {0.0010, 0.0014, 0.0018, 0.0022, 0.0026};
        const CurrencyPair_ pair(Ccy_("USD"), Ccy_("EUR"));
        CrossCurrencyMarket_ quoteMarket(domestic.market_, foreign.market_, kFxSpot, fixture.valuationTime_, pair.domestic_, fixture.fixings_);
        quoteMarket.SetBasisCurve(Handle_<DiscountCurve_>(
            NewDiscountPWC("xccy_perf_true_joint_basis", pair.domestic_.String(), PiecewiseConstant_(knots, basisParameters))));

        JointXccyCalibrationSpec_ result;
        result.valuationTime_ = fixture.valuationTime_;
        result.pair_ = pair;
        result.collateralCurrency_ = pair.domestic_;
        result.fxSpot_ = kFxSpot;
        result.domestic_ = domestic.spec_;
        result.foreign_ = foreign.spec_;
        result.basis_.curveName_ = "xccy_perf_joint_basis";
        result.basis_.knotDates_ = knots;
        result.basis_.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        result.basis_.smoothingWeight_ = 1.0;
        result.fixings_ = fixture.fixings_;
        result.tolerance_ = 1.0e-8;
        result.fitTolerance_ = 1.0e-6;
        result.initialGuess_ = 0.005;
        result.maxEvaluations_ = 500;
        result.maxRestarts_ = 40;
        result.solveMode_ = CurveSolveMode_::Value_::EXACT;

        const Vector_<int> xccyMaturities = {6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 96, 108, 120};
        for (int i = 0; i < static_cast<int>(xccyMaturities.size()); ++i) {
            const XccyNotionalMode_ mode = i % 2 == 0 ? XccyNotionalMode_::Value_::RESETTABLE : XccyNotionalMode_::Value_::MARK_TO_MARKET;
            const CrossCurrencySwapConfig_ config = MakeJointXccyConfig(pair, domestic.index_, foreign.index_, mode);
            const Date_ maturity = Date::AddMonths(fixture.today_, xccyMaturities[i]);
            const CrossCurrencySwap_ prototype(fixture.today_, fixture.today_, maturity, 0.0, config);
            result.basis_.instruments_.push_back(Handle_<CrossCurrencySwap_>(
                new CrossCurrencySwap_(fixture.today_, fixture.today_, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        return result;
    }

    CrossCurrencyCalibrationOptions_ BasisOptions(CurveJacobianMode_::Value_ mode, bool diagnostics) {
        CrossCurrencyCalibrationOptions_ result;
        result.jacobianMode_ = CurveJacobianMode_(mode);
        result.computeEffJacobianInverse_ = diagnostics;
        result.computeForwardJacobian_ = diagnostics;
        return result;
    }

    JointXccyCalibrationOptions_ JointOptions(CurveJacobianMode_::Value_ mode, bool diagnostics) {
        JointXccyCalibrationOptions_ result;
        result.jacobianMode_ = CurveJacobianMode_(mode);
        result.computeEffJacobianInverse_ = diagnostics;
        result.computeForwardJacobian_ = diagnostics;
        return result;
    }

    void ValidatePricing(const PricingCase_& pricing) {
        const double value = (*pricing.rate_)(pricing.market_);
        REQUIRE(std::isfinite(value) && std::fabs(value) > 0.0, "XCCY benchmark requires a finite, non-zero 10Y price");
        if (pricing.basket_.empty())
            return;
        REQUIRE(pricing.basket_.size() == 10, "XCCY benchmark requires exactly ten basket instruments");
        double checksum = 0.0;
        for (const auto& rate : pricing.basket_) {
            const double basketValue = (*rate)(pricing.market_);
            REQUIRE(std::isfinite(basketValue), "XCCY basket requires finite prices");
            checksum += basketValue;
        }
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "XCCY basket checksum must be finite and non-zero");
    }

    void ValidateBasisCalibration(const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationOptions_& options) {
        const auto result = CalibrateCrossCurrencyMarket(spec, options);
        REQUIRE(std::isfinite(result.diagnostics_.maxAbsResidual_) && result.diagnostics_.maxAbsResidual_ <= spec.tolerance_,
                "Basis-only XCCY calibration benchmark must reprice inside its configured tolerance");
        REQUIRE(result.diagnostics_.residuals_.size() == spec.instruments_.size(),
                "Basis-only XCCY calibration benchmark must report every instrument residual");
        REQUIRE(!result.fxForwardCurve_.forwards_.empty() && std::isfinite(result.fxForwardCurve_.forwards_.back()) &&
                    std::fabs(result.fxForwardCurve_.forwards_.back()) > 0.0,
                "Basis-only XCCY calibration benchmark requires a finite, non-zero dry-run checksum");
    }

    void ValidateJointCalibration(const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationOptions_& options) {
        const auto result = CalibrateJointXccyMarket(spec, options);
        const double tolerance = spec.solveMode_ == CurveSolveMode_::Value_::EXACT ? spec.tolerance_ : spec.fitTolerance_;
        REQUIRE(result.converged_, "Joint XCCY calibration benchmark must converge");
        REQUIRE(std::isfinite(result.jointMaxAbsResidual_) && result.jointMaxAbsResidual_ <= tolerance,
                "Joint XCCY calibration benchmark must reprice inside its configured tolerance");
        for (const auto& diagnostics : result.domesticDiagnostics_)
            REQUIRE(diagnostics.maxAbsResidual_ <= tolerance, "Joint XCCY domestic residual exceeds its configured tolerance");
        for (const auto& diagnostics : result.foreignDiagnostics_)
            REQUIRE(diagnostics.maxAbsResidual_ <= tolerance, "Joint XCCY foreign residual exceeds its configured tolerance");
        REQUIRE(result.xccyDiagnostics_.maxAbsResidual_ <= tolerance, "Joint XCCY basis residual exceeds its configured tolerance");
        REQUIRE(!result.fxForwardCurve_.forwards_.empty() && std::isfinite(result.fxForwardCurve_.forwards_.back()) &&
                    std::fabs(result.fxForwardCurve_.forwards_.back()) > 0.0 && result.solverEvaluations_ > 0,
                "Joint XCCY calibration benchmark requires a finite, non-zero dry-run checksum");
    }

    void RunPrecompute(const PricingCase_& pricing) {
        RateHandle_ sink = pricing.rate_;
        const std::string rawName = "XCCY " + pricing.label_ + " 10Y PRECOMPUTE";
        const auto raw = Bench::Run(
            rawName,
            [&]() {
                for (int i = 0; i < kPrecomputeBatch; ++i)
                    sink = pricing.swap_->Precompute();
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(sink.get());
        REQUIRE(sink, "XCCY PRECOMPUTE sink must retain a rate handle");
        Bench::Print(Normalize(raw, rawName + " / operation", kPrecomputeBatch));
    }

    void RunPrice(const PricingCase_& pricing) {
        double checksum = 0.0;
        const std::string rawName = "XCCY " + pricing.label_ + " 10Y PRICE";
        const auto raw = Bench::Run(
            rawName,
            [&]() {
                for (int i = 0; i < kPriceBatch; ++i)
                    checksum += (*pricing.rate_)(pricing.market_);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "XCCY PRICE checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, rawName + " / operation", kPriceBatch));
    }

    void RunBasket(const PricingCase_& pricing) {
        if (pricing.basket_.empty())
            return;
        double checksum = 0.0;
        const std::string rawName = "XCCY " + pricing.label_ + " 10-instrument basket";
        const auto raw = Bench::Run(
            rawName,
            [&]() {
                for (int pass = 0; pass < kBasketPasses; ++pass)
                    for (const auto& rate : pricing.basket_)
                        checksum += (*rate)(pricing.market_);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "XCCY BASKET checksum must be finite and non-zero");
        Bench::Print(Normalize(raw, "XCCY " + pricing.label_ + " 10-instrument BASKET / pass", kBasketPasses));
        Bench::Print(
            Normalize(raw, "XCCY " + pricing.label_ + " 10-instrument PER-INSTRUMENT", kBasketPasses * static_cast<int64_t>(pricing.basket_.size())));
    }

    void RunPricing(const PricingCase_& pricing) {
        RunPrecompute(pricing);
        RunPrice(pricing);
        RunBasket(pricing);
    }

    void RunBasisCalibration(const char* name, const CrossCurrencyCalibrationSpec_& spec, const CrossCurrencyCalibrationOptions_& options) {
        double checksum = 0.0;
        const auto timing = Bench::Run(
            name,
            [&]() {
                const auto calibration = CalibrateCrossCurrencyMarket(spec, options);
                checksum += calibration.diagnostics_.maxAbsResidual_;
                checksum += calibration.fxForwardCurve_.forwards_.back();
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Basis-only XCCY CALIBRATION checksum must be finite and non-zero");
        Bench::Print(timing);
    }

    void RunJointCalibration(const char* name, const JointXccyCalibrationSpec_& spec, const JointXccyCalibrationOptions_& options) {
        double checksum = 0.0;
        const auto timing = Bench::Run(
            name,
            [&]() {
                const auto calibration = CalibrateJointXccyMarket(spec, options);
                checksum += calibration.jointMaxAbsResidual_;
                checksum += calibration.fxForwardCurve_.forwards_.back();
                checksum += static_cast<double>(calibration.solverEvaluations_);
            },
            kWarmups, kRepeats);
        Bench::DoNotOptimize(&checksum);
        REQUIRE(std::isfinite(checksum) && std::fabs(checksum) > 0.0, "Joint XCCY CALIBRATION checksum must be finite and non-zero");
        Bench::Print(timing);
    }
} // namespace

int main() {
    RegisterAll_::Init();
    const Fixture_ fixture = MakeFixture();
    XGLOBAL::SetEvaluationDate(fixture.today_);

    const std::vector<PricingCase_> pricingCases = {
        MakePricingCase(fixture, "fixed", XccyNotionalMode_::Value_::FIXED),
        MakePricingCase(fixture, "resettable", XccyNotionalMode_::Value_::RESETTABLE),
        MakePricingCase(fixture, "MTM", XccyNotionalMode_::Value_::MARK_TO_MARKET),
        MakeInProgressMtmCase(fixture),
    };
    const CrossCurrencyCalibrationSpec_ basisSpec = MakeCalibrationSpec(fixture);
    const CrossCurrencyCalibrationOptions_ basisAnalyticSolve = BasisOptions(CurveJacobianMode_::Value_::ANALYTIC, false);
    const CrossCurrencyCalibrationOptions_ basisAnalyticDiagnostics = BasisOptions(CurveJacobianMode_::Value_::ANALYTIC, true);
    const CrossCurrencyCalibrationOptions_ basisBumpedDiagnostics = BasisOptions(CurveJacobianMode_::Value_::BUMPED, true);

    const JointXccyCalibrationSpec_ jointSpec = MakeJointCalibrationSpec(fixture);
    const JointXccyCalibrationOptions_ jointAnalyticSolve = JointOptions(CurveJacobianMode_::Value_::ANALYTIC, false);
    const JointXccyCalibrationOptions_ jointAnalyticDiagnostics = JointOptions(CurveJacobianMode_::Value_::ANALYTIC, true);
    const JointXccyCalibrationOptions_ jointBumpedDiagnostics = JointOptions(CurveJacobianMode_::Value_::BUMPED, true);
    JointXccyCalibrationSpec_ jointApproximateSpec = jointSpec;
    jointApproximateSpec.solveMode_ = CurveSolveMode_::Value_::APPROXIMATE;

    for (const auto& pricing : pricingCases)
        ValidatePricing(pricing);
    ValidateBasisCalibration(basisSpec, basisAnalyticSolve);
    ValidateBasisCalibration(basisSpec, basisAnalyticDiagnostics);
    ValidateBasisCalibration(basisSpec, basisBumpedDiagnostics);
    ValidateJointCalibration(jointSpec, jointAnalyticSolve);
    ValidateJointCalibration(jointSpec, jointAnalyticDiagnostics);
    ValidateJointCalibration(jointSpec, jointBumpedDiagnostics);
    ValidateJointCalibration(jointApproximateSpec, jointAnalyticDiagnostics);

    Bench::PrintHeader();
    for (const auto& pricing : pricingCases)
        RunPricing(pricing);
    RunBasisCalibration("XCCY basis-only CALIBRATION (15 instruments, 5 knots)", basisSpec, basisAnalyticDiagnostics);
    RunBasisCalibration("XCCY basis-only ANALYTIC SOLVE (15 instruments, 5 knots)", basisSpec, basisAnalyticSolve);
    RunBasisCalibration("XCCY basis-only BUMPED +DIAG (15 instruments, 5 knots)", basisSpec, basisBumpedDiagnostics);
    RunJointCalibration("XCCY joint ANALYTIC SOLVE (15 XCCY, 3x5 knots)", jointSpec, jointAnalyticSolve);
    RunJointCalibration("XCCY joint ANALYTIC +DIAG (15 XCCY, 3x5 knots)", jointSpec, jointAnalyticDiagnostics);
    RunJointCalibration("XCCY joint BUMPED +DIAG (15 XCCY, 3x5 knots)", jointSpec, jointBumpedDiagnostics);
    RunJointCalibration("XCCY joint ANALYTIC APPROXIMATE (15 XCCY, 3x5 knots)", jointApproximateSpec, jointAnalyticDiagnostics);
    return 0;
}
