#include <chrono>
#include <iomanip>
#include <iostream>

#include <dal-public/src/global.hpp>
#include <dal-public/src/xccymontecarlobasket.hpp>
#include <dal/curve/yclogdf.hpp>

#include "../../../dal-cpp/examples/floatformat.hpp"
#include "../xccyexample.hpp"

namespace {
    using namespace Dal;
    using Clock_ = std::chrono::steady_clock;

    XccyGaussianModelSpec_ ModelSpec(const Date_& valuation) {
        const auto horizon = valuation.AddDays(1095);
        const auto curve = [&](const String_& currency, double rate) {
            return Handle_<DiscountCurve_>(
                NewDiscountLogDF(currency, currency, {valuation, horizon}, {0., -rate * 3.}, DayBasis_("ACT365F"), LogDfScheme_::Value_::LOG_LINEAR));
        };
        XccyGaussianModelSpec_ spec;
        auto& market = spec.market_;
        market.valuationTime_ = DateTime_(valuation);
        market.pair_ = CurrencyPair_(Ccy_("USD"), Ccy_("EUR"));
        market.collateralCurrency_ = Ccy_("USD");
        market.fxSpot_ = 1.1;
        market.discountRoute_ = "USD.OIS";
        market.domesticBlock_.reset(
            new CurveBlock_("usd", "USD", {{CollateralType_("OIS"), curve("USD", .04)}}, {{PeriodLength_("3M"), curve("USD", .045)}}));
        market.foreignBlock_.reset(
            new CurveBlock_("eur", "EUR", {{CollateralType_("OIS"), curve("EUR", .02)}}, {{PeriodLength_("6M"), curve("EUR", .026)}}));
        market.basis_ = curve("USD", .003);
        auto& dynamics = spec.dynamics_;
        dynamics.domesticA_ = .03;
        dynamics.foreignA_ = .07;
        dynamics.bucketEndDates_ = {horizon};
        dynamics.domesticSigma_ = {.01};
        dynamics.foreignSigma_ = {.009};
        dynamics.fxSigma_ = {.12};
        dynamics.factorNames_ = {"rate:USD", "rate:EUR", "fx:EUR/USD"};
        dynamics.correlation_ = Matrix_<>(3, 3, 0.);
        for (int i = 0; i < 3; ++i)
            dynamics.correlation_(i, i) = 1.;
        dynamics.correlation_(0, 1) = dynamics.correlation_(1, 0) = .30;
        dynamics.correlation_(0, 2) = dynamics.correlation_(2, 0) = -.20;
        dynamics.correlation_(1, 2) = dynamics.correlation_(2, 1) = -.25;
        return spec;
    }

    Vector_<XccyMcBasketMember_> Members(const Date_& valuation) {
        auto contract = Examples::XccyContract();
        contract.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
        contract.fxReset_.fixingLag_ = contract.fxReset_.fixingHour_ = contract.fxReset_.fixingMinute_ = 0;
        const auto instrument = [&](const String_& id, const Date_& end) {
            const CrossCurrencySwap_ native(valuation, Date_(2027, 5, 4), end, .001, contract);
            return NewXccyScriptInstrument(native, {id, "USD.OIS", true, 1e-12});
        };
        return {{"Short", instrument("six-month", Date_(2027, 11, 4)), "Price"},
                {"short", instrument("twelve-month", Date_(2028, 5, 4)), "BasisSpread"}};
    }

    void PrintMembers(const XccyMcBasketSnapshot_& result, const XccyMcBasketSnapshot_& plain) {
        std::cout << "XCCY basket / Validation / Estimate (4 batches x 64 paths per member)\n"
                  << std::left << std::setw(12) << "Member ID" << std::setw(16) << "Measure" << std::setw(16) << "Unit" << std::right << std::setw(18)
                  << "Plain mean" << std::setw(18) << "AAD mean" << std::setw(18) << "Batch SE" << '\n'
                  << std::string(98, '-') << '\n';
        for (size_t i = 0; i < result.members_.size(); ++i) {
            const auto& member = result.members_[i];
            const auto& allocation = result.allocation_.members_[i];
            const auto& estimate = member.completedEstimate_->estimate_;
            std::cout << std::left << std::setw(12) << member.memberId_ << std::setw(16) << allocation.measureId_ << std::setw(16)
                      << allocation.measureUnit_ << std::right << std::setw(18) << ExampleFloat(plain.members_[i].completedEstimate_->estimate_.mean_)
                      << std::setw(18) << ExampleFloat(estimate.mean_) << std::setw(18) << ExampleFloat(estimate.standardError_) << '\n'
                      << "  mapping D=" << allocation.mapping_.dimension_ << "; raw dMean/dSpot=" << ExampleFloat(estimate.gradient_[0])
                      << "; derivative SE=" << ExampleFloat(estimate.gradientStandardError_[0])
                      << "; AAD minus plain=" << ExampleFloat(estimate.mean_ - plain.members_[i].completedEstimate_->estimate_.mean_) << '\n';
        }
        std::cout << "Spot axis: fxspot:EUR/USD (USD per EUR); raw partials hold model curves fixed.\n"
                  << "Plain/AAD reuse the same coordinates; the replay adds no independent samples.\n";
    }

    void PrintCovariance(const char* title, const Vector_<XccyMcBasketAxis_>& axes, const Matrix_<>& covariance) {
        std::cout << '\n'
                  << title << " (entry units = row unit x column unit)\n"
                  << std::left << std::setw(8) << "Axis" << std::setw(12) << "Member ID" << std::setw(32) << "Label" << "Unit\n"
                  << std::string(84, '-') << '\n';
        for (size_t i = 0; i < axes.size(); ++i)
            std::cout << std::left << std::setw(8) << i << std::setw(12) << axes[i].memberId_ << std::setw(32) << axes[i].label_ << axes[i].unit_
                      << '\n';
        std::cout << std::right << std::setw(8) << "Row" << std::setw(8) << "Column" << std::setw(24) << "Covariance" << '\n'
                  << std::string(40, '-') << '\n';
        for (int i = 0; i < covariance.Rows(); ++i)
            for (int j = 0; j < covariance.Cols(); ++j)
                std::cout << std::right << std::setw(8) << i << std::setw(8) << j << std::setw(24) << ExampleFloat(covariance(i, j)) << '\n';
    }

    void PrintFailure(const CalibrationError_& error, const Handle_<XccyMcBasketPartial_>& partial) {
        std::cerr << "Failed: " << error.Failure().diagnostic_.code_ << '\n'
                  << "Validated candidate available: " << static_cast<bool>(error.Failure().candidate_) << '\n'
                  << DumpCalibrationReportJson(error.Failure().report_) << '\n';
        if (partial)
            std::cerr << "Owned partial JSON:\n" << DumpXccyMonteCarloBasketPartialJson(partial) << '\n';
        else
            std::cerr << "Partial: unavailable (failure before a complete allocation).\n";
    }

    double Milliseconds(Clock_::time_point start, Clock_::time_point end) { return std::chrono::duration<double, std::milli>(end - start).count(); }

    void Run() {
        InitGlobalData(1);
        const Date_ valuation(2026, 10, 21);
        const auto model = NewXccyGaussianModel("basket-model", ModelSpec(valuation));
        const auto members = Members(valuation);
        XccyMonteCarloBasketRequest_ request;
        request.phase_ = XccyMonteCarloPhase_::Value_::Validation;
        request.sampling_ = {"PseudoRandom", 64, 4, 1024, "dal.mrg32-normal-paths/1"};
        request.fixings_.reset(new MarketFixingSnapshot_({}));
        const auto plain = PrepareXccyBasketMonteCarlo(members, model, request).Evaluate();
        request.derivativeMethod_ = CalibrationDerivative_::Value_::AAD;
        request.parameterLabels_ = {"fxspot:EUR/USD"};
        const auto start = Clock_::now();
        const auto prepared = PrepareXccyBasketMonteCarlo(members, model, request);
        const auto ready = Clock_::now();
        const auto& allocation = prepared.Allocation();
        REQUIRE(allocation.members_[0].mapping_.identity_ != allocation.members_[1].mapping_.identity_, "different dated mappings");
        const auto result = prepared.Evaluate();
        const auto evaluated = Clock_::now();
        const auto& snapshot = result.Snapshot();
        REQUIRE(snapshot.allocation_.identity_ == plain.Snapshot().allocation_.identity_, "same-coordinate plain/AAD comparison");
        PrintMembers(snapshot, plain.Snapshot());
        PrintCovariance("Coefficient batch covariance C_batch", snapshot.coefficientAxes_, snapshot.coefficientCovariance_);
        PrintCovariance("Coefficient mean covariance C_mean", snapshot.coefficientAxes_, snapshot.meanCovariance_);
        PrintCovariance("Output estimation-error covariance", snapshot.outputAxes_, snapshot.outputCovariance_);
        std::cout << "\nEstimate JSON:\n"
                  << DumpXccyMonteCarloBasketResultJson(result) << '\n'
                  << "Numerical acceptance: unavailable; calibrated quote risk: unavailable.\n"
                  << "Marginal estimation error covariance; no common economic scenarios.\n"
                  << "Small samples illustrate calls only; no untouched HeldOut claim.\n";
        const auto printed = Clock_::now();
        std::cout << "Informational ms: prepare=" << ExampleFloat(Milliseconds(start, ready))
                  << "; evaluate=" << ExampleFloat(Milliseconds(ready, evaluated))
                  << "; format/JSON/output=" << ExampleFloat(Milliseconds(evaluated, printed)) << '\n';
    }
} // namespace

int main() {
    try {
        Run();
        return 0;
    } catch (const Dal::XccyMcBasketError_& error) {
        PrintFailure(error, error.Partial());
    } catch (const Dal::CalibrationError_& error) {
        PrintFailure(error, {});
    }
    return 1;
}
