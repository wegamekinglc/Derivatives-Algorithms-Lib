#include <iostream>

#include <dal/math/interp/interp2d.hpp>
#include <dal/math/matrix/banded.hpp>
#include <dal/math/matrix/decompositions.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/model/ivs.hpp>

#include "dupirereference.hpp"
#include "dupiresecondorder.hpp"

using namespace Dal;

namespace TapePerfDupire {
    namespace {
        class FlatImpliedVol_ final : public AAD::IVS_ {
        public:
            FlatImpliedVol_() : IVS_(SPOT, RATE, DIVIDEND) {}
            double ImpliedVol(double, double) const override { return BASE_VOL; }
        };

        double PassivePrice(const LocalVolSurfaceData_& surface) {
            Vector_<> logSpots(surface.spots_.size());
            for (size_t i = 0; i < logSpots.size(); ++i)
                logSpots[i] = std::log(surface.spots_[i] / SPOT);
            Vector_<> previous(INTERIOR), next;
            for (int row = 0; row < INTERIOR; ++row) {
                const double spot = SPOT * std::exp(LOG_LOW + DX * (row + 1));
                previous[row] = std::max(spot - STRIKE, 0.0);
            }
            Sparse::TriDiagonal_ matrix(INTERIOR);
            for (int step = 0; step < TIME_STEPS; ++step) {
                const double time = 1.0 - (step + 0.5) * DT;
                const double tau = (step + 1) * DT;
                const double right = SPOT * std::exp(0.8 - DIVIDEND * tau) - STRIKE * std::exp(-RATE * tau);
                for (int row = 0; row < INTERIOR; ++row) {
                    const double x = LOG_LOW + DX * (row + 1);
                    const double vol = Interp2DLinearImplX(logSpots, surface.times_, surface.vols_, x, time);
                    const double variance = vol * vol;
                    const double drift = RATE - DIVIDEND - 0.5 * variance;
                    matrix.Set(row, row, 1.0 + DT * (RATE + variance / (DX * DX)));
                    if (row > 0)
                        matrix.Set(row, row - 1, -0.5 * DT * (variance / (DX * DX) - drift / DX));
                    const double upper = -0.5 * DT * (variance / (DX * DX) + drift / DX);
                    if (row + 1 < INTERIOR)
                        matrix.Set(row, row + 1, upper);
                    else
                        previous[row] -= upper * right;
                }
                matrix.Decompose()->SolveLeft(previous, &next);
                std::swap(previous, next);
            }
            return previous[INTERIOR / 2];
        }

        Matrix_<> QuoteMatrix(const Vector_<>& quotes) {
            Matrix_<> result(QUOTE_STRIKES.size(), QUOTE_TIMES.size());
            std::copy(quotes.begin(), quotes.end(), result.Data());
            return result;
        }

        struct ScenarioResult_ {
            double price_, passivePrice_, surfaceError_, priceError_;
            Margins_ margins_;
        };

        ScenarioResult_ CheckScenario(
            const Kernel_& kernel, const DupireCalibrationSnapshot_& base, const Vector_<>& quotes, double referencePrice, bool baseScenario) {
            Margins_ margins;
            const auto production = RecalibrateDupireWithRisk(base, QuoteMatrix(quotes));
            const auto& surface = *production.Surface();
            const auto local = kernel.Calibrate(quotes, &margins);
            double surfaceError = 0.0;
            for (size_t t = 0; t < LOCAL_TIMES.size(); ++t)
                for (size_t j = 0; j < LOCAL_STRIKES.size(); ++j) {
                    const double value = local[t][j], expected = surface.vols_(static_cast<int>(j), static_cast<int>(t));
                    surfaceError = std::max(surfaceError, std::abs(value - expected));
                    if (baseScenario) {
                        const double precise = Reference::BASE_VOLS[t][j];
                        REQUIRE(std::abs(precise - value) <= 5e-7 && std::abs(precise - expected) <= 5e-7,
                                "Dupire benchmark base local vol differs from its high-precision reference");
                        std::cout << "dupire_surface," << LOCAL_STRIKES[j] << ',' << LOCAL_TIMES[t] << ',' << value << ',' << expected << ','
                                  << precise << '\n';
                    }
                }
            REQUIRE(surfaceError <= 5e-7, "Dupire benchmark surface disagrees with passive production recalibration");
            const double price = kernel.Rollback(local, &margins);
            const double passive = PassivePrice(surface);
            const double error = std::max(std::abs(price - referencePrice), std::abs(passive - referencePrice));
            REQUIRE(std::isfinite(price) && std::isfinite(passive) && error <= 2e-6 && std::abs(price - passive) <= 2e-6,
                    "Dupire benchmark repricing disagrees with the passive or high-precision reference");
            return {price, passive, surfaceError, error, margins};
        }

        void PrintScenario(int direction, double signedStep, double reference, const ScenarioResult_& result) {
            const auto& m = result.margins_;
            std::cout << "dupire_scenario," << direction << ',' << signedStep << ',' << reference << ',' << result.price_ << ','
                      << result.passivePrice_ << ',' << result.surfaceError_ << ',' << result.priceError_ << ',' << m.iv_ << ',' << m.ckk_ << ','
                      << m.numerator_ << ',' << m.variance_ << ',' << m.curvatureUlps_ << ',' << m.pivot_ << '\n';
        }
    } // namespace

    Vector_<> QuoteSpreads() { return Vector_<>(QUOTE_SPREADS.begin(), QUOTE_SPREADS.end()); }

    Matrix_<> Directions() {
        Matrix_<> result(3, 6, 0.0);
        result(0, 3) = result(1, 2) = 1.0;
        const std::array<double, 6> mixed{0.5, -0.25, -0.5, 1.0, 0.25, -0.5};
        for (int column = 0; column < result.Cols(); ++column)
            result(2, column) = mixed[column];
        return result;
    }

    DupireCalibrationSnapshot_ Snapshot(const Vector_<>& quotes) {
        const FlatImpliedVol_ flat;
        const Vector_<> strikes(LOCAL_STRIKES.begin(), LOCAL_STRIKES.end()), times(LOCAL_TIMES.begin(), LOCAL_TIMES.end());
        const DupireRiskInputs_ inputs{Vector_<>(QUOTE_STRIKES.begin(), QUOTE_STRIKES.end()),
                                       Vector_<>(QUOTE_TIMES.begin(), QUOTE_TIMES.end()),
                                       QuoteMatrix(quotes),
                                       strikes,
                                       10.0,
                                       times,
                                       0.5};
        return CalibrateDupireWithRisk(flat, inputs, "second-order benchmark");
    }

    void Qualify(const Kernel_& kernel, const Vector_<>& quotes, const Matrix_<>& directions) {
        const FlatImpliedVol_ flat;
        const Vector_<> strikes(LOCAL_STRIKES.begin(), LOCAL_STRIKES.end()), times(LOCAL_TIMES.begin(), LOCAL_TIMES.end());
        const auto base = Snapshot(quotes);
        REQUIRE(base.Surface()->spots_ == strikes && base.Surface()->times_ == times, "Dupire benchmark production axes changed");
        for (size_t t = 0; t < times.size(); ++t) {
            const double halfWidth = 2.5 * flat.Call(SPOT, times[t]) * M_SQRT_2_PI;
            const auto begin = std::lower_bound(strikes.begin(), strikes.end(), SPOT - halfWidth);
            const auto end = std::upper_bound(strikes.begin(), strikes.end(), SPOT + halfWidth);
            REQUIRE(begin - strikes.begin() == FIRST[t] && end - strikes.begin() == LAST[t] + 1, "Dupire benchmark production band changed");
            std::cout << "dupire_band," << times[t] << ',' << *begin << ',' << *(end - 1) << ',' << halfWidth << '\n';
        }
        REQUIRE(Reference::REFINEMENT <= 5e-3, "Dupire benchmark high-precision HVP refinement exceeded tolerance");
        std::cout
            << "# dupire_surface,strike,time,kernel_vol,production_vol,high_precision_vol\n"
            << "# dupire_scenario,direction,signed_step,high_precision_price,kernel_price,production_price,max_surface_abs_error,max_price_abs_error,"
               "min_iv,min_ckk,min_numerator,min_local_variance,min_curvature_resolution,min_thomas_pivot\n";
        const auto baseResult = CheckScenario(kernel, base, quotes, Reference::PRICE, true);
        PrintScenario(-1, 0.0, Reference::PRICE, baseResult);
        std::array<double, Reference::SCENARIOS.size()> prices;
        for (size_t i = 0; i < prices.size(); ++i) {
            const auto& scenario = Reference::SCENARIOS[i];
            Vector_<> bumped = quotes;
            for (int column = 0; column < directions.Cols(); ++column)
                bumped[column] = std::fma(scenario.signedStep_, directions(scenario.direction_, column), quotes[column]);
            const auto result = CheckScenario(kernel, base, bumped, scenario.price_, false);
            prices[i] = result.price_;
            PrintScenario(scenario.direction_, scenario.signedStep_, scenario.price_, result);
        }
        std::cout << "# dupire_price_volga_step,step,double_price_volga,abs_error_against_reference\n";
        for (size_t i = 0; i + 1 < prices.size(); ++i) {
            const auto& down = Reference::SCENARIOS[i];
            if (down.direction_ == 0 && down.signedStep_ < 0.0) {
                const auto& up = Reference::SCENARIOS[i + 1];
                REQUIRE(up.direction_ == 0 && up.signedStep_ == -down.signedStep_, "Dupire benchmark reference scenario order changed");
                const double h = up.signedStep_;
                const double volga = (prices[i] + prices[i + 1] - 2.0 * baseResult.price_) / (h * h);
                std::cout << "dupire_price_volga_step," << h << ',' << volga << ',' << std::abs(volga - Reference::PRODUCTS[0][3]) << '\n';
            }
        }
        std::cout << "dupire_qualification," << 1 + prices.size() << ',' << kernel.StrikeMargin() << ',' << kernel.TimeMargin() << ','
                  << Reference::REFINEMENT << '\n';
    }
} // namespace TapePerfDupire
