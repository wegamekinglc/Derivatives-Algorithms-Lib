//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <dal-public/src/ratecurvature.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/jointcalibration_internal.hpp>
#include <dal/curve/xccypricing.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>
#include <dal/time/holidays.hpp>

#include "ratexccycurvaturefixtures.hpp"

namespace {
    using RateXccyCurvatureFixtures::FixedBlock;
    using RateXccyCurvatureFixtures::FixedCurve;
    using RateXccyCurvatureFixtures::JointSpec;
    using RateXccyCurvatureFixtures::QuoteCurrency;
    using RateXccyCurvatureFixtures::QuoteXccy;
    using RateXccyCurvatureFixtures::QuoteYc;
    using RateXccyCurvatureFixtures::StagedSpec;
    using RateXccyCurvatureFixtures::XccyIndex;
    using RateXccyCurvatureFixtures::XccyLeg;

    Dal::JointCalibrationInternal::CurveCollectionSpec_ Collection(const Dal::JointCurrencyCurveSpec_& currency, const Dal::Date_& today) {
        Dal::JointCalibrationInternal::CurveCollectionSpec_ collection;
        collection.today_ = today;
        collection.ccy_ = currency.ccy_.String();
        collection.liborBasis_ = currency.liborBasis_;
        collection.curves_ = &currency.curves_;
        return collection;
    }

    Dal::XccyCashflowPlan_ TradePlan(const Dal::Date_& today, const Dal::CrossCurrencySwapConfig_& config) {
        const auto start = Dal::Date::AddMonths(today, 1);
        return Dal::BuildXccyCashflowPlan(start, Dal::Date::AddMonths(start, 30), config);
    }

    Dal::Tape::JointCurveBlock_<double> PassiveBlock(const Dal::CurveBlock_& block) {
        Dal::Tape::JointCurveBlock_<double> typed;
        for (const auto& entry : block.DiscountCurves())
            typed.discountCurves[entry.first] = entry.second.get();
        for (const auto& entry : block.ForwardCurves())
            typed.forwardCurves[entry.first] = entry.second.get();
        return typed;
    }

    double PassiveContract(const Dal::CrossCurrencyMarket_& market, const Dal::XccyCashflowPlan_& plan, const Dal::Vector_<>& quotes) {
        const auto domestic = PassiveBlock(market.DomesticBlock()), foreign = PassiveBlock(market.ForeignBlock());
        Dal::XccyMarketView_<double> view;
        view.valuationTime_ = market.ValuationTime();
        view.pair_ = Dal::CurrencyPair_(market.DomesticCcy(), market.ForeignCcy());
        view.collateralCurrency_ = market.CollateralCurrency();
        view.fxSpot_ = market.FxSpot();
        view.domestic_ = &domestic;
        view.foreign_ = &foreign;
        view.basis_ = market.BasisCurve();
        return Dal::PriceXccyContract(plan, view, *market.Fixings(), 0.002, true, true) + 0.3 * quotes.front() * quotes.back();
    }

    double PassivePrice(Dal::CrossCurrencyCalibrationSpec_ spec, const Dal::Vector_<>& quotes) {
        const auto plan = TradePlan(spec.today_, spec.instruments_.front()->Config());
        for (int index = 0; index < static_cast<int>(quotes.size()); ++index)
            spec.instruments_[index] = QuoteXccy(spec.instruments_[index], quotes[index]);
        const auto result = Dal::CalibrateCrossCurrencyMarket(spec);
        return PassiveContract(result.market_, plan, quotes);
    }

    double PassivePrice(Dal::JointXccyCalibrationSpec_ spec, const Dal::Vector_<>& quotes) {
        const auto plan = TradePlan(spec.valuationTime_.Date(), spec.basis_.instruments_.front()->Config());
        size_t offset = 0;
        spec.domestic_ = QuoteCurrency(spec.domestic_, quotes, &offset);
        spec.foreign_ = QuoteCurrency(spec.foreign_, quotes, &offset);
        for (auto& instrument : spec.basis_.instruments_)
            instrument = QuoteXccy(instrument, quotes[offset++]);
        const auto result = Dal::CalibrateJointXccyMarket(spec);
        Dal::CrossCurrencyMarket_ market(result.domesticCurveBlock_, result.foreignCurveBlock_, spec.fxSpot_, spec.valuationTime_,
                                         spec.collateralCurrency_, result.fixings_);
        market.SetBasisCurve(result.basisCurve_);
        return PassiveContract(market, plan, quotes);
    }

    struct FixedBlockState_ {
        std::map<Dal::CollateralType_, Dal::CurveParameterState_> discounts_;
        std::map<Dal::PeriodLength_, Dal::CurveParameterState_> forwards_;
    };

    FixedBlockState_ FixedState(const Dal::CurveBlock_& block, const Dal::Date_& today) {
        FixedBlockState_ state;
        for (const auto& entry : block.DiscountCurves())
            state.discounts_.emplace(entry.first, Dal::InspectCurveParameters(*entry.second, today));
        for (const auto& entry : block.ForwardCurves())
            state.forwards_.emplace(entry.first, Dal::InspectCurveParameters(*entry.second, today));
        return state;
    }

    template <class T_>
    void AddFixedCurves(const std::map<T_, Dal::CurveParameterState_>& states,
                        std::map<T_, std::shared_ptr<Dal::Tape::DiscountCurve_<Dal::AAD::Number_>>>* owners,
                        std::map<T_, const Dal::Tape::DiscountCurve_<Dal::AAD::Number_>*>* routes) {
        using Number_ = Dal::AAD::Number_;
        for (const auto& entry : states) {
            const auto& curve = entry.second;
            (*owners)[entry.first] = Dal::BuildDiscountCurveT<Number_>(
                curve.definition_, Dal::Vector_<Number_>(curve.passiveParameters_.begin(), curve.passiveParameters_.end()));
            (*routes)[entry.first] = owners->at(entry.first).get();
        }
    }

    Dal::JointCalibrationInternal::TypedCurveBlockStorage_<Dal::AAD::Number_> ActiveFixedBlock(const FixedBlockState_& state) {
        Dal::JointCalibrationInternal::TypedCurveBlockStorage_<Dal::AAD::Number_> typed;
        AddFixedCurves(state.discounts_, &typed.discountCurves_, &typed.block_.discountCurves);
        AddFixedCurves(state.forwards_, &typed.forwardCurves_, &typed.block_.forwardCurves);
        return typed;
    }

    template <class T_>
    Dal::XccyMarketView_<T_> MarketView(const Dal::CrossCurrencyCalibrationSpec_& spec,
                                        const Dal::Tape::JointCurveBlock_<T_>& domestic,
                                        const Dal::Tape::JointCurveBlock_<T_>& foreign,
                                        const Dal::Tape::DiscountCurve_<T_>& basis) {
        Dal::XccyMarketView_<T_> view;
        view.valuationTime_ = spec.valuationTime_;
        view.pair_ = spec.basisPair_;
        view.collateralCurrency_ = spec.collateralCurrency_;
        view.fxSpot_ = T_(spec.fxSpot_);
        view.domestic_ = &domestic;
        view.foreign_ = &foreign;
        view.basis_ = &basis;
        return view;
    }

    Dal::AAD::NativeScalarFunction_ ContractObjective(const Dal::CrossCurrencyCalibrationSpec_& spec) {
        const auto domestic = FixedState(*spec.domesticCurveBlock_, spec.today_);
        const auto foreign = FixedState(*spec.foreignCurveBlock_, spec.today_);
        const auto definition =
            Dal::MakeCurveDefinition("basis", spec.basisPair_.domestic_.String(), Dal::CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD,
                                     Dal::LogDfScheme_::Value_::LOG_LINEAR, spec.knotDates_, spec.today_, Dal::DayBasis::Act365F());
        const auto plan = TradePlan(spec.today_, spec.instruments_.front()->Config());
        return [spec, domestic, foreign, definition, plan](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
            const int count = static_cast<int>(spec.knotDates_.size());
            const auto basis = Dal::BuildDiscountCurveT<Dal::AAD::Number_>(definition, Dal::Vector_<Dal::AAD::Number_>(x.begin(), x.begin() + count));
            const auto activeDomestic = ActiveFixedBlock(domestic), activeForeign = ActiveFixedBlock(foreign);
            const auto view = MarketView(spec, activeDomestic.block_, activeForeign.block_, *basis);
            return Dal::PriceXccyContract(plan, view, *spec.fixings_, 0.002, true, true) + 0.3 * x[count] * x.back();
        };
    }

    Dal::AAD::NativeScalarFunction_ ContractObjective(const Dal::JointXccyCalibrationSpec_& spec) {
        const auto today = spec.valuationTime_.Date();
        const auto domestic = Dal::JointCalibrationInternal::ValidateAndBuildSlots(Collection(spec.domestic_, today));
        const int foreignOffset = domestic.back().paramOffset_ + domestic.back().nParams_;
        const auto foreign = Dal::JointCalibrationInternal::ValidateAndBuildSlots(Collection(spec.foreign_, today), foreignOffset);
        const int basisOffset = foreign.back().paramOffset_ + foreign.back().nParams_;
        const auto definition = Dal::MakeCurveDefinition(spec.basis_.curveName_, spec.pair_.domestic_.String(), spec.basis_.parameterization_,
                                                         spec.basis_.logDfScheme_, spec.basis_.knotDates_, today, Dal::DayBasis::Act365F());
        const int count = basisOffset + Dal::BuildCurveParameterLayout(definition).parameterCount_;
        const auto plan = TradePlan(today, spec.basis_.instruments_.front()->Config());
        return [spec, domestic, foreign, basisOffset, definition, count, plan](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
            using Number_ = Dal::AAD::Number_;
            const auto today = spec.valuationTime_.Date();
            const auto activeDomestic = Dal::JointCalibrationInternal::BuildTypedCurveBlock(Collection(spec.domestic_, today), domestic, x);
            const auto activeForeign = Dal::JointCalibrationInternal::BuildTypedCurveBlock(Collection(spec.foreign_, today), foreign, x);
            const auto basis = Dal::BuildDiscountCurveT<Number_>(definition, Dal::Vector_<Number_>(x.begin() + basisOffset, x.begin() + count));
            Dal::CrossCurrencyCalibrationSpec_ header;
            header.valuationTime_ = spec.valuationTime_;
            header.basisPair_ = spec.pair_;
            header.collateralCurrency_ = spec.collateralCurrency_;
            header.fxSpot_ = spec.fxSpot_;
            const auto view = MarketView(header, activeDomestic.block_, activeForeign.block_, *basis);
            return Dal::PriceXccyContract(plan, view, *spec.fixings_, 0.002, true, true) + 0.3 * x[count] * x.back();
        };
    }

    double PriceCurvature(const Dal::Vector_<>& quotes,
                          const Dal::AAD::BumpOverAADRequest_& request,
                          int column,
                          double innerStep,
                          const std::function<double(const Dal::Vector_<>&)>& price) {
        const double outerStep = request.steps_[0];
        double prices[2][2];
        for (int outer = 0; outer < 2; ++outer) {
            for (int inner = 0; inner < 2; ++inner) {
                auto point = quotes;
                for (int index = 0; index < static_cast<int>(quotes.size()); ++index)
                    point[index] += (outer == 0 ? outerStep : -outerStep) * request.directions_(0, index);
                point[column] += inner == 0 ? innerStep : -innerStep;
                prices[outer][inner] = price(point);
            }
        }
        return ((prices[0][0] - prices[0][1]) - (prices[1][0] - prices[1][1])) / (4.0 * outerStep * innerStep);
    }

    double PriceGradient(const Dal::Vector_<>& quotes, int column, double step, const std::function<double(const Dal::Vector_<>&)>& price) {
        auto plus = quotes, minus = quotes;
        plus[column] += step;
        minus[column] -= step;
        return (price(plus) - price(minus)) / (2.0 * step);
    }

    void AssertPriceCurvature(const Dal::RateCalibrationSnapshot_& snapshot,
                              const Dal::AAD::NativeScalarFunction_& objective,
                              const std::function<double(const Dal::Vector_<>&)>& price) {
        const int count = static_cast<int>(snapshot.Point().size());
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, count, 0.0);
        for (int index = 0; index < count; ++index)
            request.directions_(0, index) = index % 2 == 0 ? 0.6 : -0.4;
        const double innerStep = 1.0e-4;
        Dal::Vector_<> gradient(count);
        for (int column = 0; column < count; ++column)
            gradient[column] =
                (4.0 * PriceGradient(snapshot.Point(), column, 5.0e-6, price) - PriceGradient(snapshot.Point(), column, 1.0e-5, price)) / 3.0;
        const double expectedValue = price(snapshot.Point());
        for (double outerStep : {4.0e-4, 2.0e-4, 1.0e-4}) {
            request.steps_ = {outerStep};
            const auto result = Dal::EvaluateRateQuoteCurvature(objective, snapshot, request);
            ASSERT_NEAR(result.Value(), expectedValue, 1.0e-9);
            for (int column = 0; column < count; ++column) {
                ASSERT_NEAR(result.Gradient()[column], gradient[column], 1.0e-6) << "column=" << column;
                const double coarse = PriceCurvature(snapshot.Point(), request, column, innerStep, price);
                const double fine = PriceCurvature(snapshot.Point(), request, column, 0.5 * innerStep, price);
                const double curvature = (4.0 * fine - coarse) / 3.0;
                ASSERT_NEAR(result.HessianProducts()(0, column), curvature, 2.0e-3) << "column=" << column << "; step=" << outerStep;
            }
        }
    }

    Dal::CrossCurrencyCalibrationSpec_ RepriceStaged(Dal::CrossCurrencyCalibrationSpec_ spec) {
        Dal::CrossCurrencyMarket_ market(spec.domesticCurveBlock_, spec.foreignCurveBlock_, spec.fxSpot_, spec.valuationTime_,
                                         spec.collateralCurrency_, spec.fixings_);
        market.SetBasisCurve(
            Dal::Handle_<Dal::DiscountCurve_>(new Dal::Tape::DiscountPWC_<double>("basis", "USD", spec.knotDates_, {0.001, 0.0014})));
        for (auto& instrument : spec.instruments_)
            instrument = QuoteXccy(instrument, (*instrument->Precompute())(market));
        return spec;
    }

    Dal::String_ EscapePattern(const Dal::String_& text) {
        Dal::String_ result;
        for (char letter : text) {
            if (Dal::String_("\\.^$|()[]{}*+?").find(letter) != Dal::String_::npos)
                result.push_back('\\');
            result.push_back(letter);
        }
        return result;
    }

    class ScopedFixings_ {
        Dal::Vector_<std::pair<Dal::String_, Dal::FixHistory_>> saved_;

    public:
        explicit ScopedFixings_(const Dal::MarketFixingSnapshot_::values_t& values) {
            for (const auto& entry : values)
                saved_.push_back({entry.first, Dal::Global::Fixings_().History(entry.first)});
        }
        ~ScopedFixings_() {
            for (const auto& entry : saved_) {
                if (entry.second.vals_.empty())
                    (void)Dal::ObjectAccess_::Erase(EscapePattern(Dal::String_("##GLOBAL##FixingsFor:") + entry.first + "~"));
                else
                    Dal::XGLOBAL::StoreFixings(entry.first, entry.second, false);
            }
        }
    };

    void StoreFixings(const Dal::MarketFixingSnapshot_::values_t& values, double shift = 0.0) {
        for (const auto& entry : values) {
            Dal::FixHistory_ history;
            for (const auto& value : entry.second)
                history.vals_.push_back({value.first, value.second + shift});
            Dal::XGLOBAL::StoreFixings(entry.first, history, false);
        }
    }

    Dal::Vector_<Dal::FixingRequest_> HistoricalRequests(const Dal::CrossCurrencyCalibrationSpec_& spec) {
        Dal::Vector_<Dal::FixingRequest_> requests;
        for (const auto& instrument : spec.instruments_) {
            const auto span = instrument->TimeSpan();
            requests.Append(
                Dal::RequiredHistoricalFixings(Dal::BuildXccyCashflowPlan(span.first, span.second, instrument->Config()), spec.valuationTime_));
        }
        return requests;
    }

    Dal::CrossCurrencyCalibrationSpec_ HistoricalSpec() {
        auto spec = StagedSpec(Dal::XccyNotionalMode_::Value_::MARK_TO_MARKET);
        spec.knotDates_ = {Dal::Date::AddMonths(spec.today_, 1), Dal::Date::AddMonths(spec.today_, 8)};
        const auto start = Dal::Date::AddMonths(spec.today_, -10);
        for (int index = 0; index < 2; ++index) {
            const auto config = spec.instruments_[index]->Config();
            spec.instruments_[index] = Dal::Handle_<Dal::CrossCurrencySwap_>(
                new Dal::CrossCurrencySwap_(start, start, Dal::Date::AddMonths(start, index == 0 ? 24 : 12), 0.0, config));
        }
        Dal::MarketFixingSnapshot_::values_t values;
        for (const auto& request : HistoricalRequests(spec)) {
            const double value = request.indexName_.find("FX[") == 0 ? 1.1 : (request.indexName_.find("USD") == 0 ? 0.03 : 0.023);
            values[request.indexName_][request.fixingTime_] = value;
        }
        spec.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
        return RepriceStaged(spec);
    }

    class CustomBlock_ final : public Dal::CurveBlock_ {
        int* calls_;

    public:
        CustomBlock_(const Dal::Handle_<Dal::DiscountCurve_>& curve, int* calls) : CurveBlock_(curve), calls_(calls) {}
        const Dal::DiscountCurve_& Discount(const Dal::CollateralType_& collateral) const override {
            ++*calls_;
            return CurveBlock_::Discount(collateral);
        }
    };

    Dal::AAD::BumpOverAADRequest_ SingleDirection(int count) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, count, 0.0);
        request.directions_(0, 0) = 1.0;
        request.steps_ = {1.0e-5};
        return request;
    }

    Dal::AAD::Number_ ParameterObjective(Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
        return x[0] * x[0] + 0.3 * x.back() * x.back();
    }
} // namespace

TEST(RateQuoteCurvatureTest, TestStagedXccySnapshotFactoryAndReplay) {
    const auto spec = StagedSpec();
    const auto plain = Dal::CalibrateCrossCurrencyMarket(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    ASSERT_TRUE(snapshot.Provenance().Available());
    ASSERT_EQ(snapshot.Point(), plain.diagnostics_.marketRates_);
    ASSERT_EQ(snapshot.Parameters(), Dal::InspectCurveParameters(*plain.basisCurves_.at(spec.basisPair_), spec.today_).passiveParameters_);
    auto point = snapshot.Point();
    point[0] += 1.0e-4;
    const auto replay = Dal::RecalibrateRateWithRisk(snapshot, point);
    ASSERT_EQ(replay.Point(), point);
    ASSERT_EQ(replay.Provenance().Axis().fingerprint_, snapshot.Provenance().Axis().fingerprint_);
    ASSERT_NE(replay.Provenance().State().fingerprint_, snapshot.Provenance().State().fingerprint_);
}

TEST(RateQuoteCurvatureTest, TestJointXccySnapshotFactoryAndAxis) {
    const auto spec = JointSpec();
    const auto plain = Dal::CalibrateJointXccyMarket(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    ASSERT_TRUE(snapshot.Provenance().Available());
    ASSERT_EQ(snapshot.Point(), plain.marketRates_);
    ASSERT_EQ(snapshot.Parameters().size(), 8U);
    ASSERT_EQ(snapshot.Provenance().Axis().parameterRanges_.size(), 5U);
    ASSERT_EQ(snapshot.Provenance().Axis().parameterRanges_[0].blockKey_, "domestic:0:discount");
    ASSERT_EQ(snapshot.Provenance().Axis().parameterRanges_[3].blockKey_, "foreign:1:projection");
    ASSERT_EQ(snapshot.Provenance().Axis().residualRanges_.back().blockKey_, "xccy:xccy_basis");
    auto point = snapshot.Point();
    point[0] += 1.0e-4;
    point[7] -= 1.0e-4;
    const auto replay = Dal::RecalibrateRateWithRisk(snapshot, point);
    ASSERT_EQ(replay.Point(), point);
    ASSERT_EQ(replay.Provenance().Axis().fingerprint_, snapshot.Provenance().Axis().fingerprint_);
}

TEST(RateQuoteCurvatureTest, TestStagedXccyIndependentContractCurvature) {
    for (auto mode :
         {Dal::XccyNotionalMode_::Value_::FIXED, Dal::XccyNotionalMode_::Value_::RESETTABLE, Dal::XccyNotionalMode_::Value_::MARK_TO_MARKET}) {
        const auto spec = StagedSpec(mode);
        const auto snapshot = Dal::NewRateCalibration(spec);
        AssertPriceCurvature(snapshot, ContractObjective(spec), [spec](const Dal::Vector_<>& quotes) { return PassivePrice(spec, quotes); });
    }
}

TEST(RateQuoteCurvatureTest, TestJointXccyIndependentContractCurvature) {
    for (bool layered : {false, true}) {
        const auto spec = JointSpec(layered);
        const auto snapshot = Dal::NewRateCalibration(spec);
        AssertPriceCurvature(snapshot, ContractObjective(spec), [spec](const Dal::Vector_<>& quotes) { return PassivePrice(spec, quotes); });
    }
}

TEST(RateQuoteCurvatureTest, TestJointXccyDuplicateDeclarationNames) {
    auto spec = JointSpec();
    for (auto* currency : {&spec.domestic_, &spec.foreign_})
        for (auto& curve : currency->curves_)
            curve.curveName_ = "shared";
    const auto plain = Dal::CalibrateJointXccyMarket(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    ASSERT_EQ(snapshot.Point(), plain.marketRates_);
    ASSERT_EQ(snapshot.Provenance().ComponentKeyByParameterBlock().size(), 5U);
    const auto& ranges = snapshot.Provenance().Axis().parameterRanges_;
    ASSERT_EQ(ranges[0].blockKey_, "domestic:0:shared");
    ASSERT_EQ(ranges[1].blockKey_, "domestic:1:shared");
    ASSERT_EQ(ranges[2].blockKey_, "foreign:0:shared");
    ASSERT_EQ(ranges[3].blockKey_, "foreign:1:shared");
    ASSERT_EQ(spec.domestic_.curves_[0].curveName_, "shared");
    ASSERT_EQ(spec.foreign_.curves_[1].curveName_, "shared");
    auto point = snapshot.Point();
    point.front() += 1.0e-4;
    point.back() -= 1.0e-4;
    const auto replay = Dal::RecalibrateRateWithRisk(snapshot, point);
    ASSERT_EQ(replay.Point(), point);
    ASSERT_EQ(replay.Provenance().Axis().fingerprint_, snapshot.Provenance().Axis().fingerprint_);
    AssertPriceCurvature(snapshot, ContractObjective(spec), [spec](const Dal::Vector_<>& quotes) { return PassivePrice(spec, quotes); });
}

TEST(RateQuoteCurvatureTest, TestStagedXccyOwnsFixedCurveGraph) {
    auto spec = StagedSpec();
    auto base = std::make_shared<Dal::Tape::DiscountPWC_<double>>("base", "USD", Dal::Vector_<Dal::Date_>{spec.today_}, Dal::Vector_<>{0.01});
    auto projection = std::make_shared<Dal::Tape::DiscountPWC_<double>>("projection", "USD", Dal::Vector_<Dal::Date_>{spec.today_},
                                                                        Dal::Vector_<>{0.02}, Dal::Handle_<Dal::DiscountCurve_>(base));
    const Dal::CollateralType_ ois(Dal::CollateralType_::Value_::OIS);
    spec.domesticCurveBlock_ = Dal::Handle_<Dal::CurveBlock_>(
        new Dal::CurveBlock_("domestic", "USD", {{ois, spec.domesticCurveBlock_->DiscountCurves().at(ois)}},
                             {{Dal::PeriodLength_("6M"), Dal::Handle_<Dal::DiscountCurve_>(projection)}}, Dal::DayBasis_("ACT/360")));
    const auto snapshot = Dal::NewRateCalibration(spec);
    const auto before = Dal::EvaluateRateQuoteCurvature(ParameterObjective, snapshot, SingleDirection(2));
    const Dal::Vector_<> shift{0.1};
    base->ApplyDX(shift.begin(), 1.0);
    projection->ApplyDX(shift.begin(), 1.0);
    spec = Dal::CrossCurrencyCalibrationSpec_();
    base.reset();
    projection.reset();
    const auto after = Dal::EvaluateRateQuoteCurvature(ParameterObjective, snapshot, SingleDirection(2));
    ASSERT_EQ(before.Value(), after.Value());
    ASSERT_EQ(before.Gradient(), after.Gradient());
    ASSERT_EQ(before.HessianProducts()(0, 0), after.HessianProducts()(0, 0));
    ASSERT_EQ(before.BaseCalibration().Provenance().State().fingerprint_, after.BaseCalibration().Provenance().State().fingerprint_);
}

TEST(RateQuoteCurvatureTest, TestStagedXccyLegacyCurveBlockFallback) {
    auto spec = StagedSpec();
    auto curve = std::make_shared<Dal::Tape::DiscountPWC_<double>>("legacy", "USD", Dal::Vector_<Dal::Date_>{spec.today_}, Dal::Vector_<>{0.025});
    spec.domesticCurveBlock_ = Dal::Handle_<Dal::CurveBlock_>(new Dal::CurveBlock_(*curve, Dal::DayBasis_("ACT/360")));
    spec = RepriceStaged(spec);
    const auto plain = Dal::CalibrateCrossCurrencyMarket(spec);
    const auto snapshot = Dal::NewRateCalibration(spec);
    ASSERT_EQ(snapshot.Parameters(), Dal::InspectCurveParameters(*plain.basisCurves_.at(spec.basisPair_), spec.today_).passiveParameters_);
    const auto expected = snapshot.Parameters();
    spec = Dal::CrossCurrencyCalibrationSpec_();
    curve.reset();
    ASSERT_EQ(Dal::RecalibrateRateWithRisk(snapshot, snapshot.Point()).Parameters(), expected);
}

TEST(RateQuoteCurvatureTest, TestXccyFactoriesSealHistoricalGlobalFixings) {
    auto staged = HistoricalSpec();
    const auto values = staged.fixings_->Values();
    const ScopedFixings_ restore(values);
    StoreFixings(values);
    const auto requests = HistoricalRequests(staged);
    ASSERT_GE(values.size(), 3U);
    staged.fixings_ = {};
    auto joint = JointSpec();
    joint.basis_.instruments_ = staged.instruments_;
    joint.basis_.knotDates_ = staged.knotDates_;
    joint.fixings_ = {};
    ASSERT_FALSE(Dal::CalibrateCrossCurrencyMarket(staged).diagnostics_.effJacobianInverse_.Empty());
    ASSERT_FALSE(Dal::CalibrateJointXccyMarket(joint).effJacobianInverse_.Empty());
    const auto stagedSnapshot = Dal::NewRateCalibration(staged);
    const auto jointSnapshot = Dal::NewRateCalibration(joint);
    StoreFixings(values, 0.1);
    const auto changed = Dal::SnapshotGlobalFixings(requests);
    for (const auto& entry : values)
        for (const auto& value : entry.second)
            ASSERT_NEAR(changed->Require(entry.first, value.first, "changed global fixing"), value.second + 0.1, 1.0e-14);
    for (const auto& snapshot : {stagedSnapshot, jointSnapshot}) {
        const auto replay = Dal::RecalibrateRateWithRisk(snapshot, snapshot.Point());
        ASSERT_EQ(replay.Parameters(), snapshot.Parameters());
        ASSERT_EQ(replay.Provenance().State().fingerprint_, snapshot.Provenance().State().fingerprint_);
        ASSERT_EQ(replay.Provenance().Axis().fingerprint_, snapshot.Provenance().Axis().fingerprint_);
    }
}

TEST(RateQuoteCurvatureTest, TestXccyShapeModeAndNativeAdmission) {
    auto staged = StagedSpec();
    auto joint = JointSpec();
    staged.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
    joint.solveMode_ = Dal::CurveSolveMode_::Value_::APPROXIMATE;
    ASSERT_THROW((void)Dal::NewRateCalibration(staged), Dal::Exception_);
    ASSERT_THROW((void)Dal::NewRateCalibration(joint), Dal::Exception_);
    staged.solveMode_ = Dal::CurveSolveMode_::Value_::EXACT;
    joint.solveMode_ = Dal::CurveSolveMode_::Value_::EXACT;
    staged.knotDates_.pop_back();
    joint.domestic_.curves_[0].parameterization_ = Dal::CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    ASSERT_THROW((void)Dal::NewRateCalibration(staged), Dal::Exception_);
    ASSERT_THROW((void)Dal::NewRateCalibration(joint), Dal::Exception_);
    joint = JointSpec();
    joint.domestic_.liborBasis_ = Dal::DayBasis_("ACT/360");
    ASSERT_THROW((void)Dal::NewRateCalibration(joint), Dal::Exception_);
    staged = StagedSpec();
    int calls = 0;
    staged.domesticCurveBlock_ = Dal::Handle_<Dal::CurveBlock_>(new CustomBlock_(FixedCurve(staged.today_, "USD", "custom", 0.02), &calls));
    ASSERT_THROW((void)Dal::NewRateCalibration(staged), Dal::Exception_);
    ASSERT_EQ(calls, 0);
}

TEST(RateQuoteCurvatureTest, TestXccyWideModeCapacityFailureAndRecovery) {
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 3);
    const auto staged = Dal::NewRateCalibration(StagedSpec());
    const auto joint = Dal::NewRateCalibration(JointSpec());
    for (const auto& snapshot : {staged, joint}) {
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(0, static_cast<int>(snapshot.Point().size()));
        const auto baseline = Dal::EvaluateRateQuoteCurvature(ParameterObjective, snapshot, request);
        ASSERT_EQ(baseline.Execution().calibrations_, 1U);
        ASSERT_GT(baseline.Execution().peakTapeBytes_, 0U);
        request.recordingCapacityBudgetBytes_ = 0U;
        int calls = 0;
        const Dal::AAD::NativeScalarFunction_ objective = [&](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::Number_>& x) {
            ++calls;
            return x[0] * x[0];
        };
        ASSERT_THROW((void)Dal::EvaluateRateQuoteCurvature(objective, snapshot, request), Dal::Exception_);
        ASSERT_EQ(calls, 0);
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 3U);
        request.recordingCapacityBudgetBytes_.reset();
        const auto recovered = Dal::EvaluateRateQuoteCurvature(ParameterObjective, snapshot, request);
        ASSERT_EQ(recovered.Value(), baseline.Value());
        ASSERT_EQ(recovered.Gradient(), baseline.Gradient());
    }
    ASSERT_FALSE(Dal::AAD::NativeOperations_::Capabilities().higherOrder_);
}

TEST(RateQuoteCurvatureTest, TestXccyFactoriesAndReplayPreserveActiveOuterRecording) {
    const auto stagedSpec = StagedSpec();
    const auto jointSpec = JointSpec();
    const auto staged = Dal::NewRateCalibration(stagedSpec);
    const auto joint = Dal::NewRateCalibration(jointSpec);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    ASSERT_THROW((void)Dal::NewRateCalibration(stagedSpec), Dal::Exception_);
    ASSERT_THROW((void)Dal::NewRateCalibration(jointSpec), Dal::Exception_);
    for (const auto& snapshot : {staged, joint}) {
        ASSERT_THROW((void)Dal::RecalibrateRateWithRisk(snapshot, snapshot.Point()), Dal::Exception_);
        ASSERT_THROW((void)Dal::EvaluateRateQuoteCurvature(ParameterObjective, snapshot, SingleDirection(static_cast<int>(snapshot.Point().size()))),
                     Dal::Exception_);
    }
    const Dal::AAD::Number_ output = input * input;
    recording.FinishRecording();
    recording.ClearAdjoints();
    auto root = output;
    Dal::AAD::NativeOperations_::SetSeed(root, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(RateQuoteCurvatureTest, TestXccyDefaultAndMalformedHeaderAdmission) {
    ASSERT_THROW((void)Dal::NewRateCalibration(Dal::CrossCurrencyCalibrationSpec_()), Dal::Exception_);
    ASSERT_THROW((void)Dal::NewRateCalibration(Dal::JointXccyCalibrationSpec_()), Dal::Exception_);
    auto joint = JointSpec();
    joint.domestic_.ccy_ = Dal::Ccy_();
    ASSERT_THROW((void)Dal::NewRateCalibration(joint), Dal::Exception_);
    joint = JointSpec();
    joint.domestic_.curves_[0].curveName_.clear();
    ASSERT_THROW((void)Dal::NewRateCalibration(joint), Dal::Exception_);
    auto staged = StagedSpec();
    staged.basisPair_.domestic_ = Dal::Ccy_();
    ASSERT_THROW((void)Dal::NewRateCalibration(staged), Dal::Exception_);
}
