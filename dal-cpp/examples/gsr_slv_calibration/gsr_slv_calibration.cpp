//
// Created by Codex on 2026/10/3.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>

#include <dal/platform/platform.hpp>

#include <dal/platform/initall.hpp>
#include <dal/storage/globals.hpp>
#include <dal/math/matrix/matrixutils.hpp>
#include <dal-public/src/gsr.hpp>
#include <dal-public/src/models.hpp>

#include "../floatformat.hpp"

using namespace Dal;

namespace {
    const Date_ TODAY(2026, 10, 3);
    constexpr double INPUT_RATE = 0.03;
    constexpr double PRICE_SCALE = 0.0005;

    GSRCaplet_ Caplet(int years, double offset = 0.0) {
        const auto expiry = TODAY.AddDays(365 * years), end = expiry.AddDays(182);
        const double accrual = 182.0 / 365.0, forward = std::expm1(INPUT_RATE * accrual) / accrual;
        return {expiry, expiry, end, end, accrual, accrual, "6M", forward + offset, OptionType_("CALL")};
    }

    Handle_<ModelData_> GaussianModel(const Handle_<GSRCurveData_>& curve) {
        MultiFactorGSRVolSettings_ settings;
        settings.factorNames_ = {"level", "slope", "curvature"};
        settings.gKnotDates_ = {TODAY, TODAY.AddDays(365)};
        settings.gValues_ = Matrix::FromVectors<double>({{0.009, 0.009}, {0.003, 0.003}, {0.002, 0.002}});
        settings.hKnotDates_ = {TODAY, TODAY.AddDays(365), TODAY.AddDays(1095)};
        settings.hValues_ = Matrix::FromVectors<double>({{1.0, 1.0, 1.0}, {0.4, 0.3, 0.2}, {0.1, 0.2, 0.1}});
        settings.correlations_ = Matrix::FromVectors<double>({{1.0, 0.25, 0.1}, {0.25, 1.0, 0.15}, {0.1, 0.15, 1.0}});
        return NewMultiFactorGSRModelData("gaussian", curve, NewMultiFactorGSRVolData("gaussian_vol", settings));
    }

    Handle_<ModelData_> CalibrateGaussian(const Handle_<GSRCurveData_>& curve) {
        const Vector_<GSRMarketQuote_> market{{"1Y ATM", Caplet(1), 0.0100, 0.000001}, {"2Y ATM", Caplet(2), 0.0110, 0.000001}};
        const auto prices = ConvertGSRMarketQuotes(Handle_<Storable_>(curve), market);
        Vector_<GSRCalibrationQuote_> quotes;
        for (size_t i = 0; i < market.size(); ++i)
            quotes.push_back({market[i].name_, market[i].option_, prices[i].price_, market[i].priceScale_});
        const auto fit = CalibrateGSRVolatility(GaussianModel(curve), quotes, {{0, 0, 0.001, 0.04}, {0, 1, 0.001, 0.04}});
        std::cout << "\nThree-factor Gaussian fit: two level-factor g buckets; slope, curvature, H and correlations fixed\n"
                  << "Converged: " << fit.converged_ << "; fit: " << fit.fitWithinTolerance_ << "; numerical: " << fit.numericalValidationPassed_
                  << "\nFitted g: " << ExampleFloat(fit.parameters_[0]) << ", " << ExampleFloat(fit.parameters_[1]) << '\n';
        REQUIRE(fit.converged_ && fit.fitWithinTolerance_ && fit.numericalValidationPassed_,
                "Gaussian calibration failed: " + fit.terminationReason_);
        return Handle_<ModelData_>(fit.model_);
    }

    Vector_<GSRMarketQuote_> SmileQuotes() {
        struct Input_ {
            int years_;
            double offset_, volatility_;
        };
        const std::array<Input_, 6> inputs{
            {{1, -0.005, 0.01120}, {1, 0.0, 0.01050}, {1, 0.005, 0.01005}, {2, -0.005, 0.01175}, {2, 0.0, 0.01140}, {2, 0.005, 0.01105}}};
        Vector_<GSRMarketQuote_> quotes;
        for (const auto& input : inputs) {
            const String_ name = String::FromInt(input.years_) + "Y " + String_(ExampleFloat(input.offset_, 3));
            quotes.push_back({name, Caplet(input.years_, input.offset_), input.volatility_, PRICE_SCALE});
        }
        return quotes;
    }

    Handle_<ModelData_> InitialSLV(const Handle_<ModelData_>& gaussian) {
        GSRSLVSettings_ settings;
        settings.kappa_ = 1.0;
        settings.volOfVol_ = 0.5;
        settings.varianceCorrelations_ = {-0.2, 0.0, 0.0};
        settings.maxStep_ = 1.0 / 12.0;
        const auto leverage = NewGSRLeverageData("leverage", {-0.01, 0.0, 0.01}, {0.0}, Matrix_<>(3, 1, 1.0));
        return NewGSRSLVModelData("initial_slv", gaussian, leverage, settings);
    }

    void ReportFit(const Handle_<GSRCurveData_>& curve, const Vector_<GSRMarketQuote_>& quotes, const GSRSLVCalibrationResult_& fit) {
        std::cout << "\nSLV fit: three leverage nodes; Gaussian, CIR and correlations fixed\n"
                  << "Converged: " << fit.converged_ << "; fit: " << fit.fitWithinTolerance_ << "; numerical: " << fit.numericalValidationPassed_
                  << "; held-out: " << fit.heldOutWithinTolerance_ << "\nJacobian rank: " << fit.jacobianRank_
                  << "/3; iterations: " << fit.iterations_ << '\n';
        for (size_t row = 0; row < fit.parameters_.size(); ++row)
            std::cout << "  leverage:" << row << ":0: " << ExampleFloat(fit.parameters_[row]) << '\n';
        const auto prices = ConvertGSRMarketQuotes(Handle_<Storable_>(curve), quotes);
        std::cout << std::left << std::setw(15) << "Quote" << std::right << std::setw(15) << "Market PV" << std::setw(15) << "Fitted PV"
                  << std::setw(18) << "Residual/scale" << std::setw(15) << "Pair SE" << '\n';
        for (size_t i = 0; i < quotes.size(); ++i)
            std::cout << std::left << std::setw(15) << quotes[i].name_ << std::right << std::setw(15) << ExampleFloat(prices[i].price_)
                      << std::setw(15) << ExampleFloat(fit.modelPrices_[i]) << std::setw(18) << ExampleFloat(fit.residuals_[i] / PRICE_SCALE)
                      << std::setw(15) << ExampleFloat(fit.standardErrors_[i]) << '\n';
        std::cout << "Max validation error: " << ExampleFloat(*std::max_element(fit.numericalErrors_.begin(), fit.numericalErrors_.end()))
                  << "; budget: " << ExampleFloat(0.25 * PRICE_SCALE) << "\nHeld-out 2Y +0.0025 PV: " << ExampleFloat(fit.heldOutPrices_[0])
                  << "; PV error: " << ExampleFloat(fit.heldOutResiduals_[0]) << '\n';
    }

    Handle_<ModelData_> CalibrateSLV(const Handle_<GSRCurveData_>& curve, const Handle_<ModelData_>& gaussian) {
        GSRSLVCalibrationSettings_ settings;
        settings.pricing_ = {16384, 1729};
        settings.validation_ = {32768, 81173};
        settings.solver_.priorWeight_ = 0.01;
        settings.solver_.smoothingWeight_ = 0.0001;
        const Vector_<GSRSLVCalibrationParameter_> parameters{{"leverage:0:0", 0.2, 2.0}, {"leverage:1:0", 0.2, 2.0}, {"leverage:2:0", 0.2, 2.0}};
        const auto quotes = SmileQuotes();
        const Vector_<GSRMarketQuote_> heldOut{{"2Y +0.0025 held-out", Caplet(2, 0.0025), 0.01120, PRICE_SCALE}};
        const auto fit = CalibrateGSRSLVMarket(InitialSLV(gaussian), quotes, parameters, settings, heldOut);
        ReportFit(curve, quotes, fit);
        REQUIRE(fit.converged_ && fit.fitWithinTolerance_ && fit.numericalValidationPassed_ && fit.heldOutWithinTolerance_,
                "SLV calibration diagnostics failed: " + fit.terminationReason_);
        return Handle_<ModelData_>(fit.model_);
    }

    GSRSwaption_ Swaption() {
        const auto expiry = TODAY.AddDays(365), start = expiry.AddDays(2), secondFixing = expiry.AddDays(365), secondStart = secondFixing.AddDays(2),
                   end = secondStart.AddDays(365), firstPayment = secondStart.AddDays(2), payment = end.AddDays(2);
        return {expiry,
                {{firstPayment, 1.0}, {payment, 1.0}},
                {{expiry, start, secondStart, firstPayment, 1.0, 1.0, "12M"}, {secondFixing, secondStart, end, payment, 1.0, 1.0, "12M"}},
                0.03,
                OptionType_("CALL")};
    }

    void PriceProducts(const Handle_<ModelData_>& model) {
        const auto prices = PriceGSRSLVEuropeanOptions(model, {Caplet(1), Swaption()}, {8192, 27183, 32});
        const std::array<const char*, 2> names{{"1Y ATM caplet", "1Y payer swaption"}};
        std::cout << '\n'
                  << std::left << std::setw(24) << "Product" << std::right << std::setw(15) << "PV/notional" << std::setw(15) << "Pair SE"
                  << std::setw(20) << "Conditional diff" << '\n';
        for (size_t i = 0; i < prices.size(); ++i) {
            std::cout << std::left << std::setw(24) << names[i] << std::right << std::setw(15) << ExampleFloat(prices[i].price_) << std::setw(15)
                      << ExampleFloat(prices[i].standardError_) << std::setw(20) << ExampleFloat(prices[i].conditionalError_) << '\n';
            REQUIRE(std::isfinite(prices[i].price_) && prices[i].price_ > 0.0, "Invalid example product price");
        }
    }
} // namespace

int main() {
    RegisterAll_::Init();
    Global::Dates_::SetEvaluationDate(TODAY);
    const auto curve = NewGSRCurveData("flat_ois", TODAY, "USD", {TODAY, TODAY.AddDays(1825)}, {0.0, -INPUT_RATE * 5.0}, {}, Matrix_<>(0, 0));
    std::cout << std::boolalpha << "GSR + SLV calibration and pricing; illustrative Normal quotes, unit notional\n"
              << "Flat 3% discount/forecast curve; quotes are annualized decimals.\n";
    const auto gaussian = CalibrateGaussian(curve);
    const auto smile = CalibrateSLV(curve, gaussian);
    PriceProducts(smile);
    return 0;
}
