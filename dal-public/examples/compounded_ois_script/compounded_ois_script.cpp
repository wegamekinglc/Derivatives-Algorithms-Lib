#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal-public/src/calibrationinstrument.hpp>
#include <dal-public/src/global.hpp>

#include "../../../dal-cpp/examples/floatformat.hpp"

using namespace Dal;

namespace {
    CompoundedOisScriptSpec_ Contract() {
        const Date_ start(2024, 2, 29), friday(2024, 3, 1), tuesday(2024, 3, 5), end(2024, 3, 6), payment(2024, 3, 8);
        CompoundedOisScriptSpec_ spec;
        spec.instrumentId_ = "mixed-sofr";
        spec.notional_ = 100.;
        spec.coupon_ = .02;
        spec.direction_ = "PayFixed";
        spec.currency_ = spec.collateralCurrency_ = Ccy_("USD");
        spec.collateralType_ = CollateralType_("OIS");
        spec.discountRoute_ = "USD.OIS";
        spec.projectionRoute_ = "USD.SOFR";
        spec.fixingIndex_ = "USD-SOFR";
        spec.fixedBasis_ = DayBasis_("ACT_365F");
        spec.observationBasis_ = DayBasis_("ACT_360");
        spec.calendar_ = HolidayCenterData_("explicit-Mar4-holiday", {Date_(2024, 3, 4)});
        spec.fixedLeg_ = {{start, end, payment, 6. / 365.}};
        spec.coupons_ = {{"coupon", start, end, payment}};
        spec.observations_ = {{start, friday, DateTime_(start), 1. / 360., "coupon", start, friday},
                              {friday, tuesday, DateTime_(friday), 4. / 360., "coupon", friday, tuesday},
                              {tuesday, end, DateTime_(tuesday), 1. / 360., "coupon", tuesday, end}};
        return spec;
    }

    CurveScriptEvaluationRequest_ Request(bool withHistory) {
        const Date_ today(2024, 3, 1);
        CurveScriptEvaluationRequest_ request;
        request.valuation_.evaluationDate_ = today;
        request.valuation_.todayFixingPolicy_ = TodayFixingPolicy_("Model");
        request.valuation_.fixings_ =
            withHistory ? Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"USD-SOFR", {{DateTime_(Date_(2024, 2, 29)), -.012}}}}))
                        : Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_());
        request.compoundedOis_ = PrepareCompoundedOis(Contract(), request.valuation_);
        request.geometry_.today_ = today;
        request.geometry_.ccy_ = "USD";
        request.geometry_.parameterization_ = CurveParameterization_("LOG_DISCOUNT");
        request.geometry_.knotDates_ = {today, today.AddDays(20)};
        request.geometry_.calibrateDiscountCurve_ = false;
        request.geometry_.targetOvernightRoute_ = "USD.SOFR";
        request.market_.routes_ = {{"USD.OIS", false, CollateralType_("OIS"), {}, false, true}, {"USD.SOFR", true, CollateralType_("OIS"), {}, true}};
        request.parameters_ = {-.002};
        request.derivativeMethod_ = CalibrationDerivative_("AAD");
        return request;
    }

    CurveScriptEvaluationResult_ Evaluate() {
        const auto request = Request(true);
        return EvaluateCurveScriptInstrument(NewCompoundedOisScriptInstrument(request.compoundedOis_), request);
    }

    Handle_<CalibrationReport_> MissingFixingReport() {
        try {
            const auto request = Request(false);
            EvaluateCurveScriptInstrument(NewCompoundedOisScriptInstrument(request.compoundedOis_), request);
        } catch (const CalibrationError_& error) {
            return error.Failure().report_;
        }
        THROW("Expected missing historical fixing");
    }
    void PrintValues(const CurveScriptEvaluationResult_& result, const Matrix_<>& partials) {
        std::cout << "PayFixed, N=100 USD, fixed K=0.02, today policy=Model\n"
                  << "Friday-Mar5: one 4/360 factor across weekend and Mar4 holiday; payment=2024-03-08\n\n"
                  << std::left << std::setw(16) << "Measure" << std::setw(16) << "Unit" << "Value\n";
        for (size_t i = 0; i < result.values_.size(); ++i)
            std::cout << std::setw(16) << result.measureIds_[i] << std::setw(16) << result.measureUnits_[i] << ExampleFloat(result.values_[i], 10)
                      << '\n';
        std::cout << "\nRaw native partials (no basis-point scaling)\n"
                  << std::setw(18) << "Parameter date" << std::setw(20) << "Component" << std::setw(20) << "dPrice/dTheta" << "dParRate/dTheta\n";
        for (size_t j = 0; j < result.freeParameters_.size(); ++j) {
            const auto& axis = result.freeParameters_[j];
            std::cout << std::setw(18) << Date::ToString(axis.date_) << std::setw(20) << axis.component_.String() << std::setw(20)
                      << ExampleFloat(partials(0, j), 10) << ExampleFloat(partials(1, j), 10) << '\n';
        }
    }
} // namespace

int main() {
    InitGlobalData(1);
    const auto result = Evaluate();
    REQUIRE(result.report_ && result.report_->Errors().empty() && result.values_.size() == 2, "successful Price/ParRate evaluation required");
    const double price = CurveScriptEvaluationValue(result, "Price"), par = CurveScriptEvaluationValue(result, "ParRate");
    const auto partials = CurveScriptEvaluationPartials(result);
    REQUIRE(partials.Rows() == 2 && partials.Cols() == static_cast<int>(result.freeParameters_.size()), "native axes required");
    const double floating = (1. - .012 / 360.) * std::exp(.0005) - 1., annuity = 6. / 365., df = std::exp(-.0007);
    REQUIRE(std::abs(price - 100. * (floating - .02 * annuity) * df) <= 1e-10, "independent mixed-coupon PV oracle");
    REQUIRE(std::abs(par - floating / annuity) <= 1e-10, "independent par oracle");
    REQUIRE(result.observations_[0].source_ == "Historical" && result.observations_[1].source_ == "Projection", "mixed provenance required");
    PrintValues(result, partials);
    REQUIRE(CurveScriptEvaluationTable(result, "cashflows").Rows() == 3, "two payments plus header");
    const auto report = MissingFixingReport();
    REQUIRE(report && !report->Errors().empty(), "owning report must survive request, tables and exception");
    REQUIRE(report->Errors()[0].code_ == "MissingFixing", "explicit historical fixing must be the failure cause");
    std::cout << "\nOwning missing-fixing report\n";
    for (const auto& diagnostic : report->Errors())
        std::cout << diagnostic.code_ << " / " << diagnostic.field_.value_or("") << " / " << diagnostic.message_ << '\n';
}
