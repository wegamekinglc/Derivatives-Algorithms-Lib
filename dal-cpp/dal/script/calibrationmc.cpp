#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/gsrcalibration.hpp>
#include <dal/script/calibrationmc.hpp>
#include <dal/script/calibrationmc_internal.hpp>
#include <dal/script/preparation.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;

        CalibrationDiagnostic_
        McDiagnostic(const CalibrationInstrument_& instrument, const String_& measure, CalibrationStage_ stage = CalibrationStage_::Value_::Prepare) {
            CalibrationDiagnostic_ d;
            d.stage_ = stage;
            d.instrumentId_ = instrument.Id();
            d.measureId_ = measure;
            return d;
        }
        Vector_<CalibrationMcBatch_> Ranges(size_t first, int batches, int paths) {
            Vector_<CalibrationMcBatch_> result;
            for (int i = 0; i < batches; ++i)
                result.push_back({first + static_cast<size_t>(i) * paths, static_cast<size_t>(paths)});
            return result;
        }
        void ValidateSampling(const CalibrationMcSettings_& s, const CalibrationDiagnostic_& d) {
            Check(s.seed_ >= 0, "InvalidCalibrationPlan", "/mc/seed", "nonnegative MRG32 seed", "", d);
            Check(s.fitBatches_ >= 2 && s.validationBatches_ >= 2 && s.fitPathsPerBatch_ >= 2 && s.validationPathsPerBatch_ >= 2,
                  "InvalidCalibrationPlan", "/mc/batches", "at least two equal-sized batches of at least two paths in each stream", "", d);
            Check(s.maxPathsPerView_ <= INT_MAX, "InvalidCalibrationPlan", "/mc/max_paths_per_view", "budget within exact count range", "", d);
            const auto fit = static_cast<size_t>(s.fitBatches_) * s.fitPathsPerBatch_;
            const auto val = static_cast<size_t>(s.validationBatches_) * s.validationPathsPerBatch_;
            Check((s.heldOutBatches_ == 0 && s.heldOutPathsPerBatch_ == 0) || (s.heldOutBatches_ >= 2 && s.heldOutPathsPerBatch_ >= 2),
                  "InvalidCalibrationPlan", "/mc/held_out", "disabled or at least two batches of two paths", "", d);
            const auto held = static_cast<size_t>(s.heldOutBatches_) * s.heldOutPathsPerBatch_;
            Check(val > fit && fit <= s.maxPathsPerView_ && val <= s.maxPathsPerView_ - fit, "CalibrationBudgetExceeded", "/mc/max_paths_per_view",
                  "fit plus larger independent validation within explicit budget", "", d);
            Check(held <= s.maxPathsPerView_ - fit - val, "CalibrationBudgetExceeded", "/mc/max_paths_per_view",
                  "fit, validation and held-out within explicit budget", "", d);
        }
        void ValidateSettings(const CalibrationMcSettings_& s, const CalibrationDiagnostic_& d) {
            Check(s.allowMonteCarloCalibration_, "UnsupportedFastEvaluator", "/mc/allow_monte_carlo_calibration", "explicit opt-in", "", d);
            ValidateSampling(s, d);
            Check(s.aad_ || s.parameterLabels_.empty(), "DerivativeUnavailable", "/mc/parameter_labels", "AAD for requested native partials", "", d);
            Check(!s.aad_ || !s.parameterLabels_.empty(), "DerivativeUnavailable", "/mc/parameter_labels", "explicit ordered native labels", "", d);
            Check(std::isfinite(s.annuitySigma_) && s.annuitySigma_ > 0., "InvalidCalibrationPlan", "/mc/annuity_sigma", "finite positive sigma", "",
                  d);
        }
        String_ ExecutionIdentity(const PreparedCalibrationInstrument_& admitted,
                                  const CalibrationMcMapping_& mapping,
                                  const CalibrationMcSettings_& settings) {
            return Fingerprint(Obj({{"executor", Str("dal.script-mc/1")},
                                    {"admission", Str(admitted.PlanIdentity())},
                                    {"mapping", Str(mapping.identity_)},
                                    {"fit_batches", Num(settings.fitBatches_)},
                                    {"fit_paths", Num(settings.fitPathsPerBatch_)},
                                    {"validation_batches", Num(settings.validationBatches_)},
                                    {"validation_paths", Num(settings.validationPathsPerBatch_)},
                                    {"held_out_batches", Num(settings.heldOutBatches_)},
                                    {"held_out_paths", Num(settings.heldOutPathsPerBatch_)},
                                    {"budget", Num(static_cast<double>(settings.maxPathsPerView_))},
                                    {"compiled", Bool(settings.compiled_)},
                                    {"aad", Bool(settings.aad_)},
                                    {"annuity_sigma", Num(settings.annuitySigma_)},
                                    {"labels", Array(settings.parameterLabels_, [](const auto& label) { return Str(label); })}}));
        }
        void RequireScalarTape(bool aad, const CalibrationDiagnostic_& d) {
            Check(!aad || (!AAD::Tape()->multi_ && AAD::Tape()->numAdj_ == 1), "DerivativeUnavailable", "/mc/tape",
                  "scalar native adjoints; caller tape mode is preserved", "", d);
        }
        Vector_<String_> FactorNames(const Handle_<ModelData_>& data) {
            if (typeid(*data) == typeid(MultiFactorGSRModelData_))
                return static_cast<const MultiFactorGSRModelData_&>(*data).vol_->factorNames_;
            return {"legacy"};
        }
        void RequireGCoordinate(const GSRCalibrationParameter_& c, size_t factors, size_t knots) {
            Check(c.factor_ >= 0 && static_cast<size_t>(c.factor_) < factors && c.knot_ >= 0 && static_cast<size_t>(c.knot_) < knots,
                  "DerivativeUnavailable", "/mc/g_coordinates", "existing factor and g knot");
        }
        CalibrationMcMapping_
        MakeMapping(const Script::PreparedScript_& script, const AAD::Model_<double>& model, const Vector_<String_>& factorNames, int seed) {
            CalibrationMcMapping_ m;
            m.seed_ = seed;
            m.timeline_ = script.TimeLine();
            m.dimension_ = model.SimDim();
            double previous = 0.;
            for (size_t i = 0; i < m.timeline_.size(); ++i) {
                if (m.timeline_[i] > previous)
                    for (size_t f = 0; f < model.NumFactors(); ++f)
                        m.normalCoordinates_.push_back("sample:" + OrdinalText(i) + ":factor:" + factorNames[f]);
                previous = m.timeline_[i];
            }
            m.identity_ = Fingerprint(Obj({{"stream_version", Str(m.streamVersion_)},
                                           {"generator", Str(m.generator_)},
                                           {"seed", Num(seed)},
                                           {"precision", Str(m.normalPrecision_)},
                                           {"bridge", Bool(false)},
                                           {"numeraire", Str(m.numeraire_)},
                                           {"timeline", Array(m.timeline_, [](double t) { return Num(t); })},
                                           {"coordinates", Array(m.normalCoordinates_, [](const auto& c) { return Str(c); })}}));
            return m;
        }
        void DerivativeDomain(const PreparedCalibrationInstrument_& admitted,
                              const AAD::Model_<double>& model,
                              const Vector_<String_>& labels,
                              const CalibrationPreparationContext_& c,
                              const CalibrationDiagnostic_& d) {
            const auto columns = CalibrationMcParameterColumns(model.ParameterLabels(), labels);
            for (size_t j = 0; j < columns.size(); ++j)
                if (Bytes(labels[j]).rfind("g:", 0) == 0)
                    Check(*model.Parameters()[columns[j]] > 0., "DerivativeUnavailable", "/mc/parameter_labels",
                          "positive interior g; zero-g boundary requires a separate one-sided method", labels[j], d);
            const auto* option = std::get_if<CalibrationBondOptionDescriptor_>(&admitted.Descriptor());
            if (!option)
                return;
            const auto& gaussian = static_cast<const AAD::GSR_<double>&>(model);
            const double expiry = (option->expiry_ - *c.valuation_.evaluationDate_) / 365.;
            const double maturity = (option->maturity_ - *c.valuation_.evaluationDate_) / 365.;
            const auto loading = gaussian.GaussianBondLoading(expiry, maturity);
            const auto covariance = gaussian.GaussianVariance(expiry);
            double variance = 0.;
            for (size_t i = 0; i < loading.size(); ++i)
                for (size_t j = 0; j < loading.size(); ++j)
                    variance += loading[i] * covariance(static_cast<int>(i), static_cast<int>(j)) * loading[j];
            Check(std::isfinite(variance) && variance > 0., "DerivativeUnavailable", "/mc/model", "nondegenerate Gaussian option distribution", "",
                  d);
        }
        double StandardError(const Vector_<>& values, double mean) {
            double squared = 0.;
            for (double v : values)
                squared += (v - mean) * (v - mean);
            return std::sqrt(squared / (static_cast<double>(values.size()) * (values.size() - 1)));
        }
        void Summarize(CalibrationMcResult_* result) {
            Vector_<> means;
            for (const auto& batch : result->batches_) {
                means.push_back(batch.value_);
                result->mean_ += batch.value_ / result->batches_.size();
            }
            result->standardError_ = StandardError(means, result->mean_);
            REQUIRE(std::isfinite(result->mean_) && std::isfinite(result->standardError_), "Nonfinite batch estimate or uncertainty");
            const size_t n = result->parameterLabels_.size();
            result->gradient_ = Vector_<>(n, 0.);
            result->gradientStandardError_ = Vector_<>(n, 0.);
            result->quoteJacobian_ = Matrix_<>(1, static_cast<int>(n));
            for (size_t j = 0; j < n; ++j) {
                Vector_<> gradients;
                for (const auto& b : result->batches_) {
                    gradients.push_back(b.derivatives_[j]);
                    result->gradient_[j] += b.derivatives_[j] / result->batches_.size();
                }
                result->gradientStandardError_[j] = StandardError(gradients, result->gradient_[j]);
                REQUIRE(std::isfinite(result->gradient_[j]) && std::isfinite(result->gradientStandardError_[j]),
                        "Nonfinite batch gradient or uncertainty");
                result->quoteJacobian_(0, static_cast<int>(j)) = result->gradient_[j];
            }
        }
        template <class E_> void RecordingConstants(E_*) {}
        void RecordingConstants(Script::EvalState_<AAD::Number_>* evaluator) { evaluator->InitRecordingConstants(); }

        template <class T_, class E_>
        std::optional<AAD::Checkpoint_> InitializeRun(
            const Script::PreparedScript_& script, AAD::Model_<T_>* model, E_* evaluator, AAD::Scenario_<T_>* path, AAD::RecordingScope_* recording) {
            AAD::AllocatePath(script.DefLine(), *path);
            if constexpr (std::is_same_v<T_, AAD::Number_>) {
                AAD::Rewind(*AAD::Tape());
                for (auto* parameter : model->Parameters())
                    recording->RegisterInput(*parameter, Value(*parameter));
                for (auto& constant : evaluator->ConstVarVals())
                    recording->RegisterInput(constant, Value(constant));
                recording->StartRecording();
                RecordingConstants(evaluator);
            }
            model->Init(script.TimeLine(), script.DefLine());
            AAD::InitializePath(*path);
            if constexpr (std::is_same_v<T_, AAD::Number_>) {
                script.InitializeHistoricalState(evaluator);
                const auto checkpoint = recording->MakeCheckpoint();
                recording->FinishRecording();
                return checkpoint;
            }
            return {};
        }
        template <class T_>
        void Harvest(T_* root,
                     const AAD::Model_<T_>& model,
                     const Vector_<size_t>& columns,
                     AAD::RecordingScope_* recording,
                     size_t pathIndex,
                     size_t pathCount,
                     CalibrationMcBatchValue_* batch,
                     CalibrationMcResult_* result,
                     bool retain) {
            if constexpr (std::is_same_v<T_, AAD::Number_>) {
                recording->FinishRecording();
                const size_t nodes = AAD::Tape()->nodes_.Size();
                if (nodes > result->peakTapeNodes_) {
                    result->peakTapeNodes_ = nodes;
                    result->peakTapeBytes_ = std::max(result->peakTapeBytes_, AAD::MeasureTape(*AAD::Tape()).capacityBytes_);
                }
                recording->ClearAdjoints();
                Adjoint(*root) = 1.;
                recording->Reverse();
                for (size_t j = 0; j < columns.size(); ++j) {
                    const double derivative = Adjoint(*model.Parameters()[columns[j]]);
                    REQUIRE(std::isfinite(derivative), "Nonfinite script gradient");
                    batch->derivatives_[j] += derivative / pathCount;
                    if (retain)
                        result->pathDerivatives_(static_cast<int>(pathIndex), static_cast<int>(j)) = derivative;
                }
            }
        }
        template <class T_, class E_, class F_>
        void Paths(const Script::PreparedScript_& script,
                   AAD::Model_<T_>* model,
                   const CalibrationMcSettings_& settings,
                   CalibrationMcResult_* result,
                   E_* evaluator,
                   const F_& evaluate,
                   AAD::RecordingScope_* recording) {
            auto rng = New(RNGType_("MRG32"), settings.seed_, model->SimDim(), true);
            Vector_<> normals(model->SimDim());
            AAD::Scenario_<T_> path;
            const auto checkpoint = InitializeRun(script, model, evaluator, &path, recording);
            const auto columns = CalibrationMcParameterColumns(model->ParameterLabels(), settings.parameterLabels_);
            size_t pathIndex = 0;
            if (settings.retainPaths_)
                result->pathDerivatives_ = Matrix_<>(static_cast<int>(result->pathsPerView_), static_cast<int>(columns.size()));
            for (const auto& range : result->ranges_) {
                rng->SkipNormalTo(range.firstPath_);
                CalibrationMcBatchValue_ batch{0., Vector_<>(columns.size(), 0.)};
                for (size_t p = 0; p < range.pathCount_; ++p) {
                    if constexpr (std::is_same_v<T_, AAD::Number_>)
                        recording->Restore(*checkpoint);
                    rng->FillNormal(&normals);
                    model->GeneratePath(normals, &path);
                    REQUIRE(AAD::IsValidModelPath(path), "Invalid model path or numeraire");
                    evaluate(path, *evaluator);
                    auto root = evaluator->VarVals()[script.PayOffIdx()];
                    const double value = Value(root);
                    REQUIRE(std::isfinite(value), "Nonfinite script payoff");
                    batch.value_ += value / range.pathCount_;
                    if (settings.retainPaths_)
                        result->pathValues_.push_back(value);
                    Harvest(&root, *model, columns, recording, pathIndex, range.pathCount_, &batch, result, settings.retainPaths_);
                    ++pathIndex;
                }
                result->batches_.push_back(std::move(batch));
            }
        }
        template <class T_>
        void Run(const Script::PreparedScript_& script,
                 const Handle_<ModelData_>& data,
                 const CalibrationMcSettings_& settings,
                 CalibrationMcResult_* result) {
            std::optional<AAD::RecordingScope_> recording;
            if constexpr (std::is_same_v<T_, AAD::Number_>)
                recording.emplace();
            auto model = CreateModel<T_>(data);
            model->Allocate(script.TimeLine(), script.DefLine());
            if (settings.compiled_) {
                auto evaluator = script.BuildEvalState<T_>();
                Paths(
                    script, model.get(), settings, result, &evaluator, [&](const auto& path, auto& e) { script.CompiledProgram().Evaluate(path, e); },
                    recording ? &*recording : nullptr);
            } else {
                auto evaluator = script.BuildEvaluator<T_>();
                Paths(
                    script, model.get(), settings, result, &evaluator, [&](const auto& path, auto& e) { script.Evaluate(path, e); },
                    recording ? &*recording : nullptr);
            }
        }
        std::unique_ptr<Script::ScriptProduct_> ViewProduct(const PreparedCalibrationInstrument_& admitted, bool a) {
            auto product = admitted.Instrument()->Product();
            size_t i = 0;
            for (const auto& event : product->Events())
                for (const auto& statement : event)
                    if (auto* pays = dynamic_cast<Script::NodePays_*>(statement.get())) {
                        const auto& c = admitted.Cashflows()[i++];
                        pays->arguments_[1] = std::make_unique<Script::NodeConst_>(a ? c.a_ : c.b_);
                    }
            REQUIRE(i == admitted.Cashflows().size(), "Coefficient view must preserve every admitted payment");
            return product;
        }
        void RunView(const Script::PreparedScript_& script,
                     const Handle_<ModelData_>& data,
                     const CalibrationMcSettings_& settings,
                     CalibrationMcResult_* result) {
            if (settings.aad_)
                Run<AAD::Number_>(script, data, settings, result);
            else
                Run<double>(script, data, settings, result);
        }
        void CombineAffine(CalibrationMcResult_* b,
                           const CalibrationMcResult_& a,
                           const CalibrationMeasureSpec_& measure,
                           double sigma,
                           const CalibrationDiagnostic_& d) {
            Vector_<CalibrationAffineCoefficients_> coefficients;
            for (size_t i = 0; i < b->batches_.size(); ++i) {
                const CalibrationAffineCoefficients_ c{a.batches_[i].value_, b->batches_[i].value_, a.batches_[i].derivatives_,
                                                       b->batches_[i].derivatives_};
                coefficients.push_back(c);
                b->batches_[i] = {0., {}, c};
            }
            b->affine_ = CalibrationMcInternal::AffineEstimate(coefficients, *measure.annuitySign_, *measure.minAbsAnnuity_, sigma, d);
            b->mean_ = b->affine_->value_.value_;
            b->standardError_ = b->affine_->standardError_;
            b->gradient_ = b->affine_->value_.derivatives_;
            b->gradientStandardError_ = b->affine_->gradientStandardError_;
            b->quoteJacobian_ = Matrix_<>(1, static_cast<int>(b->gradient_.size()));
            for (size_t j = 0; j < b->gradient_.size(); ++j)
                b->quoteJacobian_(0, static_cast<int>(j)) = b->gradient_[j];
            b->pathValues_.clear();
            b->pathDerivatives_ = {};
            b->engineRuns_ *= 2;
            b->peakTapeNodes_ = std::max(b->peakTapeNodes_, a.peakTapeNodes_);
            b->peakTapeBytes_ = std::max(b->peakTapeBytes_, a.peakTapeBytes_);
        }
    } // namespace

    Vector_<size_t> CalibrationMcParameterColumns(const Vector_<String_>& runtimeLabels,
                                                  const Vector_<String_>& requestedLabels,
                                                  const CalibrationDiagnostic_& diagnostic) {
        Vector_<size_t> result;
        for (const auto& requested : requestedLabels) {
            size_t count = 0, column = 0;
            for (size_t j = 0; j < runtimeLabels.size(); ++j)
                if (CalibrationInternal::Bytes(runtimeLabels[j]) == CalibrationInternal::Bytes(requested)) {
                    ++count;
                    column = j;
                }
            CalibrationInternal::Check(count == 1 && std::find(result.begin(), result.end(), column) == result.end(), "DerivativeUnavailable",
                                       "/mc/parameter_labels", "unique exact native label", requested, diagnostic);
            result.push_back(column);
        }
        return result;
    }

    Vector_<String_> CalibrationMcGLabels(const Handle_<ModelData_>& data, const Vector_<GSRCalibrationParameter_>& coordinates) {
        Vector_<String_> factors;
        Vector_<Date_> knots;
        bool legacy = false;
        if (data && typeid(*data) == typeid(MultiFactorGSRModelData_)) {
            const auto& vol = *static_cast<const MultiFactorGSRModelData_&>(*data).vol_;
            factors = vol.factorNames_;
            knots = vol.gKnotDates_;
        } else if (data && typeid(*data) == typeid(GSRModelData_)) {
            legacy = true;
            factors = {"legacy"};
            knots = static_cast<const GSRModelData_&>(*data).vol_->gKnotDates_;
        } else
            CalibrationInternal::Fail("DerivativeUnavailable", "/mc/model", "native Gaussian g coordinates");
        Vector_<String_> result;
        for (const auto& c : coordinates) {
            RequireGCoordinate(c, factors.size(), knots.size());
            result.push_back("g:" + (legacy ? String_() : factors[c.factor_] + ":") + Date::ToString(knots[c.knot_]));
        }
        const auto model = CreateModel<double>(data);
        static_cast<void>(CalibrationMcParameterColumns(model->ParameterLabels(), result));
        return result;
    }

    struct PreparedCalibrationMc_::Impl_ {
        PreparedCalibrationInstrument_ admitted_;
        std::shared_ptr<const Script::PreparedScript_> script_, a_;
        CalibrationPreparationContext_ context_;
        CalibrationMcSettings_ settings_;
        CalibrationMcMapping_ mapping_;
        Vector_<CalibrationMcBatch_> fit_, validation_, heldOut_;
        String_ identity_;
    };

    PreparedCalibrationMc_ PrepareCalibrationMc(const Handle_<CalibrationInstrument_>& instrument,
                                                const String_& measure,
                                                const CalibrationPreparationContext_& context,
                                                const CalibrationMcSettings_& settings) {
        Check(static_cast<bool>(instrument), "InvalidCalibrationPlan", "/instrument", "nonnull instrument");
        const auto d = McDiagnostic(*instrument, measure);
        ValidateSettings(settings, d);
        CalibrationEvaluationOptions_ admission;
        admission.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
        admission.allowMonteCarloCalibration_ = true;
        admission.derivativeMethod_ = settings.aad_ ? CalibrationDerivative_::Value_::AAD : CalibrationDerivative_::Value_::None;
        admission.sampling_ = CalibrationSamplingSpec_{settings.fitPathsPerBatch_, settings.seed_, "mrg32"};
        auto admitted = PrepareCalibrationInstrument(instrument, measure, context, admission);
        try {
            auto model = CreateModel<double>(context.model_);
            static_cast<void>(CalibrationMcParameterColumns(model->ParameterLabels(), settings.parameterLabels_, d));
            if (settings.aad_)
                DerivativeDomain(admitted, *model, settings.parameterLabels_, context, d);
            Script::MonteCarloSettings_ simulation;
            simulation.rsg_ = "mrg32";
            simulation.compiled_ = settings.compiled_;
            const bool affine = admitted.Measure().kind_ != CalibrationMeasureKind_::Value_::Price;
            auto script = Script::Detail::PrepareBoundScript(affine ? ViewProduct(admitted, false) : instrument->Product(), model.get(),
                                                             context.valuation_, simulation, instrument->Definition()->Spec().source_.settings_);
            std::optional<Script::PreparedScript_> a;
            if (affine)
                a = Script::Detail::PrepareBoundScript(ViewProduct(admitted, true), model.get(), context.valuation_, simulation,
                                                       instrument->Definition()->Spec().source_.settings_);
            Check(script.Product().VarNames()[script.PayOffIdx()] == instrument->Definition()->Spec().header_.receiver_,
                  "InvalidCalibrationOutputBinding", "/receiver", "admitted receiver equals prepared payoff slot", "", d);
            auto mapping = MakeMapping(script, *model, FactorNames(context.model_), settings.seed_);
            Check(mapping.normalCoordinates_.size() == mapping.dimension_, "InvalidCalibrationPlan", "/mc/mapping", "complete normal coordinates", "",
                  d);
            const size_t fitPaths = static_cast<size_t>(settings.fitBatches_) * settings.fitPathsPerBatch_;
            const auto identity = ExecutionIdentity(admitted, mapping, settings);
            return PreparedCalibrationMc_(std::make_shared<PreparedCalibrationMc_::Impl_>(
                PreparedCalibrationMc_::Impl_{std::move(admitted), std::make_shared<const Script::PreparedScript_>(std::move(script)),
                                              a ? std::make_shared<const Script::PreparedScript_>(std::move(*a)) : nullptr, context, settings,
                                              std::move(mapping), Ranges(0, settings.fitBatches_, settings.fitPathsPerBatch_),
                                              Ranges(fitPaths, settings.validationBatches_, settings.validationPathsPerBatch_),
                                              Ranges(fitPaths + static_cast<size_t>(settings.validationBatches_) * settings.validationPathsPerBatch_,
                                                     settings.heldOutBatches_, settings.heldOutPathsPerBatch_),
                                              identity}));
        } catch (const CalibrationError_&) {
            throw;
        } catch (const Exception_& error) {
            Fail("InvalidCalibrationPlan", "/mc/preparation", "executable admitted Gaussian script", String_(error.what()), d);
        }
    }
    const CalibrationPreparationCapability_& PreparedCalibrationMc_::Capability() const { return impl_->admitted_.Capability(); }
    const CalibrationMcMapping_& PreparedCalibrationMc_::Mapping() const { return impl_->mapping_; }
    const String_& PreparedCalibrationMc_::PlanIdentity() const { return impl_->identity_; }
    const Vector_<CalibrationMcBatch_>& PreparedCalibrationMc_::Batches(bool validation) const {
        return validation ? impl_->validation_ : impl_->fit_;
    }

    PreparedCalibrationMc_ PreparedCalibrationMc_::RefreshG(const CalibrationPreparationContext_& context) const {
        const auto& instrument = impl_->admitted_.Instrument();
        const auto d = McDiagnostic(*instrument, impl_->admitted_.Measure().id_);
        auto frozen = context;
        frozen.model_ = impl_->context_.model_;
        impl_->admitted_.RequireCompatible(instrument, frozen);
        Check(context.model_ && typeid(*context.model_) == typeid(MultiFactorGSRModelData_) &&
                  typeid(*impl_->context_.model_) == typeid(MultiFactorGSRModelData_),
              "CalibrationContextMismatch", "/mc/candidate", "MultiFactor Gaussian g-only candidate", "", d);
        const auto& original = static_cast<const MultiFactorGSRModelData_&>(*impl_->context_.model_);
        const auto& candidate = static_cast<const MultiFactorGSRModelData_&>(*context.model_);
        const auto& v = *candidate.vol_;
        Check(v.gValues_.Rows() == original.vol_->gValues_.Rows() && v.gValues_.Cols() == original.vol_->gValues_.Cols(),
              "CalibrationContextMismatch", "/mc/candidate/g", "unchanged g layout", "", d);
        const Handle_<MultiFactorGSRVolData_> normalizedVol(
            new MultiFactorGSRVolData_(v.name_, v.factorNames_, v.gKnotDates_, original.vol_->gValues_, v.hKnotDates_, v.hValues_, v.correlations_));
        const MultiFactorGSRModelData_ normalized(candidate.name_, candidate.curve_, normalizedVol);
        Check(Bytes(JSON::WriteString(normalized)) == Bytes(JSON::WriteString(original)), "CalibrationContextMismatch", "/mc/candidate",
              "only g values may change; fixed curve, geometry, H and correlations", "", d);
        CalibrationEvaluationOptions_ options;
        options.evaluation_ = CalibrationEvaluation_::Value_::ScriptMC;
        options.allowMonteCarloCalibration_ = true;
        options.derivativeMethod_ = impl_->settings_.aad_ ? CalibrationDerivative_::Value_::AAD : CalibrationDerivative_::Value_::None;
        options.sampling_ = impl_->admitted_.Sampling();
        auto admitted = PrepareCalibrationInstrument(instrument, impl_->admitted_.Measure().id_, context, options);
        auto model = CreateModel<double>(context.model_);
        model->Allocate(impl_->script_->TimeLine(), impl_->script_->DefLine());
        if (impl_->settings_.aad_)
            DerivativeDomain(admitted, *model, impl_->settings_.parameterLabels_, context, d);
        const auto mapping = MakeMapping(*impl_->script_, *model, FactorNames(context.model_), impl_->settings_.seed_);
        Check(Bytes(mapping.identity_) == Bytes(impl_->mapping_.identity_), "CalibrationContextMismatch", "/mc/mapping",
              "unchanged timeline and normal coordinates", "", d);
        const auto identity = ExecutionIdentity(admitted, mapping, impl_->settings_);
        return PreparedCalibrationMc_(std::make_shared<Impl_>(Impl_{std::move(admitted), impl_->script_, impl_->a_, context, impl_->settings_,
                                                                    mapping, impl_->fit_, impl_->validation_, impl_->heldOut_, identity}));
    }

    CalibrationMcResult_ PreparedCalibrationMc_::Evaluate(const Handle_<CalibrationInstrument_>& instrument,
                                                          const CalibrationPreparationContext_& context,
                                                          bool validation) const {
        return EvaluateRanges(instrument, context, validation, false);
    }
    CalibrationMcResult_ PreparedCalibrationMc_::EvaluateHeldOut(const Handle_<CalibrationInstrument_>& instrument,
                                                                 const CalibrationPreparationContext_& context) const {
        return EvaluateRanges(instrument, context, true, true);
    }
    CalibrationMcResult_ PreparedCalibrationMc_::EvaluateRanges(const Handle_<CalibrationInstrument_>& instrument,
                                                                const CalibrationPreparationContext_& context,
                                                                bool validation,
                                                                bool heldOut) const {
        impl_->admitted_.RequireCompatible(instrument, context);
        const auto d = McDiagnostic(*instrument, impl_->admitted_.Measure().id_, CalibrationStage_::Value_::Evaluate);
        RequireScalarTape(impl_->settings_.aad_, d);
        try {
            CalibrationMcResult_ result;
            result.instrumentIdentity_ = instrument->InstrumentIdentity();
            result.contextIdentity_ = impl_->admitted_.ContextIdentity();
            result.mappingIdentity_ = impl_->mapping_.identity_;
            result.planIdentity_ = impl_->identity_;
            result.measureId_ = impl_->admitted_.Measure().id_;
            result.validation_ = validation;
            result.heldOut_ = heldOut;
            result.parameterLabels_ = impl_->settings_.parameterLabels_;
            result.ranges_ = heldOut ? impl_->heldOut_ : Batches(validation);
            Check(!result.ranges_.empty(), "InvalidCalibrationPlan", "/mc/held_out", "explicit held-out sampling", "", d);
            result.pathsPerView_ = result.ranges_.size() * result.ranges_[0].pathCount_;
            result.engineRuns_ = result.ranges_.size();
            RunView(*impl_->script_, context.model_, impl_->settings_, &result);
            if (impl_->a_) {
                auto a = result;
                a.batches_.clear();
                a.pathValues_.clear();
                RunView(*impl_->a_, context.model_, impl_->settings_, &a);
                CombineAffine(&result, a, impl_->admitted_.Measure(), impl_->settings_.annuitySigma_, d);
            } else
                Summarize(&result);
            return result;
        } catch (const CalibrationError_&) {
            throw;
        } catch (const Exception_& error) {
            Fail("CalibrationNumericalFailure", "/mc/execution", "finite script values and native partials", String_(error.what()), d);
        }
    }
} // namespace Dal
