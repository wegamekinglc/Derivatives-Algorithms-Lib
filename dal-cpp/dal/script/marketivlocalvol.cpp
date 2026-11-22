#include <dal/platform/platform.hpp>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/compensatedsum.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/script/batchstatistics_internal.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/script/plannedpreparation.hpp>
#include <dal/script/simulation.hpp>
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

        size_t RefitWorkspaceReservation(const MarketIvScriptRefitRequest_& request, size_t scenarios) {
            constexpr MarketIvLocalVolDetail::BudgetField_ bytes{"Preflight", "/budget/workspace_bytes"};
            // Reserve the configured numeric envelopes for a base and one pair at a time.
            const auto& fit = request.snapshot_.Execution();
            const auto& grid = request.grid_.settings_;
            const auto& model = request.model_;
            const size_t models = bytes.Multiply(3, model.numericPayloadBudgetBytes_);
            const size_t building = bytes.Add(bytes.Add(grid.numericPayloadBudgetBytes_, model.numericPayloadBudgetBytes_),
                                              std::max(grid.workspaceBudgetBytes_, model.workspaceBudgetBytes_));
            const size_t working = std::max(
                {bytes.Add(fit.plannedWorkspaceBytes_, fit.plannedNumericPayloadBytes_), building, bytes.Multiply(3, model.workspaceBudgetBytes_)});
            const size_t rows = bytes.Multiply(request.quoteIds_.size(), sizeof(MarketIvScriptRefitRow_) + sizeof(MarketIvScriptPairedPvs_));
            const size_t means = bytes.Multiply(request.blocks_, bytes.Multiply(scenarios, sizeof(double)));
            const size_t reserved = bytes.Add(bytes.Add(fit.plannedNumericPayloadBytes_, models), bytes.Add(working, bytes.Add(rows, means)));
            bytes.Check(reserved, request.budget_.workspaceBudgetBytes_);
            return reserved;
        }

        MarketIvScriptRefitWork_ RefitWork(const MarketIvScriptRefitRequest_& request) {
            using namespace MarketIvLocalVolDetail;
            const auto& budget = request.budget_;
            const size_t count = request.quoteIds_.size();
            REQUIRE(count > 0, "InvalidMarketIvRequest: /quote_ids must select at least one quote");
            CheckLimit(count, budget.maxSelectedQuotes_, "/budget/max_selected_quotes", "Preflight");
            REQUIRE(request.blocks_ >= 2, "InvalidMarketIvRequest: /blocks requires at least two independent blocks");
            REQUIRE(request.pathsPerBlock_ > 0, "InvalidMarketIvRequest: /paths_per_block must be positive");
            constexpr BudgetField_ scenarioBudget{"Preflight", "/budget/max_scenarios"};
            const size_t refits = scenarioBudget.Multiply(2, count), scenarios = scenarioBudget.Add(1, refits);
            scenarioBudget.Check(scenarios, budget.maxScenarios_);
            constexpr BudgetField_ pathBudget{"Preflight", "/budget/max_total_paths"};
            const size_t paths = pathBudget.Multiply(request.blocks_, request.pathsPerBlock_);
            const size_t totalPaths = pathBudget.Multiply(scenarios, paths);
            pathBudget.Check(totalPaths, budget.maxTotalPaths_);
            constexpr BudgetField_ fitBudget{"Preflight", "/budget/max_model_row_evaluations"};
            const size_t fitRows = fitBudget.Multiply(refits, request.snapshot_.Request().settings_.maxModelRowEvaluations_);
            fitBudget.Check(fitRows, budget.maxModelRowEvaluations_);

            const size_t bytes = RefitWorkspaceReservation(request, scenarios);
            return {refits, scenarios, scenarios, paths, totalPaths, fitRows, bytes};
        }
    } // namespace

    MarketIvScriptRefitPlan_ PlanMarketIvScriptRefit(const MarketIvScriptRefitRequest_& input) {
        // Even an rvalue can have outstanding references into its vector storage.
        auto request = input;
        REQUIRE(request.product_, "InvalidMarketIvRequest: /product is required");
        REQUIRE(request.simulation_.rsg_ == "mrg32", "InvalidMarketIvRequest: /simulation/rsg requires mrg32");
        REQUIRE(request.simulation_.normalPrecision_ == "Precise", "InvalidMarketIvRequest: /simulation/normal_precision requires Precise");
        const auto work = RefitWork(request);
        auto rows = RefitRows(request);
        const auto grid = CalibrateMarketIvDupire(request.snapshot_, request.grid_);
        auto base = BuildMarketIvLocalVolModelData("refit-base", grid, request.model_);
        AAD::MarketIvLocalVolModel_ model(*base);
        model.ValidatePaths(work.pathsPerScenario_, request.simulation_.rsg_);
        auto prepared =
            std::make_shared<PreparedScript_>(Detail::PrepareWithFrozenFixings(*request.product_, &model, request.valuation_, request.simulation_));
        request.valuation_ = prepared->Settings();
        return {std::move(request), std::move(rows), work, std::move(base), std::move(prepared)};
    }

    namespace {
        void CheckSamplingTimes(const Vector_<>& expected, const Vector_<>& times, const String_& modelField) {
            if (times.size() != expected.size())
                MarketIvLocalVolDetail::Fail("MarketIvCoordinateMismatch", "Sample", modelField + "/simulation/times",
                                             String_(std::to_string(expected.size())), String_(std::to_string(times.size())));
            for (size_t i = 0; i < times.size(); ++i)
                if (times[i] != expected[i])
                    MarketIvLocalVolDetail::Fail("MarketIvCoordinateMismatch", "Sample",
                                                 modelField + "/simulation/times/" + String_(std::to_string(i)), ToString(expected[i]),
                                                 ToString(times[i]));
        }

        struct PayoffMean_ {
            // A power of two leaves room for any size_t path count without rounding individual normal terms.
            static constexpr double SCALE = .5 / (1. + static_cast<double>(std::numeric_limits<size_t>::max()));
            Dal::Detail::CompensatedSum_ scaled_, tiny_;
            size_t count_ = 0;

            void operator+=(double value) {
                ++count_;
                if (std::abs(value) < std::numeric_limits<double>::min() / SCALE)
                    tiny_.Add(value);
                else
                    scaled_.Add(value * SCALE);
            }

            [[nodiscard]] double Mean() const {
                const double sum = scaled_.Value();
                if (std::abs(sum) < 1.)
                    return (sum / SCALE + tiny_.Value()) / count_;
                // Here even size_t::max tiny terms are below one ulp. Bound final rounding at the largest finite payoff.
                constexpr double LIMIT = std::numeric_limits<double>::max() * SCALE;
                return std::clamp(sum / count_, -LIMIT, LIMIT) / SCALE;
            }
        };

        MarketIvScriptBlockPvs_
        SampleBlocks(const PreparedScript_& product, const AAD::MarketIvLocalVolModel_& model, size_t blocks, size_t paths, size_t* totalPaths) {
            const auto& simulation = product.Simulation();
            Detail::DoubleSimulationState_<PreparedScript_> state(product, model, simulation.rsg_, simulation.useBb_, simulation.normalPrecision_);
            MarketIvScriptBlockPvs_ pvs{Vector_<>(blocks)};
            PayoffMean_ scenario;
            const auto sample = [&](auto* evaluator, const auto& evaluate) {
                const auto evaluatePath = [&](const auto& path, auto& state) {
                    ++*totalPaths;
                    evaluate(path, state);
                };
                for (size_t b = 0; b < blocks; ++b) {
                    const PathBatch_ batch{b * paths, paths};
                    const auto block = Detail::EvaluateDoubleBatch<PayoffMean_>(model, &state, evaluator, batch, product.PayOffIdx(), evaluatePath);
                    pvs.blockMeans_[b] = block.Mean();
                    scenario += pvs.blockMeans_[b];
                }
            };
            if (simulation.compiled_.value_or(DEFAULT_COMPILED)) {
                const auto& program = product.CompiledProgram();
                sample(&state.compiledState_, [&](const auto& path, auto& eval) { program.Evaluate(path, eval); });
            } else
                sample(&state.evaluator_, [&](const auto& path, auto& eval) { product.Evaluate(path, eval); });
            pvs.mean_ = scenario.Mean();
            return pvs;
        }

        size_t AdditionalModelBytes(const MarketIvScriptModelPair_& pair, const Handle_<MarketIvLocalVolModelData_>& base) {
            // An absent side adds zero bytes to the base reservation.
            const auto& plusModel = pair.plus_ ? *pair.plus_ : *base;
            const auto& minusModel = pair.minus_ ? *pair.minus_ : *base;
            const auto plus = plusModel.ConstructionExecution().numericPayloadBytes_ - plusModel.SharedNumericPayloadBytes(*base);
            // Each source owns its grid and retains one fit. Shared portions of a side are therefore nested.
            const auto sharedMinus = std::max(minusModel.SharedNumericPayloadBytes(*base), minusModel.SharedNumericPayloadBytes(plusModel));
            const auto minus = minusModel.ConstructionExecution().numericPayloadBytes_ - sharedMinus;
            return MarketIvLocalVolDetail::CheckedAdd(plus, minus);
        }

        MarketIvScriptBlockPvs_ SampleScenario(const MarketIvScriptRefitPlan_& plan,
                                               const MarketIvLocalVolModelData_& data,
                                               const String_& modelField,
                                               BufferCapacityBudget_* workspace,
                                               MarketIvScriptSamplingResult_* result) {
            const auto& request = plan.request_;
            const auto& product = *plan.prepared_;
            const auto config = data.Configuration();
            MarketIvLocalVolDetail::CheckLimit(config.workspaceBudgetBytes_, request.model_.workspaceBudgetBytes_,
                                               modelField + "/settings/workspace_budget_bytes", "Sample");
            MarketIvLocalVolDetail::CheckLimit(data.ConstructionExecution().numericPayloadBytes_, request.model_.numericPayloadBudgetBytes_,
                                               modelField + "/settings/numeric_payload_budget_bytes", "Sample");
            if (config.factor_ != result->factor_)
                MarketIvLocalVolDetail::Fail("MarketIvCoordinateMismatch", "Sample", modelField + "/simulation/factor", result->factor_,
                                             config.factor_);
            const auto frozen = plan.baseModel_->Configuration();
            REQUIRE(config.index_ == frozen.index_ && config.currency_ == frozen.currency_,
                    "InvalidMarketIvRequest: /models must use the frozen index and PV currency");
            AAD::MarketIvLocalVolModel_ model(data);
            model.ValidateFrozenMarket(request.snapshot_.Request().market_, modelField + "/market");
            model.ValidateRequest(false, false, request.valuation_.evaluationDate_, request.simulation_.rsg_);
            model.ValidatePaths(plan.work_.pathsPerScenario_, request.simulation_.rsg_);
            ++result->scenarios_;
            if (product.AllExpired())
                return {Vector_<>(request.blocks_, 0.)};
            {
                // Allocate has its own capacity ledger. Charge its peak and its retained shared state in their respective phases.
                Dal::Detail::BufferCapacitySuspension_ separateModelLedger;
                model.Allocate(product.TimeLine(), product.DefLine());
                model.Init(product.TimeLine(), product.DefLine());
            }
            const auto execution = model.Execution();
            constexpr MarketIvLocalVolDetail::BudgetField_ bytes{"Sample", "/budget/workspace_bytes"};
            const auto preparationPeak = bytes.Add(workspace->CapacityBytes(), model.PreparationPeakBytes());
            bytes.Check(preparationPeak, workspace->LimitBytes());
            result->peakWorkspaceBytes_ = std::max(result->peakWorkspaceBytes_, preparationPeak);
            const auto shared = BufferCapacityScope_::ForWorker(workspace, execution.sharedNumericBytes_);
            if (result->simulationTimes_.empty())
                result->simulationTimes_ = model.SimulationTimes();
            else
                CheckSamplingTimes(result->simulationTimes_, model.SimulationTimes(), modelField);

            return SampleBlocks(product, model, request.blocks_, request.pathsPerBlock_, &result->totalPaths_);
        }
        struct SamplingPair_ {
            MarketIvScriptModelPair_ models_;
            MarketIvScriptRiskRow_* report_ = nullptr;
        };

        MarketIvScriptPairedPvs_ SamplePair(const MarketIvScriptRefitPlan_& plan,
                                            const MarketIvScriptRefitRow_& quote,
                                            const SamplingPair_& pair,
                                            BufferCapacityBudget_* workspace,
                                            MarketIvScriptSamplingResult_* result) {
            const auto payload = BufferCapacityScope_::ForWorker(workspace, AdditionalModelBytes(pair.models_, plan.baseModel_));
            MarketIvScriptPairedPvs_ row;
            const auto sample = [&](const Handle_<MarketIvLocalVolModelData_>& model, const char* sign,
                                    MarketIvScriptRiskSide_ MarketIvScriptRiskRow_::* side) -> MarketIvScriptBlockPvs_ {
                if (!model)
                    return {};
                try {
                    return SampleScenario(plan, *model, "/models/" + quote.id_ + sign, workspace, result);
                } catch (const Exception_& error) {
                    if (!pair.report_)
                        throw;
                    pair.report_->status_ = "SIDE_FAILED";
                    (pair.report_->*side).failureStage_ = "SAMPLE";
                    (pair.report_->*side).failure_ = error.what();
                    return {};
                }
            };
            row.plus_ = sample(pair.models_.plus_, "/plus", &MarketIvScriptRiskRow_::plus_);
            row.minus_ = sample(pair.models_.minus_, "/minus", &MarketIvScriptRiskRow_::minus_);
            if (row.plus_.blockMeans_.empty() || row.minus_.blockMeans_.empty())
                return row;
            const auto slope = [&](size_t b) { return (row.plus_.blockMeans_[b] - row.minus_.blockMeans_[b]) / (2. * quote.step_); };
            for (size_t b = 0; b < plan.request_.blocks_; ++b)
                row.slope_ += slope(b) / plan.request_.blocks_;
            row.standardError_ = BatchStatisticsInternal::StandardError(plan.request_.blocks_, [&](size_t b) { return slope(b) - row.slope_; });
            REQUIRE2(std::isfinite(row.slope_) && std::isfinite(row.standardError_), "InvalidPayoff: non-finite paired slope or error", ScriptError_);
            return row;
        }

        template <class Models_> MarketIvScriptSamplingResult_ SampleModels(const MarketIvScriptRefitPlan_& plan, Models_ models) {
            BufferCapacityBudget_ workspace(plan.work_.reservedWorkspaceBytes_);
            BufferCapacityScope_ scope(&workspace, plan.baseModel_->ConstructionExecution().numericPayloadBytes_);
            MarketIvScriptSamplingResult_ result;
            result.factor_ = plan.baseModel_->Configuration().factor_;
            result.useBb_ = plan.prepared_->Simulation().useBb_;
            result.pathsPerBlock_ = plan.request_.pathsPerBlock_;
            result.rows_.reserve(plan.rows_.size());
            result.base_ = SampleScenario(plan, *plan.baseModel_, "/models/base", &workspace, &result);
            for (size_t i = 0; i < plan.rows_.size(); ++i) {
                const auto pair = [&] {
                    Dal::Detail::BufferCapacitySuspension_ separateModelLedger;
                    return models(i);
                }();
                result.rows_.push_back(SamplePair(plan, plan.rows_[i], pair, &workspace, &result));
            }
            result.peakWorkspaceBytes_ = std::max(result.peakWorkspaceBytes_, workspace.PeakCapacityBytes());
            return result;
        }

        MarketIvScriptRiskSide_ RiskSide(const MarketIvScriptRefitSide_& side, MarketIvScriptRefitExecution_* work) {
            MarketIvScriptRiskSide_ result;
            result.fitStatus_ = side.fitStatus_;
            result.failureStage_ = side.failureStage_;
            result.failure_ = side.failure_;
            if (side.selectedStart_ >= 0)
                result.selectedStart_ = side.selectedStart_;
            if (side.failureStage_ != "BUMP")
                ++work->refits_;
            if (side.model_ || side.failureStage_ == "GRID" || side.failureStage_ == "MODEL") {
                ++work->grids_;
                result.regimeChanged_ = side.regimeChanged_;
            }
            if (side.model_)
                result.modelIdentity_ = side.model_->ModelIdentity();
            return result;
        }
    } // namespace

    MarketIvScriptSamplingResult_
    SampleMarketIvScriptModelPairs(const MarketIvScriptRefitPlan_& plan,
                                   const std::function<MarketIvScriptModelPair_(const MarketIvScriptRefitRow_&)>& models) {
        REQUIRE(plan.prepared_ && plan.baseModel_ && models,
                "InvalidMarketIvRequest: /models requires the prepared plan and a pair per selected quote");
        return SampleModels(plan, [&](size_t i) {
            const auto pair = models(plan.rows_[i]);
            REQUIRE(pair.plus_ && pair.minus_, "InvalidMarketIvRequest: /models requires both fixed sides for " + plan.rows_[i].id_);
            return SamplingPair_{pair};
        });
    }

    MarketIvScriptRefitRiskResult_ EvaluateMarketIvScriptRefitRisk(const MarketIvScriptRefitRequest_& request) {
        const auto plan = PlanMarketIvScriptRefit(request);
        MarketIvScriptRefitRiskResult_ result;
        result.rows_.reserve(plan.rows_.size());
        auto& work = result.execution_;
        work.blocks_ = plan.request_.blocks_;
        work.pathsPerBlock_ = plan.request_.pathsPerBlock_;
        work.grids_ = 1;
        const auto currency = plan.baseModel_->Configuration().currency_;
        auto sampled = SampleModels(plan, [&](size_t i) {
            const auto pair = BuildMarketIvScriptRefitPair(plan, i);
            result.rows_.push_back({pair.row_, currency, pair.Status(), 0., RiskSide(pair.plus_, &work), RiskSide(pair.minus_, &work)});
            return SamplingPair_{{pair.plus_.model_, pair.minus_.model_}, &result.rows_.back()};
        });
        work.scenarios_ = sampled.scenarios_;
        result.baseBlockMeans_ = std::move(sampled.base_.blockMeans_);
        work.totalPaths_ = sampled.totalPaths_;
        work.samplingPeakWorkspaceBytes_ = sampled.peakWorkspaceBytes_;
        for (size_t i = 0; i < result.rows_.size(); ++i) {
            auto& row = result.rows_[i];
            const auto& pvs = sampled.rows_[i];
            row.basePv_ = sampled.base_.mean_;
            if (!pvs.plus_.blockMeans_.empty())
                row.plus_.pv_ = pvs.plus_.mean_;
            if (!pvs.minus_.blockMeans_.empty())
                row.minus_.pv_ = pvs.minus_.mean_;
            if (row.plus_.pv_ && row.minus_.pv_) {
                row.slope_ = pvs.slope_;
                row.standardError_ = pvs.standardError_;
            }
        }
        return result;
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
