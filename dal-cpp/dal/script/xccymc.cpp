#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/aad/statistics.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/script/xccymc_internal.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        using XccyMcInternal::CouponViews_;
        CouponViews_ CouponViews(const PreparedCalibrationInstrument_& admitted) {
            Check(std::holds_alternative<CalibrationXccy_>(admitted.Descriptor()), "MCCrossCurrencyUnsupported", "/instrument",
                  "complete admitted XCCY source");
            const auto& declaration = admitted.Instrument()->Definition()->Spec().parameters_[1];
            double lo = declaration.lower_.value_or(-std::numeric_limits<double>::max());
            double hi = declaration.upper_.value_or(std::numeric_limits<double>::max());
            if (declaration.lower_ && !declaration.lowerInclusive_)
                lo = std::nextafter(lo, std::numeric_limits<double>::infinity());
            if (declaration.upper_ && !declaration.upperInclusive_)
                hi = std::nextafter(hi, -std::numeric_limits<double>::infinity());
            Check(lo <= hi, "CalibrationNumericalFailure", "/execution/affine_binding_interval", "representable coupon interval");
            double low = std::clamp(0., lo, hi), high = std::clamp(1., lo, hi);
            if (low == high) {
                const double coupon = std::get<CalibrationXccy_>(admitted.Descriptor())->coupon_;
                low = std::clamp(coupon - 1., lo, hi);
                high = std::clamp(coupon + 1., lo, hi);
            }
            if (low == high) {
                low = lo;
                high = hi;
            }
            Check(std::isfinite(high - low) && high > low, "CalibrationNumericalFailure", "/execution/affine_binding_interval",
                  "two distinct finite admissible coupon bindings for affine coefficients");
            return {low, high};
        }
        Handle_<CalibrationInstrument_> Rebind(const Handle_<CalibrationInstrument_>& instrument, double coupon) {
            CalibrationParameterBindings_ bindings;
            bindings.values_.push_back({"K", coupon, CalibrationValueUnit_::Parse("BasisSpread")});
            return BindCalibrationInstrumentParameters(instrument, bindings);
        }
        CalibrationPreparationContext_ Freeze(const PreparedCalibrationInstrument_& admitted, const CalibrationPreparationContext_& context) {
            Check(context.xccy_ && std::holds_alternative<CalibrationXccy_>(admitted.Descriptor()), "MCCrossCurrencyUnsupported", "/instrument",
                  "complete admitted XCCY source and dedicated execution context");
            auto frozen = context;
            auto xccy = *context.xccy_;
            xccy.fixings_ = std::get<CalibrationXccy_>(admitted.Descriptor())->fixings_;
            frozen.xccy_ = std::make_shared<const XccyScriptContext_>(std::move(xccy));
            return frozen;
        }
        CalibrationMcMapping_ Mapping(const AAD::XccyGaussian_<double>& model, int seed) {
            CalibrationMcMapping_ mapping;
            mapping.dimension_ = model.SimDim();
            mapping.seed_ = seed;
            mapping.numeraire_ = "dal.domestic-collateral-bank/1";
            for (size_t i = 0; i < model.Grid().size(); ++i) {
                mapping.timeline_.push_back((model.Grid()[i] - model.Grid().front()) / 365.);
                if (i == 0)
                    continue;
                for (const auto& slot : model.InnovationSlots())
                    mapping.normalCoordinates_.push_back("interval:" + Date::ToString(model.Grid()[i - 1]) + ":" + Date::ToString(model.Grid()[i]) +
                                                         ":innovation:" + slot);
            }
            Check(mapping.normalCoordinates_.size() == mapping.dimension_, "CalibrationContextMismatch", "/sampling/mapping",
                  "five explicit innovation coordinates per advancing interval");
            mapping.identity_ = XccyMcInternal::MappingIdentity(mapping);
            return mapping;
        }
        [[noreturn]] void OwnFailure(const Handle_<CalibrationInstrument_>& instrument,
                                     const String_& measure,
                                     const Handle_<XccyGaussianModelData_>& candidate,
                                     const CalibrationError_& error,
                                     CalibrationStage_ stage) {
            auto diagnostic = error.Failure().diagnostic_;
            diagnostic.instrumentId_ = instrument ? instrument->Id() : "";
            diagnostic.measureId_ = measure;
            diagnostic.stage_ = stage;
            const Handle_<CalibrationReport_> report(new CalibrationReport_(stage, {}, {diagnostic}));
            throw CalibrationError_(
                __FILE__, __LINE__, __func__,
                std::make_shared<const CalibrationFailure_>(CalibrationFailure_{diagnostic, report, Handle_<Storable_>(candidate)}));
        }
        template <class T_> struct PathPrice_ {
            T_ value_{0.};
            double roundingBound_ = 0.;
        };
        template <class T_> PathPrice_<T_> Price(const Script::PreparedXccyScript_& script, const Script::XccyScriptPath_<T_>& path, bool compiled) {
            PathPrice_<T_> result;
            const auto payments = script.EvaluatePayments(path, compiled);
            for (const auto& payment : payments) {
                result.value_ += payment.discountedAmount_;
                result.roundingBound_ += std::abs(Value(payment.discountedAmount_));
            }
            result.roundingBound_ *= (1. + 2. * static_cast<double>(payments.size())) * std::numeric_limits<double>::epsilon();
            return result;
        }
        template <class T_> void RequireResolved(const PathPrice_<T_>& low, const PathPrice_<T_>& high, const CouponViews_& views) {
            const double epsilon = std::numeric_limits<double>::epsilon();
            const double rounding = low.roundingBound_ + high.roundingBound_ + epsilon * (std::abs(Value(low.value_)) + std::abs(Value(high.value_)));
            const double resolution = std::sqrt(epsilon) * std::max(views.high_ - views.low_, std::abs(Value(high.value_) - Value(low.value_)));
            CalibrationDiagnostic_ diagnostic;
            diagnostic.message_ = "Admissible coupon price subtraction cannot resolve affine coefficients above floating-point cancellation";
            Check(std::isfinite(rounding), "CalibrationNumericalFailure", "/execution/affine_coefficients", "finite gross cashflow rounding bound",
                  "nonfinite rounding bound", diagnostic);
            Check(rounding <= resolution, "CalibrationNumericalFailure", "/execution/affine_coefficients",
                  "cashflow rounding bound within sqrt(machine epsilon) coefficient resolution",
                  String_("rounding_bound=" + Jcs(Num(rounding)) + "; resolution=" + Jcs(Num(resolution))), diagnostic);
        }
        template <class T_> std::unique_ptr<AAD::RecordingScope_> Recording() {
            if constexpr (std::is_same_v<T_, AAD::Number_>) {
                Check(!AAD::Tape()->multi_ && AAD::Tape()->numAdj_ == 1, "DerivativeUnavailable", "/derivatives/tape", "scalar native tape");
                try {
                    auto recording = std::make_unique<AAD::RecordingScope_>();
                    recording->StartRecording();
                    return recording;
                } catch (const Exception_& error) {
                    Fail("DerivativeUnavailable", "/derivatives/tape", "available independent scalar recording", String_(error.what()));
                }
            }
            return {};
        }
        template <class T_> class Execution_ {
            std::unique_ptr<AAD::RecordingScope_> recording_;
            Script::XccyScriptPathProvider_<T_> zero_, unit_;
            std::optional<AAD::Checkpoint_> checkpoint_;
            Matrix_<> nativeMap_;
            Vector_<size_t> columns_;

            Vector_<> Gradient(T_* root) {
                Vector_<> values(columns_.size(), 0.);
                if constexpr (std::is_same_v<T_, AAD::Number_>) {
                    recording_->ClearAdjoints();
                    Adjoint(*root) = 1.;
                    recording_->Reverse();
                    for (size_t j = 0; j < columns_.size(); ++j)
                        for (size_t row = 0; row < zero_.Model().Parameters().size(); ++row)
                            values[j] += nativeMap_(static_cast<int>(row), static_cast<int>(columns_[j])) *
                                         (Adjoint(*zero_.Model().Parameters()[row]) + Adjoint(*unit_.Model().Parameters()[row]));
                }
                return values;
            }
            void Finish(CalibrationMcResult_* estimate) {
                if constexpr (std::is_same_v<T_, AAD::Number_>) {
                    recording_->FinishRecording();
                    estimate->peakTapeNodes_ = std::max(estimate->peakTapeNodes_, static_cast<size_t>(AAD::Tape()->nodes_.Size()));
                    estimate->peakTapeBytes_ = std::max(estimate->peakTapeBytes_, AAD::MeasureTape(*AAD::Tape()).capacityBytes_);
                }
            }

        public:
            Execution_(const Script::PreparedXccyScript_& zero,
                       const Script::PreparedXccyScript_& unit,
                       const Handle_<XccyGaussianModelData_>& data,
                       const XccyMcSettings_& settings)
                : recording_(Recording<T_>()), zero_(zero, data), unit_(unit, data) {
                if constexpr (std::is_same_v<T_, AAD::Number_>) {
                    Vector_<String_> labels;
                    nativeMap_ = XccyGaussianMomentNativeMap(*data, &labels);
                    Check(zero_.Model().ParameterLabels() == XccyGaussianMomentInputs(*data).labels_, "CalibrationContextMismatch",
                          "/derivatives/mapping", "runtime and snapshot map have identical ordered axes");
                    columns_ = CalibrationMcParameterColumns(labels, settings.parameterLabels_);
                    checkpoint_ = recording_->MakeCheckpoint();
                    recording_->FinishRecording();
                }
            }
            void RequireMapping(size_t dimension) const {
                Check(zero_.Model().SimDim() == dimension && zero_.Model().Grid() == unit_.Model().Grid(), "CalibrationContextMismatch",
                      "/sampling/mapping", "unchanged prepared grid and five-slot mapping");
            }
            CalibrationAffineCoefficients_ Coefficients(const Script::PreparedXccyScript_& zero,
                                                        const Script::PreparedXccyScript_& unit,
                                                        const Vector_<>& normals,
                                                        const CouponViews_& views,
                                                        bool compiled,
                                                        CalibrationMcResult_* estimate) {
                if constexpr (std::is_same_v<T_, AAD::Number_>)
                    recording_->Restore(*checkpoint_);
                const auto low = Price(zero, zero_.GeneratePath(normals), compiled);
                const auto high = Price(unit, unit_.GeneratePath(normals), compiled);
                T_ aRoot = (high.value_ - low.value_) / (views.high_ - views.low_);
                T_ bRoot = low.value_ - views.low_ * aRoot;
                const double b = Value(bRoot), a = Value(aRoot);
                Check(std::isfinite(a) && std::isfinite(b), "CalibrationNumericalFailure", "/execution/payment", "finite source A/B");
                RequireResolved(low, high, views);
                Finish(estimate);
                const auto db = Gradient(&bRoot), da = Gradient(&aRoot);
                return {a, b, da, db};
            }
        };
    } // namespace

    void XccyMcInternal::Accumulate(const CalibrationAffineCoefficients_& c, size_t paths, CalibrationMcBatchValue_* batch) {
        batch->coefficients_->a_ += c.a_ / paths;
        batch->coefficients_->b_ += c.b_ / paths;
        for (size_t j = 0; j < c.da_.size(); ++j) {
            Check(std::isfinite(c.da_[j]) && std::isfinite(c.db_[j]), "CalibrationNumericalFailure", "/derivatives", "finite native partials");
            batch->coefficients_->da_[j] += c.da_[j] / paths;
            batch->coefficients_->db_[j] += c.db_[j] / paths;
        }
    }
    void XccyMcInternal::FinishBatch(double coupon, CalibrationMcBatchValue_* batch) {
        batch->value_ = batch->coefficients_->b_ + coupon * batch->coefficients_->a_;
        for (size_t j = 0; j < batch->coefficients_->db_.size(); ++j)
            batch->derivatives_.push_back(batch->coefficients_->db_[j] + coupon * batch->coefficients_->da_[j]);
    }
    XccyMcInternal::Member_::Member_(const Handle_<CalibrationInstrument_>& instrument,
                                     const String_& measure,
                                     const CalibrationPreparationContext_& context,
                                     const Handle_<XccyGaussianModelData_>& model,
                                     const XccyMcSettings_& settings)
        : admitted_(PrepareCalibrationInstrument(instrument, measure, context)), views_(CouponViews(admitted_)),
          frozenContext_(Freeze(admitted_, context)), zero_(Rebind(instrument, views_.low_), frozenContext_, model),
          unit_(Rebind(instrument, views_.high_), frozenContext_, model), model_(model), settings_(settings),
          coupon_(std::get<CalibrationXccy_>(admitted_.Descriptor())->coupon_) {
        const Script::XccyScriptPathProvider_<> provider(zero_);
        mapping_ = Dal::Mapping(provider.Model(), settings.seed_);
        reservation_ = PrepareXccyMcReservation(settings, mapping_);
        identity_ = Fingerprint(Obj({{"admission", Str(admitted_.PlanIdentity())},
                                     {"model", Str(model->Identity())},
                                     {"mapping", Str(mapping_.identity_)},
                                     {"reservation", Str(reservation_.identity_)},
                                     {"compiled", Bool(settings.compiled_)},
                                     {"aad", Bool(settings.aad_)},
                                     {"labels", Array(settings.parameterLabels_, [](const auto& x) { return Str(x); })},
                                     {"rule", Str("dal.xccy-gaussian-script-mc/1")}}));
    }

    namespace {
        template <class T_> class BoundExecution_ final : public XccyMcInternal::CoefficientExecution_ {
            const XccyMcInternal::Member_& member_;
            Execution_<T_> execution_;

        public:
            BoundExecution_(const XccyMcInternal::Member_& member, const Handle_<XccyGaussianModelData_>& data)
                : member_(member), execution_(member.zero_, member.unit_, data, member.settings_) {
                execution_.RequireMapping(member.mapping_.dimension_);
            }
            CalibrationAffineCoefficients_ Next(const Vector_<>& normals, CalibrationMcResult_* estimate) override {
                return execution_.Coefficients(member_.zero_, member_.unit_, normals, member_.views_, member_.settings_.compiled_, estimate);
            }
        };
    } // namespace
    void XccyMcInternal::RequireDerivatives(const XccyGaussianModelData_& model, const XccyMcSettings_& settings) {
        Check(settings.aad_ != settings.parameterLabels_.empty(), "DerivativeUnavailable", "/derivatives",
              "AAD with explicit native labels, or None without labels");
        if (!settings.aad_)
            return;
        const auto inputs = XccyGaussianMomentInputs(model);
        Vector_<String_> labels;
        static_cast<void>(XccyGaussianMomentNativeMap(model, &labels));
        static_cast<void>(CalibrationMcParameterColumns(labels, settings.parameterLabels_));
        for (size_t i = inputs.sigmaOffsets_[0]; i < inputs.values_.size(); ++i)
            Check(inputs.values_[i] > 0., "DerivativeUnavailable", "/derivatives/sigma", "interior nonzero-rank volatility configuration");
    }
    void XccyMcInternal::Member_::Preflight(const Handle_<XccyGaussianModelData_>& data) const {
        model_->RequireSameGeometry(*data);
        RequireDerivatives(*data, settings_);
        const Script::XccyScriptPathProvider_<> low(zero_, data), high(unit_, data);
        const auto mapping = Dal::Mapping(low.Model(), settings_.seed_);
        Check(Bytes(mapping.identity_) == Bytes(mapping_.identity_) && mapping.dimension_ == mapping_.dimension_ &&
                  low.Model().Grid() == high.Model().Grid(),
              "CalibrationContextMismatch", "/sampling/mapping", "unchanged full dated mapping");
        // Each independent recording is destroyed here, before the next member is preflighted.
        static_cast<void>(Execution(data));
    }
    std::unique_ptr<XccyMcInternal::CoefficientExecution_> XccyMcInternal::Member_::Execution(const Handle_<XccyGaussianModelData_>& data) const {
        if (settings_.aad_)
            return std::make_unique<BoundExecution_<AAD::Number_>>(*this, data);
        return std::make_unique<BoundExecution_<double>>(*this, data);
    }

    struct PreparedXccyMc_::Impl_ : XccyMcInternal::Member_ {
        using Member_::Member_;
        template <class T_> [[nodiscard]] XccyMcResult_ Run(const Handle_<XccyGaussianModelData_>& data) const {
            Execution_<T_> execution(zero_, unit_, data, settings_);
            execution.RequireMapping(mapping_.dimension_);
            XccyMcResult_ result;
            result.mapping_ = mapping_;
            result.reservation_ = reservation_;
            result.modelIdentity_ = data->Identity();
            auto& estimate = result.estimate_;
            estimate.instrumentIdentity_ = admitted_.Instrument()->InstrumentIdentity();
            estimate.contextIdentity_ = Fingerprint(Obj({{"source_context", Str(admitted_.ContextIdentity())}, {"model", Str(data->Identity())}}));
            estimate.mappingIdentity_ = mapping_.identity_;
            estimate.planIdentity_ = Fingerprint(Obj({{"preparation", Str(identity_)}, {"evaluation", Str(estimate.contextIdentity_)}}));
            estimate.measureId_ = admitted_.Measure().id_;
            estimate.parameterLabels_ = settings_.parameterLabels_;
            estimate.validation_ = settings_.phase_ == XccyMonteCarloPhase_::Value_::Validation;
            estimate.heldOut_ = settings_.phase_ == XccyMonteCarloPhase_::Value_::HeldOut;
            auto rng = New(RNGType_("MRG32"), settings_.seed_, mapping_.dimension_, true);
            Vector_<> normals(mapping_.dimension_);
            const size_t n = settings_.parameterLabels_.size();
            for (const auto& range : reservation_.ranges_) {
                rng->SkipNormalTo(range.firstPath_);
                const size_t paths = range.pathEnd_ - range.firstPath_;
                CalibrationMcBatchValue_ batch;
                batch.coefficients_ = CalibrationAffineCoefficients_{0., 0., Vector_<>(n, 0.), Vector_<>(n, 0.)};
                for (size_t path = 0; path < paths; ++path) {
                    if (!normals.empty()) {
                        rng->FillNormal(&normals);
                        result.consumedCoordinates_ += normals.size();
                    }
                    if (path == 0)
                        result.firstNormals_.push_back(normals);
                    const auto coefficients = execution.Coefficients(zero_, unit_, normals, views_, settings_.compiled_, &estimate);
                    XccyMcInternal::Accumulate(coefficients, paths, &batch);
                    Retain(coefficients, &result);
                }
                XccyMcInternal::FinishBatch(coupon_, &batch);
                estimate.batches_.push_back(batch);
                estimate.ranges_.push_back({range.firstPath_, paths});
                estimate.pathsPerView_ += paths;
            }
            estimate.engineRuns_ = 2 * estimate.batches_.size();
            XccyMcInternal::Summarize(&result, admitted_, coupon_);
            result.report_ = XccyMcInternal::Report(result, admitted_, *data, settings_);
            return result;
        }
        void Retain(const CalibrationAffineCoefficients_& coefficients, XccyMcResult_* result) const {
            if (!settings_.retainPaths_)
                return;
            result->pathCoefficients_.push_back(coefficients);
            if (admitted_.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
                result->estimate_.pathValues_.push_back(coefficients.b_ + coupon_ * coefficients.a_);
        }
    };

    PreparedXccyMc_::PreparedXccyMc_(const Handle_<CalibrationInstrument_>& instrument,
                                     const String_& measure,
                                     const CalibrationPreparationContext_& context,
                                     const Handle_<XccyGaussianModelData_>& model,
                                     const XccyMcSettings_& settings) try {
        Check(static_cast<bool>(model), "InvalidCalibrationPlan", "/model", "explicit immutable XCCY model");
        CalibrationMcMapping_ preliminary;
        preliminary.seed_ = settings.seed_;
        preliminary.identity_ = "preflight";
        static_cast<void>(PrepareXccyMcReservation(settings, preliminary));
        XccyMcInternal::RequireDerivatives(*model, settings);
        impl_ = std::make_shared<const Impl_>(instrument, measure, context, model, settings);
    } catch (const CalibrationError_& error) {
        OwnFailure(instrument, measure, model, error, CalibrationStage_::Value_::Prepare);
    }
    const CalibrationMcMapping_& PreparedXccyMc_::Mapping() const { return impl_->mapping_; }
    const XccyMcReservation_& PreparedXccyMc_::Reservation() const { return impl_->reservation_; }
    const String_& PreparedXccyMc_::PlanIdentity() const { return impl_->identity_; }
    XccyMcResult_ PreparedXccyMc_::Evaluate(const Handle_<XccyGaussianModelData_>& candidate) const {
        Handle_<XccyGaussianModelData_> validated;
        try {
            const auto data = candidate ? candidate : impl_->model_;
            impl_->model_->RequireSameGeometry(*data);
            validated = data;
            if (impl_->settings_.aad_)
                return impl_->Run<AAD::Number_>(data);
            return impl_->Run<double>(data);
        } catch (const CalibrationError_& error) {
            OwnFailure(impl_->admitted_.Instrument(), impl_->admitted_.Measure().id_, validated, error, CalibrationStage_::Value_::Evaluate);
        }
    }
} // namespace Dal
