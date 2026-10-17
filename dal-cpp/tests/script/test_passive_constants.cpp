#include <gtest/gtest.h>

#include <cstring>
#include <optional>

#include <dal/math/aad/statistics.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

using namespace Dal;
using namespace Dal::Script;
namespace SimDetail = Dal::Script::Detail;

namespace {
    Handle_<ModelData_> GSRData(size_t factors = 3) {
        const Date_ today(2024, 1, 1);
        Matrix_<> projection(1, 3, 0.0);
        projection(0, 1) = -0.04;
        projection(0, 2) = -0.4;
        const Handle_<GSRCurveData_> curve(
            new GSRCurveData_("curve", today, "USD", {today, today.AddDays(365), today.AddDays(3650)}, {0.0, -0.03, -0.3}, {"3M"}, projection));
        MultiFactorGSRVolSettings_ vol;
        vol.gKnotDates_ = {today, today.AddDays(120)};
        vol.hKnotDates_ = {today, today.AddDays(180)};
        vol.gValues_ = vol.hValues_ = Matrix_<>(static_cast<int>(factors), 2);
        vol.correlations_ = Matrix_<>(static_cast<int>(factors), static_cast<int>(factors), 0.3);
        for (size_t i = 0; i < factors; ++i) {
            vol.factorNames_.push_back("factor" + String::FromInt(static_cast<int>(i)));
            vol.gValues_(static_cast<int>(i), 0) = 0.02 / (i + 1);
            vol.gValues_(static_cast<int>(i), 1) = 0.025 / (i + 1);
            vol.hValues_(static_cast<int>(i), 0) = 1.0 - 0.3 * i;
            vol.hValues_(static_cast<int>(i), 1) = 0.8 - 0.2 * i;
            vol.correlations_(static_cast<int>(i), static_cast<int>(i)) = 1.0;
        }
        return Handle_<ModelData_>(
            new MultiFactorGSRModelData_("rates", curve, Handle_<MultiFactorGSRVolData_>(new MultiFactorGSRVolData_("vol", vol))));
    }

    ScriptProductData_ Product() {
        Vector_<Cell_> dates;
        Vector_<String_> events;
        for (int quarter = 0; quarter <= 16; ++quarter) {
            dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(quarter * 91)));
            events.push_back("total = total + (FIX(IR[USD,SWAP,5Y]) + FIX(IR[USD,LIBOR_3M_LCH]) + FIX(IR[USD,DF,2033-01-01])) / 17.0");
        }
        events.back() += " pay PAYS total ON 2029-01-01";
        return {"rates", dates, events};
    }

    PreparedScript_ Prepare(const Handle_<ModelData_>& data, bool passive, const ScriptProductData_& product = Product()) {
        MonteCarloSettings_ settings;
        settings.enableAad_ = true;
        settings.compiled_ = true;
        settings.usePassiveConstants_ = passive;
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2024, 1, 1);
        auto model = CreateModel<double>(data);
        return PrepareScript(product, model.get(), valuation, settings);
    }

    struct Workers_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t old_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        explicit Workers_(size_t count) { pool_->Start(count, true); }
        ~Workers_() {
            pool_->Start(old_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    struct GraphCollector_ : SimDetail::ScalarPayoffCollector_ {
        AAD::TapeStatistics_* statistics_;
        explicit GraphCollector_(AAD::TapeStatistics_* statistics) : statistics_(statistics) {}
        AAD::Number_ operator()(const Vector_<AAD::Number_>& values, size_t index, const AAD::Number_& zero) const {
            auto root = SimDetail::ScalarPayoffCollector_::operator()(values, index, zero);
            *statistics_ = AAD::MeasureTape(*AAD::Tape());
            return root;
        }
    };

    AAD::TapeStatistics_ Graph(const PreparedScript_& product, const Handle_<ModelData_>& data) {
        const auto model = CreateModel<double>(data);
        const auto names = Vector::Join(model->ParameterLabels(), product.ConstVarNames());
        SimDetail::AADBatchSettings_ settings{"sobol",
                                              false,
                                              0,
                                              DEFAULT_SMOOTH,
                                              8,
                                              model->Parameters().size(),
                                              product.ConstVarNames().size(),
                                              product.PayOffIdx(),
                                              "Default",
                                              product.Simulation().usePassiveConstants_};
        const std::optional<ScriptCompiled_> program(product.Compile(true));
        SimResults_ result(names);
        AAD::TapeStatistics_ statistics;
        SimDetail::WithAADWorker(product, data, settings, program,
                                 [&](const auto& evaluate) { evaluate(PathBatch_{0, 8}, &result, GraphCollector_(&statistics), nullptr); });
        return statistics;
    }
} // namespace

TEST(GSRPassiveConstantsTest, TestOptInPreservesEveryPvAndNamedRiskBitAcrossWorkers) {
    const auto data = GSRData();
    const auto base = Prepare(data, false);
    const auto sink = Prepare(data, true);
    for (const size_t workers : {size_t{1}, size_t{4}}) {
        Workers_ pool(workers);
        for (int repeat = 0; repeat < 2; ++repeat) {
            const auto expected = MCSimulation<AAD::Number_>(base, data, 4096, "sobol", false, true);
            const auto actual = MCSimulation<AAD::Number_>(sink, data, 4096, "sobol", false, true);
            ASSERT_EQ(actual.names_, expected.names_);
            ASSERT_EQ(actual.risks_.size(), 16U);
            ASSERT_EQ(std::memcmp(&actual.aggregated_, &expected.aggregated_, sizeof(double)), 0);
            ASSERT_EQ(std::memcmp(actual.risks_.data(), expected.risks_.data(), actual.risks_.size() * sizeof(double)), 0);
        }
    }
}

TEST(GSRPassiveConstantsTest, TestReclaimsRetainedBlockAndPreservesDefaultAfterOptIn) {
    Workers_ workers(1);
    const auto data = GSRData();
    const auto base = Prepare(data, false);
    const auto sink = Prepare(data, true);
    AAD::Clear(*AAD::Tape());
    const auto original = Graph(base, data);
    const auto shared = Graph(sink, data);
    const auto restored = Graph(base, data);
    ASSERT_EQ(original.nodes_ - shared.nodes_, 3813U);
    ASSERT_EQ(original.edges_, shared.edges_);
    ASSERT_EQ(original.capacityBytes_ - shared.capacityBytes_, AAD::BLOCK_SIZE * sizeof(AAD::TapNode_));
    ASSERT_EQ(original.occupiedBytes_ - shared.occupiedBytes_, 3813U * sizeof(AAD::TapNode_));
    ASSERT_EQ(restored.nodes_, original.nodes_);
    ASSERT_EQ(restored.capacityBytes_, original.capacityBytes_);
    AAD::Number_ a(3.0), b(3.0);
    ASSERT_FALSE(a.IsPassive());
    ASSERT_NE(&AAD::Adjoint(a), &AAD::Adjoint(b));
    AAD::Clear(*AAD::Tape());
}

TEST(GSRPassiveConstantsTest, TestNonlinearPayoffKeepsScriptConstantRiskIndependent) {
    Workers_ workers(1);
    const auto data = GSRData();
    const ScriptProductData_ product("option", {Cell_("K"), Cell_(Date_(2025, 1, 1)), Cell_(Date_(2026, 1, 1))},
                                     {"0.04", "x = FIX(IR[USD,SWAP,5Y])", "pay PAYS MAX(x - K, 0)"});
    const auto base = Prepare(data, false, product);
    const auto sink = Prepare(data, true, product);
    const auto expected = MCSimulation<AAD::Number_>(base, data, 2048, "sobol", false, true);
    const auto actual = MCSimulation<AAD::Number_>(sink, data, 2048, "sobol", false, true);
    ASSERT_EQ(actual.names_, expected.names_);
    ASSERT_EQ(actual.risks_.size(), 17U);
    ASSERT_EQ(actual.names_.back(), "K");
    ASSERT_LT(actual.risks_.back(), 0.0);
    ASSERT_EQ(std::memcmp(&actual.aggregated_, &expected.aggregated_, sizeof(double)), 0);
    ASSERT_EQ(std::memcmp(actual.risks_.data(), expected.risks_.data(), actual.risks_.size() * sizeof(double)), 0);
}

TEST(GSRPassiveConstantsTest, TestRejectsUnadmittedModelsModesAndObjectives) {
    const auto data = GSRData();
    for (const size_t factors : {size_t{1}, size_t{2}, size_t{4}})
        ASSERT_THROW(Prepare(GSRData(factors), true), ScriptError_);
    const Handle_<ModelData_> bs(new BSModelData_("bs", 100.0, 0.2, 0.05, 0.02));
    const ScriptProductData_ call("call", {Cell_(Date_(2025, 1, 1))}, {"pay PAYS SPOT()"});
    ASSERT_THROW(Prepare(bs, true, call), ScriptError_);
    MonteCarloSettings_ settings;
    settings.usePassiveConstants_ = true;
    ASSERT_THROW(ValidateSimulationSettings(settings), ScriptError_);
    settings.enableAad_ = true;
    ASSERT_THROW(ValidateSimulationSettings(settings), ScriptError_);
    settings.compiled_ = true;
    settings.useBb_ = true;
    ASSERT_THROW(ValidateSimulationSettings(settings), ScriptError_);
    settings.useBb_ = false;
    settings.rsg_ = "irn";
    ASSERT_THROW(ValidateSimulationSettings(settings), ScriptError_);
    settings.rsg_ = "sobol";
    settings.normalPrecision_ = "Precise";
    ASSERT_THROW(ValidateSimulationSettings(settings), ScriptError_);
    const auto sink = Prepare(data, true);
    ASSERT_THROW(MCSimulation<AAD::Number_>(sink, bs, 8, "sobol", false, true), ScriptError_);
    ASSERT_THROW(MCSimulation<AAD::Number_>(sink, data, 8, "sobol", true, true), ScriptError_);
    const ScriptProductData_ exercise("exercise", {Cell_(Date_(2025, 1, 1))}, {"EXERCISE 1"});
    ASSERT_THROW(Prepare(data, true, exercise), ScriptError_);
    struct OtherObjective_ : SimDetail::ScalarSimulationObjective_ {};
    ASSERT_THROW(MCAADSimulationWithObjective(sink, data, 8, "sobol", false, true, -1, DEFAULT_SMOOTH, OtherObjective_{}), ScriptError_);
    const auto vectorMode = AAD::SetNumResultsForAAD(true, 4);
    Workers_ workers(1);
    ASSERT_THROW(MCSimulation<AAD::Number_>(sink, data, 8, "sobol", false, true), ScriptError_);
}
