//
// Created by dal-implementer on 2026/8/31.
//

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>
#include <type_traits>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/piecewiseconstant.hpp>
#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/platform/platform.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using SingleCurveFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CurveCalibrationSpec_&,
                                                                       const Dal::CurveCalibrationResult_&,
                                                                       const Dal::CurveCalibrationOptions_&,
                                                                       const Dal::RatePricingMarket_&,
                                                                       const Dal::RateQuoteRiskProvenanceConfig_&);
using JointXccyFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::JointXccyCalibrationSpec_&,
                                                                     const Dal::JointXccyCalibrationResult_&,
                                                                     const Dal::JointXccyCalibrationOptions_&,
                                                                     const Dal::RatePricingMarket_&,
                                                                     const Dal::RateQuoteRiskProvenanceConfig_&);
using StagedXccyFactorySignature_ = Dal::RateQuoteRiskProvenance_ (*)(const Dal::CrossCurrencyCalibrationSpec_&,
                                                                      const Dal::CrossCurrencyCalibrationResult_&,
                                                                      const Dal::CrossCurrencyCalibrationOptions_&,
                                                                      const Dal::RatePricingMarket_&,
                                                                      const Dal::RateQuoteRiskProvenanceConfig_&);

static_assert(std::is_same_v<decltype(&Dal::BuildSingleCurveQuoteRiskProvenance), SingleCurveFactorySignature_>);
static_assert(std::is_same_v<decltype(&Dal::BuildJointXccyQuoteRiskProvenance), JointXccyFactorySignature_>);
static_assert(std::is_same_v<decltype(&Dal::BuildStagedXccyBasisQuoteRiskProvenance), StagedXccyFactorySignature_>);

namespace {
    class CyclicDiscountCurve_ : public Dal::DiscountCurve_ {
        Dal::Handle_<Dal::DiscountCurve_> base_;

    public:
        explicit CyclicDiscountCurve_(const Dal::String_& name) : DiscountCurve_(name, "USD") {}

        void SetBase(const Dal::Handle_<Dal::DiscountCurve_>& base) { base_ = base; }
        void ClearBase() { base_ = Dal::Handle_<Dal::DiscountCurve_>(); }
        double operator()(const Dal::Date_&, const Dal::Date_&) const override { return 1.0; }
        void Poll(Dal::Vector_<const Dal::YCComponent_*>* all) const override { all->push_back(this); }
        void Poll(std::map<const Dal::YCComponent_*, Dal::Handle_<Dal::YCComponent_>>*) const override {}
        [[nodiscard]] std::unique_ptr<Dal::YCComponent_> Clone(const Dal::String_& newName,
                                                               const Dal::YCComponent_::substitutions_t&) const override {
            return std::make_unique<CyclicDiscountCurve_>(newName);
        }
        void Write(Dal::Archive::Store_& dst) const override {
            dst.SetType("CyclicDiscountCurve_TestOnly");
            Dal::Archive::Utils::Set(dst, "base", base_);
            dst.Done();
        }
    };

    Dal::RateIndexConvention_ SingleIndex() {
        Dal::RateIndexConvention_ result;
        result.forecastTenor_ = Dal::PeriodLength_("6M");
        result.dayBasis_ = Dal::DayBasis_("ACT_365F");
        result.businessDayConvention_ = Dal::BizDayConvention_("Unadjusted");
        result.fixingHolidays_ = Dal::Holidays::None();
        result.accrualHolidays_ = Dal::Holidays::None();
        result.collateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        return result;
    }

    struct SingleProvenanceInput_ {
        Dal::CurveCalibrationSpec_ spec_;
        Dal::CurveCalibrationOptions_ options_;
        Dal::CurveCalibrationResult_ result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    SingleProvenanceInput_ MakeSingleInput(Dal::CurveJacobianMode_ mode) {
        SingleProvenanceInput_ result;
        result.spec_.today_ = Dal::Date_(2025, 1, 2);
        result.spec_.ccy_ = "USD";
        result.spec_.curveName_ = "single_quote_risk";
        result.spec_.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        result.spec_.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        result.spec_.knotPolicy_ = Dal::CurveKnotPolicy_::Value_::INPUT;
        result.spec_.solveMode_ = Dal::CurveSolveMode_::Value_::EXACT;
        result.spec_.liborBasis_ = Dal::DayBasis_("ACT_365F");
        result.spec_.tolerance_ = 1.0e-10;
        result.spec_.initialGuess_ = 0.01;
        result.spec_.knotDates_ = {Dal::Date::AddMonths(result.spec_.today_, 6), Dal::Date::AddMonths(result.spec_.today_, 12)};

        const Dal::Handle_<Dal::DiscountCurve_> known(
            Dal::NewDiscountPWC("single_quote_risk_known", "USD", Dal::PiecewiseConstant_(result.spec_.knotDates_, Dal::Vector_<>{0.02, 0.025})));
        const Dal::CurveBlock_ knownBlock(known, result.spec_.liborBasis_);
        const Dal::RateIndexConvention_ index = SingleIndex();
        for (const auto& maturity : result.spec_.knotDates_) {
            const Dal::Handle_<Dal::YCInstrument_> prototype(new Dal::Deposit_(result.spec_.today_, result.spec_.today_, maturity, 0.0, index));
            const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(knownBlock);
            result.spec_.instruments_.push_back(
                Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(result.spec_.today_, result.spec_.today_, maturity, quote, index)));
        }

        result.options_.jacobianMode_ = mode;
        result.result_ = Dal::CalibrateYieldCurve(result.spec_, result.options_);
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), result.result_.curve_.get());
        result.market_.valuationTime_ = Dal::DateTime_(result.spec_.today_, 9, 0);
        result.market_.resultCurrency_ = Dal::Ccy_("USD");
        result.market_.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
        result.market_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        result.config_.calibrationId_ = "single-calibration";
        result.config_.componentKeyByParameterBlock_[result.spec_.curveName_] = "discount";
        return result;
    }

    bool IsSha256Fingerprint(const Dal::String_& value) {
        if (value.size() != 71 || value.substr(0, 7) != "sha256:")
            return false;
        for (std::size_t i = 7; i < value.size(); ++i) {
            const char ch = value[i];
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                return false;
        }
        return true;
    }

    void ExpectSingleFailure(SingleProvenanceInput_* input, const std::string& token) {
        try {
            static_cast<void>(
                Dal::BuildSingleCurveQuoteRiskProvenance(input->spec_, input->result_, input->options_, input->market_, input->config_));
            FAIL() << "Expected quote-risk provenance construction to fail with " << token;
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find(token), std::string::npos) << exception.what();
        }
    }

    Dal::RateQuoteRiskProvenance_ BuildSingle(const SingleProvenanceInput_& input) {
        return Dal::BuildSingleCurveQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
    }

    void RecalibrateAndBindSingle(SingleProvenanceInput_* input) {
        input->result_ = Dal::CalibrateYieldCurve(input->spec_, input->options_);
        const auto alias = std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), input->result_.curve_.get());
        input->market_.curveComponents_["discount"] = Dal::Handle_<Dal::DiscountCurve_>(alias);
    }

    Dal::RateIndexConvention_ JointIndex(bool projection) {
        auto result = SingleIndex();
        result.forecastTenor_ = Dal::PeriodLength_("3M");
        result.useProjectionCurve_ = projection;
        result.fixingLag_ = 0;
        return result;
    }

    Dal::Handle_<Dal::DiscountCurve_>
    JointPwc(const Dal::String_& name, const Dal::Ccy_& ccy, const Dal::Vector_<Dal::Date_>& knots, const Dal::Vector_<>& parameters) {
        return Dal::Handle_<Dal::DiscountCurve_>(Dal::NewDiscountPWC(name, ccy.String(), Dal::PiecewiseConstant_(knots, parameters)));
    }

    Dal::Handle_<Dal::CurveBlock_> JointBlock(const Dal::String_& name,
                                              const Dal::Ccy_& ccy,
                                              const Dal::Vector_<Dal::Date_>& knots,
                                              const Dal::Vector_<>& discountParameters,
                                              const Dal::Vector_<>& forwardParameters) {
        return Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_(
            name, ccy.String(), {{Dal::CollateralType_(Dal::CollateralType_::Value_::OIS), JointPwc(name + "_ois", ccy, knots, discountParameters)}},
            {{Dal::PeriodLength_("3M"), JointPwc(name + "_3m", ccy, knots, forwardParameters)}}, Dal::DayBasis_("ACT_365F")));
    }

    Dal::Handle_<Dal::YCInstrument_>
    JointDeposit(const Dal::Date_& today, const Dal::Date_& maturity, const Dal::RateIndexConvention_& index, const Dal::CurveBlock_& block) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(new Dal::Deposit_(today, today, maturity, 0.0, index));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(block);
        return Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(today, today, maturity, quote, index));
    }

    Dal::JointCurrencyCurveSpec_ JointCurrencySpec(const Dal::Date_& today,
                                                   const Dal::Ccy_& ccy,
                                                   const Dal::Vector_<Dal::Date_>& knots,
                                                   const Dal::Vector_<Dal::Date_>& maturities,
                                                   const Dal::Handle_<Dal::CurveBlock_>& market) {
        Dal::JointCurveDeclaration_ discount;
        discount.curveName_ = Dal::String_(ccy.String()) + "_ois_provenance";
        discount.knotDates_ = knots;
        discount.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        discount.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);

        Dal::JointCurveDeclaration_ forward;
        forward.curveName_ = Dal::String_(ccy.String()) + "_3m_provenance";
        forward.knotDates_ = knots;
        forward.parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
        forward.targetCollateral_ = Dal::CollateralType_(Dal::CollateralType_::Value_::OIS);
        forward.targetTenor_ = Dal::PeriodLength_("3M");
        forward.calibrateDiscountCurve_ = false;
        for (const auto& maturity : maturities) {
            discount.instruments_.push_back(JointDeposit(today, maturity, JointIndex(false), *market));
            forward.instruments_.push_back(JointDeposit(today, maturity, JointIndex(true), *market));
        }

        Dal::JointCurrencyCurveSpec_ result;
        result.ccy_ = ccy;
        result.curves_ = {discount, forward};
        return result;
    }

    Dal::CrossCurrencySwapConfig_ JointXccyConfig(const Dal::CurrencyPair_& pair) {
        Dal::RateLegConvention_ leg;
        leg.paymentFrequency_ = Dal::PeriodLength_("3M");
        leg.dayBasis_ = Dal::DayBasis_("ACT_365F");
        leg.accrualHolidays_ = Dal::Holidays::None();
        leg.paymentHolidays_ = Dal::Holidays::None();
        Dal::CrossCurrencySwapConfig_ result;
        result.pair_ = pair;
        result.domesticNotional_ = 110.0;
        result.foreignNotional_ = 100.0;
        result.convention_.initialNotionalExchange_ = true;
        result.convention_.finalNotionalExchange_ = true;
        result.convention_.spreadOnForeignLeg_ = true;
        result.convention_.domesticIndex_ = JointIndex(true);
        result.convention_.foreignIndex_ = JointIndex(true);
        result.convention_.domesticLeg_ = leg;
        result.convention_.foreignLeg_ = leg;
        result.notionalMode_ = Dal::XccyNotionalMode_::Value_::FIXED;
        return result;
    }

    struct JointProvenanceInput_ {
        Dal::JointXccyCalibrationSpec_ spec_;
        Dal::JointXccyCalibrationOptions_ options_;
        Dal::JointXccyCalibrationResult_ result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    Dal::Handle_<Dal::DiscountCurve_> AliasCurve(const Dal::DiscountCurve_& curve) {
        return Dal::Handle_<Dal::DiscountCurve_>(std::shared_ptr<const Dal::DiscountCurve_>(std::shared_ptr<void>(), &curve));
    }

    JointProvenanceInput_ MakeJointInput(Dal::CurveJacobianMode_ mode, bool computeInverse = true) {
        JointProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::Vector_<Dal::Date_> maturities = {Dal::Date::AddMonths(today, 12), Dal::Date::AddMonths(today, 24)};
        const Dal::Vector_<Dal::Date_> knots = {Dal::Date::AddMonths(today, 6), Dal::Date::AddMonths(today, 18)};
        const auto domestic = JointBlock("usd_true_provenance", Dal::Ccy_("USD"), knots, {0.015, 0.018}, {0.024, 0.027});
        const auto foreign = JointBlock("eur_true_provenance", Dal::Ccy_("EUR"), knots, {0.010, 0.013}, {0.019, 0.022});
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("basis_true_provenance", pair.domestic_, knots, {0.0010, 0.0020}));

        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.pair_ = pair;
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.domestic_ = JointCurrencySpec(today, pair.domestic_, knots, maturities, domestic);
        input.spec_.foreign_ = JointCurrencySpec(today, pair.foreign_, knots, maturities, foreign);
        input.spec_.basis_.curveName_ = "usd_eur_basis_provenance";
        input.spec_.basis_.knotDates_ = knots;
        input.spec_.fixings_ = fixings;
        input.spec_.initialGuess_ = 0.005;
        input.spec_.tolerance_ = 1.0e-10;
        input.spec_.fitTolerance_ = 1.0e-8;
        const auto config = JointXccyConfig(pair);
        for (const auto& maturity : maturities) {
            const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
            input.spec_.basis_.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        input.options_.jacobianMode_ = mode;
        input.options_.computeEffJacobianInverse_ = computeInverse;
        input.result_ = Dal::CalibrateJointXccyMarket(input.spec_, input.options_);

        Dal::Vector_<Dal::Handle_<Dal::DiscountCurve_>> curves = {
            AliasCurve(input.result_.domesticCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.domesticCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.foreignCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            AliasCurve(input.result_.foreignCurveBlock_->Forward(Dal::PeriodLength_("3M"), Dal::CollateralType_(Dal::CollateralType_::Value_::OIS))),
            input.result_.basisCurve_,
        };
        REQUIRE(curves.size() == input.result_.parameterRanges_.size(), "Unexpected joint XCCY parameter range count");
        input.config_.calibrationId_ = "joint-xccy-calibration";
        for (int i = 0; i < static_cast<int>(input.result_.parameterRanges_.size()); ++i) {
            const Dal::String_ component = "joint-component-" + Dal::String::FromInt(i);
            input.config_.componentKeyByParameterBlock_[input.result_.parameterRanges_[i].name_] = component;
            input.market_.curveComponents_[component] = curves[i];
        }
        input.market_.valuationTime_ = input.spec_.valuationTime_;
        input.market_.resultCurrency_ = pair.domestic_;
        input.market_.fixings_ = input.result_.fixings_;
        auto xccy =
            std::make_shared<Dal::CrossCurrencyMarket_>(input.result_.domesticCurveBlock_, input.result_.foreignCurveBlock_, input.spec_.fxSpot_,
                                                        input.spec_.valuationTime_, input.spec_.collateralCurrency_, input.result_.fixings_);
        xccy->SetBasisCurve(input.result_.basisCurve_);
        input.market_.xccyMarket_ = xccy;
        return input;
    }

    struct StagedProvenanceInput_ {
        Dal::CrossCurrencyCalibrationSpec_ spec_;
        Dal::CrossCurrencyCalibrationOptions_ options_;
        std::unique_ptr<Dal::CrossCurrencyCalibrationResult_> result_;
        Dal::RatePricingMarket_ market_;
        Dal::RateQuoteRiskProvenanceConfig_ config_;
    };

    StagedProvenanceInput_ MakeStagedInput(Dal::CurveJacobianMode_ mode, double dependencyShift = 0.0, bool computeInverse = true) {
        StagedProvenanceInput_ input;
        const Dal::Date_ today(2025, 1, 16);
        const Dal::Vector_<Dal::Date_> knots = {Dal::Date::AddMonths(today, 12), Dal::Date::AddMonths(today, 24)};
        const Dal::CurrencyPair_ pair(Dal::Ccy_("USD"), Dal::Ccy_("EUR"));
        const auto domestic = JointBlock("usd_staged_provenance", pair.domestic_, knots, {0.015 + dependencyShift, 0.018 + dependencyShift},
                                         {0.024 + dependencyShift, 0.027 + dependencyShift});
        const auto foreign = JointBlock("eur_staged_provenance", pair.foreign_, knots, {0.010 - dependencyShift, 0.013 - dependencyShift},
                                        {0.019 - dependencyShift, 0.022 - dependencyShift});
        const Dal::Handle_<Dal::MarketFixingSnapshot_> fixings(new Dal::MarketFixingSnapshot_());
        const auto config = JointXccyConfig(pair);
        Dal::CrossCurrencyMarket_ quoteMarket(domestic, foreign, 1.10, Dal::DateTime_(today, 9, 0), pair.domestic_, fixings);
        quoteMarket.SetBasisCurve(JointPwc("known_staged_provenance", pair.domestic_, knots, {0.0010, 0.0020}));

        input.spec_.today_ = today;
        input.spec_.valuationTime_ = Dal::DateTime_(today, 9, 0);
        input.spec_.collateralCurrency_ = pair.domestic_;
        input.spec_.fixings_ = fixings;
        input.spec_.basisPair_ = pair;
        input.spec_.domesticCurveBlock_ = domestic;
        input.spec_.foreignCurveBlock_ = foreign;
        input.spec_.fxSpot_ = 1.10;
        input.spec_.knotDates_ = knots;
        input.spec_.initialGuess_ = 0.001;
        input.spec_.tolerance_ = 1.0e-10;
        for (const auto& maturity : knots) {
            const Dal::CrossCurrencySwap_ prototype(today, today, maturity, 0.0, config);
            input.spec_.instruments_.push_back(Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(today, today, maturity, (*prototype.Precompute())(quoteMarket), config)));
        }
        input.options_.jacobianMode_ = mode;
        input.options_.computeEffJacobianInverse_ = computeInverse;
        input.result_ = std::make_unique<Dal::CrossCurrencyCalibrationResult_>(Dal::CalibrateCrossCurrencyMarket(input.spec_, input.options_));
        const auto basis = input.result_->basisCurves_.at(pair);
        const Dal::String_ parameterBlock = Dal::String_("basis:xccy_basis_") + pair.domestic_.String();
        input.config_.calibrationId_ = "staged-xccy-calibration";
        input.config_.componentKeyByParameterBlock_[parameterBlock] = "staged-basis-component";
        input.market_.valuationTime_ = input.spec_.valuationTime_;
        input.market_.resultCurrency_ = pair.domestic_;
        input.market_.curveComponents_["staged-basis-component"] = basis;
        input.market_.fixings_ = input.result_->market_.Fixings();
        input.market_.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(input.result_->market_);
        return input;
    }

    void RebindJointMarket(JointProvenanceInput_* input) {
        auto xccy =
            std::make_shared<Dal::CrossCurrencyMarket_>(input->result_.domesticCurveBlock_, input->result_.foreignCurveBlock_, input->spec_.fxSpot_,
                                                        input->spec_.valuationTime_, input->spec_.collateralCurrency_, input->result_.fixings_);
        xccy->SetBasisCurve(input->result_.basisCurve_);
        input->market_.xccyMarket_ = xccy;
    }
} // namespace

TEST(QuoteRiskProvenanceTest, TestCoreHeaderFactorySignatures) {
    Dal::RateQuoteRiskProvenanceConfig_ config;
    config.calibrationId_ = "header-isolation";
    ASSERT_EQ(config.calibrationId_, "header-isolation");
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeSingleInput(mode);
        const Dal::RateQuoteRiskProvenance_ provenance =
            Dal::BuildSingleCurveQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);

        ASSERT_TRUE(provenance.Available());
        ASSERT_TRUE(provenance.Reason().empty());
        ASSERT_EQ(provenance.Kind(), "SINGLE_CURVE");
        ASSERT_EQ(provenance.CalibrationId(), input.config_.calibrationId_);
        ASSERT_EQ(provenance.ComponentKeyByParameterBlock(), input.config_.componentKeyByParameterBlock_);
        ASSERT_EQ(provenance.Axis().scheme_, Dal::RateQuoteRiskAxisFingerprintScheme());
        ASSERT_EQ(provenance.State().scheme_, Dal::RateQuoteRiskStateFingerprintScheme());
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().parameters_.size(), 2);
        ASSERT_EQ(provenance.Axis().quotes_.size(), 2);
        ASSERT_EQ(provenance.State().components_.size(), 1);
        ASSERT_EQ(provenance.EffectiveInverse().Rows(), 2);
        ASSERT_EQ(provenance.EffectiveInverse().Cols(), 2);
        ASSERT_DOUBLE_EQ(provenance.Tolerance(), input.spec_.tolerance_);
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC) {
            analyticAxis = provenance.Axis().fingerprint_;
            ASSERT_EQ(analyticAxis, "sha256:da526c4aad2e15b7adbf95f5e2ebd33a20779d27b810edde48baf2eb0754f1ad");
        } else {
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
        }
    }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveUnavailableReasonsRemainStructured) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.options_.computeEffJacobianInverse_ = false;
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.spec_.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
        input.result_.diagnostics_.usedApproximateFit_ = true;
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::BUMPED);
        input.result_.diagnostics_.effJacobianInverse_ = Dal::Matrix_<>();
        const auto provenance = BuildSingle(input);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE");
    }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveMalformedInputsFailClosed) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.config_.calibrationId_.clear();
        ExpectSingleFailure(&input, "QUOTE_RISK_CALIBRATION_ID_EMPTY");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.config_.componentKeyByParameterBlock_.clear();
        ExpectSingleFailure(&input, "QUOTE_RISK_PARAMETER_BLOCK_BINDINGS_INCOMPLETE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.diagnostics_.effJacobianInverse_(0, 0) = std::numeric_limits<double>::quiet_NaN();
        ExpectSingleFailure(&input, "QUOTE_RISK_EFFECTIVE_INVERSE");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.spec_.tolerance_ = std::numeric_limits<double>::infinity();
        ExpectSingleFailure(&input, "QUOTE_RISK_TOLERANCE_INVALID");
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.diagnostics_.marketRates_[0] += 1.0e-4;
        ExpectSingleFailure(&input, "QUOTE_RISK_SPEC_RESULT_MISMATCH");
    }
}

TEST(QuoteRiskProvenanceTest, TestSingleCurveAxisIsStableWhileStateTracksMutableInputs) {
    auto baselineInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = BuildSingle(baselineInput);

    auto asOfInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    asOfInput.market_.valuationTime_ = Dal::DateTime_(asOfInput.spec_.today_, 10, 0);
    const auto asOf = BuildSingle(asOfInput);

    auto fixingInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    Dal::MarketFixingSnapshot_::values_t fixingValues;
    fixingValues["USD-TEST"][Dal::DateTime_(fixingInput.spec_.today_, 8, 0)] = 0.0123;
    fixingInput.market_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(fixingValues));
    const auto fixing = BuildSingle(fixingInput);

    auto inverseInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    inverseInput.result_.diagnostics_.effJacobianInverse_(0, 0) += 1.0e-12;
    const auto inverse = BuildSingle(inverseInput);

    auto toleranceInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    toleranceInput.spec_.tolerance_ *= 10.0;
    const auto tolerance = BuildSingle(toleranceInput);

    for (const auto* changed : {&asOf, &fixing, &inverse, &tolerance}) {
        ASSERT_EQ(changed->Axis().fingerprint_, baseline.Axis().fingerprint_);
        ASSERT_NE(changed->State().fingerprint_, baseline.State().fingerprint_);
    }
}

TEST(QuoteRiskProvenanceTest, TestJointXccyAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeJointInput(mode);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        ASSERT_TRUE(provenance.Available());
        ASSERT_EQ(provenance.Kind(), "JOINT_XCCY");
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), input.result_.parameterRanges_.size());
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), input.result_.residualRanges_.size());
        ASSERT_EQ(provenance.Axis().parameters_.size(), static_cast<std::size_t>(input.result_.effJacobianInverse_.Rows()));
        ASSERT_EQ(provenance.Axis().quotes_.size(), static_cast<std::size_t>(input.result_.effJacobianInverse_.Cols()));
        ASSERT_EQ(provenance.State().components_.size(), input.result_.parameterRanges_.size());
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC)
            analyticAxis = provenance.Axis().fingerprint_;
        else
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedXccyBasisAnalyticAndBumpedConstruction) {
    Dal::String_ analyticAxis;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        auto input = MakeStagedInput(mode);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        ASSERT_TRUE(provenance.Available());
        ASSERT_EQ(provenance.Kind(), "STAGED_XCCY_BASIS");
        ASSERT_EQ(provenance.Axis().parameterRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().residualRanges_.size(), 1);
        ASSERT_EQ(provenance.Axis().parameters_.size(), input.spec_.knotDates_.size());
        ASSERT_EQ(provenance.Axis().quotes_.size(), input.spec_.instruments_.size());
        ASSERT_EQ(provenance.State().components_.size(), 1);
        ASSERT_TRUE(IsSha256Fingerprint(provenance.Axis().fingerprint_));
        ASSERT_TRUE(IsSha256Fingerprint(provenance.State().fingerprint_));
        if (mode == Dal::CurveJacobianMode_::Value_::ANALYTIC)
            analyticAxis = provenance.Axis().fingerprint_;
        else
            ASSERT_EQ(provenance.Axis().fingerprint_, analyticAxis);
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedInverseMetadataMismatchFailsClosed) {
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_->diagnostics_.effJacobianInverseAvailability_ = "not_requested";
        ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                     Dal::Exception_);
    }
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_->diagnostics_.effJacobianInverseScaling_ = "unscaled";
        ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                     Dal::Exception_);
    }
}

TEST(QuoteRiskProvenanceTest, TestJointMissingLayeredBaseTopologyFailsClosed) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    input.spec_.domestic_.curves_[1].baseLayeredOverDiscount_ = true;
    ASSERT_THROW(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_), Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestStagedUnexpectedBaseTopologyFailsClosed) {
    auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto original = input.result_->basisCurves_.at(input.spec_.basisPair_);
    const auto* typed = dynamic_cast<const Dal::Tape::DiscountPWC_<double>*>(original.get());
    ASSERT_TRUE(typed);
    const auto base = AliasCurve(input.spec_.domesticCurveBlock_->Discount(Dal::CollateralType_(Dal::CollateralType_::Value_::OIS)));
    const Dal::Handle_<Dal::DiscountCurve_> replacement(
        Dal::NewDiscountPWC(original->Name(), original->ccy_.String(), Dal::PiecewiseConstant_(typed->KnotDates(), typed->FRight()), base));
    input.result_->basisCurves_[input.spec_.basisPair_] = replacement;
    input.result_->market_.SetBasisCurve(replacement);
    input.market_.curveComponents_["staged-basis-component"] = replacement;
    input.market_.xccyMarket_ = std::make_shared<Dal::CrossCurrencyMarket_>(input.result_->market_);
    ASSERT_THROW(Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_),
                 Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestJointExtraSlotTopologyFailsClosed) {
    auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    auto discounts = input.result_.domesticCurveBlock_->DiscountCurves();
    auto forwards = input.result_.domesticCurveBlock_->ForwardCurves();
    forwards[Dal::PeriodLength_("6M")] =
        JointPwc("unexpected_6m", input.spec_.domestic_.ccy_, input.spec_.domestic_.curves_[0].knotDates_, {0.01, 0.01});
    input.result_.domesticCurveBlock_ =
        Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_(input.result_.domesticCurveBlock_->Name(), input.spec_.domestic_.ccy_.String(), discounts,
                                                            forwards, input.result_.domesticCurveBlock_->LiborBasis()));
    RebindJointMarket(&input);
    ASSERT_THROW(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_), Dal::Exception_);
}

TEST(QuoteRiskProvenanceTest, TestJointRangesBindingsAndNonFiniteStateFailClosed) {
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.parameterRanges_[1].offset_ += 1;
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected gapped parameter ranges to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("complete partition"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        auto binding = input.config_.componentKeyByParameterBlock_.begin();
        const Dal::String_ duplicate = binding->second;
        ++binding;
        binding->second = duplicate;
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected duplicate component binding to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_PARAMETER_BLOCK_BINDING_DUPLICATE"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        input.result_.effJacobianInverse_(0, 0) = std::numeric_limits<double>::infinity();
        try {
            static_cast<void>(Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_));
            FAIL() << "Expected non-finite joint inverse to fail";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("QUOTE_RISK_EFFECTIVE_INVERSE"), std::string::npos) << exception.what();
        }
    }
}

TEST(QuoteRiskProvenanceTest, TestStagedDependenciesChangeStateWithoutChangingAxis) {
    auto baselineInput = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = Dal::BuildStagedXccyBasisQuoteRiskProvenance(baselineInput.spec_, *baselineInput.result_, baselineInput.options_,
                                                                       baselineInput.market_, baselineInput.config_);
    auto changedInput = MakeStagedInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, 0.001);
    const auto changed = Dal::BuildStagedXccyBasisQuoteRiskProvenance(changedInput.spec_, *changedInput.result_, changedInput.options_,
                                                                      changedInput.market_, changedInput.config_);
    ASSERT_EQ(changed.Axis().fingerprint_, baseline.Axis().fingerprint_);
    ASSERT_NE(changed.State().fingerprint_, baseline.State().fingerprint_);
    ASSERT_NE(changed.State().components_.front().fingerprint_, baseline.State().components_.front().fingerprint_);
}

TEST(QuoteRiskProvenanceTest, TestXccyFactoriesReturnUnavailableWhenInverseWasNotRequested) {
    {
        auto input = MakeJointInput(Dal::CurveJacobianMode_::Value_::ANALYTIC, false);
        const auto provenance = Dal::BuildJointXccyQuoteRiskProvenance(input.spec_, input.result_, input.options_, input.market_, input.config_);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
    {
        auto input = MakeStagedInput(Dal::CurveJacobianMode_::Value_::BUMPED, 0.0, false);
        const auto provenance =
            Dal::BuildStagedXccyBasisQuoteRiskProvenance(input.spec_, *input.result_, input.options_, input.market_, input.config_);
        ASSERT_FALSE(provenance.Available());
        ASSERT_EQ(provenance.Reason(), "QUOTE_RISK_INVERSE_NOT_REQUESTED");
        ASSERT_TRUE(provenance.EffectiveInverse().Empty());
    }
}

TEST(QuoteRiskProvenanceTest, TestSolvedCurveAndBaseChangesAffectStateNotAxis) {
    auto baselineInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const auto baseline = BuildSingle(baselineInput);

    auto curveInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    const Dal::Handle_<Dal::DiscountCurve_> changedKnown(Dal::NewDiscountPWC(
        "single_quote_risk_changed_known", "USD", Dal::PiecewiseConstant_(curveInput.spec_.knotDates_, Dal::Vector_<>{0.021, 0.026})));
    const Dal::CurveBlock_ changedKnownBlock(changedKnown, curveInput.spec_.liborBasis_);
    curveInput.spec_.instruments_.clear();
    for (const auto& maturity : curveInput.spec_.knotDates_) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(
            new Dal::Deposit_(curveInput.spec_.today_, curveInput.spec_.today_, maturity, 0.0, SingleIndex()));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(changedKnownBlock);
        curveInput.spec_.instruments_.push_back(
            Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(curveInput.spec_.today_, curveInput.spec_.today_, maturity, quote, SingleIndex())));
    }
    ASSERT_NO_THROW(RecalibrateAndBindSingle(&curveInput));
    const auto curveChanged = BuildSingle(curveInput);

    auto baseInput = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
    baseInput.spec_.baseCurve_ = Dal::Handle_<Dal::DiscountCurve_>(
        Dal::NewDiscountPWC("single_quote_risk_base", "USD", Dal::PiecewiseConstant_(baseInput.spec_.knotDates_, Dal::Vector_<>{0.001, 0.001})));
    const Dal::Handle_<Dal::DiscountCurve_> knownWithBase(
        Dal::NewDiscountPWC("single_quote_risk_known_with_base", "USD",
                            Dal::PiecewiseConstant_(baseInput.spec_.knotDates_, Dal::Vector_<>{0.02, 0.025}), baseInput.spec_.baseCurve_));
    const Dal::CurveBlock_ knownBaseBlock(knownWithBase, baseInput.spec_.liborBasis_);
    baseInput.spec_.instruments_.clear();
    for (const auto& maturity : baseInput.spec_.knotDates_) {
        const Dal::Handle_<Dal::YCInstrument_> prototype(
            new Dal::Deposit_(baseInput.spec_.today_, baseInput.spec_.today_, maturity, 0.0, SingleIndex()));
        const double quote = (*prototype->Precompute(Dal::Handle_<Dal::YieldCurve_>()))(knownBaseBlock);
        baseInput.spec_.instruments_.push_back(
            Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(baseInput.spec_.today_, baseInput.spec_.today_, maturity, quote, SingleIndex())));
    }
    baseInput.spec_.initialGuess_ = 0.02;
    ASSERT_NO_THROW(RecalibrateAndBindSingle(&baseInput));
    const auto baseChanged = BuildSingle(baseInput);

    for (const auto* changed : {&curveChanged, &baseChanged}) {
        ASSERT_EQ(changed->Axis().fingerprint_, baseline.Axis().fingerprint_);
        ASSERT_NE(changed->State().fingerprint_, baseline.State().fingerprint_);
        ASSERT_NE(changed->State().components_.front().fingerprint_, baseline.State().components_.front().fingerprint_);
    }
}

TEST(QuoteRiskProvenanceTest, TestNonFiniteFixingAndCyclicCurveGraphFailBeforeHashing) {
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        Dal::MarketFixingSnapshot_::values_t values;
        values["USD-NONFINITE"][Dal::DateTime_(input.spec_.today_, 8, 0)] = std::numeric_limits<double>::quiet_NaN();
        try {
            static_cast<void>(Dal::MarketFixingSnapshot_(values));
            FAIL() << "Expected non-finite fixing state to fail before provenance hashing";
        } catch (const Dal::Exception_& exception) {
            ASSERT_NE(std::string(exception.what()).find("requires finite values"), std::string::npos) << exception.what();
        }
    }
    {
        auto input = MakeSingleInput(Dal::CurveJacobianMode_::Value_::ANALYTIC);
        auto first = std::make_shared<CyclicDiscountCurve_>("cycle-first");
        auto second = std::make_shared<CyclicDiscountCurve_>("cycle-second");
        const Dal::Handle_<Dal::DiscountCurve_> firstHandle(std::static_pointer_cast<const Dal::DiscountCurve_>(first));
        const Dal::Handle_<Dal::DiscountCurve_> secondHandle(std::static_pointer_cast<const Dal::DiscountCurve_>(second));
        first->SetBase(secondHandle);
        second->SetBase(firstHandle);
        input.spec_.forwardCurves_[Dal::PeriodLength_("3M")] = firstHandle;
        ExpectSingleFailure(&input, "QUOTE_RISK_CYCLIC_BASE_GRAPH");
        first->ClearBase();
        second->ClearBase();
    }
}
