//
// Created by dal-implementer on 2026/9/20.
//

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>
#include <dal/script/visitor/compiler.hpp>
#include <dal/script/visitor/fuzzy.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    namespace {
        //  N3: sample-size guard needs ten paths per basis function
        constexpr size_t PATHS_PER_BASIS_FUNCTION = 10;
        //  N3: sigma floor keeps the z-normalization finite on (legal) constant regressors
        constexpr double SIGMA_FLOOR_SCALE = 1e-12;
        //  N3: explicit relative ridge applied before CholeskySolve
        constexpr double RIDGE_LAMBDA = 1e-12;
        //  Bound both the Gram diagonal spread and the scaled Cholesky pivots.
        constexpr double CONDITION_LIMIT = 1e12;
        constexpr double QR_RANK_TOLERANCE = 1e-10;

        struct PathCounts_ {
            size_t training_;
            size_t validation_;
            size_t pricingOffset_;
        };

        PathCounts_ LsmcPathCounts(const MonteCarloSettings_& simulation, size_t nPaths) {
            const size_t training = simulation.lsmcTrainingPaths_ ? static_cast<size_t>(*simulation.lsmcTrainingPaths_) : nPaths;
            const size_t validation = simulation.lsmcValidationPaths_ ? static_cast<size_t>(*simulation.lsmcValidationPaths_) : 0;
            constexpr size_t MAX_SOBOL_PATHS = std::numeric_limits<uint32_t>::max();
            REQUIRE2(training <= MAX_SOBOL_PATHS && validation <= MAX_SOBOL_PATHS - training && nPaths <= MAX_SOBOL_PATHS - training - validation,
                     "InvalidPathCount: LSMC training, validation, and pricing paths exceed the 32-bit Sobol sequence", ScriptError_);
            return {training, validation, training + validation};
        }

        double MeanOf(const Vector_<>& values, const Vector_<char>& included, size_t count) {
            if (count == 0)
                return 0.0;
            double sum = 0.0;
            for (size_t i = 0; i < values.size(); ++i)
                if (included[i]) {
                    REQUIRE(std::isfinite(values[i]), "InvalidRegressionInput: non-finite included target");
                    sum += values[i];
                }
            REQUIRE(std::isfinite(sum), "InvalidRegressionInput: target sum overflow");
            return sum / static_cast<double>(count);
        }

        //  Count, mean and sigma of the regressor over the included paths, with the sigma floor
        struct RegressionStats_ {
            size_t count_ = 0;
            double mean_ = 0.0;
            double sigma_ = 0.0;
            double sigmaFloor_ = 0.0;
        };

        RegressionStats_ RegressionStats(const Vector_<>& x, const Vector_<char>& included) {
            RegressionStats_ stats;
            double sumX = 0.0;
            for (size_t i = 0; i < x.size(); ++i)
                if (included[i]) {
                    REQUIRE(std::isfinite(x[i]), "InvalidRegressionInput: non-finite included regressor");
                    ++stats.count_;
                    sumX += x[i];
                }
            if (stats.count_ != 0) {
                stats.mean_ = sumX / static_cast<double>(stats.count_);
                double sumSq = 0.0;
                for (size_t i = 0; i < x.size(); ++i)
                    if (included[i]) {
                        const double dev = x[i] - stats.mean_;
                        sumSq += dev * dev;
                    }
                stats.sigma_ = std::sqrt(sumSq / static_cast<double>(stats.count_));
                REQUIRE(std::isfinite(stats.mean_) && std::isfinite(stats.sigma_), "InvalidRegressionInput: regressor moments overflow");
            }
            //  the floor engages even on an empty regression set, so a degenerate day
            //  never carries sigma_ = 0 into RegressionPredict's z-normalization
            stats.sigmaFloor_ = SIGMA_FLOOR_SCALE * std::max(1.0, std::abs(stats.mean_));
            return stats;
        }

        void DegradeToConstant(ExerciseRegression_* result, const char* reason, double constant) {
            result->degenerate_ = true;
            result->degenerateReason_ = reason;
            result->fallbackReason_ = reason;
            result->effectiveRank_ = result->numCondTrue_ > 0 ? 1 : 0;
            result->solver_ = "Constant";
            result->basisDegree_ = 0;
            result->coefficients_.Resize(1);
            result->coefficients_[0] = constant;
        }

        //  A[j,k] = sum(z^(j+k)): accumulate each moment once, O(N*d), then
        //  expand the small Hankel matrix. The fit retains the monomial convention.
        void AccumulateNormalEquations(const Vector_<>& x,
                                       const Vector_<>& targets,
                                       const Vector_<char>& included,
                                       double mean,
                                       double sigma,
                                       SquareMatrix_<>* gram,
                                       Vector_<>* rhs) {
            const size_t nBasis = static_cast<size_t>(gram->Rows());
            std::array<double, 17> moments{};
            for (size_t i = 0; i < x.size(); ++i) {
                if (!included[i])
                    continue;
                const double z = (x[i] - mean) / sigma;
                double power = 1.0;
                for (size_t j = 0; j < nBasis; ++j) {
                    moments[j] += power;
                    (*rhs)[j] += power * targets[i];
                    power *= z;
                }
                for (size_t j = nBasis; j < 2 * nBasis - 1; ++j) {
                    moments[j] += power;
                    power *= z;
                }
            }
            for (size_t j = 0; j < nBasis; ++j)
                for (size_t k = j; k < nBasis; ++k)
                    (*gram)(j, k) = moments[j + k];
        }

        bool HasIndependentColumns(const SquareMatrix_<>& gram, const std::array<double, 9>& scale) {
            std::array<std::array<double, 9>, 9> lower{};
            for (int j = 0; j < gram.Rows(); ++j) {
                for (int k = 0; k <= j; ++k) {
                    double residual = gram(k, j) / scale[j] / scale[k];
                    for (int l = 0; l < k; ++l)
                        residual -= lower[j][l] * lower[k][l];
                    if (!std::isfinite(residual) || (j == k && residual <= 1.0 / CONDITION_LIMIT))
                        return false;
                    lower[j][k] = j == k ? std::sqrt(residual) : residual / lower[k][k];
                }
            }
            return true;
        }

        //  Equal diagonal entries do not imply independent columns. Test the
        //  unregularized, column-scaled Gram matrix before ridge can hide rank loss.
        bool RankDeficient(const SquareMatrix_<>& gram) {
            std::array<double, 9> scale{};
            for (int j = 0; j < gram.Rows(); ++j) {
                if (!std::isfinite(gram(j, j)) || gram(j, j) <= 0.0)
                    return true;
                scale[j] = std::sqrt(gram(j, j));
            }
            return !HasIndependentColumns(gram, scale);
        }

        //  Explicit relative ridge, then the Gram diagonal-ratio conditioning guard
        const char* RidgeAndIllConditioned(SquareMatrix_<>* gram) {
            if (RankDeficient(*gram))
                return "GramRankLoss";
            const size_t nBasis = static_cast<size_t>(gram->Rows());
            double maxDiag = (*gram)(0, 0);
            double minDiag = (*gram)(0, 0);
            for (size_t j = 0; j < nBasis; ++j) {
                (*gram)(j, j) *= 1.0 + RIDGE_LAMBDA;
                maxDiag = std::max(maxDiag, (*gram)(j, j));
                minDiag = std::min(minDiag, (*gram)(j, j));
            }
            return minDiag <= 0.0 || maxDiag / minDiag > CONDITION_LIMIT ? "GramScale" : nullptr;
        }

        struct QrWorkspace_ {
            size_t nRows_ = 0;
            size_t nBasis_ = 0;
            std::array<Vector_<>, 9> columns_;
            std::array<double, 9> scales_{};
            std::array<size_t, 9> permutation_{};
            std::array<std::array<double, 9>, 9> upper_{};
            std::array<double, 9> projection_{};
            Vector_<> response_;
        };

        QrWorkspace_
        MakeQrWorkspace(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, double mean, double sigma, int degree) {
            QrWorkspace_ ws;
            ws.nBasis_ = static_cast<size_t>(degree + 1);
            ws.nRows_ = static_cast<size_t>(std::count(included.begin(), included.end(), 1));
            ws.response_.Resize(ws.nRows_);
            for (size_t j = 0; j < ws.nBasis_; ++j) {
                ws.columns_[j].Resize(ws.nRows_);
                ws.permutation_[j] = j;
            }
            size_t row = 0;
            for (size_t i = 0; i < x.size(); ++i) {
                if (!included[i])
                    continue;
                const double z = (x[i] - mean) / sigma;
                ws.response_[row] = targets[i];
                double power = 1.0;
                for (size_t j = 0; j < ws.nBasis_; ++j) {
                    ws.columns_[j][row] = power;
                    power *= z;
                }
                ++row;
            }
            return ws;
        }

        size_t NormalizeQrColumns(QrWorkspace_* ws) {
            for (size_t j = 0; j < ws->nBasis_; ++j) {
                double norm = 0.0;
                for (double value : ws->columns_[j])
                    norm = std::hypot(norm, value);
                if (!std::isfinite(norm) || norm == 0.0)
                    return j;
                ws->scales_[j] = norm;
                for (double& value : ws->columns_[j])
                    value /= norm;
            }
            return ws->nBasis_;
        }

        std::pair<size_t, double> BestQrPivot(const QrWorkspace_& ws, size_t step) {
            size_t pivot = step;
            double bestNorm = 0.0;
            for (size_t j = step; j < ws.nBasis_; ++j) {
                double normSq = 0.0;
                for (double value : ws.columns_[j])
                    normSq += value * value;
                if (normSq > bestNorm) {
                    bestNorm = normSq;
                    pivot = j;
                }
            }
            return {pivot, bestNorm};
        }

        void PivotQrColumns(QrWorkspace_* ws, size_t step, size_t pivot) {
            if (pivot == step)
                return;
            std::swap(ws->columns_[step], ws->columns_[pivot]);
            std::swap(ws->scales_[step], ws->scales_[pivot]);
            std::swap(ws->permutation_[step], ws->permutation_[pivot]);
            for (size_t i = 0; i < step; ++i)
                std::swap(ws->upper_[i][step], ws->upper_[i][pivot]);
        }

        void OrthogonalizeQrStep(QrWorkspace_* ws, size_t step, double normSq) {
            ws->upper_[step][step] = std::sqrt(normSq);
            for (double& value : ws->columns_[step])
                value /= ws->upper_[step][step];
            for (size_t i = 0; i < ws->nRows_; ++i)
                ws->projection_[step] += ws->columns_[step][i] * ws->response_[i];
            for (size_t j = step + 1; j < ws->nBasis_; ++j)
                for (int pass = 0; pass < 2; ++pass) {
                    double dot = 0.0;
                    for (size_t i = 0; i < ws->nRows_; ++i)
                        dot += ws->columns_[step][i] * ws->columns_[j][i];
                    ws->upper_[step][j] += dot;
                    for (size_t i = 0; i < ws->nRows_; ++i)
                        ws->columns_[j][i] -= dot * ws->columns_[step][i];
                }
        }

        void RecoverQrCoefficients(const QrWorkspace_& ws, Vector_<>* coefficients) {
            std::array<double, 9> pivoted{};
            for (size_t k = ws.nBasis_; k-- > 0;) {
                double residual = ws.projection_[k];
                for (size_t j = k + 1; j < ws.nBasis_; ++j)
                    residual -= ws.upper_[k][j] * pivoted[j];
                pivoted[k] = residual / ws.upper_[k][k];
            }
            coefficients->Resize(ws.nBasis_);
            for (size_t k = 0; k < ws.nBasis_; ++k)
                (*coefficients)[ws.permutation_[k]] = pivoted[k] / ws.scales_[k];
        }

        //  Column-pivoted, twice-reorthogonalized QR on actual design rows runs
        //  only after the O(Md) moment solve rejects a fit.
        size_t PivotedQrFit(const Vector_<>& x,
                            const Vector_<>& targets,
                            const Vector_<char>& included,
                            double mean,
                            double sigma,
                            int degree,
                            Vector_<>* coefficients) {
            auto ws = MakeQrWorkspace(x, targets, included, mean, sigma, degree);
            const size_t normalized = NormalizeQrColumns(&ws);
            if (normalized != ws.nBasis_)
                return normalized;
            size_t rank = 0;
            for (size_t step = 0; step < ws.nBasis_; ++step) {
                const auto [pivot, normSq] = BestQrPivot(ws, step);
                if (!std::isfinite(normSq) || normSq < QR_RANK_TOLERANCE * QR_RANK_TOLERANCE)
                    break;
                PivotQrColumns(&ws, step, pivot);
                OrthogonalizeQrStep(&ws, step, normSq);
                ++rank;
            }
            if (rank == ws.nBasis_)
                RecoverQrCoefficients(ws, coefficients);
            return rank;
        }

        constexpr size_t NO_SLOT = static_cast<size_t>(-1);

        struct ExerciseDayPlan_ {
            size_t eventId_;
            bool conditional_;
            double eps_; //  S17: the node's :eps option resolved against the simulation smoothing width
        };

        bool EventHasPays(const Event_& event) {
            for (const auto& statement : event)
                if (FindNode(*statement, [](const Node_& visited) { return dynamic_cast<const NodePays_*>(&visited) != nullptr; }))
                    return true;
            return false;
        }

        //  EXERCISE is a top-level statement only (nesting is rejected at parse time)
        const NodeExercise_* EventExercise(const Event_& event) {
            for (const auto& statement : event)
                if (const auto* exercise = dynamic_cast<const NodeExercise_*>(statement.get()))
                    return exercise;
            return nullptr;
        }

        struct LsmcPlan_ {
            Vector_<size_t> eventToPays_;
            Vector_<size_t> eventToExercise_;
            Vector_<size_t> paysEventIds_;
            Vector_<ExerciseDayPlan_> days_;
            bool anyConditional_ = false;
        };

        LsmcPlan_ ScanEvents(const Vector_<Event_>& events, double smooth) {
            LsmcPlan_ scan;
            scan.eventToPays_.Resize(events.size());
            scan.eventToExercise_.Resize(events.size());
            for (size_t e = 0; e < events.size(); ++e)
                scan.eventToPays_[e] = scan.eventToExercise_[e] = NO_SLOT;
            for (size_t e = 0; e < events.size(); ++e) {
                if (EventHasPays(events[e])) {
                    scan.eventToPays_[e] = scan.paysEventIds_.size();
                    scan.paysEventIds_.push_back(e);
                }
                if (const NodeExercise_* exercise = EventExercise(events[e])) {
                    const bool conditional = exercise->arguments_.size() > 1;
                    scan.anyConditional_ |= conditional;
                    scan.eventToExercise_[e] = scan.days_.size();
                    scan.days_.push_back({e, conditional, exercise->eps_ < 0.0 ? smooth : exercise->eps_});
                }
            }
            return scan;
        }

        //  Training keeps only regression inputs; hard pricing uses worker-local scalars.
        struct LsmcStorage_ {
            Vector_<Vector_<>> pays_;
            Vector_<Vector_<>> xByDay_;
            Vector_<Vector_<>> hByDay_;
            Vector_<Vector_<char>> condByDay_; //  empty row = unconditional day
        };

        Vector_<Vector_<>> PathRows(size_t nRows, size_t nPaths) {
            Vector_<Vector_<>> rows(nRows);
            for (auto& row : rows)
                row = Vector_<>(nPaths, 0.0);
            return rows;
        }

        LsmcStorage_ MakeStorage(const LsmcPlan_& scan, size_t nPaths) {
            LsmcStorage_ storage;
            storage.pays_ = PathRows(scan.paysEventIds_.size(), nPaths);
            storage.xByDay_ = PathRows(scan.days_.size(), nPaths);
            storage.hByDay_ = PathRows(scan.days_.size(), nPaths);
            if (scan.anyConditional_) {
                storage.condByDay_.Resize(scan.days_.size());
                for (size_t k = 0; k < scan.days_.size(); ++k)
                    if (scan.days_[k].conditional_)
                        storage.condByDay_[k] = Vector_<char>(nPaths, 1);
            }
            return storage;
        }

        struct PricingObservation_ {
            double x_;
            double h_;
            bool cond_;
        };

        struct ThreadState_ {
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<> path_;
            //  Shared with MCDoubleSimulation via Detail::LocalCheckedPaths_ (simulation.hpp)
            std::unique_ptr<Detail::LocalCheckedPaths_> bsPaths_;
            LsmcEvaluator_<double> evaluator_;
            //  Compiled mode: worker-local state feeding training or pricing sinks
            std::optional<EvalState_<double>> compiledState_;
            LsmcSinks_ sinks_;
            size_t exercisedDay_ = NO_SLOT;
            double preExercise_ = 0.0;
            double exerciseValue_ = 0.0;

            explicit ThreadState_(const struct LsmcContext_& ctx);

            [[nodiscard]] const Scenario_<>& Path() const { return bsPaths_ ? bsPaths_->Path() : path_; }
            [[nodiscard]] PricingObservation_ PricingObservation() const {
                if (compiledState_)
                    return {sinks_.pricingX_, sinks_.pricingH_, sinks_.pricingCond_};
                return {evaluator_.pricingX_, evaluator_.pricingH_, evaluator_.pricingCond_};
            }
        };

        //  Immutable per-run references shared by the three phases
        struct LsmcContext_ {
            const PreparedScript_& prepared_;
            AAD::Model_<double>* model_;
            const LsmcPlan_& scan_;
            LsmcStorage_& storage_;
            const ScriptCompiled_* compiled_ = nullptr;            //  shared immutable program for every worker
            const Vector_<ExerciseRegression_>* policy_ = nullptr; //  pricing stops evaluating after the first hard exercise

            const ScriptProduct_& Product() const { return prepared_.Product(); }
            const ObservationPlan_& Plan() const { return prepared_.Plan(); }
            [[nodiscard]] size_t PayOffIdx() const { return Product().PayOffIdx(); }
        };

        ThreadState_::ThreadState_(const LsmcContext_& ctx)
            : random_(CreateRNG(ctx.prepared_.Simulation().rsg_, ctx.model_->SimDim(), ctx.prepared_.Simulation().useBb_)),
              gauss_(ctx.model_->SimDim()), evaluator_(ctx.Product().VarValues(), ctx.Product().ConstVarValues()) {
            if (typeid(*ctx.model_) == typeid(AAD::BlackScholes_<double>))
                bsPaths_ = std::make_unique<Detail::LocalCheckedPaths_>(static_cast<const AAD::BlackScholes_<double>&>(*ctx.model_));
            else {
                AllocatePath(ctx.Plan().DefLine(), path_);
                InitializePath(path_);
            }
            evaluator_.eventToPays_ = &ctx.scan_.eventToPays_;
            evaluator_.payoffIdx_ = ctx.PayOffIdx();
            evaluator_.eventToExercise_ = &ctx.scan_.eventToExercise_;
            evaluator_.paysStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.pays_;
            evaluator_.xStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.xByDay_;
            evaluator_.hStorage_ = ctx.policy_ ? nullptr : &ctx.storage_.hByDay_;
            evaluator_.condStorage_ = ctx.policy_ || ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
            if (ctx.compiled_) {
                compiledState_.emplace(ctx.prepared_.BuildEvalState<double>());
                sinks_.eventToPays_ = &ctx.scan_.eventToPays_;
                sinks_.payoffIdx_ = ctx.PayOffIdx();
                sinks_.eventToExercise_ = &ctx.scan_.eventToExercise_;
                sinks_.pays_ = ctx.policy_ ? nullptr : &ctx.storage_.pays_;
                sinks_.x_ = ctx.policy_ ? nullptr : &ctx.storage_.xByDay_;
                sinks_.h_ = ctx.policy_ ? nullptr : &ctx.storage_.hByDay_;
                sinks_.cond_ = ctx.policy_ || ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
                compiledState_->lsmcSinks_ = &sinks_;
            }
        }

        //  S4: exercise replaces same-day and later payments only, so the payoff
        //  accumulated strictly before the exercise event survives (both engines)
        void SnapshotPreExercise(const LsmcContext_& ctx, const Vector_<>& variables, size_t event, ThreadState_* state) {
            const size_t slot = ctx.scan_.eventToExercise_[event];
            if (slot != NO_SLOT && ctx.policy_)
                state->preExercise_ = ctx.Product().HasPays() ? variables[ctx.PayOffIdx()] : 0.0;
        }

        bool StopAfterEvent(const LsmcContext_& ctx, size_t event, ThreadState_* state) {
            const size_t day = ctx.scan_.eventToExercise_[event];
            if (!ctx.policy_ || day == NO_SLOT)
                return false;
            const auto observation = state->PricingObservation();
            if (!(observation.cond_ && observation.h_ > 0.0 && observation.h_ > RegressionPredict((*ctx.policy_)[day], observation.x_)))
                return false;
            state->exercisedDay_ = day;
            state->exerciseValue_ = observation.h_;
            return true;
        }

        void TreeEvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            auto& eval = state.evaluator_;
            eval.pathSlot_ = pathSlot;
            eval.SetScenario(&state.Path());
            eval.SetObservations(&ctx.Plan());
            eval.Init();
            const auto& events = ctx.Product().Events();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                SnapshotPreExercise(ctx, eval.VarVals(), e, &state);
                eval.SetCurEvt(eventToSample[e]);
                eval.SetEventOrdinal(e);
                for (const auto& statement : events[e])
                    statement->Accept(eval);
                if (StopAfterEvent(ctx, e, &state))
                    break;
            }
        }

        //  Compiled engine mirror: one event stream per event, the driver installs the
        //  sinks and owns the event boundaries; EXERCISE is recording-only on the state
        void CompiledEvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            const ScriptCompiled_& compiled = *ctx.compiled_;
            auto& eval = *state.compiledState_;
            auto& sinks = state.sinks_;
            sinks.pathSlot_ = pathSlot;
            eval.Init();
            eval.observations_ = &ctx.Plan();
            eval.scenario_ = &state.Path();
            const auto& nodeStreams = compiled.NodeStreams();
            const auto& constStreams = compiled.ConstStreams();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < nodeStreams.size(); ++e) {
                sinks.eventOrdinal_ = e;
                SnapshotPreExercise(ctx, eval.variables_, e, &state);
                const Detail::CompiledEventView_<double> view{nodeStreams[e], constStreams[e], state.Path()[eventToSample[e]]};
                Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &eval);
                if (StopAfterEvent(ctx, e, &state))
                    break;
            }
        }

        //  One forward evaluation: training records regression inputs, while hard
        //  pricing retains only the current decision and stops at first exercise.
        void EvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            state.exercisedDay_ = NO_SLOT;
            state.preExercise_ = 0.0;
            state.exerciseValue_ = 0.0;
            state.random_->FillNormal(&state.gauss_);
            if (state.bsPaths_) {
                if (!state.bsPaths_->Generate(state.gauss_))
                    Detail::DiagnoseInvalidSimulationPath(state.bsPaths_->Path());
            } else {
                ctx.model_->GeneratePath(state.gauss_, &state.path_);
                ValidateSimulationPath(state.path_);
            }
            if (state.compiledState_)
                CompiledEvaluateRecordedPath(state, ctx, pathSlot);
            else
                TreeEvaluateRecordedPath(state, ctx, pathSlot);
        }

        //  Phase A: forward storage over disjoint per-batch path slots
        void RunForwardPhase(LsmcContext_& ctx, const BatchPlan_& batchPlan, size_t pathOffset = 0) {
            ThreadPool_* pool = ThreadPool_::GetInstance();
            Vector_<std::unique_ptr<ThreadState_>> threadStates(pool->NumThreads());
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch]() {
                    auto& local = threadStates[ThreadPool_::ThreadNum()];
                    if (!local)
                        local = std::make_unique<ThreadState_>(ctx);
                    ThreadState_& state = *local;
                    state.random_->SkipTo(pathOffset + batch.firstPath_);
                    for (size_t i = 0; i < batch.pathCount_; ++i)
                        EvaluateRecordedPath(state, ctx, batch.firstPath_ + i);
                    return true;
                });
            }
            tasks.Complete();
        }

        //  N5: the backward discounting ratios anchor on one probe path's numeraires;
        //  Black-Scholes and Dupire, the only models the factory constructs, carry
        //  deterministic rates, so the numeraire is path-independent and any path
        //  pins the same ratios. The entry point debug-asserts that model set, and the
        //  probe goes through the same validation as every worker path
        Vector_<> SampleGridNumeraires(const LsmcContext_& ctx) {
            const auto& simulation = ctx.prepared_.Simulation();
            const auto& events = ctx.Product().Events();
            auto probe = CreateRNG(simulation.rsg_, ctx.model_->SimDim(), simulation.useBb_);
            Vector_<> gauss(ctx.model_->SimDim());
            Scenario_<> path;
            AllocatePath(ctx.Plan().DefLine(), path);
            probe->SkipTo(0);
            probe->FillNormal(&gauss);
            ctx.model_->GeneratePath(gauss, &path);
            ValidateSimulationPath(path);
            Vector_<> eventNumeraire(events.size(), 1.0);
            for (size_t e = 0; e < events.size(); ++e)
                eventNumeraire[e] = path[ctx.Plan().EventToSample()[e]].numeraire_;
            return eventNumeraire;
        }

        //  Holding value of one event in date-i units: H = p + D * W (S3), W := H in place
        void InductBackward(const Vector_<Vector_<>>& pays, size_t paysSlot, double dNext, bool hasNext, Vector_<>* w) {
            for (size_t j = 0; j < w->size(); ++j)
                (*w)[j] = (paysSlot == NO_SLOT ? 0.0 : pays[paysSlot][j]) + (hasNext ? dNext * (*w)[j] : 0.0);
        }

        //  Longstaff-Schwartz: the regression set is the in-the-money condition-true
        //  paths (h > 0); deep-OTM paths carry no exercise information, and a
        //  continuation estimate that undershoots below zero must not "exercise" a
        //  worthless option on them
        void FillIncluded(const Vector_<Vector_<char>>& condByDay, const Vector_<>& h, size_t day, Vector_<char>* included) {
            const Vector_<char>* cond = condByDay.empty() || condByDay[day].empty() ? nullptr : &condByDay[day];
            for (size_t j = 0; j < included->size(); ++j)
                (*included)[j] = (!cond || (*cond)[j]) && h[j] > 0.0 ? 1 : 0;
        }

        //  S3/S4: exercise on strictly-better continuation estimates replaces the future
        void
        ApplyExerciseDecisions(const Vector_<>& h, const Vector_<>& x, const Vector_<char>& included, const ExerciseRegression_& c, Vector_<>* w) {
            for (size_t j = 0; j < w->size(); ++j)
                if (included[j] && h[j] > RegressionPredict(c, x[j]))
                    (*w)[j] = h[j];
        }

        struct RegressionRows_ {
            const Vector_<>& x_;
            const Vector_<>& targets_;
            const Vector_<char>& included_;
        };

        struct ValidationLoss_ {
            double mse_ = std::numeric_limits<double>::infinity();
            double standardError_ = 0.0;
        };

        ValidationLoss_ EvaluateValidationLoss(const ExerciseRegression_& candidate, const RegressionRows_& validation, size_t count) {
            double sumLoss = 0.0;
            double sumLossSq = 0.0;
            for (size_t i = 0; i < validation.x_.size(); ++i) {
                if (!validation.included_[i])
                    continue;
                const double error = RegressionPredict(candidate, validation.x_[i]) - validation.targets_[i];
                const double loss = error * error;
                sumLoss += loss;
                sumLossSq += loss * loss;
            }
            const double mse = sumLoss / static_cast<double>(count);
            const double secondMoment = sumLossSq / static_cast<double>(count);
            return {mse, std::sqrt(std::max(0.0, secondMoment - mse * mse) / static_cast<double>(count))};
        }

        int ChooseValidationDegree(const std::array<ValidationLoss_, 8>& losses, int maxDegree, const ValidationLoss_& best) {
            if (!std::isfinite(best.mse_))
                return 0;
            const double threshold = best.mse_ + best.standardError_ + 1e-10 * std::max(1.0, best.mse_);
            for (int degree = 1; degree <= maxDegree; ++degree)
                if (std::isfinite(losses[static_cast<size_t>(degree - 1)].mse_) && losses[static_cast<size_t>(degree - 1)].mse_ <= threshold)
                    return degree - 1;
            return 0;
        }

        ExerciseRegression_ SelectRegression(const RegressionRows_& training, const RegressionRows_& validation, int maxDegree) {
            const size_t validationCount = static_cast<size_t>(std::count(validation.included_.begin(), validation.included_.end(), 1));
            if (validationCount == 0)
                return SolveExerciseRegression(training.x_, training.targets_, training.included_, 1);

            std::array<ExerciseRegression_, 8> candidates;
            std::array<ValidationLoss_, 8> losses;
            ValidationLoss_ best;
            for (int degree = 1; degree <= maxDegree; ++degree) {
                auto& candidate = candidates[static_cast<size_t>(degree - 1)];
                candidate = SolveExerciseRegression(training.x_, training.targets_, training.included_, degree);
                auto& loss = losses[static_cast<size_t>(degree - 1)];
                loss = EvaluateValidationLoss(candidate, validation, validationCount);
                if (std::isfinite(loss.mse_) && loss.mse_ < best.mse_)
                    best = loss;
            }
            const int chosen = ChooseValidationDegree(losses, maxDegree, best);
            auto selected = std::move(candidates[static_cast<size_t>(chosen)]);
            if (std::isfinite(best.mse_))
                selected.validationMse_ = losses[static_cast<size_t>(chosen)].mse_;
            return selected;
        }

        //  Phase B: backward induction and continuation regressions, single threaded in
        //  global path order (thread-count independent by construction, N9/N10)
        Vector_<ExerciseRegression_> RunBackwardPhase(const LsmcContext_& ctx,
                                                      const Vector_<>& eventNumeraire,
                                                      size_t nPaths,
                                                      int degree,
                                                      const LsmcStorage_* validationStorage = nullptr,
                                                      size_t nValidation = 0) {
            const auto& scan = ctx.scan_;
            const auto& storage = ctx.storage_;
            const auto& events = ctx.Product().Events();
            Vector_<> w(nPaths, 0.0);
            Vector_<ExerciseRegression_> regressions(scan.days_.size());
            Vector_<char> included(nPaths, 1);
            Vector_<> validationW(nValidation, 0.0);
            Vector_<char> validationIncluded(nValidation, 1);
            for (size_t ei = events.size(); ei-- > 0;) {
                const bool hasNext = ei + 1 < events.size();
                const double dNext = hasNext ? eventNumeraire[ei] / eventNumeraire[ei + 1] : 1.0;
                InductBackward(storage.pays_, scan.eventToPays_[ei], dNext, hasNext, &w);
                if (validationStorage)
                    InductBackward(validationStorage->pays_, scan.eventToPays_[ei], dNext, hasNext, &validationW);

                const size_t day = scan.eventToExercise_[ei];
                if (day == NO_SLOT)
                    continue;
                FillIncluded(storage.condByDay_, storage.hByDay_[day], day, &included);
                if (validationStorage) {
                    FillIncluded(validationStorage->condByDay_, validationStorage->hByDay_[day], day, &validationIncluded);
                    regressions[day] = SelectRegression({storage.xByDay_[day], w, included},
                                                        {validationStorage->xByDay_[day], validationW, validationIncluded}, degree);
                } else {
                    regressions[day] = SolveExerciseRegression(storage.xByDay_[day], w, included, degree);
                }
                ApplyExerciseDecisions(storage.hByDay_[day], storage.xByDay_[day], included, regressions[day], &w);
                if (validationStorage)
                    ApplyExerciseDecisions(validationStorage->hByDay_[day], validationStorage->xByDay_[day], validationIncluded, regressions[day],
                                           &validationW);
            }
            return regressions;
        }

        //  First exercise wins (S3/S4): the earliest true decision replaces the payoff
        double PathPayoff(const LsmcContext_& ctx, const Vector_<>& eventNumeraire, const ThreadState_& state, Vector_<size_t>* exerciseCounts) {
            if (state.exercisedDay_ != NO_SLOT) {
                //  The event loop already selected the first exercise. Reusing it
                //  avoids a second scan and a second polynomial evaluation per date.
                const double payoff = state.preExercise_ + state.exerciseValue_ / eventNumeraire[ctx.scan_.days_[state.exercisedDay_].eventId_];
                REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite exercise value", ScriptError_);
                ++(*exerciseCounts)[state.exercisedDay_];
                return payoff;
            }
            if (!ctx.Product().HasPays())
                return 0.0;
            const double payoff = (state.compiledState_ ? state.compiledState_->VarVals() : state.evaluator_.VarVals())[ctx.PayOffIdx()];
            REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite path value", ScriptError_);
            return payoff;
        }

        struct ReplayOutcome_ {
            double sum_ = 0.0;
            double sumSq_ = 0.0;
            Vector_<size_t> exerciseCounts_;
        };

        struct ReplayWorkspace_ {
            LsmcStorage_ storage_;
            LsmcContext_ pricing_;
            ThreadState_ state_;

            ReplayWorkspace_(const LsmcContext_& ctx, const Vector_<ExerciseRegression_>& regressions)
                : pricing_{ctx.prepared_, ctx.model_, ctx.scan_, storage_, ctx.compiled_, &regressions}, state_(pricing_) {}
        };

        //  Phase C evaluates the frozen strategy on a disjoint Sobol block. Each
        //  worker reuses its evaluator and scalar state; batch-index reduction
        //  preserves thread invariance.
        ReplayOutcome_ RunReplayPhase(const LsmcContext_& ctx,
                                      const Vector_<>& eventNumeraire,
                                      const BatchPlan_& batchPlan,
                                      size_t pricingPathOffset,
                                      const Vector_<ExerciseRegression_>& regressions) {
            Vector_<ReplayOutcome_> outcomes(batchPlan.BatchCount());
            for (auto& outcome : outcomes)
                outcome.exerciseCounts_ = Vector_<size_t>(ctx.scan_.days_.size(), 0);

            ThreadPool_* pool = ThreadPool_::GetInstance();
            Vector_<std::unique_ptr<ReplayWorkspace_>> workspaces(pool->NumThreads());
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch, batchIndex]() {
                    ReplayOutcome_& outcome = outcomes[batchIndex];
                    auto& local = workspaces[ThreadPool_::ThreadNum()];
                    if (!local)
                        local = std::make_unique<ReplayWorkspace_>(ctx, regressions);
                    auto& pricing = local->pricing_;
                    auto& state = local->state_;
                    state.random_->SkipTo(pricingPathOffset + batch.firstPath_);
                    for (size_t i = 0; i < batch.pathCount_; ++i) {
                        EvaluateRecordedPath(state, pricing, 0);
                        const double payoff = PathPayoff(pricing, eventNumeraire, state, &outcome.exerciseCounts_);
                        outcome.sum_ += payoff;
                        outcome.sumSq_ += payoff * payoff;
                    }
                    return true;
                });
            }
            tasks.Complete();

            //  N9: reduce the batch blocks in batch-index order
            ReplayOutcome_ reduction{0.0, 0.0, Vector_<size_t>(ctx.scan_.days_.size(), 0)};
            for (const auto& outcome : outcomes) {
                reduction.sum_ += outcome.sum_;
                reduction.sumSq_ += outcome.sumSq_;
                for (size_t k = 0; k < reduction.exerciseCounts_.size(); ++k)
                    reduction.exerciseCounts_[k] += outcome.exerciseCounts_[k];
            }
            return reduction;
        }

        void FillDiagnostics(LsmcDiagnostics_* diagnostics,
                             const PreparedScript_& prepared,
                             const LsmcPlan_& scan,
                             const Vector_<ExerciseRegression_>& regressions,
                             const Vector_<size_t>& exerciseCounts,
                             const ReplayOutcome_& reduction,
                             size_t nPaths) {
            diagnostics->events_.clear();
            diagnostics->payoffSum_ = reduction.sum_;
            diagnostics->payoffSumSq_ = reduction.sumSq_;
            diagnostics->nPaths_ = nPaths;
            const auto& bindings = prepared.Plan().ModelBindingNames();
            for (size_t k = 0; k < scan.days_.size(); ++k) {
                ExerciseEventStats_ stats;
                stats.eventId_ = scan.days_[k].eventId_;
                stats.date_ = prepared.Product().EventDates()[scan.days_[k].eventId_];
                stats.requestedDegree_ = prepared.Simulation().lsmcBasisDegree_;
                stats.basisDegree_ = regressions[k].basisDegree_;
                stats.regressorIndex_ = bindings.empty() ? String_() : bindings.front();
                stats.numCondTruePaths_ = regressions[k].numCondTrue_;
                stats.coefficients_ = regressions[k].coefficients_;
                stats.degenerate_ = regressions[k].degenerate_;
                stats.degenerateReason_ = regressions[k].degenerateReason_;
                stats.effectiveRank_ = regressions[k].effectiveRank_;
                stats.solver_ = regressions[k].solver_;
                stats.fallbackReason_ = regressions[k].fallbackReason_;
                stats.validationMse_ = regressions[k].validationMse_;
                stats.exerciseRate_ = static_cast<double>(exerciseCounts[k]) / static_cast<double>(nPaths);
                diagnostics->events_.push_back(stats);
            }
        }

        //  S9 recursive blend, live on the worker's tape: walks the recorded per-path rows
        //  backward, d_k = CSpr(h_k - C_k(z_k), eps) * CSpr(h_k, 0, eps) * condition degree
        //  — the fuzzy-AND of h_k > C_k with the h_k > 0 exercise gate (the one-sided ramp
        //  puts degree 0 on the h == 0 atom, matching the hard mode) — blending each fuzzy
        //  decision into the continuation; the explicit last step discounts to the
        //  evaluation date through the first event's numeraire (N5)
        template <class T_>
        T_ FuzzyPathValue(const LsmcPlan_& scan,
                          const Vector_<T_>& pays,
                          const Vector_<T_>& h,
                          const Vector_<T_>& cond,
                          const Vector_<ExerciseRegression_>& regressions,
                          const Vector_<size_t>& eventToSample,
                          const Scenario_<T_>& path) {
            T_ value(0.0);
            if (eventToSample.empty())
                return value;
            for (size_t e = scan.eventToExercise_.size(); e-- > 0;) {
                if (e + 1 < scan.eventToExercise_.size())
                    value *= path[eventToSample[e]].numeraire_ / path[eventToSample[e + 1]].numeraire_;
                value += pays[e];
                const size_t day = scan.eventToExercise_[e];
                if (day != NO_SLOT) {
                    //  materialize the continuation gap: CSpr's early-return constants need a
                    //  value type, not an expression proxy
                    const T_ continuation = RegressionPredict(regressions[day], path[eventToSample[e]].spot_);
                    const T_ gap = h[day] - continuation;
                    const T_ degree = CSpr(gap, scan.days_[day].eps_) * CSpr(h[day], 0.0, scan.days_[day].eps_) * cond[day];
                    value = degree * h[day] + (1.0 - degree) * value;
                }
            }
            return value / path[eventToSample.front()].numeraire_;
        }

        //  Phase C (fuzzy): raw adjoint sums per batch slot, reduced in batch-index order (N9)
        struct AadReplayOutcome_ {
            double sum_ = 0.0;
            Vector_<> risks_;
        };

        //  Per-worker replay state: active model, regenerated paths, and the per-path
        //  recording rows (payments reset between paths, h/cond overwritten per statement)
        struct FuzzyReplayWorkspace_ {
            std::unique_ptr<AAD::Model_<AAD::Number_>> model_;
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<AAD::Number_> path_;
            LsmcFuzzySinks_<AAD::Number_> sinks_;
            Vector_<AAD::Number_> pays_;
            Vector_<AAD::Number_> h_;
            Vector_<AAD::Number_> cond_;
        };

        FuzzyReplayWorkspace_ MakeFuzzyReplayWorkspace(const PreparedScript_& prepared,
                                                       const Handle_<ModelData_>& modelData,
                                                       const LsmcPlan_& scan,
                                                       const PathBatch_& batch) {
            const auto& simulation = prepared.Simulation();
            FuzzyReplayWorkspace_ ws;
            ws.model_ = CreateModel<AAD::Number_>(modelData);
            ws.model_->Allocate(prepared.TimeLine(), prepared.DefLine());
            ws.random_ = CreateRNG(simulation.rsg_, ws.model_->SimDim(), simulation.useBb_);
            ws.gauss_.Resize(ws.model_->SimDim());
            AllocatePath(prepared.DefLine(), ws.path_);
            InitializePath(ws.path_);
            if (ws.random_)
                ws.random_->SkipTo(batch.firstPath_);
            ws.sinks_.eventToExercise_ = &scan.eventToExercise_;
            ws.sinks_.payoffIdx_ = prepared.PayOffIdx();
            ws.pays_ = Vector_<AAD::Number_>(scan.eventToPays_.size(), 0.0);
            ws.h_ = Vector_<AAD::Number_>(scan.days_.size(), 0.0);
            ws.cond_ = Vector_<AAD::Number_>(scan.days_.size(), 1.0);
            return ws;
        }

        //  Mirrors the double recorder: the driver owns the event boundaries and the sinks'
        //  event ordinal while the fuzzy evaluator records through its installed sinks
        void TreeEvaluateFuzzyPath(FuzzyReplayWorkspace_& ws, const PreparedScript_& prepared, FuzzyEvaluator_<AAD::Number_>& evaluator) {
            evaluator.lsmcFuzzySinks_ = &ws.sinks_;
            evaluator.SetScenario(&ws.path_);
            evaluator.SetObservations(&prepared.Plan());
            evaluator.Init();
            const auto& events = prepared.Product().Events();
            const auto& eventToSample = prepared.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                ws.sinks_.eventOrdinal_ = e;
                evaluator.SetCurEvt(eventToSample[e]);
                for (const auto& statement : events[e])
                    statement->Accept(evaluator);
            }
        }

        void CompiledEvaluateFuzzyPath(FuzzyReplayWorkspace_& ws,
                                       const PreparedScript_& prepared,
                                       const ScriptCompiled_& compiled,
                                       EvalState_<AAD::Number_>& state) {
            state.lsmcFuzzySinks_ = &ws.sinks_;
            state.Init();
            state.observations_ = &prepared.Plan();
            state.scenario_ = &ws.path_;
            const auto& nodeStreams = compiled.NodeStreams();
            const auto& constStreams = compiled.ConstStreams();
            const auto& eventToSample = prepared.Plan().EventToSample();
            for (size_t e = 0; e < nodeStreams.size(); ++e) {
                ws.sinks_.eventOrdinal_ = e;
                const Detail::CompiledEventView_<AAD::Number_> view{nodeStreams[e], constStreams[e], ws.path_[eventToSample[e]]};
                Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &state);
            }
        }

        template <class E_, class F_>
        void FuzzyReplayPaths(const PreparedScript_& prepared,
                              const LsmcPlan_& scan,
                              const Vector_<ExerciseRegression_>& regressions,
                              const PathBatch_& batch,
                              FuzzyReplayWorkspace_& ws,
                              E_& evaluator,
                              const F_& evaluate,
                              AadReplayOutcome_* outcome) {
            InitModel4ParallelAAD(prepared, *ws.model_, ws.path_, evaluator, nullptr);
            for (size_t i = 0; i < batch.pathCount_; ++i) {
                AAD::RewindToMark(*AAD::Tape());
                for (auto& payment : ws.pays_)
                    payment = 0.0;
                if (ws.random_)
                    ws.random_->FillNormal(&ws.gauss_);
                ws.model_->GeneratePath(ws.gauss_, &ws.path_);
                ValidateSimulationPath(ws.path_);
                evaluate(ws, prepared, evaluator);
                AAD::Number_ value = FuzzyPathValue(scan, ws.pays_, ws.h_, ws.cond_, regressions, prepared.Plan().EventToSample(), ws.path_);
                REQUIRE2(std::isfinite(Value(value)), "InvalidPayoff: non-finite path value", ScriptError_);
                Adjoint(value) = 1.0;
                AAD::PropagateToMark(*AAD::Tape());
                outcome->sum_ += Value(value);
            }
            AAD::PropagateMarkToStart(*AAD::Tape());
            size_t j = 0;
            for (const auto* parameter : ws.model_->Parameters())
                outcome->risks_[j++] += Adjoint(*parameter);
            for (const auto& constVar : evaluator.ConstVarVals())
                outcome->risks_[j++] += Adjoint(constVar);
        }

        void RunFuzzyReplayBatch(const PreparedScript_& prepared,
                                 const Handle_<ModelData_>& modelData,
                                 const LsmcPlan_& scan,
                                 const Vector_<ExerciseRegression_>& regressions,
                                 const ScriptCompiled_* fuzzyCompiled,
                                 const PathBatch_& batch,
                                 AadReplayOutcome_* outcome) {
            AAD::Activate(*AAD::Tape());
            AAD::Rewind(*AAD::Tape());
            FuzzyReplayWorkspace_ ws = MakeFuzzyReplayWorkspace(prepared, modelData, scan, batch);
            //  Bind after the return-by-value, without relying on optional NRVO.
            ws.sinks_.pays_ = &ws.pays_;
            ws.sinks_.h_ = &ws.h_;
            ws.sinks_.cond_ = &ws.cond_;
            if (fuzzyCompiled) {
                EvalState_<AAD::Number_> state = prepared.BuildEvalState<AAD::Number_>(0, prepared.Simulation().smooth_);
                FuzzyReplayPaths(
                    prepared, scan, regressions, batch, ws, state,
                    [fuzzyCompiled](FuzzyReplayWorkspace_& w, const PreparedScript_& p, EvalState_<AAD::Number_>& s) {
                        CompiledEvaluateFuzzyPath(w, p, *fuzzyCompiled, s);
                    },
                    outcome);
            } else {
                FuzzyEvaluator_<AAD::Number_> evaluator = prepared.BuildFuzzyEvaluator<AAD::Number_>(0, prepared.Simulation().smooth_);
                FuzzyReplayPaths(prepared, scan, regressions, batch, ws, evaluator, TreeEvaluateFuzzyPath, outcome);
            }
        }
    } // namespace

    ExerciseRegression_ SolveExerciseRegression(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree) {
        ValidateLsmcBasisDegree(degree);
        REQUIRE(x.size() == targets.size() && x.size() == included.size(), "InvalidRegressionInput: mismatched regression vectors");

        ExerciseRegression_ result;
        const auto stats = RegressionStats(x, included);
        result.numCondTrue_ = stats.count_;
        result.mean_ = stats.mean_;
        result.sigma_ = std::max(stats.sigma_, stats.sigmaFloor_);
        const double constantFit = MeanOf(targets, included, stats.count_);

        //  Guard order: sample size, then sigma floor, then Gram conditioning
        if (stats.count_ == 0) {
            DegradeToConstant(&result, "ConditionPathsBelowMin", 0.0);
            return result;
        }
        if (stats.sigma_ < stats.sigmaFloor_) {
            DegradeToConstant(&result, "SigmaFloor", constantFit);
            return result;
        }
        if (stats.count_ < PATHS_PER_BASIS_FUNCTION * static_cast<size_t>(degree + 1)) {
            DegradeToConstant(&result, "ConditionPathsBelowMin", constantFit);
            return result;
        }

        const size_t nBasis = static_cast<size_t>(degree) + 1;
        SquareMatrix_<> gram(static_cast<int>(nBasis), 0.0);
        Vector_<> rhs(nBasis, 0.0);
        AccumulateNormalEquations(x, targets, included, result.mean_, result.sigma_, &gram, &rhs);
        if (const char* fallbackReason = RidgeAndIllConditioned(&gram)) {
            result.solver_ = "PivotedQR";
            result.fallbackReason_ = fallbackReason;
            int fitDegree = degree;
            size_t effectiveRank = nBasis;
            while (fitDegree > 0) {
                Vector_<> coefficients;
                const size_t rank = PivotedQrFit(x, targets, included, result.mean_, result.sigma_, fitDegree, &coefficients);
                effectiveRank = std::min(effectiveRank, rank);
                if (rank == static_cast<size_t>(fitDegree + 1)) {
                    const bool finite = std::all_of(coefficients.begin(), coefficients.end(), [](double c) { return std::isfinite(c); });
                    if (finite) {
                        result.coefficients_ = std::move(coefficients);
                        result.basisDegree_ = fitDegree;
                        result.effectiveRank_ = effectiveRank;
                        if (fitDegree < degree)
                            result.fallbackReason_ = "RankDeficient";
                        return result;
                    }
                    break;
                }
                fitDegree = std::min(fitDegree - 1, static_cast<int>(rank) - 1);
            }
            DegradeToConstant(&result, "IllConditioned", constantFit);
            return result;
        }

        Vector_<Vector_<>> rhsWrapped(1);
        rhsWrapped[0] = rhs;
        CholeskySolve(&gram, &rhsWrapped);
        for (double coefficient : rhsWrapped[0])
            if (!std::isfinite(coefficient)) {
                DegradeToConstant(&result, "IllConditioned", constantFit);
                return result;
            }
        result.coefficients_ = rhsWrapped[0];
        result.basisDegree_ = degree;
        result.effectiveRank_ = nBasis;
        result.solver_ = "MomentsCholesky";
        return result;
    }

    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics) {
        const auto& product = prepared.Product();
        const auto& simulation = prepared.Simulation();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        REQUIRE2(!simulation.enableAad_,
                 "UnsupportedExecutionMode: the double LSMC driver values hard decisions only; AAD products route to the fuzzy driver", ScriptError_);
        //  N5: the probe-path discount ratios are path-independent only for
        //  deterministic-rate models; the model base class exposes no rate-kind
        //  query, so the factory's two models are pinned here unconditionally --
        //  a stochastic-rate model would otherwise be silently mis-discounted
        REQUIRE2(typeid(*mdl) == typeid(AAD::BlackScholes_<double>) || typeid(*mdl) == typeid(AAD::Dupire_<double>),
                 "UnsupportedModel: LSMC requires a deterministic-rate model (BlackScholes or Dupire)", ScriptError_);

        const auto scan = ScanEvents(product.Events(), simulation.smooth_);
        const auto counts = LsmcPathCounts(simulation, nPaths);
        auto storage = MakeStorage(scan, counts.training_);
        LsmcStorage_ validationStorage;
        //  Each phase has its own count-specific, thread-independent batch layout.
        const BatchPlan_ trainingPlan(counts.training_, 1);
        const BatchPlan_ batchPlan(nPaths, 1);
        const ScriptCompiled_* compiled = simulation.compiled_.value_or(false) ? &prepared.CompiledProgram() : nullptr;
        LsmcContext_ ctx{prepared, mdl, scan, storage, compiled};
        RunForwardPhase(ctx, trainingPlan);
        const Vector_<> eventNumeraire = SampleGridNumeraires(ctx);
        if (counts.validation_) {
            validationStorage = MakeStorage(scan, counts.validation_);
            LsmcContext_ validationCtx{prepared, mdl, scan, validationStorage, compiled};
            RunForwardPhase(validationCtx, BatchPlan_(counts.validation_, 1), counts.training_);
        }
        const auto regressions = RunBackwardPhase(ctx, eventNumeraire, counts.training_, simulation.lsmcBasisDegree_,
                                                  counts.validation_ ? &validationStorage : nullptr, counts.validation_);
        storage = LsmcStorage_();
        validationStorage = LsmcStorage_();
        const auto reduction = RunReplayPhase(ctx, eventNumeraire, batchPlan, counts.pricingOffset_, regressions);

        SimResults_ results(Vector::Join(mdl->ParameterLabels(), product.ConstVarNames()));
        results.aggregated_ = reduction.sum_;
        if (diagnostics)
            FillDiagnostics(diagnostics, prepared, scan, regressions, reduction.exerciseCounts_, reduction, nPaths);
        return results;
    }

    SimResults_ MCLsmcAadSimulation(const PreparedScript_& prepared, const Handle_<ModelData_>& modelData, size_t nPaths) {
        const auto& product = prepared.Product();
        const auto& simulation = prepared.Simulation();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        REQUIRE2(simulation.enableAad_, "UnsupportedExecutionMode: the fuzzy LSMC driver requires AAD preparation", ScriptError_);

        //  Phases A/B run exactly as the double driver so the frozen policy is the
        //  thread-count independent hard-decision artifact; the recording stream
        //  therefore lowers hard even though the replay itself is fuzzy
        auto doubleModel = CreateModel<double>(modelData);
        doubleModel->Allocate(prepared.TimeLine(), prepared.DefLine());
        doubleModel->Init(prepared.TimeLine(), prepared.DefLine());

        const auto scan = ScanEvents(product.Events(), simulation.smooth_);
        const auto counts = LsmcPathCounts(simulation, nPaths);
        auto storage = MakeStorage(scan, counts.training_);
        LsmcStorage_ validationStorage;
        //  Each phase has its own count-specific, thread-independent batch layout.
        const BatchPlan_ trainingPlan(counts.training_, 1);
        const BatchPlan_ batchPlan(nPaths, 1);
        ThreadPool_* pool = ThreadPool_::GetInstance();
        std::optional<ScriptCompiled_> hardCompiled;
        if (simulation.compiled_.value_or(false))
            hardCompiled.emplace(ScriptCompiled_::Build(product.Events(), false, prepared.PlanHandle(), false, true));
        LsmcContext_ ctx{prepared, doubleModel.get(), scan, storage, hardCompiled ? &*hardCompiled : nullptr};

        RunForwardPhase(ctx, trainingPlan);
        const Vector_<> eventNumeraire = SampleGridNumeraires(ctx);
        if (counts.validation_) {
            validationStorage = MakeStorage(scan, counts.validation_);
            LsmcContext_ validationCtx{prepared, doubleModel.get(), scan, validationStorage, hardCompiled ? &*hardCompiled : nullptr};
            RunForwardPhase(validationCtx, BatchPlan_(counts.validation_, 1), counts.training_);
        }
        const auto regressions = RunBackwardPhase(ctx, eventNumeraire, counts.training_, simulation.lsmcBasisDegree_,
                                                  counts.validation_ ? &validationStorage : nullptr, counts.validation_);
        storage = LsmcStorage_();
        validationStorage = LsmcStorage_();

        const ScriptCompiled_* fuzzyCompiled = simulation.compiled_.value_or(false) ? &prepared.CompiledProgram(true) : nullptr;
        Vector_<AadReplayOutcome_> outcomes(batchPlan.BatchCount());
        for (auto& outcome : outcomes)
            outcome.risks_ = Vector_<>(doubleModel->ParameterLabels().size() + product.ConstVarNames().size(), 0.0);

        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            tasks.Spawn([&, batch, batchIndex]() {
                const PathBatch_ pricingBatch{counts.pricingOffset_ + batch.firstPath_, batch.pathCount_};
                RunFuzzyReplayBatch(prepared, modelData, scan, regressions, fuzzyCompiled, pricingBatch, &outcomes[batchIndex]);
                return true;
            });
        }
        tasks.Complete();

        //  N9: reduce the batch blocks in batch-index order, one final division by nPaths
        SimResults_ results(Vector::Join(doubleModel->ParameterLabels(), product.ConstVarNames()));
        for (const auto& outcome : outcomes)
            results.aggregated_ += outcome.sum_;
        for (size_t j = 0; j < results.risks_.size(); ++j) {
            double total = 0.0;
            for (const auto& outcome : outcomes)
                total += outcome.risks_[j];
            results.risks_[j] = total / static_cast<double>(nPaths);
        }
        return results;
    }
} // namespace Dal::Script
