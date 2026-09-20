//
// Created by dal-implementer on 2026/9/20.
//

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
        //  N3: Gram diagonal ratio above which the day degrades to the constant basis
        constexpr double CONDITION_LIMIT = 1e12;

        double MeanOf(const Vector_<>& values, const Vector_<char>& included, size_t count) {
            if (count == 0)
                return 0.0;
            double sum = 0.0;
            for (size_t i = 0; i < values.size(); ++i)
                if (included[i])
                    sum += values[i];
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
            }
            //  the floor engages even on an empty regression set, so a degenerate day
            //  never carries sigma_ = 0 into RegressionPredict's z-normalization
            stats.sigmaFloor_ = SIGMA_FLOOR_SCALE * std::max(1.0, std::abs(stats.mean_));
            return stats;
        }

        void DegradeToConstant(ExerciseRegression_* result, const char* reason, double constant) {
            result->degenerate_ = true;
            result->degenerateReason_ = reason;
            result->basisDegree_ = 0;
            result->coefficients_.Resize(1);
            result->coefficients_[0] = constant;
        }

        //  Normal equations of the z-normalized monomial basis over the included paths.
        //  Only the diagonal and upper triangle of the Gram matrix are needed downstream.
        void AccumulateNormalEquations(const Vector_<>& x,
                                       const Vector_<>& targets,
                                       const Vector_<char>& included,
                                       double mean,
                                       double sigma,
                                       SquareMatrix_<>* gram,
                                       Vector_<>* rhs) {
            const size_t nBasis = static_cast<size_t>(gram->Rows());
            Vector_<> basis(nBasis);
            for (size_t i = 0; i < x.size(); ++i) {
                if (!included[i])
                    continue;
                const double z = (x[i] - mean) / sigma;
                basis[0] = 1.0;
                for (size_t j = 1; j < nBasis; ++j)
                    basis[j] = basis[j - 1] * z;
                for (size_t j = 0; j < nBasis; ++j) {
                    (*rhs)[j] += basis[j] * targets[i];
                    for (size_t k = j; k < nBasis; ++k)
                        (*gram)(j, k) += basis[j] * basis[k];
                }
            }
        }

        //  Explicit relative ridge, then the Gram diagonal-ratio conditioning guard
        bool RidgeAndIllConditioned(SquareMatrix_<>* gram) {
            const size_t nBasis = static_cast<size_t>(gram->Rows());
            double maxDiag = (*gram)(0, 0);
            double minDiag = (*gram)(0, 0);
            for (size_t j = 0; j < nBasis; ++j) {
                (*gram)(j, j) *= 1.0 + RIDGE_LAMBDA;
                maxDiag = std::max(maxDiag, (*gram)(j, j));
                minDiag = std::min(minDiag, (*gram)(j, j));
            }
            return minDiag <= 0.0 || maxDiag / minDiag > CONDITION_LIMIT;
        }

        constexpr size_t NO_SLOT = static_cast<size_t>(-1);

        struct ExerciseDayPlan_ {
            size_t eventId_;
            size_t sampleId_;
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

        LsmcPlan_ ScanEvents(const Vector_<Event_>& events, const ObservationPlan_& plan, double smooth) {
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
                    scan.days_.push_back({e, plan.EventToSample()[e], conditional, exercise->eps_ < 0.0 ? smooth : exercise->eps_});
                }
            }
            return scan;
        }

        //  N7 storage: payments per PAYS event, (x, h[, condition]) per exercise day
        struct LsmcStorage_ {
            Vector_<Vector_<>> pays_;
            Vector_<Vector_<>> xByDay_;
            Vector_<Vector_<>> hByDay_;
            Vector_<Vector_<>> preExercise_;   //  payoff accumulated strictly before each exercise date
            Vector_<Vector_<char>> condByDay_; //  empty row = unconditional day
        };

        LsmcStorage_ MakeStorage(const LsmcPlan_& scan, size_t nPaths) {
            LsmcStorage_ storage;
            storage.pays_.Resize(scan.paysEventIds_.size());
            for (auto& row : storage.pays_)
                row = Vector_<>(nPaths, 0.0);
            storage.xByDay_.Resize(scan.days_.size());
            storage.hByDay_.Resize(scan.days_.size());
            storage.preExercise_.Resize(scan.days_.size());
            for (size_t k = 0; k < scan.days_.size(); ++k) {
                storage.xByDay_[k] = Vector_<>(nPaths, 0.0);
                storage.hByDay_[k] = Vector_<>(nPaths, 0.0);
                storage.preExercise_[k] = Vector_<>(nPaths, 0.0);
            }
            if (scan.anyConditional_) {
                storage.condByDay_.Resize(scan.days_.size());
                for (size_t k = 0; k < scan.days_.size(); ++k)
                    if (scan.days_[k].conditional_)
                        storage.condByDay_[k] = Vector_<char>(nPaths, 1);
            }
            return storage;
        }

        struct ThreadState_ {
            std::unique_ptr<Random_> random_;
            Vector_<> gauss_;
            Scenario_<> path_;
            LsmcEvaluator_<double> evaluator_;
            //  Compiled mode (T3 parity): per-thread recording state feeding LsmcSinks_
            std::optional<EvalState_<double>> compiledState_;
            LsmcSinks_ sinks_;

            explicit ThreadState_(const struct LsmcContext_& ctx);
        };

        //  Immutable per-run references shared by the three phases
        struct LsmcContext_ {
            const PreparedScript_& prepared_;
            AAD::Model_<double>* model_;
            const LsmcPlan_& scan_;
            LsmcStorage_& storage_;
            Vector_<std::unique_ptr<ThreadState_>>& threadStates_;
            std::optional<ScriptCompiled_> compiled_; //  engaged when the prepared simulation selects the compiled evaluator

            const ScriptProduct_& Product() const { return prepared_.Product(); }
            const ObservationPlan_& Plan() const { return prepared_.Plan(); }
            [[nodiscard]] size_t PayOffIdx() const { return Product().PayOffIdx(); }

            ThreadState_& StateFor(size_t threadNum) {
                auto& state = threadStates_[threadNum];
                if (!state)
                    state = std::make_unique<ThreadState_>(*this);
                return *state;
            }
        };

        ThreadState_::ThreadState_(const LsmcContext_& ctx)
            : random_(CreateRNG(ctx.prepared_.Simulation().rsg_, ctx.model_->SimDim(), ctx.prepared_.Simulation().useBb_)),
              gauss_(ctx.model_->SimDim()), evaluator_(ctx.Product().VarValues(), ctx.Product().ConstVarValues()) {
            AllocatePath(ctx.Plan().DefLine(), path_);
            InitializePath(path_);
            evaluator_.eventToPays_ = &ctx.scan_.eventToPays_;
            evaluator_.eventToExercise_ = &ctx.scan_.eventToExercise_;
            evaluator_.paysStorage_ = &ctx.storage_.pays_;
            evaluator_.xStorage_ = &ctx.storage_.xByDay_;
            evaluator_.hStorage_ = &ctx.storage_.hByDay_;
            evaluator_.condStorage_ = ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
            if (ctx.compiled_) {
                compiledState_.emplace(ctx.prepared_.BuildEvalState<double>());
                sinks_.eventToPays_ = &ctx.scan_.eventToPays_;
                sinks_.eventToExercise_ = &ctx.scan_.eventToExercise_;
                sinks_.pays_ = &ctx.storage_.pays_;
                sinks_.x_ = &ctx.storage_.xByDay_;
                sinks_.h_ = &ctx.storage_.hByDay_;
                sinks_.cond_ = ctx.storage_.condByDay_.empty() ? nullptr : &ctx.storage_.condByDay_;
                compiledState_->lsmcSinks_ = &sinks_;
            }
        }

        //  S4: exercise replaces same-day and later payments only, so the payoff
        //  accumulated strictly before the exercise event survives (both engines)
        void SnapshotPreExercise(const LsmcContext_& ctx, const Vector_<>& variables, size_t event, size_t pathSlot) {
            const size_t slot = ctx.scan_.eventToExercise_[event];
            if (slot != NO_SLOT)
                ctx.storage_.preExercise_[slot][pathSlot] = ctx.Product().HasPays() ? variables[ctx.PayOffIdx()] : 0.0;
        }

        void TreeEvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            auto& eval = state.evaluator_;
            eval.pathSlot_ = pathSlot;
            eval.SetScenario(&state.path_);
            eval.SetObservations(&ctx.Plan());
            eval.Init();
            const auto& events = ctx.Product().Events();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                SnapshotPreExercise(ctx, eval.VarVals(), e, pathSlot);
                eval.SetCurEvt(eventToSample[e]);
                eval.SetEventOrdinal(e);
                for (const auto& statement : events[e])
                    statement->Accept(eval);
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
            eval.scenario_ = &state.path_;
            const auto& nodeStreams = compiled.NodeStreams();
            const auto& constStreams = compiled.ConstStreams();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < nodeStreams.size(); ++e) {
                sinks.eventOrdinal_ = e;
                SnapshotPreExercise(ctx, eval.variables_, e, pathSlot);
                const Detail::CompiledEventView_<double> view{nodeStreams[e], constStreams[e], state.path_[eventToSample[e]]};
                Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &eval);
            }
        }

        //  One forward evaluation with recording (EXERCISE is a no-op on the script state)
        void EvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            state.random_->FillNormal(&state.gauss_);
            ctx.model_->GeneratePath(state.gauss_, &state.path_);
            ValidateSimulationPath(state.path_);
            if (state.compiledState_)
                CompiledEvaluateRecordedPath(state, ctx, pathSlot);
            else
                TreeEvaluateRecordedPath(state, ctx, pathSlot);
        }

        //  Phase A: forward storage over disjoint per-batch path slots
        void RunForwardPhase(LsmcContext_& ctx, const BatchPlan_& batchPlan) {
            ThreadPool_* pool = ThreadPool_::GetInstance();
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch]() {
                    ThreadState_& state = ctx.StateFor(ThreadPool_::ThreadNum());
                    state.random_->SkipTo(batch.firstPath_);
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
            const auto& cond = condByDay.empty() ? Vector_<char>() : condByDay[day];
            for (size_t j = 0; j < included->size(); ++j)
                (*included)[j] = (cond.empty() || cond[j]) && h[j] > 0.0 ? 1 : 0;
        }

        //  S3/S4: exercise on strictly-better continuation estimates replaces the future
        void
        ApplyExerciseDecisions(const Vector_<>& h, const Vector_<>& x, const Vector_<char>& included, const ExerciseRegression_& c, Vector_<>* w) {
            for (size_t j = 0; j < w->size(); ++j)
                if (included[j] && h[j] > RegressionPredict(c, x[j]))
                    (*w)[j] = h[j];
        }

        //  Phase B: backward induction and continuation regressions, single threaded in
        //  global path order (thread-count independent by construction, N9/N10)
        Vector_<ExerciseRegression_> RunBackwardPhase(const LsmcContext_& ctx, const Vector_<>& eventNumeraire, size_t nPaths, int degree) {
            const auto& scan = ctx.scan_;
            const auto& storage = ctx.storage_;
            const auto& events = ctx.Product().Events();
            Vector_<> w(nPaths, 0.0);
            Vector_<ExerciseRegression_> regressions(scan.days_.size());
            Vector_<char> included(nPaths, 1);
            for (size_t ei = events.size(); ei-- > 0;) {
                const bool hasNext = ei + 1 < events.size();
                const double dNext = hasNext ? eventNumeraire[ei] / eventNumeraire[ei + 1] : 1.0;
                InductBackward(storage.pays_, scan.eventToPays_[ei], dNext, hasNext, &w);

                const size_t day = scan.eventToExercise_[ei];
                if (day == NO_SLOT)
                    continue;
                FillIncluded(storage.condByDay_, storage.hByDay_[day], day, &included);
                regressions[day] = SolveExerciseRegression(storage.xByDay_[day], w, included, degree);
                ApplyExerciseDecisions(storage.hByDay_[day], storage.xByDay_[day], included, regressions[day], &w);
            }
            return regressions;
        }

        //  First exercise wins (S3/S4): the earliest true decision replaces the payoff
        double PathPayoff(ThreadState_& state,
                          LsmcContext_& ctx,
                          const Vector_<ExerciseRegression_>& regressions,
                          size_t pathSlot,
                          bool hasPayoffVar,
                          Vector_<size_t>* exerciseCounts) {
            const auto& scan = ctx.scan_;
            const auto& storage = ctx.storage_;
            for (size_t k = 0; k < scan.days_.size(); ++k) {
                const bool condTrue = storage.condByDay_.empty() || storage.condByDay_[k].empty() || storage.condByDay_[k][pathSlot] != 0;
                //  S3/S4 + Longstaff-Schwartz: a positive exercise value that strictly
                //  beats the continuation estimate; h == 0 paths never exercise
                if (condTrue && storage.hByDay_[k][pathSlot] > 0.0 &&
                    storage.hByDay_[k][pathSlot] > RegressionPredict(regressions[k], storage.xByDay_[k][pathSlot])) {
                    //  S4: exercise replaces the same-day and later payments; earlier ones survive
                    const double payoff =
                        storage.preExercise_[k][pathSlot] + storage.hByDay_[k][pathSlot] / state.path_[scan.days_[k].sampleId_].numeraire_;
                    REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite exercise value", ScriptError_);
                    ++(*exerciseCounts)[k];
                    return payoff;
                }
            }
            if (!hasPayoffVar)
                return 0.0;
            const Vector_<>& variables = state.compiledState_ ? state.compiledState_->VarVals() : state.evaluator_.VarVals();
            const double payoff = variables[ctx.PayOffIdx()];
            REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite path value", ScriptError_);
            return payoff;
        }

        //  Phase C accumulates per batch, then reduces in batch-index order (N9)
        struct ReplayOutcome_ {
            double sum_ = 0.0;
            double sumSq_ = 0.0;
            Vector_<size_t> exerciseCounts_;
        };

        ReplayOutcome_ RunReplayPhase(LsmcContext_& ctx, const BatchPlan_& batchPlan, const Vector_<ExerciseRegression_>& regressions) {
            const bool hasPayoffVar = ctx.Product().HasPays();
            Vector_<ReplayOutcome_> outcomes(batchPlan.BatchCount());
            for (auto& outcome : outcomes)
                outcome.exerciseCounts_ = Vector_<size_t>(ctx.scan_.days_.size(), 0);

            ThreadPool_* pool = ThreadPool_::GetInstance();
            SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
            for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
                const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
                tasks.Spawn([&, batch, batchIndex]() {
                    ThreadState_& state = ctx.StateFor(ThreadPool_::ThreadNum());
                    state.random_->SkipTo(batch.firstPath_);
                    ReplayOutcome_& outcome = outcomes[batchIndex];
                    for (size_t i = 0; i < batch.pathCount_; ++i) {
                        EvaluateRecordedPath(state, ctx, batch.firstPath_ + i);
                        const double payoff = PathPayoff(state, ctx, regressions, batch.firstPath_ + i, hasPayoffVar, &outcome.exerciseCounts_);
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
            ws.pays_ = Vector_<AAD::Number_>(scan.eventToPays_.size(), 0.0);
            ws.h_ = Vector_<AAD::Number_>(scan.days_.size(), 0.0);
            ws.cond_ = Vector_<AAD::Number_>(scan.days_.size(), 1.0);
            ws.sinks_.pays_ = &ws.pays_;
            ws.sinks_.h_ = &ws.h_;
            ws.sinks_.cond_ = &ws.cond_;
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
                                 const std::optional<ScriptCompiled_>& fuzzyCompiled,
                                 const PathBatch_& batch,
                                 AadReplayOutcome_* outcome) {
            AAD::Activate(*AAD::Tape());
            AAD::Rewind(*AAD::Tape());
            FuzzyReplayWorkspace_ ws = MakeFuzzyReplayWorkspace(prepared, modelData, scan, batch);
            if (fuzzyCompiled) {
                EvalState_<AAD::Number_> state = prepared.BuildEvalState<AAD::Number_>(0, prepared.Simulation().smooth_);
                FuzzyReplayPaths(
                    prepared, scan, regressions, batch, ws, state,
                    [&fuzzyCompiled](FuzzyReplayWorkspace_& w, const PreparedScript_& p, EvalState_<AAD::Number_>& s) {
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
        REQUIRE(degree >= 1, "InvalidLsmcBasisDegree: regression degree must be positive");
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
        if (RidgeAndIllConditioned(&gram)) {
            DegradeToConstant(&result, "IllConditioned", constantFit);
            return result;
        }

        Vector_<Vector_<>> rhsWrapped(1);
        rhsWrapped[0] = rhs;
        CholeskySolve(&gram, &rhsWrapped);
        result.coefficients_ = rhsWrapped[0];
        result.basisDegree_ = degree;
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

        const auto scan = ScanEvents(product.Events(), prepared.Plan(), simulation.smooth_);
        auto storage = MakeStorage(scan, nPaths);
        //  N9 batch layout depends on nPaths only
        const BatchPlan_ batchPlan(nPaths, 1);
        ThreadPool_* pool = ThreadPool_::GetInstance();
        Vector_<std::unique_ptr<ThreadState_>> threadStates(pool->NumThreads());
        LsmcContext_ ctx{prepared, mdl, scan, storage, threadStates};
        if (simulation.compiled_.value_or(false))
            ctx.compiled_.emplace(prepared.Compile());
        static_cast<void>(ctx.StateFor(0));

        RunForwardPhase(ctx, batchPlan);
        const Vector_<> eventNumeraire = SampleGridNumeraires(ctx);
        const auto regressions = RunBackwardPhase(ctx, eventNumeraire, nPaths, simulation.lsmcBasisDegree_);
        const auto reduction = RunReplayPhase(ctx, batchPlan, regressions);

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

        const auto scan = ScanEvents(product.Events(), prepared.Plan(), simulation.smooth_);
        auto storage = MakeStorage(scan, nPaths);
        //  N9 batch layout depends on nPaths only
        const BatchPlan_ batchPlan(nPaths, 1);
        ThreadPool_* pool = ThreadPool_::GetInstance();
        Vector_<std::unique_ptr<ThreadState_>> threadStates(pool->NumThreads());
        LsmcContext_ ctx{prepared, doubleModel.get(), scan, storage, threadStates};
        if (simulation.compiled_.value_or(false))
            ctx.compiled_.emplace(ScriptCompiled_::Build(product.Events(), false, prepared.PlanHandle(), false, true));

        RunForwardPhase(ctx, batchPlan);
        const Vector_<> eventNumeraire = SampleGridNumeraires(ctx);
        const auto regressions = RunBackwardPhase(ctx, eventNumeraire, nPaths, simulation.lsmcBasisDegree_);
        threadStates.clear();

        const std::optional<ScriptCompiled_> fuzzyCompiled =
            simulation.compiled_.value_or(false) ? std::optional<ScriptCompiled_>(prepared.Compile(true)) : std::nullopt;
        Vector_<AadReplayOutcome_> outcomes(batchPlan.BatchCount());
        for (auto& outcome : outcomes)
            outcome.risks_ = Vector_<>(doubleModel->ParameterLabels().size() + product.ConstVarNames().size(), 0.0);

        SimulationTaskGroup_ tasks(pool, batchPlan.BatchCount());
        for (size_t batchIndex = 0; batchIndex < batchPlan.BatchCount(); ++batchIndex) {
            const PathBatch_ batch = batchPlan.BatchAt(batchIndex);
            tasks.Spawn([&, batch, batchIndex]() {
                RunFuzzyReplayBatch(prepared, modelData, scan, regressions, fuzzyCompiled, batch, &outcomes[batchIndex]);
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
