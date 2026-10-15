//
// Created by Codex on 2026/10/15.
//

#include <charconv>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

// A separate static executable can reuse the native regression/replay internals without exporting a production checkpoint API.
#include "../../dal/script/lsmc.cpp"

namespace Dal::Script::CheckpointStudy {
    struct Options_ {
        std::string schedule_ = "verify";
        std::string mode_ = "hard";
        size_t events_ = 17;
        size_t block_ = 4;
        size_t training_ = 257;
        size_t validation_ = 0;
        size_t pricing_ = 129;
        bool growingVector_ = false;
        bool bridge_ = false;
    };

    void SetCount(const std::string& name, const std::string& value, Options_* options) {
        size_t number = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        REQUIRE(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size(), "invalid count");
        const std::pair<const char*, size_t Options_::*> fields[] = {{"--events", &Options_::events_},
                                                                     {"--block", &Options_::block_},
                                                                     {"--training", &Options_::training_},
                                                                     {"--validation", &Options_::validation_},
                                                                     {"--pricing", &Options_::pricing_}};
        for (const auto& field : fields) {
            if (name == field.first) {
                options->*field.second = number;
                return;
            }
        }
        THROW("unknown option " + name);
    }

    bool ReadBool(const std::string& value) {
        REQUIRE(value == "true" || value == "false", "expected true or false");
        return value == "true";
    }

    void SetOption(const std::string& name, const std::string& value, Options_* options) {
        const std::pair<const char*, std::string Options_::*> strings[] = {{"--schedule", &Options_::schedule_}, {"--mode", &Options_::mode_}};
        for (const auto& field : strings) {
            if (name == field.first) {
                options->*field.second = value;
                return;
            }
        }
        const std::pair<const char*, bool Options_::*> flags[] = {{"--growing-vector", &Options_::growingVector_}, {"--bridge", &Options_::bridge_}};
        for (const auto& field : flags) {
            if (name == field.first) {
                options->*field.second = ReadBool(value);
                return;
            }
        }
        SetCount(name, value, options);
    }

    void ValidateModes(const Options_& options) {
        REQUIRE(options.schedule_ == "verify" || options.schedule_ == "materialized" || options.schedule_ == "checkpoint", "invalid schedule");
        REQUIRE(options.mode_ == "hard" || options.mode_ == "frozen" || options.mode_ == "retrained", "invalid mode");
    }

    void ValidateCounts(const Options_& options) {
        REQUIRE(options.events_ >= 2 && options.events_ <= 256 && options.block_ > 0, "invalid event/block count");
        REQUIRE(options.training_ > 0 && options.training_ <= 1000000 && options.validation_ <= 1000000, "invalid training/validation count");
        REQUIRE(options.pricing_ > 0 && options.pricing_ <= 1000000, "invalid pricing count");
    }

    Options_ ReadOptions(int argc, char** argv) {
        REQUIRE(argc % 2 == 1, "expected --option value pairs");
        Options_ options;
        for (int i = 1; i < argc; i += 2)
            SetOption(argv[i], argv[i + 1], &options);
        ValidateModes(options);
        ValidateCounts(options);
        return options;
    }

    ScriptProductData_ Product(const Options_& options) {
        ScriptProductSettings_ settings;
        settings.regressionFeatures_ = {"VAR[s]", "VAR[avg]", "VAR[peak]"};
        Vector_<Cell_> dates{Cell_("K"), Cell_(Date_(2023, 12, 31))};
        Vector_<String_> events{"100", "total = K n = 1 peak = K APPEND(history, K)"};
        for (size_t e = 0; e < options.events_; ++e) {
            dates.push_back(Cell_(Date_(2024, 1, 1).AddDays(static_cast<int>(7 * (e + 1)))));
            String_ event = "s = SPOT() total = total + s n = n + 1 peak = MAX(peak, s) ";
            event += options.growingVector_ ? "APPEND(history, s) avg = AVERAGE(history) " : "avg = (total + SUM(history)) / (n + 1) ";
            event += "pay PAYS 0.001 * avg ";
            if (e + 1 < options.events_)
                event += "EXERCISE MAX(K - avg, 0) IF s < 125";
            else
                event += "pay PAYS MAX(K - avg, 0)";
            events.push_back(event);
        }
        return {"checkpoint path-state study", dates, events, settings};
    }

    MonteCarloSettings_ Simulation(const Options_& options, bool production = false) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        simulation.smooth_ = 2.0;
        simulation.useBb_ = options.bridge_;
        simulation.lsmcBasisDegree_ = 2;
        simulation.lsmcTrainingPaths_ = static_cast<int>(options.training_);
        simulation.lsmcValidationPaths_ = static_cast<int>(options.validation_);
        simulation.lsmcRqmcReplicates_ = 2;
        simulation.lsmcTrainingSeed_ = 17;
        simulation.lsmcPricingSeed_ = 29;
        if (production) {
            simulation.lsmcTrainingMemoryMb_ = 512;
            simulation.lsmcCheckpointBlockEvents_ = static_cast<int>(options.block_);
        }
        return simulation;
    }

    struct Work_ {
        size_t normalVectors_ = 0;
        size_t normalCoordinates_ = 0;
        size_t modelSteps_ = 0;
        size_t bootstrapModelSteps_ = 0;
        size_t scriptEvents_ = 0;
        size_t peakStorageBytes_ = 0;
        size_t peakCheckpointBytes_ = 0;
        size_t rowValuesCompared_ = 0;
        size_t sampleValuesCompared_ = 0;
        size_t stateValuesCompared_ = 0;
        size_t negativeControls_ = 0;
    };

    struct Workload_ {
        Options_ options_;
        Handle_<ModelData_> modelData_{new BSModelData_("", 100.0, 0.2, 0.05, 0.02)};
        std::unique_ptr<AAD::Model_<double>> model_ = CreateModel<double>(modelData_);
        PreparedScript_ prepared_;
        ScriptCompiled_ hardCompiled_;
        LsmcPlan_ scan_;
        PathCounts_ counts_;
        Scenario_<> pathTemplate_;
        Vector_<> numeraires_;

        explicit Workload_(const Options_& options, bool production = false)
            : options_(options), prepared_(PrepareScript(Product(options), model_.get(), Valuation(), Simulation(options, production))),
              hardCompiled_(ScriptCompiled_::Build(prepared_.Product().Events(), false, prepared_.PlanHandle(), false, true)),
              scan_(ScanEvents(prepared_.Product().Events(), prepared_.Simulation().smooth_)),
              counts_(LsmcPathCounts(prepared_.Simulation(), options.pricing_)) {
            REQUIRE(prepared_.TimeLine().size() == options.events_, "prototype requires one future model sample per event");
            for (size_t e = 0; e < options.events_; ++e)
                REQUIRE(prepared_.Plan().EventToSample()[e] == e && prepared_.TimeLine()[e] > 0.0, "unsupported observation grid");
            model_->Allocate(prepared_.TimeLine(), prepared_.DefLine());
            model_->Init(prepared_.TimeLine(), prepared_.DefLine());
            AllocatePath(prepared_.DefLine(), pathTemplate_);
            InitializePath(pathTemplate_);
            model_->GeneratePath(Vector_<>(model_->SimDim(), 0.0), &pathTemplate_);
            for (const auto& sample : pathTemplate_)
                numeraires_.push_back(sample.numeraire_);
        }

        static ScriptValuationSettings_ Valuation() {
            ScriptValuationSettings_ valuation;
            valuation.evaluationDate_ = Date_(2024, 1, 1);
            return valuation;
        }
    };

    struct Checkpoint_ {
        double logSpot_ = 0.0;
        Vector_<> variables_;
        Vector_<Vector_<>> vectors_;

        [[nodiscard]] size_t HeapBytes() const {
            size_t bytes = variables_.capacity() * sizeof(double) + vectors_.capacity() * sizeof(Vector_<>);
            for (const auto& values : vectors_)
                bytes += values.capacity() * sizeof(double);
            return bytes;
        }
    };

    struct Checkpoints_ {
        // Addresses are implicit: pathOffset + path; the boundary is block * blockSize, before that event.
        const PreparedScript_* prepared_;
        size_t paths_;
        size_t pathOffset_;
        size_t blockSize_;
        uint64_t scrambleKey_ = LsmcScrambleKey(false, 17);
        Vector_<> parameters_;
        Vector_<> constants_;
        std::vector<Checkpoint_> states_;

        Checkpoints_(const Workload_& workload, const AAD::Model_<double>& model, const Vector_<>* constants, size_t paths, size_t offset)
            : prepared_(&workload.prepared_), paths_(paths), pathOffset_(offset), blockSize_(workload.options_.block_),
              constants_(constants ? *constants : workload.prepared_.Product().ConstVarValues()),
              states_(((workload.options_.events_ - 1) / blockSize_ + 1) * paths) {
            for (const auto* value : model.Parameters())
                parameters_.push_back(*value);
        }

        [[nodiscard]] size_t Bytes() const {
            size_t bytes =
                sizeof(*this) + states_.capacity() * sizeof(Checkpoint_) + (parameters_.capacity() + constants_.capacity()) * sizeof(double);
            for (const auto& state : states_)
                bytes += state.HeapBytes();
            return bytes;
        }
    };

    size_t StorageBytes(const LsmcStorage_& storage) {
        size_t bytes = sizeof(storage) + storage.condByDay_.capacity() * sizeof(Vector_<char>);
        for (const auto* rows : {&storage.pays_, &storage.xByDay_, &storage.hByDay_, &storage.discountByEvent_})
            bytes += rows->Rows() * rows->Paths() * sizeof(double);
        for (const auto& row : storage.condByDay_)
            bytes += row.capacity() * sizeof(char);
        return bytes;
    }

    struct Workspace_ {
        const Workload_& workload_;
        AAD::Model_<double>* model_;
        std::unique_ptr<Random_> random_;
        Vector_<> gauss_;
        Scenario_<> path_;
        EvalState_<double> state_;
        LsmcSinks_ sinks_;
        Vector_<AAD::Detail::BlackScholesCoefficients_<double>> steps_;

        Workspace_(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, Work_* work)
            : workload_(workload), model_(model), random_(CreateRNG(workload.prepared_.Simulation().rsg_,
                                                                    *model,
                                                                    workload.options_.bridge_,
                                                                    LsmcScrambleKey(false, 17),
                                                                    workload.prepared_.Simulation().normalPrecision_)),
              gauss_(model->SimDim()), path_(workload.pathTemplate_), state_(workload.prepared_.BuildEvalState<double>()) {
            model->GeneratePath(Vector_<>(model->SimDim(), 0.0), &path_);
            work->modelSteps_ += model->SimDim();
            work->bootstrapModelSteps_ += model->SimDim();
            const auto& parameters = model->Parameters();
            double previous = 0.0;
            for (double time : workload.prepared_.TimeLine()) {
                steps_.push_back(AAD::Detail::BlackScholesCoefficients(*parameters[1], *parameters[2] - *parameters[3], time - previous));
                previous = time;
            }
            if (constants) {
                state_.ConstVarVals() = *constants;
                PastEvaluator_<double> past(Vector_<>(workload.prepared_.Product().VarNames().size(), 0.0), *constants,
                                            workload.prepared_.Product().VectorCapacities());
                past.SetObservations(&workload.prepared_.Plan());
                workload.prepared_.Product().Visit(past, true, false);
                state_.SetHistoricalSeed(past.VarVals());
                state_.SetHistoricalVectorSeed(past.VectorVals());
            }
            state_.observations_ = &workload.prepared_.Plan();
            state_.scenario_ = &path_;
            state_.lsmcSinks_ = &sinks_;
            sinks_.plan_ = &workload.prepared_.Plan();
            sinks_.payoffIdx_ = workload.prepared_.PayOffIdx();
        }

        void Bind(const LsmcPlan_& scan, LsmcStorage_* storage) {
            sinks_.eventToPays_ = &scan.eventToPays_;
            sinks_.eventToExercise_ = &scan.eventToExercise_;
            sinks_.pays_ = storage ? &storage->pays_ : nullptr;
            sinks_.x_ = storage ? &storage->xByDay_ : nullptr;
            sinks_.h_ = storage ? &storage->hByDay_ : nullptr;
            sinks_.cond_ = storage && !storage->condByDay_.empty() ? &storage->condByDay_ : nullptr;
        }

        void Random(size_t address, Work_* work) {
            random_->SkipTo(address);
            random_->FillNormal(&gauss_);
            ++work->normalVectors_;
            work->normalCoordinates_ += gauss_.size();
        }

        double Advance(size_t first, size_t last, double logSpot, Work_* work) {
            for (size_t e = first; e < last; ++e) {
                logSpot += steps_[e].drift_ + steps_[e].std_ * gauss_[e];
                path_[e].spot_ = Dal::exp(logSpot);
                std::fill(path_[e].observations_.begin(), path_[e].observations_.end(), path_[e].spot_);
                ++work->modelSteps_;
            }
            return logSpot;
        }

        void Event(size_t event, size_t path, Work_* work) {
            sinks_.eventOrdinal_ = event;
            sinks_.pathSlot_ = path;
            const auto& program = workload_.hardCompiled_;
            const Detail::CompiledEventView_<double> view{program.NodeStreams()[event], program.ConstStreams()[event], path_[event]};
            Detail::EvalCompiledEvents<true, true>(1, [&](size_t) { return view; }, &state_);
            REQUIRE(state_.dStack_.IsEmpty() && state_.bStack_.IsEmpty(), "checkpoint must be at an empty-stack event boundary");
            ++work->scriptEvents_;
        }

        [[nodiscard]] Checkpoint_ Save(double logSpot) const { return {logSpot, state_.variables_, state_.vectors_}; }
        void Restore(const Checkpoint_& checkpoint) {
            state_.Init();
            state_.variables_ = checkpoint.variables_;
            state_.vectors_ = checkpoint.vectors_;
        }
    };

    void Equal(double actual, double expected, const char* label) {
        REQUIRE(std::memcmp(&actual, &expected, sizeof(double)) == 0, std::string("checkpoint mismatch: ") + label);
    }

    void CompareState(const Checkpoint_& actual, const Checkpoint_& expected, Work_* work) {
        Equal(actual.logSpot_, expected.logSpot_, "model accumulator");
        REQUIRE(actual.variables_.size() == expected.variables_.size() && actual.vectors_.size() == expected.vectors_.size(), "state shape mismatch");
        for (size_t j = 0; j < actual.variables_.size(); ++j) {
            Equal(actual.variables_[j], expected.variables_[j], "scalar state");
            ++work->stateValuesCompared_;
        }
        for (size_t v = 0; v < actual.vectors_.size(); ++v) {
            REQUIRE(actual.vectors_[v].size() == expected.vectors_[v].size(), "vector length mismatch");
            for (size_t j = 0; j < actual.vectors_[v].size(); ++j) {
                Equal(actual.vectors_[v][j], expected.vectors_[v][j], "vector state");
                ++work->stateValuesCompared_;
            }
        }
    }

    Checkpoints_
    Capture(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, size_t paths, size_t offset, Work_* work) {
        Checkpoints_ checkpoints(workload, *model, constants, paths, offset);
        Workspace_ workspace(workload, model, constants, work);
        workspace.Bind(workload.scan_, nullptr);
        for (size_t p = 0; p < paths; ++p) {
            workspace.Random(offset + p, work);
            workspace.state_.Init();
            double logSpot = Dal::log(*model->Parameters()[0]);
            for (size_t e = 0; e < workload.options_.events_; ++e) {
                if (e % checkpoints.blockSize_ == 0)
                    checkpoints.states_[(e / checkpoints.blockSize_) * paths + p] = workspace.Save(logSpot);
                logSpot = workspace.Advance(e, e + 1, logSpot, work);
                workspace.Event(e, p, work);
            }
        }
        return checkpoints;
    }

    LsmcPlan_ Slice(const LsmcPlan_& scan, size_t first, size_t last) {
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

    LsmcStorage_
    Materialize(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, size_t paths, size_t offset, Work_* work) {
        if (paths == 0)
            return {};
        auto storage = MakeStorage(workload.scan_, paths, 3, false);
        storage.ZeroPaths(0, paths);
        Workspace_ workspace(workload, model, constants, work);
        workspace.Bind(workload.scan_, &storage);
        for (size_t p = 0; p < paths; ++p) {
            workspace.Random(offset + p, work);
            model->GeneratePath(workspace.gauss_, &workspace.path_);
            work->modelSteps_ += model->SimDim();
            workspace.state_.Init();
            for (size_t e = 0; e < workload.options_.events_; ++e)
                workspace.Event(e, p, work);
        }
        return storage;
    }

    void CompareRows(const LsmcStorage_& actual,
                     const LsmcStorage_& expected,
                     const LsmcPlan_& slice,
                     const LsmcPlan_& scan,
                     size_t first,
                     size_t last,
                     Work_* work) {
        if (actual.pays_.Paths() == 0)
            return;
        auto compare = [&](const double* a, const double* b) {
            for (size_t p = 0; p < actual.pays_.Paths(); ++p) {
                Equal(a[p], b[p], "recording row");
                ++work->rowValuesCompared_;
            }
        };
        for (size_t e = first; e < last; ++e) {
            if (slice.eventToPays_[e] != NO_SLOT)
                compare(actual.pays_[slice.eventToPays_[e]], expected.pays_[scan.eventToPays_[e]]);
            const size_t day = slice.eventToExercise_[e];
            if (day == NO_SLOT)
                continue;
            const size_t fullDay = scan.eventToExercise_[e];
            compare(actual.hByDay_[day], expected.hByDay_[fullDay]);
            for (size_t j = 0; j < 3; ++j)
                compare(actual.xByDay_[day * 3 + j], expected.xByDay_[fullDay * 3 + j]);
            for (size_t p = 0; p < actual.pays_.Paths(); ++p) {
                REQUIRE(actual.condByDay_[day][p] == expected.condByDay_[fullDay][p], "condition mismatch");
                ++work->rowValuesCompared_;
            }
        }
    }

    void ValidateCheckpoint(
        const Workload_& workload, const AAD::Model_<double>& model, const Vector_<>* constants, const Checkpoints_& checkpoints, size_t first) {
        REQUIRE(checkpoints.prepared_ == &workload.prepared_ && first % checkpoints.blockSize_ == 0, "checkpoint preparation/boundary mismatch");
        REQUIRE(checkpoints.scrambleKey_ == LsmcScrambleKey(false, 17), "checkpoint stream mismatch");
        const auto& constantValues = constants ? *constants : workload.prepared_.Product().ConstVarValues();
        REQUIRE(checkpoints.constants_ == constantValues, "checkpoint constant point mismatch");
        REQUIRE(checkpoints.parameters_.size() == model.Parameters().size(), "checkpoint model schema mismatch");
        for (size_t j = 0; j < checkpoints.parameters_.size(); ++j)
            Equal(*model.Parameters()[j], checkpoints.parameters_[j], "checkpoint model point");
    }

    void CompareSamples(const Workspace_& workspace, AAD::Model_<double>* model, size_t first, size_t last, Work_* work) {
        auto nativePath = workspace.workload_.pathTemplate_;
        model->GeneratePath(workspace.gauss_, &nativePath);
        for (size_t e = first; e < last; ++e) {
            Equal(workspace.path_[e].spot_, nativePath[e].spot_, "native path spot");
            Equal(workspace.path_[e].numeraire_, nativePath[e].numeraire_, "native path numeraire");
            work->sampleValuesCompared_ += 2;
            for (size_t j = 0; j < nativePath[e].observations_.size(); ++j) {
                Equal(workspace.path_[e].observations_[j], nativePath[e].observations_[j], "native observation");
                ++work->sampleValuesCompared_;
            }
        }
    }

    LsmcStorage_ Regenerate(const Workload_& workload,
                            AAD::Model_<double>* model,
                            const Vector_<>* constants,
                            const Checkpoints_& checkpoints,
                            const LsmcPlan_& slice,
                            size_t first,
                            size_t last,
                            Work_* work,
                            bool verify) {
        if (checkpoints.paths_ == 0)
            return {};
        ValidateCheckpoint(workload, *model, constants, checkpoints, first);
        auto storage = MakeStorage(slice, checkpoints.paths_, 3, false);
        storage.ZeroPaths(0, checkpoints.paths_);
        Workspace_ workspace(workload, model, constants, work);
        workspace.Bind(slice, &storage);
        for (size_t p = 0; p < checkpoints.paths_; ++p) {
            workspace.Random(checkpoints.pathOffset_ + p, work);
            const auto& checkpoint = checkpoints.states_[(first / checkpoints.blockSize_) * checkpoints.paths_ + p];
            workspace.Restore(checkpoint);
            const double logSpot = workspace.Advance(first, last, checkpoint.logSpot_, work);
            if (verify)
                CompareSamples(workspace, model, first, last, work);
            for (size_t e = first; e < last; ++e)
                workspace.Event(e, p, work);
            if (verify && last < workload.options_.events_)
                CompareState(workspace.Save(logSpot), checkpoints.states_[(last / checkpoints.blockSize_) * checkpoints.paths_ + p], work);
        }
        return storage;
    }

    void ApplyPending(BackwardRows_* rows) {
        if (!rows->pending_.regression_)
            return;
        const auto& pending = rows->pending_;
        const LocalPredictor_ predict(*pending.regression_);
        for (size_t p = 0; p < rows->Size(); ++p) {
            std::array<double, 3> features{};
            for (size_t j = 0; j < pending.nFeatures_; ++j)
                features[j] = pending.features_[j][p];
            rows->w_[p] = Select((rows->included_[p] != 0) & (pending.h_[p] > predict(features)), pending.h_[p], rows->w_[p]);
        }
        rows->pending_ = {};
    }

    Vector_<ExerciseRegression_> TrainMaterialized(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, Work_* work) {
        const size_t n = workload.counts_.training_, v = workload.counts_.validation_;
        const auto& scan = workload.scan_;
        auto storage = MakeStorage(scan, n, 3, false);
        LsmcContext_ ctx{workload.prepared_, model, scan, storage, &workload.hardCompiled_, nullptr, LsmcScrambleKey(false, 17), constants};
        RunForwardPhase(ctx, BatchPlan_(n, 1));
        const auto numeraires = SampleGridNumeraires(ctx);
        LsmcStorage_ validation;
        if (v) {
            validation = MakeStorage(scan, v, 3, false);
            LsmcContext_ validationCtx{workload.prepared_,         model,    scan, validation, &workload.hardCompiled_, nullptr,
                                       LsmcScrambleKey(false, 17), constants};
            RunForwardPhase(validationCtx, BatchPlan_(v, 1), n);
        }
        // Native forward phases visit every event; the extra path is their deterministic-numeraire probe.
        work->normalVectors_ += n + v + 1;
        work->normalCoordinates_ += (n + v + 1) * model->SimDim();
        work->modelSteps_ += (n + v + 1) * model->SimDim();
        work->scriptEvents_ += (n + v) * workload.options_.events_;
        work->peakStorageBytes_ = std::max(work->peakStorageBytes_, StorageBytes(storage) + StorageBytes(validation) + (n + v) * 9);
        return RunBackwardPhase(ctx, numeraires, n, 2, v ? &validation : nullptr, v);
    }

    void SweepBlock(const Workload_& workload,
                    const LsmcPlan_& slice,
                    size_t first,
                    size_t last,
                    const Scenario_<>& numeraires,
                    const Vector_<BackwardRows_*>& active,
                    Vector_<ExerciseRegression_>* policies) {
        for (size_t e = last; e-- > first;) {
            const bool hasNext = e + 1 < workload.options_.events_;
            const double discount = hasNext ? numeraires[e].numeraire_ / numeraires[e + 1].numeraire_ : 1.0;
            std::array<IncludedSums_, 2> sums{};
            for (size_t j = 0; j < active.size(); ++j)
                sums[j] = SweepAndClear(slice, e, discount, hasNext, active[j]);
            const size_t localDay = slice.eventToExercise_[e];
            if (localDay != NO_SLOT) {
                auto& policy = (*policies)[workload.scan_.eventToExercise_[e]];
                policy = FitExerciseDay(active, sums, localDay, 2);
                for (auto* row : active)
                    DeferDecision(policy, localDay, row);
            }
        }
    }

    void BackwardBlock(const Workload_& workload,
                       const LsmcPlan_& slice,
                       size_t first,
                       size_t last,
                       const Scenario_<>& numeraires,
                       const LsmcStorage_& storage,
                       const LsmcStorage_& validationStorage,
                       Vector_<ExerciseRegression_>* policies,
                       Vector_<>* w,
                       Vector_<>* validationW) {
        BackwardRows_ rows(storage, workload.counts_.training_, nullptr), validationRows(validationStorage, workload.counts_.validation_, nullptr);
        rows.w_.Swap(w);
        validationRows.w_.Swap(validationW);
        Vector_<BackwardRows_*> active{&rows};
        if (workload.counts_.validation_)
            active.push_back(&validationRows);
        SweepBlock(workload, slice, first, last, numeraires, active, policies);
        if (first > 0)
            for (auto* row : active)
                ApplyPending(row);
        w->Swap(&rows.w_);
        validationW->Swap(&validationRows.w_);
    }

    Vector_<ExerciseRegression_>
    TrainCheckpoint(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, Work_* work, bool verify) {
        const size_t n = workload.counts_.training_, v = workload.counts_.validation_;
        const auto& scan = workload.scan_;
        const auto training = Capture(workload, model, constants, n, 0, work);
        const auto validation = Capture(workload, model, constants, v, n, work);
        const size_t checkpointBytes = training.Bytes() + validation.Bytes();
        work->peakCheckpointBytes_ = std::max(work->peakCheckpointBytes_, checkpointBytes);
        LsmcStorage_ reference, referenceValidation;
        if (verify) {
            Work_ excluded;
            reference = Materialize(workload, model, constants, n, 0, &excluded);
            referenceValidation = Materialize(workload, model, constants, v, n, &excluded);
        }
        Vector_<ExerciseRegression_> policies(scan.days_.size());
        Vector_<> w(n, 0.0), validationW(v, 0.0);
        const auto numeraires = Workspace_(workload, model, constants, work).path_;
        const size_t blocks = (workload.options_.events_ - 1) / workload.options_.block_ + 1;
        for (size_t b = blocks; b-- > 0;) {
            const size_t first = b * workload.options_.block_, last = std::min(workload.options_.events_, first + workload.options_.block_);
            const auto slice = Slice(scan, first, last);
            auto storage = Regenerate(workload, model, constants, training, slice, first, last, work, verify);
            auto validationStorage = Regenerate(workload, model, constants, validation, slice, first, last, work, verify);
            if (verify) {
                CompareRows(storage, reference, slice, scan, first, last, work);
                CompareRows(validationStorage, referenceValidation, slice, scan, first, last, work);
            }
            work->peakStorageBytes_ =
                std::max(work->peakStorageBytes_, checkpointBytes + StorageBytes(storage) + StorageBytes(validationStorage) + (n + v) * 17);
            BackwardBlock(workload, slice, first, last, numeraires, storage, validationStorage, &policies, &w, &validationW);
        }
        return policies;
    }

    Vector_<ExerciseRegression_>
    Train(const Workload_& workload, AAD::Model_<double>* model, const Vector_<>* constants, bool checkpoint, Work_* work, bool verify = false) {
        return checkpoint ? TrainCheckpoint(workload, model, constants, work, verify) : TrainMaterialized(workload, model, constants, work);
    }

    void ComparePolicyMetadata(const ExerciseRegression_& a, const ExerciseRegression_& b) {
        REQUIRE(a.coefficients_.size() == b.coefficients_.size(), "policy coefficient count mismatch");
        REQUIRE(a.powers_ == b.powers_, "policy powers mismatch");
        REQUIRE(a.basisDegree_ == b.basisDegree_, "policy degree mismatch");
        REQUIRE(a.means_.size() == b.means_.size(), "policy mean count mismatch");
        REQUIRE(a.sigmas_.size() == b.sigmas_.size(), "policy sigma count mismatch");
        REQUIRE(a.numCondTrue_ == b.numCondTrue_, "policy condition count mismatch");
        REQUIRE(a.effectiveRank_ == b.effectiveRank_, "policy rank mismatch");
        REQUIRE(a.solver_ == b.solver_, "policy solver mismatch");
        REQUIRE(a.degenerate_ == b.degenerate_, "policy degeneracy mismatch");
        REQUIRE(a.degenerateReason_ == b.degenerateReason_, "policy degeneracy reason mismatch");
        REQUIRE(a.fallbackReason_ == b.fallbackReason_, "policy fallback reason mismatch");
        REQUIRE(a.validationMse_ == b.validationMse_, "policy validation MSE mismatch");
    }

    void ComparePolicy(const Vector_<ExerciseRegression_>& actual, const Vector_<ExerciseRegression_>& expected) {
        REQUIRE(actual.size() == expected.size(), "policy date count mismatch");
        for (size_t e = 0; e < actual.size(); ++e) {
            const auto& a = actual[e];
            const auto& b = expected[e];
            ComparePolicyMetadata(a, b);
            Equal(a.mean_, b.mean_, "policy scalar mean");
            Equal(a.sigma_, b.sigma_, "policy scalar sigma");
            for (size_t j = 0; j < a.coefficients_.size(); ++j)
                Equal(a.coefficients_[j], b.coefficients_[j], "policy coefficient");
            for (size_t j = 0; j < a.means_.size(); ++j) {
                Equal(a.means_[j], b.means_[j], "policy mean");
                Equal(a.sigmas_[j], b.sigmas_[j], "policy sigma");
            }
        }
    }

    SimResults_ Price(const Workload_& workload, const Vector_<ExerciseRegression_>& policy, bool fuzzy, Vector_<size_t>* exerciseCounts) {
        const auto& prepared = workload.prepared_;
        const auto& counts = workload.counts_;
        const BatchPlan_ batches(workload.options_.pricing_, 1);
        SimResults_ result(Vector::Join(workload.model_->ParameterLabels(), prepared.Product().ConstVarNames()));
        Vector_<> risks(result.risks_.size(), 0.0);
        for (size_t r = 0; r < counts.replicates_; ++r) {
            const uint64_t key = LsmcScrambleKey(true, 29, r);
            const size_t offset = counts.pricingOffset_ + r * workload.options_.pricing_;
            if (fuzzy) {
                AccumulateFuzzyReplicate(prepared, workload.modelData_, workload.scan_, policy, &prepared.CompiledProgram(true), batches, offset, key,
                                         &result, &risks);
            } else {
                LsmcStorage_ empty;
                LsmcContext_ ctx{prepared, workload.model_.get(), workload.scan_, empty, &workload.hardCompiled_, &policy, key};
                const auto replay = RunReplayPhase(ctx, batches, offset, policy);
                result.aggregated_ += replay.sum_;
                if (exerciseCounts->empty())
                    *exerciseCounts = Vector_<size_t>(replay.exerciseCounts_.size(), 0);
                for (size_t e = 0; e < replay.exerciseCounts_.size(); ++e)
                    (*exerciseCounts)[e] += replay.exerciseCounts_[e];
            }
        }
        result.aggregated_ /= static_cast<double>(counts.replicates_);
        for (size_t j = 0; j < risks.size(); ++j)
            result.risks_[j] = risks[j] / static_cast<double>(counts.replicates_ * workload.options_.pricing_);
        return result;
    }

    void RetrainedRisks(const Workload_& workload, bool checkpoint, SimResults_* result, Work_* work) {
        const auto& prepared = workload.prepared_;
        const auto& constants = prepared.Product().ConstVarValues();
        const size_t modelInputs = workload.model_->NumParams();
        for (size_t input = 0; input < modelInputs + constants.size(); ++input) {
            const double value = input < modelInputs ? *workload.model_->Parameters()[input] : constants[input - modelInputs];
            const double step = prepared.Simulation().lsmcPolicyBumpRelative_ * std::max(1.0, std::abs(value));
            std::array<double, 2> values{};
            for (size_t side = 0; side < 2; ++side) {
                auto model = workload.model_->Clone();
                auto bumpedConstants = constants;
                const double bump = side == 0 ? step : -step;
                if (input < modelInputs)
                    *model->Parameters()[input] += bump;
                else
                    bumpedConstants[input - modelInputs] += bump;
                model->Init(prepared.TimeLine(), prepared.DefLine());
                const auto policy = Train(workload, model.get(), &bumpedConstants, checkpoint, work);
                values[side] = ValueFuzzyPolicy(prepared, workload.modelData_, workload.scan_, policy, &prepared.CompiledProgram(true),
                                                BatchPlan_(workload.options_.pricing_, 1), workload.counts_, workload.options_.pricing_);
            }
            result->risks_[input] += (values[0] - values[1]) / (2.0 * step);
        }
    }

    void CompareResult(const SimResults_& actual, const SimResults_& expected) {
        Equal(actual.aggregated_, expected.aggregated_, "price");
        REQUIRE(actual.risks_.size() == expected.risks_.size(), "risk count mismatch");
        for (size_t j = 0; j < actual.risks_.size(); ++j)
            Equal(actual.risks_[j], expected.risks_[j], "risk");
    }

    void NegativeControls(const Workload_& workload, Work_* work) {
        auto checkpoints = Capture(workload, workload.model_.get(), nullptr, 1, 0, work);
        if (checkpoints.states_.size() < 2)
            return;
        const size_t first = workload.options_.block_, last = std::min(workload.options_.events_, first + workload.options_.block_);
        const auto slice = Slice(workload.scan_, first, last);
        const auto reference = Regenerate(workload, workload.model_.get(), nullptr, checkpoints, slice, first, last, work, false);
        const auto original = checkpoints.states_[1];
        checkpoints.states_[1].variables_ = checkpoints.states_[0].variables_;
        checkpoints.states_[1].vectors_ = checkpoints.states_[0].vectors_;
        const auto badState = Regenerate(workload, workload.model_.get(), nullptr, checkpoints, slice, first, last, work, false);
        bool stateDiffers = false;
        for (size_t row = 0; row < reference.xByDay_.Rows(); ++row)
            stateDiffers |= reference.xByDay_[row][0] != badState.xByDay_[row][0];
        REQUIRE(stateDiffers, "negative control failed to detect missing script state");
        checkpoints.states_[1] = original;
        checkpoints.states_[1].logSpot_ = checkpoints.states_[0].logSpot_;
        const auto badModel = Regenerate(workload, workload.model_.get(), nullptr, checkpoints, slice, first, last, work, false);
        REQUIRE(reference.xByDay_[0][0] != badModel.xByDay_[0][0], "negative control failed to detect missing model state");
        checkpoints.states_[1] = original;
        ++checkpoints.pathOffset_;
        const auto badAddress = Regenerate(workload, workload.model_.get(), nullptr, checkpoints, slice, first, last, work, false);
        REQUIRE(reference.xByDay_[0][0] != badAddress.xByDay_[0][0], "negative control failed to detect wrong random address");
        auto bumpedModel = workload.model_->Clone();
        *bumpedModel->Parameters()[0] += 1.0;
        bool rejected = false;
        try {
            static_cast<void>(Regenerate(workload, bumpedModel.get(), nullptr, checkpoints, slice, first, last, work, false));
        } catch (const Exception_&) {
            rejected = true;
        }
        REQUIRE(rejected, "checkpoint must reject a changed model point");
        work->negativeControls_ += 4;
    }

    void Run(const Options_& options) {
        const Workload_ workload(options);
        Work_ work;
        const auto start = std::chrono::steady_clock::now();
        const bool checkpoint = options.schedule_ != "materialized";
        const auto policy = Train(workload, workload.model_.get(), nullptr, checkpoint, &work, options.schedule_ == "verify");
        Vector_<size_t> exerciseCounts;
        auto result = Price(workload, policy, options.mode_ != "hard", &exerciseCounts);
        if (options.mode_ == "retrained")
            RetrainedRisks(workload, checkpoint, &result, &work);
        if (options.schedule_ == "verify") {
            Work_ excluded;
            const auto baseline = Train(workload, workload.model_.get(), nullptr, false, &excluded);
            ComparePolicy(policy, baseline);
            auto storage = MakeStorage(workload.scan_, options.training_, 3, false);
            LsmcContext_ native{workload.prepared_,        workload.model_.get(), workload.scan_, storage, &workload.hardCompiled_, nullptr,
                                LsmcScrambleKey(false, 17)};
            ComparePolicy(policy, TrainFrozenPolicy(native, workload.counts_).regressions_);
            Vector_<size_t> actualCounts, referenceCounts;
            CompareResult(Price(workload, policy, false, &actualCounts), Price(workload, baseline, false, &referenceCounts));
            REQUIRE(actualCounts == referenceCounts, "exercise count mismatch");
            auto frozen = Price(workload, policy, true, &referenceCounts);
            auto baselineFrozen = Price(workload, baseline, true, &referenceCounts);
            CompareResult(frozen, baselineFrozen);
            RetrainedRisks(workload, true, &frozen, &excluded);
            RetrainedRisks(workload, false, &baselineFrozen, &excluded);
            CompareResult(frozen, baselineFrozen);
            auto nativeResult = MCLsmcAadSimulation(workload.prepared_, workload.modelData_, options.pricing_);
            CompareResult(Price(workload, policy, true, &referenceCounts), nativeResult);
            AddRetrainedPolicyRisks(workload.prepared_, workload.modelData_, *workload.model_, workload.scan_, baseline, &workload.hardCompiled_,
                                    &workload.prepared_.CompiledProgram(true), BatchPlan_(options.pricing_, 1), workload.counts_,
                                    LsmcScrambleKey(false, 17), options.pricing_, &nativeResult);
            CompareResult(frozen, nativeResult);
            NegativeControls(workload, &work);
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::setprecision(17) << "{\"schedule\":\"" << options.schedule_ << "\",\"mode\":\"" << options.mode_
                  << "\",\"events\":" << options.events_ << ",\"block\":" << options.block_ << ",\"training\":" << options.training_
                  << ",\"validation\":" << options.validation_ << ",\"pricing\":" << options.pricing_ << ",\"seconds\":" << elapsed
                  << ",\"peak_owned_storage_bytes\":" << work.peakStorageBytes_ << ",\"checkpoint_bytes\":" << work.peakCheckpointBytes_
                  << ",\"normal_vectors\":" << work.normalVectors_ << ",\"normal_coordinates\":" << work.normalCoordinates_
                  << ",\"model_steps\":" << work.modelSteps_ << ",\"bootstrap_model_steps\":" << work.bootstrapModelSteps_
                  << ",\"script_events\":" << work.scriptEvents_ << ",\"compared_rows\":" << work.rowValuesCompared_
                  << ",\"compared_samples\":" << work.sampleValuesCompared_ << ",\"compared_state_values\":" << work.stateValuesCompared_
                  << ",\"negative_controls\":" << work.negativeControls_ << ",\"pv\":" << result.aggregated_ / static_cast<double>(options.pricing_)
                  << ",\"risks\":[";
        for (size_t j = 0; j < result.risks_.size(); ++j) {
            if (j)
                std::cout << ',';
            std::cout << result.risks_[j];
        }
        std::cout << "]}\n";
    }
} // namespace Dal::Script::CheckpointStudy

#ifndef DAL_LSMC_STUDY_NO_MAIN
int main(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init();
        Dal::ThreadPool_::GetInstance()->Start(1, true);
        Dal::Script::CheckpointStudy::Run(Dal::Script::CheckpointStudy::ReadOptions(argc, argv));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
