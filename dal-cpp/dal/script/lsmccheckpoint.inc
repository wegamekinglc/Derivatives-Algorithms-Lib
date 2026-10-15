//
// Created by Codex on 2026/10/16.
//

// Included inside the trainer's private namespace.
size_t TrainingAdd(size_t a, size_t b) {
    REQUIRE2(b <= std::numeric_limits<size_t>::max() - a, "LsmcTrainingBudget: size overflow", ScriptError_);
    return a + b;
}

size_t TrainingMultiply(size_t a, size_t b) {
    REQUIRE2(a == 0 || b <= std::numeric_limits<size_t>::max() / a, "LsmcTrainingBudget: size overflow", ScriptError_);
    return a * b;
}

template <class T_> size_t TrainingArrayBytes(const Vector_<T_>& values) { return TrainingMultiply(values.capacity(), sizeof(T_)); }

template <class T_> size_t TrainingNestedBytes(const Vector_<Vector_<T_>>& values) {
    size_t bytes = TrainingArrayBytes(values);
    for (const auto& row : values)
        bytes = TrainingAdd(bytes, TrainingArrayBytes(row));
    return bytes;
}

class TrainingAllocationBudget_ final : public Dal::Detail::OwnedBufferAccount_ {
    size_t limit_;
    size_t fixed_;
    size_t current_;
    size_t peak_;

public:
    TrainingAllocationBudget_(size_t limit, size_t fixed) : limit_(limit), fixed_(fixed), current_(fixed), peak_(fixed) {
        REQUIRE2(fixed <= limit, "LsmcTrainingBudget: fixed storage exceeds allocation bound", ScriptError_);
        REQUIRE2(!Dal::Detail::CurrentBufferBudget(), "LsmcTrainingBudget: nested buffer budgets are unsupported", ScriptError_);
    }
    void Reserve(size_t bytes) override {
        REQUIRE2(bytes <= limit_ - current_, "LsmcTrainingBudget: allocation capacity exceeds admitted bound", ScriptError_);
        current_ += bytes;
        peak_ = std::max(peak_, current_);
    }
    void Release(size_t bytes) noexcept override {
        if (bytes > current_ - fixed_)
            std::terminate();
        current_ -= bytes;
    }
    [[nodiscard]] size_t PeakCapacityBytes() const { return peak_; }
};

size_t RetainedTrainingTapeBytes() {
    const auto* tape = AAD::NativeThread::retainedTape;
    if (!tape)
        return 0;
    size_t bytes = sizeof(*tape);
    bytes = TrainingAdd(bytes, TrainingMultiply(tape->nodes_.AllocatedBlocks(), AAD::BLOCK_SIZE * sizeof(AAD::TapNode_)));
    bytes = TrainingAdd(bytes, TrainingMultiply(tape->ders_.AllocatedBlocks(), AAD::DATA_SIZE * sizeof(double)));
    bytes = TrainingAdd(bytes, TrainingMultiply(tape->argPtrs_.AllocatedBlocks(), AAD::DATA_SIZE * sizeof(double*)));
    bytes = TrainingAdd(bytes, TrainingMultiply(tape->adjointsMulti_.AllocatedBlocks(), AAD::ADJ_SIZE * sizeof(double)));
    return TrainingAdd(bytes, tape->ReverseEventCapacityBytes());
}

bool CheckpointCompactState(const LsmcContext_& ctx) {
    for (const auto& event : ctx.Product().Events())
        for (const auto& node : event)
            if (FindNode(*node, [](const Node_& item) { return dynamic_cast<const NodeVectorAppend_*>(&item) != nullptr; }))
                return false;
    return true;
}

void RequireTrainingBudgetDomain(const LsmcContext_& ctx) {
    REQUIRE2(ctx.compiled_ && ctx.prepared_.Simulation().rsg_ == "sobol" && typeid(*ctx.model_) == typeid(AAD::BlackScholes_<double>) &&
                 ThreadPool_::GetInstance()->NumThreads() == 1,
             "LsmcTrainingBudget: requires compiled Sobol, exact BlackScholes and one worker", ScriptError_);
    const size_t events = ctx.Product().Events().size();
    REQUIRE2(events > 0 && ctx.prepared_.TimeLine().size() == events && ctx.model_->SimDim() == events,
             "LsmcTrainingBudget: requires one positive-time model sample per event", ScriptError_);
    for (size_t e = 0; e < events; ++e)
        REQUIRE2(ctx.Plan().EventToSample()[e] == e && ctx.prepared_.TimeLine()[e] > 0.0, "LsmcTrainingBudget: unsupported event/sample mapping",
                 ScriptError_);
    const auto& liveEvents = ctx.Plan().LiveEventIds();
    for (const auto& request : ctx.Plan().Requests()) {
        if (!request.modelSlot_)
            continue;
        for (const auto& use : request.uses_) {
            const auto event = std::lower_bound(liveEvents.begin(), liveEvents.end(), use.eventId_);
            const size_t ordinal = static_cast<size_t>(event - liveEvents.begin());
            REQUIRE2(event != liveEvents.end() && *event == use.eventId_ && ordinal < ctx.Plan().EventToSample().size() &&
                         ctx.Plan().EventToSample()[ordinal] == request.modelSlot_->sampleId_,
                     "LsmcTrainingBudget: retained future fixing samples are unsupported", ScriptError_);
        }
    }
    if (!ctx.policyBumpTraining_ && ctx.prepared_.Simulation().lsmcPolicyRiskMode_ == "RetrainedBump") {
        const auto& params = ctx.model_->Parameters();
        for (size_t j = 0; j < params.size(); ++j) {
            const double step = ctx.prepared_.Simulation().lsmcPolicyBumpRelative_ * std::max(1.0, std::fabs(*params[j]));
            REQUIRE2(ctx.model_->ValidParameterValue(j, *params[j] - step) && ctx.model_->ValidParameterValue(j, *params[j] + step),
                     "LsmcTrainingBudget: one-sided policy bumps are unsupported", ScriptError_);
        }
    }
}

LsmcPlan_ CheckpointSlice(const LsmcPlan_& scan, size_t first, size_t last) {
    LsmcPlan_ slice;
    slice.eventToPays_ = Vector_<size_t>(scan.eventToPays_.size(), NO_SLOT);
    slice.eventToExercise_ = Vector_<size_t>(scan.eventToExercise_.size(), NO_SLOT);
    for (size_t e = first; e < last; ++e) {
        if (scan.eventToPays_[e] != NO_SLOT) {
            slice.eventToPays_[e] = slice.paysEventIds_.size();
            slice.paysEventIds_.push_back(e);
        }
        if (scan.eventToExercise_[e] != NO_SLOT) {
            slice.eventToExercise_[e] = slice.days_.size();
            const auto day = scan.days_[scan.eventToExercise_[e]];
            slice.days_.push_back(day);
            slice.anyConditional_ |= day.conditional_;
        }
    }
    return slice;
}

size_t TrainingRawRowsBytes(const LsmcPlan_& scan, size_t paths, size_t features) {
    const size_t rows = TrainingAdd(scan.paysEventIds_.size(), TrainingMultiply(scan.days_.size(), features + 1));
    return TrainingMultiply(TrainingMultiply(rows, paths), sizeof(double));
}

using PassiveBsRestart_ = AAD::BlackScholes_<double>::RestartPaths_;

struct TrainingCheckpoint_ {
    PassiveBsRestart_::State_ model_;
    Vector_<> variables_;
    Vector_<Vector_<>> vectors_;
};

struct TrainingCheckpoints_ {
    static constexpr size_t SCHEMA = PassiveBsRestart_::SCHEMA;
    const PreparedScript_* prepared_;
    std::optional<uint64_t> stream_;
    std::array<double, 4> parameters_{};
    Vector_<> constants_;
    size_t paths_;
    size_t offset_;
    size_t block_;
    Vector_<TrainingCheckpoint_> states_;

    TrainingCheckpoints_(const LsmcContext_& ctx, size_t paths, size_t offset, size_t block)
        : prepared_(&ctx.prepared_), stream_(ctx.scrambleKey_), constants_(ctx.constValues_ ? *ctx.constValues_ : ctx.Product().ConstVarValues()),
          paths_(paths), offset_(offset), block_(block), states_(TrainingMultiply((ctx.Product().Events().size() - 1) / block + 1, paths)) {
        for (size_t j = 0; j < parameters_.size(); ++j)
            parameters_[j] = *ctx.model_->Parameters()[j];
    }

    void RequireIdentity(const LsmcContext_& ctx, size_t first) const {
        const auto& point = ctx.constValues_ ? *ctx.constValues_ : ctx.Product().ConstVarValues();
        const bool sameConstants =
            constants_.size() == point.size() &&
            (point.empty() || std::memcmp(constants_.data(), point.data(), TrainingMultiply(point.size(), sizeof(double))) == 0);
        REQUIRE2(prepared_ == &ctx.prepared_ && stream_ == ctx.scrambleKey_ && first % block_ == 0 && sameConstants,
                 "LsmcCheckpoint: preparation/stream/constant identity mismatch", ScriptError_);
        for (size_t j = 0; j < parameters_.size(); ++j)
            REQUIRE2(std::memcmp(&parameters_[j], ctx.model_->Parameters()[j], sizeof(double)) == 0, "LsmcCheckpoint: model point mismatch",
                     ScriptError_);
    }
};

struct PassiveTrainingWorkspace_ {
    const LsmcContext_& ctx_;
    std::unique_ptr<Random_> random_;
    Vector_<> normals_;
    PassiveBsRestart_ model_;
    EvalState_<double> state_;
    LsmcSinks_ sinks_;
    size_t normalVectors_ = 0;
    size_t modelSteps_ = 0;
    size_t scriptEvents_ = 0;

    explicit PassiveTrainingWorkspace_(const LsmcContext_& ctx)
        : ctx_(ctx), random_(CreateRNG(ctx.prepared_.Simulation().rsg_,
                                       *ctx.model_,
                                       ctx.prepared_.Simulation().useBb_,
                                       ctx.scrambleKey_,
                                       ctx.prepared_.Simulation().normalPrecision_)),
          normals_(ctx.model_->SimDim()), model_(static_cast<const AAD::BlackScholes_<double>&>(*ctx.model_)),
          state_(ctx.prepared_.BuildEvalState<double>()) {
        if (ctx.constValues_) {
            state_.ConstVarVals() = *ctx.constValues_;
            PastEvaluator_<double> past(Vector_<>(ctx.Product().VarNames().size(), 0.0), *ctx.constValues_, ctx.Product().VectorCapacities());
            past.SetObservations(&ctx.Plan());
            ctx.Product().Visit(past, true, false);
            state_.SetHistoricalSeed(past.VarVals());
            state_.SetHistoricalVectorSeed(past.VectorVals());
        }
        state_.observations_ = &ctx.Plan();
        state_.scenario_ = &model_.Path();
        state_.lsmcSinks_ = &sinks_;
        sinks_.plan_ = &ctx.Plan();
        sinks_.payoffIdx_ = ctx.PayOffIdx();
    }

    void Bind(const LsmcPlan_& scan, LsmcStorage_* storage) {
        sinks_.eventToPays_ = &scan.eventToPays_;
        sinks_.eventToExercise_ = &scan.eventToExercise_;
        sinks_.pays_ = storage ? &storage->pays_ : nullptr;
        sinks_.x_ = storage ? &storage->xByDay_ : nullptr;
        sinks_.h_ = storage ? &storage->hByDay_ : nullptr;
        sinks_.cond_ = storage && !storage->condByDay_.empty() ? &storage->condByDay_ : nullptr;
    }

    void Random(size_t address) {
        random_->SkipTo(address);
        random_->FillNormal(&normals_);
        ++normalVectors_;
    }

    void Event(size_t event, size_t path) {
        model_.Advance(normals_, event + 1);
        ++modelSteps_;
        sinks_.eventOrdinal_ = event;
        sinks_.pathSlot_ = path;
        const Detail::CompiledEventView_<double> view{ctx_.compiled_->NodeStreams()[event], ctx_.compiled_->ConstStreams()[event],
                                                      model_.Path()[event]};
        Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &state_);
        REQUIRE2(state_.dStack_.IsEmpty() && state_.bStack_.IsEmpty(), "LsmcCheckpoint: nonempty boundary stacks", ScriptError_);
        ++scriptEvents_;
    }

    void Capture(TrainingCheckpoints_* checkpoints) {
        Bind(ctx_.scan_, nullptr);
        for (size_t p = 0; p < checkpoints->paths_; ++p) {
            Random(checkpoints->offset_ + p);
            model_.Reset();
            state_.Init();
            for (size_t e = 0; e < ctx_.Product().Events().size(); ++e) {
                if (e % checkpoints->block_ == 0)
                    checkpoints->states_[(e / checkpoints->block_) * checkpoints->paths_ + p] = {model_.Save(), state_.variables_, state_.vectors_};
                Event(e, p);
            }
        }
    }

    void Record(const LsmcPlan_& scan,
                size_t paths,
                size_t offset,
                size_t first,
                size_t last,
                LsmcStorage_* storage,
                const TrainingCheckpoints_* checkpoints = nullptr) {
        if (checkpoints)
            checkpoints->RequireIdentity(ctx_, first);
        storage->ZeroPaths(0, paths);
        Bind(scan, storage);
        for (size_t p = 0; p < paths; ++p) {
            Random(offset + p);
            state_.Init();
            if (checkpoints) {
                const auto& saved = checkpoints->states_[(first / checkpoints->block_) * paths + p];
                REQUIRE2(saved.model_.next_ == first, "LsmcCheckpoint: boundary mismatch", ScriptError_);
                model_.Restore(saved.model_);
                state_.variables_ = saved.variables_;
                state_.vectors_ = saved.vectors_;
            } else {
                model_.Reset();
            }
            for (size_t e = first; e < last; ++e)
                Event(e, p);
        }
    }
};

void ApplyCheckpointPending(BackwardRows_* rows) {
    if (!rows->pending_.regression_)
        return;
    const auto& pending = rows->pending_;
    const LocalPredictor_ predict(*pending.regression_);
    for (size_t p = 0; p < rows->Size(); ++p) {
        std::array<double, 3> features{};
        for (size_t j = 0; j < pending.nFeatures_; ++j)
            features[j] = pending.features_[j][p];
        const double continuation = pending.nFeatures_ == 1 && pending.regression_->powers_.empty() ? predict(features[0]) : predict(features);
        rows->w_[p] = Select((rows->included_[p] != 0) & (pending.h_[p] > continuation), pending.h_[p], rows->w_[p]);
    }
    rows->pending_ = {};
}

struct TrainingMemoryPlan_ {
    size_t fixed_;
    size_t full_;
    size_t checkpoint_;
    size_t rawBlock_;
    size_t tape_;
};

TrainingMemoryPlan_ PlanTrainingMemory(const LsmcContext_& ctx, const PathCounts_& counts, size_t block) {
    const size_t events = ctx.Product().Events().size(), paths = TrainingAdd(counts.training_, counts.validation_);
    const size_t features = ctx.Plan().RegressionFeatureCount();
    // Covers fixed QR construction/return frames, candidate fits, reducers, RNG objects and evaluator stacks.
    size_t fixed = 4 * sizeof(RegressionQR::Workspace_<20>) + 4 * sizeof(PassiveTrainingWorkspace_) + 16384;
    const size_t tape = RetainedTrainingTapeBytes();
    fixed = TrainingAdd(fixed, tape);
    fixed = TrainingAdd(fixed, TrainingMultiply(3, static_cast<const AAD::BlackScholes_<double>&>(*ctx.model_).PassiveStorageBytes()));
    fixed = TrainingAdd(fixed, TrainingNestedBytes(ctx.compiled_->NodeStreams()));
    fixed = TrainingAdd(fixed, TrainingNestedBytes(ctx.compiled_->ConstStreams()));
    const auto& preparedProgram = ctx.prepared_.CompiledProgram(ctx.prepared_.Simulation().enableAad_);
    if (&preparedProgram != ctx.compiled_) {
        fixed = TrainingAdd(fixed, TrainingNestedBytes(preparedProgram.NodeStreams()));
        fixed = TrainingAdd(fixed, TrainingNestedBytes(preparedProgram.ConstStreams()));
    }
    // Base/up/down policies and validation candidates can overlap a bumped fit.
    fixed = TrainingAdd(fixed, TrainingMultiply(TrainingAdd(TrainingMultiply(4, ctx.scan_.days_.size()), 16),
                                                sizeof(ExerciseRegression_) + 20 * (sizeof(double) + 3) + 6 * sizeof(double) + 512));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.prepared_.TimeLine()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.prepared_.DefLine()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Plan().KnownValues()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Plan().EventToSample()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Plan().LiveEventIds()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Plan().Requests()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Plan().RegressionFeatures()));
    for (const auto& request : ctx.Plan().Requests())
        fixed = TrainingAdd(fixed, TrainingArrayBytes(request.uses_));
    for (const auto& feature : ctx.Plan().RegressionFeatures())
        fixed = TrainingAdd(fixed, TrainingArrayBytes(feature.outputBySample_));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Product().VarValues()));
    fixed = TrainingAdd(fixed, TrainingNestedBytes(ctx.Product().VectorValues()));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.Product().VectorCapacities()));
    fixed = TrainingAdd(fixed, TrainingMultiply(3, TrainingArrayBytes(ctx.Product().ConstVarValues())));
    fixed = TrainingAdd(fixed, TrainingMultiply(2 * sizeof(double), events));
    fixed = TrainingAdd(fixed, TrainingMultiply(6 * sizeof(double), TrainingAdd(ctx.Product().ConstVarValues().capacity(), 4)));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.scan_.eventToPays_));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.scan_.eventToExercise_));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.scan_.paysEventIds_));
    fixed = TrainingAdd(fixed, TrainingArrayBytes(ctx.scan_.days_));
    for (const auto& def : ctx.prepared_.DefLine()) {
        fixed = TrainingAdd(fixed, TrainingArrayBytes(def.discountMats_));
        fixed = TrainingAdd(fixed, TrainingArrayBytes(def.indexNames_));
        fixed = TrainingAdd(fixed, TrainingArrayBytes(def.liborDefs_));
        fixed = TrainingAdd(fixed, TrainingNestedBytes(def.forwardMats_));
    }
    const size_t rawFull = TrainingRawRowsBytes(ctx.scan_, paths, features);
    size_t rawBlock = 0, conditionalBlock = 0, conditionalFull = 0;
    for (const auto& day : ctx.scan_.days_)
        conditionalFull += static_cast<size_t>(day.conditional_);
    for (size_t first = 0; first < events; first += std::min(block, events - first)) {
        size_t rows = 0, conditional = 0;
        for (size_t e = first; e < first + std::min(block, events - first); ++e) {
            rows = TrainingAdd(rows, ctx.scan_.eventToPays_[e] != NO_SLOT ? 1 : 0);
            rows = TrainingAdd(rows, ctx.scan_.eventToExercise_[e] != NO_SLOT ? features + 1 : 0);
            if (ctx.scan_.eventToExercise_[e] != NO_SLOT)
                conditional += static_cast<size_t>(ctx.scan_.days_[ctx.scan_.eventToExercise_[e]].conditional_);
        }
        rawBlock = std::max(rawBlock, TrainingMultiply(TrainingMultiply(rows, paths), sizeof(double)));
        conditionalBlock = std::max(conditionalBlock, conditional);
    }
    size_t state = TrainingAdd(sizeof(TrainingCheckpoint_), TrainingArrayBytes(ctx.Product().VarValues()));
    state = TrainingAdd(state, TrainingArrayBytes(ctx.Product().VectorValues()));
    for (size_t capacity : ctx.Product().VectorCapacities())
        state = TrainingAdd(state, TrainingMultiply(capacity, sizeof(double)));
    const size_t checkpoints = TrainingMultiply(TrainingMultiply((events - 1) / block + 1, paths), state);
    // Capacity growth and temporary copies are charged again by the allocator guard.
    // This envelope bounds fresh vectors including QR targets, twenty columns, path/RNG and rollback arrays.
    size_t common = TrainingMultiply(paths, 2 * (21 * sizeof(double) + 32));
    size_t path = TrainingMultiply(events, sizeof(AAD::Sample_<double>));
    for (const auto& def : ctx.prepared_.DefLine()) {
        size_t values = TrainingAdd(def.indexNames_.size(), TrainingAdd(def.discountMats_.size(), def.liborDefs_.size()));
        path = TrainingAdd(path, TrainingMultiply(def.forwardMats_.size(), sizeof(Vector_<>)));
        for (const auto& forwards : def.forwardMats_)
            values = TrainingAdd(values, forwards.size());
        path = TrainingAdd(path, TrainingMultiply(values, sizeof(double)));
    }
    constexpr size_t RNG_COORDINATE_BYTES = 2 * 34 * sizeof(uint_least32_t) + 16 * sizeof(double) + 8 * sizeof(size_t);
    common = TrainingAdd(common, TrainingMultiply(events, RNG_COORDINATE_BYTES));
    common = TrainingAdd(common, TrainingMultiply(2, path));
    const size_t workspaceCopies = TrainingAdd(16, TrainingMultiply(4, ctx.prepared_.MaxNestedIfs()));
    common = TrainingAdd(common, TrainingMultiply(state, workspaceCopies));
    common = TrainingAdd(common, TrainingMultiply(8, TrainingArrayBytes(ctx.Product().ConstVarValues())));
    const size_t fullCommon = TrainingAdd(common, TrainingMultiply(TrainingMultiply(2, conditionalFull), paths));
    const size_t checkpointCommon = TrainingAdd(common, TrainingMultiply(TrainingMultiply(2, conditionalBlock), paths));
    TrainingMemoryPlan_ plan{fixed, TrainingAdd(fixed, TrainingAdd(rawFull, fullCommon)),
                             TrainingAdd(fixed, TrainingAdd(TrainingAdd(rawBlock, checkpointCommon), checkpoints)), rawBlock, tape};
    return plan;
}

TrainingOutcome_
TrainBudgetedRows(LsmcContext_& ctx, const PathCounts_& counts, size_t block, bool checkpoint, PassiveTrainingWorkspace_* workspace) {
    const size_t events = ctx.Product().Events().size(), features = ctx.Plan().RegressionFeatureCount();
    std::optional<TrainingCheckpoints_> training, validation;
    if (checkpoint) {
        training.emplace(ctx, counts.training_, 0, block);
        validation.emplace(ctx, counts.validation_, counts.training_, block);
        workspace->Capture(&*training);
        workspace->Capture(&*validation);
    }
    TrainingOutcome_ result;
    for (const auto& sample : workspace->model_.Path())
        result.eventNumeraire_.push_back(sample.numeraire_);
    result.regressions_.Resize(ctx.scan_.days_.size());
    Vector_<> holding(counts.training_, 0.0), validationHolding(counts.validation_, 0.0);
    const size_t blocks = (events - 1) / block + 1;
    for (size_t b = blocks; b-- > 0;) {
        const size_t first = b * block, last = first + std::min(block, events - first);
        const auto slice = CheckpointSlice(ctx.scan_, first, last);
        auto storage = MakeStorage(slice, counts.training_, features, false);
        auto validationStorage = MakeStorage(slice, counts.validation_, features, false);
        workspace->Record(slice, counts.training_, 0, first, last, &storage, training ? &*training : nullptr);
        if (counts.validation_)
            workspace->Record(slice, counts.validation_, counts.training_, first, last, &validationStorage, validation ? &*validation : nullptr);
        BackwardRows_ rows(storage, counts.training_, nullptr), validationRows(validationStorage, counts.validation_, nullptr);
        rows.w_.Swap(&holding);
        validationRows.w_.Swap(&validationHolding);
        Vector_<BackwardRows_*> active{&rows};
        if (counts.validation_)
            active.push_back(&validationRows);
        SweepTrainingEvents(slice, ctx.scan_, result.eventNumeraire_, first, last, ctx.prepared_.Simulation().lsmcBasisDegree_, active,
                            &result.regressions_);
        if (first > 0)
            for (auto* row : active)
                ApplyCheckpointPending(row);
        holding.Swap(&rows.w_);
        validationHolding.Swap(&validationRows.w_);
    }
    return result;
}

TrainingOutcome_ TrainWithMemoryBudget(LsmcContext_& ctx, const PathCounts_& counts) {
    RequireTrainingBudgetDomain(ctx);
    const auto& settings = ctx.prepared_.Simulation();
    const size_t events = ctx.Product().Events().size();
    size_t block = std::min(events, static_cast<size_t>(settings.lsmcCheckpointBlockEvents_.value_or(static_cast<int>(events))));
    const auto plan = PlanTrainingMemory(ctx, counts, block);
    const size_t limit = TrainingMultiply(static_cast<size_t>(*settings.lsmcTrainingMemoryMb_), 1024 * 1024);
    const bool checkpoint =
        settings.lsmcCheckpointBlockEvents_ && CheckpointCompactState(ctx) && plan.checkpoint_ < plan.full_ && plan.checkpoint_ <= limit;
    const size_t bound = checkpoint ? plan.checkpoint_ : plan.full_;
    REQUIRE2(bound <= limit, "LsmcTrainingBudget: neither checkpoint nor full storage fits the complete allocation bound", ScriptError_);
    if (!checkpoint)
        block = events;
    TrainingAllocationBudget_ budget(
        bound, TrainingAdd(plan.fixed_, checkpoint ? plan.rawBlock_
                                                   : TrainingRawRowsBytes(ctx.scan_, TrainingAdd(counts.training_, counts.validation_),
                                                                          ctx.Plan().RegressionFeatureCount())));
    Dal::Detail::OwnedBufferScope_ scope(&budget);
    PassiveTrainingWorkspace_ workspace(ctx);
    auto result = TrainBudgetedRows(ctx, counts, block, checkpoint, &workspace);
    if (ctx.trainingDiagnostics_) {
        auto& diagnostics = *ctx.trainingDiagnostics_;
        diagnostics.checkpointTraining_ = checkpoint;
        diagnostics.trainingAllocationBound_ = bound;
        diagnostics.trainingAllocationPeak_ = budget.PeakCapacityBytes();
        diagnostics.retainedTapeBytes_ = plan.tape_;
        diagnostics.trainingNormalVectors_ = workspace.normalVectors_;
        diagnostics.trainingModelSteps_ = workspace.modelSteps_;
        diagnostics.trainingScriptEvents_ = workspace.scriptEvents_;
    }
    return result;
}
