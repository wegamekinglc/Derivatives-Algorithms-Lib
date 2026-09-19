//
// Created by dal-implementer on 2026/9/20.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/math/matrix/cholesky.hpp>
#include <dal/math/matrix/squarematrix.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>
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

        constexpr size_t NO_SLOT = static_cast<size_t>(-1);

        struct ExerciseDayPlan_ {
            size_t eventId_;
            size_t sampleId_;
            bool conditional_;
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

        LsmcPlan_ ScanEvents(const Vector_<Event_>& events, const ObservationPlan_& plan) {
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
                    scan.days_.push_back({e, plan.EventToSample()[e], conditional});
                }
            }
            return scan;
        }

        //  N7 storage: payments per PAYS event, (x, h[, condition]) per exercise day
        struct LsmcStorage_ {
            Vector_<Vector_<>> pays_;
            Vector_<Vector_<>> xByDay_;
            Vector_<Vector_<>> hByDay_;
            Vector_<Vector_<char>> condByDay_; //  empty row = unconditional day
        };

        LsmcStorage_ MakeStorage(const LsmcPlan_& scan, size_t nPaths) {
            LsmcStorage_ storage;
            storage.pays_.Resize(scan.paysEventIds_.size());
            for (auto& row : storage.pays_)
                row = Vector_<>(nPaths, 0.0);
            storage.xByDay_.Resize(scan.days_.size());
            storage.hByDay_.Resize(scan.days_.size());
            for (size_t k = 0; k < scan.days_.size(); ++k) {
                storage.xByDay_[k] = Vector_<>(nPaths, 0.0);
                storage.hByDay_[k] = Vector_<>(nPaths, 0.0);
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

            explicit ThreadState_(const struct LsmcContext_& ctx);
        };

        //  Immutable per-run references shared by the three phases
        struct LsmcContext_ {
            const PreparedScript_& prepared_;
            AAD::Model_<double>* model_;
            const LsmcPlan_& scan_;
            LsmcStorage_& storage_;
            Vector_<std::unique_ptr<ThreadState_>>& threadStates_;

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
        }

        //  One forward evaluation with recording (the LsmcEvaluator_ no-ops EXERCISE)
        void EvaluateRecordedPath(ThreadState_& state, LsmcContext_& ctx, size_t pathSlot) {
            state.random_->FillNormal(&state.gauss_);
            ctx.model_->GeneratePath(state.gauss_, &state.path_);
            ValidateSimulationPath(state.path_);
            auto& eval = state.evaluator_;
            eval.pathSlot_ = pathSlot;
            eval.SetScenario(&state.path_);
            eval.SetObservations(&ctx.Plan());
            eval.Init();
            const auto& events = ctx.Product().Events();
            const auto& eventToSample = ctx.Plan().EventToSample();
            for (size_t e = 0; e < events.size(); ++e) {
                eval.SetCurEvt(eventToSample[e]);
                eval.SetEventOrdinal(e);
                for (const auto& statement : events[e])
                    statement->Accept(eval);
            }
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

        //  N5: the backward discounting ratios anchor on one probe path's numeraires
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
            Vector_<> eventNumeraire(events.size(), 1.0);
            for (size_t e = 0; e < events.size(); ++e)
                eventNumeraire[e] = path[ctx.Plan().EventToSample()[e]].numeraire_;
            return eventNumeraire;
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
                const size_t paysSlot = scan.eventToPays_[ei];
                const bool hasNext = ei + 1 < events.size();
                const double dNext = hasNext ? eventNumeraire[ei] / eventNumeraire[ei + 1] : 1.0;
                for (size_t j = 0; j < nPaths; ++j)
                    w[j] = (paysSlot == NO_SLOT ? 0.0 : storage.pays_[paysSlot][j]) + (hasNext ? dNext * w[j] : 0.0);

                const size_t day = scan.eventToExercise_[ei];
                if (day == NO_SLOT)
                    continue;
                const auto& cond = storage.condByDay_.empty() ? Vector_<char>() : storage.condByDay_[day];
                if (!cond.empty())
                    for (size_t j = 0; j < nPaths; ++j)
                        included[j] = cond[j];
                else
                    std::fill(included.begin(), included.end(), 1);
                regressions[day] = SolveExerciseRegression(storage.xByDay_[day], w, included, degree);
                const auto& continuation = regressions[day];
                const auto& h = storage.hByDay_[day];
                const auto& x = storage.xByDay_[day];
                for (size_t j = 0; j < nPaths; ++j)
                    if (included[j] && h[j] > RegressionPredict(continuation, x[j]))
                        w[j] = h[j]; //  S4: exercise replaces the same-day and later payments
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
                if (condTrue && storage.hByDay_[k][pathSlot] > RegressionPredict(regressions[k], storage.xByDay_[k][pathSlot])) {
                    const double payoff = storage.hByDay_[k][pathSlot] / state.path_[scan.days_[k].sampleId_].numeraire_;
                    REQUIRE2(std::isfinite(payoff), "InvalidPayoff: non-finite exercise value", ScriptError_);
                    ++(*exerciseCounts)[k];
                    return payoff;
                }
            }
            if (!hasPayoffVar)
                return 0.0;
            const double payoff = state.evaluator_.VarVals()[ctx.PayOffIdx()];
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
    } // namespace

    ExerciseRegression_ SolveExerciseRegression(const Vector_<>& x, const Vector_<>& targets, const Vector_<char>& included, int degree) {
        REQUIRE(degree >= 1, "InvalidLsmcBasisDegree: regression degree must be positive");
        REQUIRE(x.size() == targets.size() && x.size() == included.size(), "InvalidRegressionInput: mismatched regression vectors");

        ExerciseRegression_ result;
        size_t nCond = 0;
        double sumX = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            if (included[i]) {
                ++nCond;
                sumX += x[i];
            }
        result.numCondTrue_ = nCond;

        const auto degrade = [&](const char* reason, double constant) {
            result.degenerate_ = true;
            result.degenerateReason_ = reason;
            result.basisDegree_ = 0;
            result.coefficients_.Resize(1);
            result.coefficients_[0] = constant;
        };

        //  Guard order: sample size, then sigma floor, then Gram conditioning
        if (nCond == 0) {
            degrade("ConditionPathsBelowMin", 0.0);
            return result;
        }
        result.mean_ = sumX / static_cast<double>(nCond);
        double sumSq = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            if (included[i]) {
                const double dev = x[i] - result.mean_;
                sumSq += dev * dev;
            }
        const double sigma = std::sqrt(sumSq / static_cast<double>(nCond));
        const double sigmaFloor = SIGMA_FLOOR_SCALE * std::max(1.0, std::abs(result.mean_));
        result.sigma_ = std::max(sigma, sigmaFloor);
        if (sigma < sigmaFloor) {
            degrade("SigmaFloor", MeanOf(targets, included, nCond));
            return result;
        }
        if (nCond < PATHS_PER_BASIS_FUNCTION * static_cast<size_t>(degree + 1)) {
            degrade("ConditionPathsBelowMin", MeanOf(targets, included, nCond));
            return result;
        }

        const size_t nBasis = static_cast<size_t>(degree) + 1;
        SquareMatrix_<> gram(static_cast<int>(nBasis), 0.0);
        Vector_<> rhs(nBasis, 0.0);
        Vector_<> basis(nBasis);
        for (size_t i = 0; i < x.size(); ++i) {
            if (!included[i])
                continue;
            const double z = (x[i] - result.mean_) / result.sigma_;
            basis[0] = 1.0;
            for (size_t j = 1; j < nBasis; ++j)
                basis[j] = basis[j - 1] * z;
            for (size_t j = 0; j < nBasis; ++j) {
                rhs[j] += basis[j] * targets[i];
                for (size_t k = j; k < nBasis; ++k)
                    gram(j, k) += basis[j] * basis[k];
            }
        }
        //  CholeskySolve reads the diagonal and upper triangle only
        double maxDiag = gram(0, 0);
        double minDiag = gram(0, 0);
        for (size_t j = 0; j < nBasis; ++j) {
            gram(j, j) *= 1.0 + RIDGE_LAMBDA;
            maxDiag = std::max(maxDiag, gram(j, j));
            minDiag = std::min(minDiag, gram(j, j));
        }
        if (!(minDiag > 0.0) || maxDiag / minDiag > CONDITION_LIMIT) {
            degrade("IllConditioned", MeanOf(targets, included, nCond));
            return result;
        }

        Vector_<Vector_<>> rhsWrapped(1);
        rhsWrapped[0] = rhs;
        CholeskySolve(&gram, &rhsWrapped);
        result.coefficients_ = rhsWrapped[0];
        result.basisDegree_ = degree;
        return result;
    }

    double RegressionPredict(const ExerciseRegression_& regression, double x) {
        const double z = (x - regression.mean_) / regression.sigma_;
        double value = 0.0;
        for (size_t j = regression.coefficients_.size(); j-- > 0;)
            value = value * z + regression.coefficients_[j];
        return value;
    }

    SimResults_ MCLsmcSimulation(const PreparedScript_& prepared, AAD::Model_<double>* mdl, size_t nPaths, LsmcDiagnostics_* diagnostics) {
        const auto& product = prepared.Product();
        const auto& simulation = prepared.Simulation();
        REQUIRE2(!simulation.compiled_.value_or(false),
                 "UnsupportedExecutionMode: compiled valuation of EXERCISE is not implemented (compiled parity arrives with the next milestone)",
                 ScriptError_);
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of Monte Carlo paths must be positive", ScriptError_);
        REQUIRE2(!simulation.enableAad_,
                 "UnsupportedExecutionMode: AAD valuation of EXERCISE is not implemented (the fuzzy driver arrives with a later milestone)",
                 ScriptError_);

        const auto scan = ScanEvents(product.Events(), prepared.Plan());
        auto storage = MakeStorage(scan, nPaths);
        //  N9 batch layout depends on nPaths only
        const BatchPlan_ batchPlan(nPaths, 1);
        ThreadPool_* pool = ThreadPool_::GetInstance();
        Vector_<std::unique_ptr<ThreadState_>> threadStates(pool->NumThreads());
        LsmcContext_ ctx{prepared, mdl, scan, storage, threadStates};
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
} // namespace Dal::Script
