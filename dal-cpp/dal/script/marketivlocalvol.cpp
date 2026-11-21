#include <dal/platform/platform.hpp>

#include <dal/concurrency/threadpool.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/plannedpreparation.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal::Script {
    namespace {
        void ValidateIdentityTextSize(const ScriptProductData_& product, const PreparedScript_& prepared) {
            using namespace MarketIvLocalVolDetail;
            size_t bytes = CheckedAdd(4096, product.Settings().defaultIndex_.size());
            for (const auto& text : product.EventTexts())
                bytes = CheckedAdd(bytes, text.size());
            for (const auto& date : product.Dates())
                bytes = CheckedAdd(bytes, Cell::ToString(date).size());
            const auto& frozen = prepared.Settings().fixings_;
            if (frozen)
                for (const auto& [name, history] : frozen->Values())
                    bytes = CheckedAdd(bytes, CheckedMultiply(history.size(), CheckedAdd(name.size(), 128)));
            const size_t envelope = CheckedMultiply(1024, CheckedMultiply(bytes, bytes));
            CheckLimit(envelope, std::string().max_size(), "/execution/request_identity", "Prepare");
        }

        String_ RequestIdentity(const ScriptProductData_& product, const PreparedScript_& prepared, const MarketIvLocalVolExecution_& execution) {
            ValidateIdentityTextSize(product, prepared);
            using J_ = CanonicalJson::Json_;
            J_ root = J_::Object();
            root.object_["schema"] = J_::String("dal.market-iv-local-vol-request/2");
            root.object_["model_identity"] = J_::String(execution.modelIdentity_);
            root.object_["product"] = J_::String(DescribeScriptProductData(product));
            root.object_["prepared"] = J_::String(ExplainPreparedScript(prepared));
            root.object_["workers"] = J_::String(String_(std::to_string(execution.workers_)));
            root.object_["layout"] = J_::String("SHARED_HYBRID_PRIVATE_PATH_CAPACITY/1");
            root.object_["policy_risk_mode"] = J_::String(prepared.Simulation().lsmcPolicyRiskMode_);
            root.object_["policy_bump"] = J_::Number(prepared.Simulation().lsmcPolicyBumpRelative_);
            root.object_["passive_constants"] = J_::Boolean(prepared.Simulation().usePassiveConstants_);
            J_ fixings = J_::Array();
            const auto& frozen = prepared.Settings().fixings_;
            if (frozen)
                for (const auto& [name, history] : frozen->Values())
                    for (const auto& [time, value] : history) {
                        J_ row = J_::Array();
                        row.array_ = {J_::String(name), J_::String(Date::ToString(time.Date())), J_::Number(time.Frac()), J_::Number(value)};
                        fixings.array_.push_back(std::move(row));
                    }
            root.object_["fixings"] = std::move(fixings);
            return CanonicalJson::Fingerprint(root);
        }

        Vector_<MarketIvScriptRefitRow_> RefitRows(const MarketIvScriptRefitRequest_& request) {
            MarketIvDetail::ValidateRefitQuotes(request.quoteIds_, request.steps_);
            Vector_<MarketIvScriptRefitRow_> rows;
            rows.reserve(request.quoteIds_.size());
            for (const auto& selection : MarketIvDetail::SelectRefitQuotes(request.snapshot_, request.quoteIds_, request.steps_)) {
                const auto& quote = request.snapshot_.Request().quotes_[selection.quoteIndex_];
                REQUIRE(std::isfinite(2. * selection.step_), "InvalidMarketIvRequest: /steps symmetric span overflows; ID=" + quote.id_);
                for (double step : {selection.step_, -selection.step_}) {
                    auto bumped = quote;
                    MarketIvDetail::BumpRefitQuote(&bumped, step);
                }
                rows.push_back({selection.quoteIndex_, quote.id_, quote.quoteType_, MarketIvDetail::RefitInputUnit(quote), selection.step_,
                                selection.forward_, selection.discount_});
            }
            return rows;
        }

        size_t RefitWorkspaceReservation(const MarketIvScriptRefitRequest_& request) {
            using namespace MarketIvLocalVolDetail;
            // Reserve the configured numeric envelopes for a base and one pair at a time.
            const auto& fit = request.snapshot_.Execution();
            const auto& grid = request.grid_.settings_;
            const auto& model = request.model_;
            const size_t models = CheckedMultiply(3, model.numericPayloadBudgetBytes_);
            const size_t building = CheckedAdd(CheckedAdd(grid.numericPayloadBudgetBytes_, model.numericPayloadBudgetBytes_),
                                               std::max(grid.workspaceBudgetBytes_, model.workspaceBudgetBytes_));
            const size_t working = std::max(
                {CheckedAdd(fit.plannedWorkspaceBytes_, fit.plannedNumericPayloadBytes_), building, CheckedMultiply(3, model.workspaceBudgetBytes_)});
            const size_t rows = CheckedMultiply(request.quoteIds_.size(), sizeof(MarketIvScriptRefitRow_));
            const size_t means = CheckedMultiply(request.blocks_, 3 * sizeof(double));
            return CheckedAdd(CheckedAdd(fit.plannedNumericPayloadBytes_, models), CheckedAdd(working, CheckedAdd(rows, means)));
        }

        MarketIvScriptRefitWork_ RefitWork(const MarketIvScriptRefitRequest_& request) {
            using namespace MarketIvLocalVolDetail;
            const auto& budget = request.budget_;
            const size_t count = request.quoteIds_.size();
            REQUIRE(count > 0, "InvalidMarketIvRequest: /quote_ids must select at least one quote");
            CheckLimit(count, budget.maxSelectedQuotes_, "/budget/max_selected_quotes", "Preflight");
            REQUIRE(request.blocks_ >= 2, "InvalidMarketIvRequest: /blocks requires at least two independent blocks");
            REQUIRE(request.pathsPerBlock_ > 0, "InvalidMarketIvRequest: /paths_per_block must be positive");
            const size_t refits = CheckedMultiply(2, count), scenarios = CheckedAdd(1, refits);
            CheckLimit(scenarios, budget.maxScenarios_, "/budget/max_scenarios", "Preflight");
            const size_t paths = CheckedMultiply(request.blocks_, request.pathsPerBlock_);
            const size_t totalPaths = CheckedMultiply(scenarios, paths);
            CheckLimit(totalPaths, budget.maxTotalPaths_, "/budget/max_total_paths", "Preflight");
            const size_t fitRows = CheckedMultiply(refits, request.snapshot_.Request().settings_.maxModelRowEvaluations_);
            CheckLimit(fitRows, budget.maxModelRowEvaluations_, "/budget/max_model_row_evaluations", "Preflight");

            const size_t bytes = RefitWorkspaceReservation(request);
            CheckLimit(bytes, budget.workspaceBudgetBytes_, "/budget/workspace_bytes", "Preflight");
            return {refits, scenarios, scenarios, paths, totalPaths, fitRows, bytes};
        }
    } // namespace

    MarketIvScriptRefitPlan_ PlanMarketIvScriptRefit(MarketIvScriptRefitRequest_ request) {
        REQUIRE(request.product_, "InvalidMarketIvRequest: /product is required");
        REQUIRE(request.simulation_.rsg_ == "mrg32", "InvalidMarketIvRequest: /simulation/rsg requires mrg32");
        REQUIRE(request.simulation_.normalPrecision_ == "Precise", "InvalidMarketIvRequest: /simulation/normal_precision requires Precise");
        const auto work = RefitWork(request);
        auto rows = RefitRows(request);
        const auto grid = CalibrateMarketIvDupire(request.snapshot_, request.grid_);
        auto base = BuildMarketIvLocalVolModelData("refit-base", grid, request.model_);
        AAD::MarketIvLocalVolModel_ model(*base);
        model.ValidatePaths(work.pathsPerScenario_, request.simulation_.rsg_);
        const auto prepared = Detail::PrepareWithFrozenFixings(*request.product_, &model, request.valuation_, request.simulation_);
        request.valuation_ = prepared.Settings();
        return {std::move(request), std::move(rows), work, std::move(base)};
    }

    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& data,
                                                                const ScriptValuationSettings_& valuation,
                                                                const MonteCarloSettings_& simulation) {
        AAD::MarketIvLocalVolModel_ model(data);
        const auto prepared = PrepareScript(product, &model, valuation, simulation);
        if (prepared.AllExpired()) {
            const Vector_<> timeline{0.};
            const Vector_<AAD::SampleDef_> definitions(1);
            model.Allocate(timeline, definitions);
            model.Init(timeline, definitions);
        }
        model.PrepareWorkers(ThreadPool_::GetInstance()->NumThreads());
        auto execution = model.Execution();
        execution.requestIdentity_ = RequestIdentity(product, prepared, execution);
        return execution;
    }
} // namespace Dal::Script
