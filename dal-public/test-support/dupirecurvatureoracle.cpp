//
// Created by Codex on 2026/10/10.
//

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

#include <dal-public/test-support/dupirecurvaturefixtures.hpp>
#include <dal/platform/initall.hpp>

using namespace Dal::Script::TestSupport::DupireCurvature;

static_assert(std::numeric_limits<long double>::digits > std::numeric_limits<double>::digits);

namespace {
    long double PreciseCall(const Dal::AAD::IVS_& ivs, const Dal::AAD::RiskView_<long double>& quotes, double strike, double maturity) {
        const long double t = maturity;
        const long double forward = static_cast<long double>(ivs.Spot()) * std::exp((ivs.Rate() - ivs.DividendYield()) * t);
        const long double sigma = ivs.ImpliedVol(strike, maturity) + quotes.Spread(strike, maturity);
        const long double deviation = sigma * std::sqrt(t);
        const long double d1 = std::log(forward / strike) / deviation + 0.5L * deviation;
        const long double d2 = d1 - deviation;
        const auto normal = [](long double d) { return 0.5L * std::erfc(-d / std::sqrt(2.0L)); };
        return std::exp(-ivs.Rate() * t) * (forward * normal(d1) - strike * normal(d2));
    }

    double PreciseLocalVol(const Dal::AAD::IVS_& ivs, const Dal::AAD::RiskView_<long double>& quotes, double strike, double maturity) {
        const double dt = 1e-4 * maturity, ds = 1e-4 * strike;
        const long double center = PreciseCall(ivs, quotes, strike, maturity);
        const long double ct = (PreciseCall(ivs, quotes, strike, maturity + dt) - PreciseCall(ivs, quotes, strike, maturity - dt)) / (2.0L * dt);
        const long double lower = PreciseCall(ivs, quotes, strike - ds, maturity), upper = PreciseCall(ivs, quotes, strike + ds, maturity);
        const long double ck = (upper - lower) / (2.0L * ds), ckk = (lower + upper - 2.0L * center) / ds / ds;
        return static_cast<double>(std::sqrt(2.0L * (ct + ivs.DividendYield() * center + (ivs.Rate() - ivs.DividendYield()) * strike * ck) / ckk) /
                                   strike);
    }

    double PreciseValue(const Dal::AAD::IVS_& ivs, const Dal::DupireCalibrationSnapshot_& original, const Dal::Matrix_<>& spreads) {
        const auto& inputs = original.Inputs();
        const auto& shape = *original.Surface();
        Dal::AAD::RiskView_<long double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
        for (int row = 0; row < spreads.Rows(); ++row)
            for (int column = 0; column < spreads.Cols(); ++column)
                quotes.Bump(row, column, spreads(row, column));
        Dal::Matrix_<> values(shape.vols_.Rows(), shape.vols_.Cols());
        for (int column = 0; column < values.Cols(); ++column) {
            const double maturity = shape.times_[column];
            const double width = 2.5 * ivs.Call(ivs.Spot(), maturity) * Dal::M_SQRT_2_PI;
            const int low = static_cast<int>(std::lower_bound(shape.spots_.begin(), shape.spots_.end(), ivs.Spot() - width) - shape.spots_.begin());
            const int high = static_cast<int>(std::upper_bound(shape.spots_.begin(), shape.spots_.end(), ivs.Spot() + width) - shape.spots_.begin());
            for (int row = low; row < high; ++row)
                values(row, column) = PreciseLocalVol(ivs, quotes, shape.spots_[row], maturity);
            for (int row = 0; row < low; ++row)
                values(row, column) = values(low, column);
            for (int row = high; row < values.Rows(); ++row)
                values(row, column) = values(high - 1, column);
        }
        const Dal::Handle_<Dal::LocalVolSurfaceData_> surface(new Dal::LocalVolSurfaceData_(shape.Name(), shape.spots_, shape.times_, values));
        const auto hybrid = Dal::handle_cast<Dal::HybridModelData_>(Model(original));
        auto components = hybrid->components_;
        const auto local = Dal::handle_cast<Dal::HybridLocalVolEquityData_>(components[0]);
        components[0] = Dal::Handle_<Dal::HybridComponentData_>(new Dal::HybridLocalVolEquityData_(
            local->Name(), local->index_, local->currency_, local->factor_, local->spot_, local->div_, surface, local->maxStep_));
        const Dal::Handle_<Dal::ModelData_> rebuilt(
            new Dal::HybridModelData_(hybrid->Name(), hybrid->domesticCurrency_, components, hybrid->correlation_));
        const auto request = RiskRequest();
        auto simulation = request.simulation_;
        simulation.enableAad_ = false;
        return Dal::ValueByMonteCarlo(MixedProduct(spreads(1, 1)), rebuilt, request.numPaths_, request.valuation_, simulation).at("PV");
    }
} // namespace

int main() {
    Dal::RegisterAll_::Init();
    Dal::ThreadPool_::GetInstance()->Start(1, true);
    const Dal::Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
    const FlatIVS_ flat;
    const Dal::AAD::MertonIVS_ mixed(100.0, 0.2, 0.08, -0.1, 0.15);
    for (const Dal::AAD::IVS_* ivs : {static_cast<const Dal::AAD::IVS_*>(&flat), static_cast<const Dal::AAD::IVS_*>(&mixed)}) {
        const auto calibration = Dal::CalibrateDupireWithRisk(*ivs, Calibration().Inputs());
        for (const double step : {4e-4, 2e-4, 1e-4}) {
            for (size_t column = 0; column < 6; ++column) {
                auto pp = calibration.Inputs().quoteSpreads_, pm = pp, mp = pp, mm = pp;
                for (size_t q = 0; q < 6; ++q) {
                    pp.Data()[q] += step * direction[q];
                    pm.Data()[q] += step * direction[q];
                    mp.Data()[q] -= step * direction[q];
                    mm.Data()[q] -= step * direction[q];
                }
                pp.Data()[column] += 2e-4;
                pm.Data()[column] -= 2e-4;
                mp.Data()[column] += 2e-4;
                mm.Data()[column] -= 2e-4;
                const double product = (PreciseValue(*ivs, calibration, pp) - PreciseValue(*ivs, calibration, pm) -
                                        PreciseValue(*ivs, calibration, mp) + PreciseValue(*ivs, calibration, mm)) /
                                       (4.0 * step * 2e-4);
                std::cout << "precise " << (ivs == &mixed) << ' ' << std::setprecision(17) << step << ' ' << column << ' ' << product << '\n';
            }
        }
    }
}
