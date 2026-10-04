//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>

namespace {
    class FlatIVS_ final : public Dal::AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    struct SingleWorker_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        SingleWorker_() {
            Dal::RegisterAll_::Init();
            pool_->Start(1, true);
        }
        ~SingleWorker_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    Dal::DupireRiskInputs_ Inputs() {
        return {{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
    }

    Dal::Handle_<Dal::ModelData_> Model(const Dal::Handle_<Dal::LocalVolSurfaceData_>& surface, double rate = 0.05, double dividend = 0.02) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::NewHybridLocalVolEquityData("Z_LOCAL", "EQ[LOCAL]", "USD", "F_LOCAL", 100.0, dividend, surface, 0.25),
                                Dal::NewHybridDeterministicRateData("00_RATE", "USD", rate),
                                Dal::NewHybridBSEquityData("A_OTHER", "EQ[OTHER]", "USD", "F_OTHER", 120.0, 0.25, 0.01)};
        Dal::Matrix_<> correlation(2, 2, 0.0);
        correlation(0, 0) = correlation(1, 1) = 1.0;
        settings.correlation_ = Dal::NewHybridConstantCorrelationData("correlation", {"F_LOCAL", "F_OTHER"}, correlation);
        return Dal::NewHybridModelData("unsorted", settings);
    }

    Dal::ScriptValuationSettings_ Valuation() {
        Dal::ScriptValuationSettings_ settings;
        settings.evaluationDate_ = Dal::Date_(2026, 9, 12);
        return settings;
    }

    auto Product(double quote = 0.0) {
        std::ostringstream value;
        value << std::setprecision(17) << quote;
        return Dal::NewScriptProduct(
            "smooth", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
            {Dal::String_(value.str()), "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 0.1 * FIX(EQ[OTHER]) + 3 * QUOTE"});
    }

    struct OracleProtocol_ {
        const char* name_;
        std::array<double, 3> steps_;
        double absolute_;
        double relative_;
    };

    constexpr OracleProtocol_ NODE_PROTOCOL{"HybridNodeOracle", {2e-5, 1e-5, 5e-6}, 1e-4, 1e-4};
    constexpr OracleProtocol_ QUOTE_PROTOCOL{"HybridQuoteOracle", {2e-4, 1e-4, 5e-5}, 1e-3, 1e-3};
    constexpr int PATHS = 257;
    constexpr int SURFACE_START = 6;

    auto PortfolioProduct() {
        return Dal::NewScriptProduct("other", {Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"other PAYS -0.3 * FIX(EQ[LOCAL]) + 0.5 * FIX(EQ[OTHER])"});
    }

    Dal::Script::RiskResult_ Reproject(const Dal::Script::RiskResult_& source,
                                       const Dal::Vector_<Dal::Script::RiskCoordinate_>& axis,
                                       Dal::Script::RiskResultProvenance_ provenance,
                                       const Dal::Script::RiskRequest_& request = {}) {
        REQUIRE(source.Jacobian().Cols() == static_cast<int>(axis.size()), "test reprojection requires the complete raw source axis");
        Dal::Vector_<Dal::String_> labels;
        for (const auto& coordinate : axis)
            labels.push_back(coordinate.label_);
        Dal::Script::SimResults_ raw(labels);
        raw.aggregated_ = source.Values()[0];
        for (int column = 0; column < source.Jacobian().Cols(); ++column)
            raw.risks_[static_cast<size_t>(column)] = source.Jacobian()(0, column);
        if (provenance.execution_)
            provenance.execution_->pathsPerReplicate_ = 1;
        return Dal::Script::ProjectMonteCarloRiskResult(raw, 1, axis, request, provenance);
    }

    template <class F_> void AssertError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected error for " << field;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }

    auto ValuationRisk(const Dal::DupireCalibrationSnapshot_& calibration) {
        return Dal::ValueByMonteCarloWithRisk(Product(), Model(calibration.Surface(), calibration.Rate(), calibration.DividendYield()), PATHS, {},
                                              Valuation());
    }

    bool
    AdjacentStepsAgree(const OracleProtocol_& protocol, double adjoint, const std::array<double, 3>& differences, const std::string& coordinate) {
        bool previous = false;
        bool adjacent = false;
        for (size_t index = 0; index < differences.size(); ++index) {
            const double error = std::abs(adjoint - differences[index]);
            const double tolerance = protocol.absolute_ + protocol.relative_ * std::abs(differences[index]);
            const bool pass = std::isfinite(differences[index]) && error <= tolerance;
            std::cout << std::setprecision(17) << protocol.name_ << " coordinate=" << coordinate << " step=" << protocol.steps_[index]
                      << " aad=" << adjoint << " fd=" << differences[index] << " error=" << error << " tolerance=" << tolerance << " pass=" << pass
                      << '\n';
            adjacent = adjacent || (previous && pass);
            previous = pass;
        }
        return adjacent;
    }

    double LegacyValue(const Dal::Handle_<Dal::LocalVolSurfaceData_>& surface,
                       double rate,
                       double dividend,
                       double quote,
                       const Dal::MonteCarloSettings_& simulation) {
        return Dal::ValueByMonteCarlo(Product(quote), Model(surface, rate, dividend), PATHS, Valuation(), simulation).at("PV");
    }

    Dal::Handle_<Dal::LocalVolSurfaceData_> LegacySurface(const Dal::AAD::IVS_& ivs, const Dal::DupireRiskInputs_& inputs) {
        Dal::AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column)
                quotes.Bump(row, column, inputs.quoteSpreads_(row, column));
        const auto result =
            Dal::AAD::DupireCalib(ivs, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
        return Dal::NewLocalVolSurfaceData("legacy", result.spots_, result.times_, result.lVols_);
    }

    std::array<double, 3> QuoteDifferences(const Dal::AAD::IVS_& ivs,
                                           const Dal::DupireRiskInputs_& inputs,
                                           const Dal::Matrix_<>& direction,
                                           const Dal::MonteCarloSettings_& simulation) {
        std::array<double, 3> differences;
        for (size_t step = 0; step < QUOTE_PROTOCOL.steps_.size(); ++step) {
            auto plus = inputs;
            auto minus = inputs;
            for (int row = 0; row < direction.Rows(); ++row)
                for (int column = 0; column < direction.Cols(); ++column) {
                    plus.quoteSpreads_(row, column) += QUOTE_PROTOCOL.steps_[step] * direction(row, column);
                    minus.quoteSpreads_(row, column) -= QUOTE_PROTOCOL.steps_[step] * direction(row, column);
                }
            const double up = LegacyValue(LegacySurface(ivs, plus), ivs.Rate(), ivs.DividendYield(), plus.quoteSpreads_(0, 0), simulation);
            const double down = LegacyValue(LegacySurface(ivs, minus), ivs.Rate(), ivs.DividendYield(), minus.quoteSpreads_(0, 0), simulation);
            differences[step] = (up - down) / (2.0 * QUOTE_PROTOCOL.steps_[step]);
        }
        return differences;
    }

    double LegacyPortfolioValue(const Dal::AAD::IVS_& ivs, const Dal::DupireRiskInputs_& inputs, const Dal::MonteCarloSettings_& simulation) {
        const auto surface = LegacySurface(ivs, inputs);
        return LegacyValue(surface, ivs.Rate(), ivs.DividendYield(), 0.0, simulation) +
               Dal::ValueByMonteCarlo(PortfolioProduct(), Model(surface, ivs.Rate(), ivs.DividendYield()), PATHS, Valuation(), simulation).at("PV");
    }

    std::array<double, 3> PortfolioDifferences(const Dal::AAD::IVS_& ivs,
                                               const Dal::DupireRiskInputs_& inputs,
                                               const Dal::Matrix_<>& direction,
                                               const Dal::MonteCarloSettings_& simulation) {
        std::array<double, 3> differences;
        for (size_t step = 0; step < QUOTE_PROTOCOL.steps_.size(); ++step) {
            auto plus = inputs;
            auto minus = inputs;
            for (int row = 0; row < direction.Rows(); ++row)
                for (int column = 0; column < direction.Cols(); ++column) {
                    plus.quoteSpreads_(row, column) += QUOTE_PROTOCOL.steps_[step] * direction(row, column);
                    minus.quoteSpreads_(row, column) -= QUOTE_PROTOCOL.steps_[step] * direction(row, column);
                }
            differences[step] =
                (LegacyPortfolioValue(ivs, plus, simulation) - LegacyPortfolioValue(ivs, minus, simulation)) / (2.0 * QUOTE_PROTOCOL.steps_[step]);
        }
        return differences;
    }

    void CheckPortfolioBuckets(const Dal::AAD::IVS_& ivs, const Dal::DupireRiskInputs_& inputs, const Dal::DupireQuoteRisk_& risk) {
        const auto simulation = Dal::DefaultRiskMonteCarloSettings();
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                Dal::Matrix_<> direction(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols(), 0.0);
                direction(row, column) = 1.0;
                const auto differences = PortfolioDifferences(ivs, inputs, direction, simulation);
                ASSERT_TRUE(AdjacentStepsAgree(QUOTE_PROTOCOL, risk.TotalAdjoints()(row, column), differences,
                                               "portfolio:" + std::to_string(row) + ":" + std::to_string(column)));
            }
    }

    void CheckPortfolioDirection(const Dal::AAD::IVS_& ivs, const Dal::DupireRiskInputs_& inputs, const Dal::DupireQuoteRisk_& risk) {
        Dal::Matrix_<> direction(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols());
        double adjoint = 0.0;
        for (int row = 0; row < direction.Rows(); ++row)
            for (int column = 0; column < direction.Cols(); ++column) {
                direction(row, column) = std::cos(0.7 * row + 0.4 * column);
                adjoint += risk.TotalAdjoints()(row, column) * direction(row, column);
            }
        const auto differences = PortfolioDifferences(ivs, inputs, direction, Dal::DefaultRiskMonteCarloSettings());
        ASSERT_TRUE(AdjacentStepsAgree(QUOTE_PROTOCOL, adjoint, differences, "portfolio:direction"));
    }

    std::array<double, 3>
    NodeDifferences(const Dal::DupireCalibrationSnapshot_& calibration, int row, int column, const Dal::MonteCarloSettings_& simulation) {
        const auto& surface = *calibration.Surface();
        std::array<double, 3> differences;
        for (size_t step = 0; step < NODE_PROTOCOL.steps_.size(); ++step) {
            auto up = surface.vols_;
            auto down = surface.vols_;
            up(row, column) += NODE_PROTOCOL.steps_[step];
            down(row, column) -= NODE_PROTOCOL.steps_[step];
            const auto plus = Dal::NewLocalVolSurfaceData("up", surface.spots_, surface.times_, up);
            const auto minus = Dal::NewLocalVolSurfaceData("down", surface.spots_, surface.times_, down);
            const double quote = calibration.Inputs().quoteSpreads_(0, 0);
            differences[step] = (LegacyValue(plus, calibration.Rate(), calibration.DividendYield(), quote, simulation) -
                                 LegacyValue(minus, calibration.Rate(), calibration.DividendYield(), quote, simulation)) /
                                (2.0 * NODE_PROTOCOL.steps_[step]);
        }
        return differences;
    }

    void CheckSurfaceNodes(const Dal::DupireCalibrationSnapshot_& calibration,
                           const Dal::Script::RiskResult_& valuation,
                           const Dal::MonteCarloSettings_& simulation,
                           const std::string& label) {
        const auto& surface = *calibration.Surface();
        for (int row = 0; row < surface.vols_.Rows(); ++row)
            for (int column = 0; column < surface.vols_.Cols(); ++column) {
                const auto differences = NodeDifferences(calibration, row, column, simulation);
                const int ordinal = SURFACE_START + row * surface.vols_.Cols() + column;
                ASSERT_TRUE(AdjacentStepsAgree(NODE_PROTOCOL, valuation.Jacobian()(0, ordinal), differences,
                                               label + ":" + std::to_string(row) + ":" + std::to_string(column)));
            }
    }

    void CheckQuoteBuckets(const Dal::AAD::IVS_& ivs,
                           const Dal::DupireRiskInputs_& inputs,
                           const Dal::DupireQuoteRisk_& risk,
                           const Dal::MonteCarloSettings_& simulation,
                           const std::string& label) {
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                Dal::Matrix_<> direction(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols(), 0.0);
                direction(row, column) = 1.0;
                const auto differences = QuoteDifferences(ivs, inputs, direction, simulation);
                ASSERT_TRUE(AdjacentStepsAgree(QUOTE_PROTOCOL, risk.TotalAdjoints()(row, column), differences,
                                               label + ":" + std::to_string(row) + ":" + std::to_string(column)));
            }
    }

    void CheckQuoteDirection(const Dal::AAD::IVS_& ivs,
                             const Dal::DupireRiskInputs_& inputs,
                             const Dal::DupireQuoteRisk_& risk,
                             const Dal::MonteCarloSettings_& simulation,
                             const std::string& label) {
        Dal::Matrix_<> direction(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols());
        double adjoint = 0.0;
        for (int row = 0; row < direction.Rows(); ++row)
            for (int column = 0; column < direction.Cols(); ++column) {
                direction(row, column) = std::cos(0.7 * row + 0.4 * column);
                adjoint += risk.TotalAdjoints()(row, column) * direction(row, column);
            }
        const auto differences = QuoteDifferences(ivs, inputs, direction, simulation);
        ASSERT_TRUE(AdjacentStepsAgree(QUOTE_PROTOCOL, adjoint, differences, label + ":direction"));
    }

    void CheckCompleteChain(const Dal::AAD::IVS_& ivs, const std::string& base) {
        auto inputs = Inputs();
        for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
            for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column)
                inputs.quoteSpreads_(row, column) = 0.001 + 0.00003 * row + 0.00004 * column;
        const auto calibration = Dal::CalibrateDupireWithRisk(ivs, inputs);
        Dal::Matrix_<> direct(inputs.quoteSpreads_.Rows(), inputs.quoteSpreads_.Cols(), 0.0);
        const double directPvRisk = 3.0 * std::exp(-ivs.Rate());
        direct(0, 0) = directPvRisk;
        for (const bool compiled : {false, true}) {
            auto simulation = Dal::DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            const auto valuation = Dal::ValueByMonteCarloWithRisk(Product(inputs.quoteSpreads_(0, 0)),
                                                                  Model(calibration.Surface(), calibration.Rate(), calibration.DividendYield()),
                                                                  PATHS, {}, Valuation(), simulation);
            const auto risk = Dal::PullbackDupireScriptRisk(valuation, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{calibration, direct});
            ASSERT_NEAR(valuation.Jacobian()(0, valuation.Jacobian().Cols() - 1), directPvRisk, 1e-10);
            ASSERT_EQ(risk.Valuation().Values()[0],
                      LegacyValue(LegacySurface(ivs, inputs), ivs.Rate(), ivs.DividendYield(), inputs.quoteSpreads_(0, 0), simulation));
            ASSERT_EQ(risk.QuoteRisk().DirectAdjoints()(0, 0), directPvRisk);
            const std::string label = base + ":compiled=" + std::to_string(compiled);
            ASSERT_NO_FATAL_FAILURE(CheckSurfaceNodes(calibration, valuation, simulation, label));
            ASSERT_NO_FATAL_FAILURE(CheckQuoteBuckets(ivs, inputs, risk.QuoteRisk(), simulation, label));
            ASSERT_NO_FATAL_FAILURE(CheckQuoteDirection(ivs, inputs, risk.QuoteRisk(), simulation, label));
        }
    }

    void PrintPortfolioSeeds(const Dal::DupireParameterAdjoints_& first, const Dal::DupireParameterAdjoints_& second) {
        for (int row = 0; row < first.adjoints_.Rows(); ++row)
            for (int column = 0; column < first.adjoints_.Cols(); ++column)
                std::cout << std::setprecision(17) << "HybridPortfolioSeed row=" << row << " column=" << column
                          << " first=" << first.adjoints_(row, column) << " second=" << second.adjoints_(row, column) << '\n';
    }

    void PrintPortfolioRisk(const Dal::DupireQuoteRisk_& combined, const Dal::DupireQuoteRisk_& first, const Dal::DupireQuoteRisk_& second) {
        for (int row = 0; row < combined.TotalAdjoints().Rows(); ++row)
            for (int column = 0; column < combined.TotalAdjoints().Cols(); ++column)
                std::cout << std::setprecision(17) << "HybridPortfolioRoundoff row=" << row << " column=" << column
                          << " combined=" << combined.TotalAdjoints()(row, column)
                          << " separate=" << first.TotalAdjoints()(row, column) + second.TotalAdjoints()(row, column) << " error="
                          << combined.TotalAdjoints()(row, column) - first.TotalAdjoints()(row, column) - second.TotalAdjoints()(row, column) << '\n';
    }
} // namespace

TEST(DupireScriptRiskTest, TestUnsortedComponentsAndReorderedSelectionsUseRawModelOrdinals) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto model = Model(calibration.Surface());
    const auto& surface = *calibration.Surface();
    constexpr size_t SURFACE_START = 6;
    const size_t nodes = static_cast<size_t>(surface.vols_.Rows() * surface.vols_.Cols());
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{"model:2", "model:0"};
    for (size_t node = nodes; node > 0; --node)
        request.inputs_->push_back("model:" + Dal::String_(std::to_string(SURFACE_START + node - 1)));
    request.reportFactors_ = Dal::Vector_<>(nodes + 2, 0.01);
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.compiled_ = true;
    const auto valuation = Dal::ValueByMonteCarloWithRisk(Product(), model, 257, request, Valuation(), simulation);
    const auto extracted = Dal::ExtractDupireParameterAdjoints(valuation, calibration, "Z_LOCAL");
    ASSERT_TRUE(extracted.calibration_.Matches(calibration));
    ASSERT_EQ(extracted.adjoints_.Rows(), surface.vols_.Rows());
    ASSERT_EQ(extracted.adjoints_.Cols(), surface.vols_.Cols());
    for (int row = 0; row < extracted.adjoints_.Rows(); ++row)
        for (int column = 0; column < extracted.adjoints_.Cols(); ++column) {
            const size_t node = static_cast<size_t>(row * extracted.adjoints_.Cols() + column);
            ASSERT_EQ(extracted.adjoints_(row, column), valuation.Jacobian()(0, static_cast<int>(nodes - node + 1)));
        }
    const auto reference = Dal::PullbackDupireCalibration(calibration, extracted);
    const auto complete = Dal::PullbackDupireScriptRisk(valuation, calibration, "Z_LOCAL");
    ASSERT_EQ(complete.Component(), "Z_LOCAL");
    ASSERT_EQ(complete.Method(), "NativeAADThenNativeAADCalibrationVJP");
    ASSERT_EQ(complete.Valuation().Values()[0], valuation.Values()[0]);
    ASSERT_EQ(complete.Valuation().Provenance().execution_->modelSnapshotJson_, valuation.Provenance().execution_->modelSnapshotJson_);
    ASSERT_TRUE(std::equal(reference.TotalAdjoints().begin(), reference.TotalAdjoints().end(), complete.QuoteRisk().TotalAdjoints().begin()));
}

TEST(DupireScriptRiskTest, TestFlatCompleteQuoteChainMatchesIndependentNodeAndRecalibrationBumps) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    ASSERT_NO_FATAL_FAILURE(CheckCompleteChain(ivs, "flat"));
}

TEST(DupireScriptRiskTest, TestMertonCompleteQuoteChainMatchesIndependentNodeAndRecalibrationBumps) {
    const SingleWorker_ worker;
    const Dal::AAD::MertonIVS_ ivs(100.0, 0.2, 0.08, -0.1, 0.15);
    ASSERT_NO_FATAL_FAILURE(CheckCompleteChain(ivs, "merton"));
}

TEST(DupireScriptRiskTest, TestMalformedModelSnapshotHasFieldContextAndRecovers) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = Dal::ValueByMonteCarloWithRisk(Product(), Model(calibration.Surface()), PATHS, {}, Valuation());
    const auto before = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    auto provenance = source.Provenance();
    provenance.execution_->modelSnapshotJson_ = "{";
    const auto invalid = Reproject(source, source.CompleteInputAxis(), provenance);
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(invalid, calibration, "Z_LOCAL"); }, "modelSnapshotJson"));
    const auto after = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    ASSERT_TRUE(
        std::equal(before.QuoteRisk().TotalAdjoints().begin(), before.QuoteRisk().TotalAdjoints().end(), after.QuoteRisk().TotalAdjoints().begin()));
}

TEST(DupireScriptRiskTest, TestMissingSelectedSurfaceRiskRejectsEvenWhenItsValueIsZero) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto product = Dal::NewScriptProduct("constant", {Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"pay PAYS 1"});
    const auto source = Dal::ValueByMonteCarloWithRisk(product, Model(calibration.Surface()), PATHS, {}, Valuation());
    ASSERT_EQ(source.Jacobian()(0, SURFACE_START), 0.0);
    const auto zero = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    ASSERT_TRUE(
        std::all_of(zero.QuoteRisk().TotalAdjoints().begin(), zero.QuoteRisk().TotalAdjoints().end(), [](double value) { return value == 0.0; }));
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{};
    for (const auto& coordinate : source.CompleteInputAxis())
        if (coordinate.id_ != "model:6")
            request.inputs_->push_back(coordinate.id_);
    const auto missing = Reproject(source, source.CompleteInputAxis(), source.Provenance(), request);
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(missing, calibration, "Z_LOCAL"); }, "model:6"));
}

TEST(DupireScriptRiskTest, TestUnknownOrNonLocalVolComponentRejectsByTypedIdentity) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    for (const auto& component : {"missing", "A_OTHER", "00_RATE"})
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(source, calibration, component); }, component));
}

TEST(DupireScriptRiskTest, TestEqualSizeChangedSurfaceGridsOrValuesReject) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto& surface = *calibration.Surface();
    auto spots = surface.spots_;
    spots[0] += 0.1;
    auto values = surface.vols_;
    values(0, 0) += 0.001;
    const auto changedGrids = Dal::NewLocalVolSurfaceData("grid", spots, surface.times_, surface.vols_);
    const auto changedValues = Dal::NewLocalVolSurfaceData("values", surface.spots_, surface.times_, values);
    for (const auto& changed : {changedGrids, changedValues}) {
        const auto source = Dal::ValueByMonteCarloWithRisk(Product(), Model(changed), PATHS, {}, Valuation());
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL"); }, "component=Z_LOCAL"));
    }
}

TEST(DupireScriptRiskTest, TestCompleteModelAxisChecksUnselectedUnrelatedCoordinates) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{};
    for (size_t ordinal = SURFACE_START; ordinal + 1 < source.CompleteInputAxis().size(); ++ordinal)
        request.inputs_->push_back("model:" + Dal::String_(std::to_string(ordinal)));
    for (const size_t ordinal : {0U, 1U, 5U}) {
        auto axis = source.CompleteInputAxis();
        axis[ordinal].value_ += 0.001;
        const auto changed = Reproject(source, axis, source.Provenance(), request);
        const std::string field = "model:" + std::to_string(ordinal);
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changed, calibration, "Z_LOCAL"); }, field.c_str()));
    }
}

TEST(DupireScriptRiskTest, TestModelCoordinateLabelsAndUnitsCannotOverrideOrdinals) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    auto changedLabel = source.CompleteInputAxis();
    changedLabel[SURFACE_START].label_ += "_changed";
    auto changedUnit = source.CompleteInputAxis();
    changedUnit[SURFACE_START].nativeUnit_ = "decimal-vol";
    auto changedPhysicalUnit = source.CompleteInputAxis();
    changedPhysicalUnit[SURFACE_START].physicalUnit_ = "decimal-vol";
    for (const auto& axis : {changedLabel, changedUnit, changedPhysicalUnit}) {
        const auto changed = Reproject(source, axis, source.Provenance());
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changed, calibration, "Z_LOCAL"); }, "model:6"));
    }
}

TEST(DupireScriptRiskTest, TestRetainedModelCannotChangeUnrelatedCarryOrModelType) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    auto provenance = source.Provenance();
    provenance.execution_->modelSnapshotJson_ = Dal::JSON::WriteString(*Model(calibration.Surface(), 0.06));
    const auto changedCarry = Reproject(source, source.CompleteInputAxis(), provenance);
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changedCarry, calibration, "Z_LOCAL"); }, "model:0"));
    provenance.execution_->modelSnapshotJson_ = Dal::JSON::WriteString(*Dal::NewBSModelData("wrong", 100.0, 0.2, 0.05, 0.02));
    const auto changedType = Reproject(source, source.CompleteInputAxis(), provenance);
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changedType, calibration, "Z_LOCAL"); }, "HybridModelData"));
}

TEST(DupireScriptRiskTest, TestMissingExecutionOrModelSnapshotRejectsBeforeRecording) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    auto missingExecution = source.Provenance();
    missingExecution.execution_.reset();
    auto missingModel = source.Provenance();
    missingModel.execution_->modelSnapshotJson_.clear();
    for (const auto& provenance : {missingExecution, missingModel}) {
        const auto changed = Reproject(source, source.CompleteInputAxis(), provenance);
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changed, calibration, "Z_LOCAL"); }, "modelSnapshotJson"));
    }
    ASSERT_NO_THROW(Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL"));
}

TEST(DupireScriptRiskTest, TestUnsupportedMethodAndPriceOnlyExecutionReject) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    auto provenance = source.Provenance();
    for (const auto& method : {"FiniteDifference", "Expired"}) {
        provenance.method_ = method;
        const auto changed = Reproject(source, source.CompleteInputAxis(), provenance);
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { Dal::PullbackDupireScriptRisk(changed, calibration, "Z_LOCAL"); }, "method="));
    }
    auto simulation = Dal::DefaultRiskMonteCarloSettings();
    simulation.enableAad_ = false;
    const auto passive = Dal::ValueByMonteCarloWithRisk(Product(), Model(calibration.Surface()), PATHS, {}, Valuation(), simulation);
    ASSERT_THROW(Dal::PullbackDupireScriptRisk(passive, calibration, "Z_LOCAL"), Dal::Exception_);
}

TEST(DupireScriptRiskTest, TestExpiredExecutionReturnsZeroAndMixedMethodRemainsExplicit) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto product = Dal::NewScriptProduct("expired", {Dal::Cell_(Dal::Date_(2026, 9, 11))}, {"pay PAYS 1"});
    const auto source = Dal::ValueByMonteCarloWithRisk(product, Model(calibration.Surface()), PATHS, {}, Valuation());
    const auto zero = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    ASSERT_EQ(zero.Method(), "ExpiredThenNativeAADCalibrationVJP");
    ASSERT_TRUE(
        std::all_of(zero.QuoteRisk().TotalAdjoints().begin(), zero.QuoteRisk().TotalAdjoints().end(), [](double value) { return value == 0.0; }));
    const auto live = ValuationRisk(calibration);
    auto provenance = live.Provenance();
    provenance.method_ = "NativeAADWithRetrainedPolicySecant";
    const auto mixed = Reproject(live, live.CompleteInputAxis(), provenance);
    const auto risk = Dal::PullbackDupireScriptRisk(mixed, calibration, "Z_LOCAL");
    ASSERT_EQ(risk.Method(), "NativeAADWithRetrainedPolicySecantThenNativeAADCalibrationVJP");
    ASSERT_EQ(risk.Valuation().Provenance().method_, provenance.method_);
}

TEST(DupireScriptRiskTest, TestDirectQuoteIdentityRejectsAndPriorPassiveResultSurvives) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    Dal::Matrix_<> direct(3, 2, -0.25);
    const auto before = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{calibration, direct});
    auto changed = Inputs();
    changed.quoteSpreads_.Fill(0.001);
    const auto other = Dal::CalibrateDupireWithRisk(ivs, changed);
    ASSERT_THROW(Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{other, direct}), Dal::Exception_);
    const auto after = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{calibration, direct});
    ASSERT_TRUE(
        std::equal(before.QuoteRisk().TotalAdjoints().begin(), before.QuoteRisk().TotalAdjoints().end(), after.QuoteRisk().TotalAdjoints().begin()));
    ASSERT_EQ(before.QuoteRisk().DirectAdjoints()(0, 0), -0.25);
}

TEST(DupireScriptRiskTest, TestCallerModelMutationAndDestructionDoNotChangeRetainedPullback) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto original = Dal::handle_cast<Dal::HybridModelData_>(Model(calibration.Surface()));
    auto caller = std::make_shared<Dal::HybridModelData_>("caller", original->domesticCurrency_, original->components_, original->correlation_);
    auto model = Dal::Handle_<Dal::ModelData_>(caller);
    const std::weak_ptr<const Dal::ModelData_> lifetime = model;
    const auto source = Dal::ValueByMonteCarloWithRisk(Product(), model, PATHS, {}, Valuation());
    const auto before = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    caller->components_.clear();
    caller->domesticCurrency_ = "changed";
    caller.reset();
    model = {};
    ASSERT_TRUE(lifetime.expired());
    const auto after = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    ASSERT_TRUE(
        std::equal(before.QuoteRisk().TotalAdjoints().begin(), before.QuoteRisk().TotalAdjoints().end(), after.QuoteRisk().TotalAdjoints().begin()));
    ASSERT_EQ(before.Valuation().Provenance().execution_->modelSnapshotJson_, after.Valuation().Provenance().execution_->modelSnapshotJson_);
}

TEST(DupireScriptRiskTest, TestNestedPullbackRejectsAndPreservesOuterGraphThenRecovers) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto source = ValuationRisk(calibration);
    const auto before = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    {
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Dal::AAD::Number_ output = input * input;
        recording.FinishRecording();
        ASSERT_THROW(Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL"), Dal::Exception_);
        Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
        recording.Reverse();
        ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
        recording.Close();
    }
    const auto after = Dal::PullbackDupireScriptRisk(source, calibration, "Z_LOCAL");
    ASSERT_TRUE(
        std::equal(before.QuoteRisk().TotalAdjoints().begin(), before.QuoteRisk().TotalAdjoints().end(), after.QuoteRisk().TotalAdjoints().begin()));
}

TEST(DupireScriptRiskTest, TestCompatibleSeedAggregationHasExactPowerTwoControl) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, Inputs());
    const auto model = Model(calibration.Surface());
    const auto first = ValuationRisk(calibration);
    const auto second = Dal::ValueByMonteCarloWithRisk(Product(), model, PATHS, {}, Valuation());
    const auto left = Dal::ExtractDupireParameterAdjoints(first, calibration, "Z_LOCAL");
    const auto right = Dal::ExtractDupireParameterAdjoints(second, calibration, "Z_LOCAL");
    ASSERT_TRUE(left.calibration_.Matches(right.calibration_));
    PrintPortfolioSeeds(left, right);
    auto sum = left;
    for (int row = 0; row < sum.adjoints_.Rows(); ++row)
        for (int column = 0; column < sum.adjoints_.Cols(); ++column) {
            ASSERT_EQ(left.adjoints_(row, column), right.adjoints_(row, column));
            sum.adjoints_(row, column) += right.adjoints_(row, column);
        }
    const auto combined = Dal::PullbackDupireCalibration(calibration, sum);
    const auto firstRisk = Dal::PullbackDupireCalibration(calibration, left);
    const auto secondRisk = Dal::PullbackDupireCalibration(calibration, right);
    PrintPortfolioRisk(combined, firstRisk, secondRisk);
    for (int row = 0; row < combined.TotalAdjoints().Rows(); ++row)
        for (int column = 0; column < combined.TotalAdjoints().Cols(); ++column)
            ASSERT_NEAR(combined.TotalAdjoints()(row, column), firstRisk.TotalAdjoints()(row, column) + secondRisk.TotalAdjoints()(row, column),
                        1e-10);
}

TEST(DupireScriptRiskTest, TestDistinctTradeSeedAggregationMatchesFullPortfolioRecalibration) {
    const SingleWorker_ worker;
    const FlatIVS_ ivs;
    const auto inputs = Inputs();
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, inputs);
    const auto first = ValuationRisk(calibration);
    const auto second = Dal::ValueByMonteCarloWithRisk(PortfolioProduct(), Model(calibration.Surface()), PATHS, {}, Valuation());
    const auto left = Dal::ExtractDupireParameterAdjoints(first, calibration, "Z_LOCAL");
    const auto right = Dal::ExtractDupireParameterAdjoints(second, calibration, "Z_LOCAL");
    ASSERT_TRUE(left.calibration_.Matches(right.calibration_));
    PrintPortfolioSeeds(left, right);
    auto sum = left;
    for (int row = 0; row < sum.adjoints_.Rows(); ++row)
        for (int column = 0; column < sum.adjoints_.Cols(); ++column)
            sum.adjoints_(row, column) += right.adjoints_(row, column);
    const auto combined = Dal::PullbackDupireCalibration(calibration, sum);
    PrintPortfolioRisk(combined, Dal::PullbackDupireCalibration(calibration, left), Dal::PullbackDupireCalibration(calibration, right));
    ASSERT_NO_FATAL_FAILURE(CheckPortfolioBuckets(ivs, inputs, combined));
    ASSERT_NO_FATAL_FAILURE(CheckPortfolioDirection(ivs, inputs, combined));
}
