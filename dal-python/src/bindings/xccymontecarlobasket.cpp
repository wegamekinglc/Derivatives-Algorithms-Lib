#include "bindings.h"

#include "calibrationinput.hpp"
#include "giltestbarrier.hpp"
#include <dal-public/src/xccymontecarlobasket.hpp>
#include <pybind11/eval.h>

using namespace Dal;
using namespace Dal::Python;
using namespace Dal::Python::Calibration;

#ifdef DAL_XCCY_TEST_BRIDGE
void init_xccy_mc_basket_test(py::module_& m);
void XccyBasketThrowForTesting(const std::string& fault,
                               const Vector_<XccyMcBasketMember_>& members,
                               const Handle_<XccyGaussianModelData_>& model,
                               const XccyMonteCarloBasketRequest_& request);
#endif

namespace {
    PyObject* basketErrorType = nullptr;
    thread_local bool barrierArmed = false;
    GilTestBarrier_ barrier;
    constexpr const char* GIL_TIMEOUT = "XCCY basket GIL barrier timed out";

    [[noreturn]] void Raise(const CalibrationError_& error, const Handle_<XccyMcBasketPartial_>& partial) {
        const auto& f = error.Failure();
        const auto type = py::reinterpret_borrow<py::object>(basketErrorType);
        const auto instance = type(py::str(error.what()), Text(f.diagnostic_.code_), py::cast(f.diagnostic_), Output(f.report_),
                                   Output(handle_cast<XccyGaussianModelData_>(f.candidate_)), Output(partial));
        PyErr_SetObject(basketErrorType, instance.ptr());
        throw py::error_already_set();
    }
    template <class F_> auto Local(F_ operation) -> decltype(operation()) {
        try {
            return operation();
        } catch (const XccyMcBasketError_& error) {
            Raise(error, error.Partial());
        } catch (const CalibrationError_& error) {
            Raise(error, {});
        }
    }
    template <class T_> Handle_<T_> Nullable(const py::handle& value, const std::string& field) {
        return value.is_none() ? Handle_<T_>() : Input_<Handle_<T_>>::Read(value, field);
    }
    py::list Matrix(const Matrix_<>& value) {
        py::list rows;
        for (int i = 0; i < value.Rows(); ++i) {
            py::list row;
            for (int j = 0; j < value.Cols(); ++j)
                row.append(value(i, j));
            rows.append(row);
        }
        return rows;
    }
    void ErrorType(py::module_& m) {
        py::exec(R"PY(
class XccyBasketCalibrationError(CalibrationError):
    def __init__(self, message, code, diagnostic, report, candidate, partial):
        super().__init__(message, code, diagnostic, report, candidate)
        self._partial = partial
    partial = property(lambda self: self._partial)
)PY",
                 m.attr("__dict__"));
        basketErrorType = m.attr("XccyBasketCalibrationError").ptr();
    }
    void InputTypes(py::module_& m) {
        auto member = py::class_<XccyMcBasketMember_>(m, "XccyMcBasketMember_");
        member.def(py::init([](const py::object& id, const py::object& instrument, const py::object& measure) {
                       return Local([&] {
                           return XccyMcBasketMember_{Input_<String_>::Read(id, "/member_id"),
                                                      Nullable<CalibrationInstrument_>(instrument, "/instrument"),
                                                      Input_<String_>::Read(measure, "/measure_id")};
                       });
                   }),
                   py::arg("member_id"), py::arg("instrument"), py::arg("measure_id"));
        Property(member, "member_id", &XccyMcBasketMember_::memberId_);
        Property(member, "instrument", &XccyMcBasketMember_::instrument_);
        Property(member, "measure_id", &XccyMcBasketMember_::measureId_);
        auto request = py::class_<XccyMonteCarloBasketRequest_>(m, "XccyMonteCarloBasketRequest_");
        request.def(py::init([](const py::object& phase, const py::object& sampling, const py::object& fixings, py::kwargs options) {
                        return Local([&] {
                            XccyMonteCarloBasketRequest_ r;
                            r.phase_ = ParseXccyMonteCarloPhase(Input_<String_>::Read(phase, "/phase"));
                            r.sampling_ = Input_<CalibrationMonteCarloSampling_>::Read(sampling, "/sampling");
                            r.fixings_ = Nullable<MarketFixingSnapshot_>(fixings, "/context/fixings");
                            if (!r.fixings_)
                                CalibrationInputFailure("InvalidCalibrationPlan", "/context/fixings", "explicit nonnull fixing snapshot");
                            Fields_ fields(std::move(options));
                            r.simulation_ = fields.Default<CalibrationSimulationSettings_>("simulation", r.simulation_);
                            r.derivativeMethod_ = fields.Default<CalibrationDerivative_>("derivative_method", r.derivativeMethod_);
                            r.parameterLabels_ = fields.Default<Vector_<String_>>("parameter_labels", {});
                            fields.Finish();
                            return r;
                        });
                    }),
                    py::arg("phase"), py::arg("sampling"), py::arg("fixings"));
        Property(request, "phase", &XccyMonteCarloBasketRequest_::phase_);
        Property(request, "sampling", &XccyMonteCarloBasketRequest_::sampling_);
        Property(request, "fixings", &XccyMonteCarloBasketRequest_::fixings_);
        Property(request, "simulation", &XccyMonteCarloBasketRequest_::simulation_);
        Property(request, "derivative_method", &XccyMonteCarloBasketRequest_::derivativeMethod_);
        Property(request, "parameter_labels", &XccyMonteCarloBasketRequest_::parameterLabels_);
    }
    void MappingTypes(py::module_& m) {
        auto range = py::class_<XccyMcRange_>(m, "XccyMcBasketRange_");
        Property(range, "first_path", &XccyMcRange_::firstPath_);
        Property(range, "path_end", &XccyMcRange_::pathEnd_);
        Property(range, "first_coordinate", &XccyMcRange_::firstCoordinate_);
        Property(range, "coordinate_end", &XccyMcRange_::coordinateEnd_);
        auto mapping = py::class_<CalibrationMcMapping_>(m, "XccyMcBasketMapping_");
        Property(mapping, "identity", &CalibrationMcMapping_::identity_);
        Property(mapping, "dimension", &CalibrationMcMapping_::dimension_);
        Property(mapping, "timeline", &CalibrationMcMapping_::timeline_);
        Property(mapping, "normal_coordinates", &CalibrationMcMapping_::normalCoordinates_);
        Property(mapping, "stream_version", &CalibrationMcMapping_::streamVersion_);
        Property(mapping, "seed", &CalibrationMcMapping_::seed_);
        Property(mapping, "generator", &CalibrationMcMapping_::generator_);
        Property(mapping, "normal_precision", &CalibrationMcMapping_::normalPrecision_);
        Property(mapping, "numeraire", &CalibrationMcMapping_::numeraire_);
        Property(mapping, "brownian_bridge", &CalibrationMcMapping_::brownianBridge_);
        auto axis = py::class_<XccyMcBasketAxis_>(m, "XccyMcBasketAxis_");
        Property(axis, "member_id", &XccyMcBasketAxis_::memberId_);
        Property(axis, "label", &XccyMcBasketAxis_::label_);
        Property(axis, "unit", &XccyMcBasketAxis_::unit_);
    }
    void AllocationTypes(py::module_& m) {
        auto member = py::class_<XccyMcBasketMemberAllocation_>(m, "XccyMcBasketMemberAllocation_");
        Property(member, "instrument", &XccyMcBasketMemberAllocation_::instrument_);
        member.def_property_readonly("context", [](const XccyMcBasketMemberAllocation_& v) { return *v.frozenContext_.xccy_; });
        member.def_property_readonly("convention_state",
                                     [](const XccyMcBasketMemberAllocation_& v) { return Text(v.frozenContext_.conventionState_); });
        Property(member, "member_id", &XccyMcBasketMemberAllocation_::memberId_);
        Property(member, "instrument_id", &XccyMcBasketMemberAllocation_::instrumentId_);
        Property(member, "instrument_identity", &XccyMcBasketMemberAllocation_::instrumentIdentity_);
        Property(member, "source_identity", &XccyMcBasketMemberAllocation_::sourceIdentity_);
        Property(member, "definition_identity", &XccyMcBasketMemberAllocation_::definitionIdentity_);
        Property(member, "binding_identity", &XccyMcBasketMemberAllocation_::bindingIdentity_);
        Property(member, "measure_id", &XccyMcBasketMemberAllocation_::measureId_);
        Property(member, "measure_unit", &XccyMcBasketMemberAllocation_::measureUnit_);
        Property(member, "preparation_identity", &XccyMcBasketMemberAllocation_::preparationIdentity_);
        Property(member, "context_identity", &XccyMcBasketMemberAllocation_::contextIdentity_);
        Property(member, "compiler_identity", &XccyMcBasketMemberAllocation_::compilerIdentity_);
        Property(member, "low_binding_identity", &XccyMcBasketMemberAllocation_::lowBindingIdentity_);
        Property(member, "high_binding_identity", &XccyMcBasketMemberAllocation_::highBindingIdentity_);
        Property(member, "native_map_identity", &XccyMcBasketMemberAllocation_::nativeMapIdentity_);
        Property(member, "mapping", &XccyMcBasketMemberAllocation_::mapping_);
        Property(member, "grid", &XccyMcBasketMemberAllocation_::grid_);
        Property(member, "observations", &XccyMcBasketMemberAllocation_::observations_);
        Property(member, "requests", &XccyMcBasketMemberAllocation_::requests_);
        Property(member, "first_coordinate", &XccyMcBasketMemberAllocation_::firstCoordinate_);
        Property(member, "coordinate_end", &XccyMcBasketMemberAllocation_::coordinateEnd_);
        Property(member, "phase_ranges", &XccyMcBasketMemberAllocation_::phaseRanges_);
        Property(member, "ranges", &XccyMcBasketMemberAllocation_::ranges_);
        Property(member, "deterministic", &XccyMcBasketMemberAllocation_::deterministic_);
        auto allocation = py::class_<XccyMcBasketAllocation_>(m, "XccyMcBasketAllocation_");
        Property(allocation, "version", &XccyMcBasketAllocation_::version_);
        Property(allocation, "identity", &XccyMcBasketAllocation_::identity_);
        Property(allocation, "execution_identity", &XccyMcBasketAllocation_::executionIdentity_);
        Property(allocation, "stream_version", &XccyMcBasketAllocation_::streamVersion_);
        Property(allocation, "normal_precision", &XccyMcBasketAllocation_::normalPrecision_);
        Property(allocation, "seek_convention", &XccyMcBasketAllocation_::seekConvention_);
        Property(allocation, "phase", &XccyMcBasketAllocation_::phase_);
        Property(allocation, "seed", &XccyMcBasketAllocation_::seed_);
        Property(allocation, "batches", &XccyMcBasketAllocation_::batches_);
        Property(allocation, "paths_per_batch", &XccyMcBasketAllocation_::pathsPerBatch_);
        Property(allocation, "reserved_paths", &XccyMcBasketAllocation_::reservedPaths_);
        Property(allocation, "coordinate_end", &XccyMcBasketAllocation_::coordinateEnd_);
        Property(allocation, "expected_consumption", &XccyMcBasketAllocation_::expectedConsumption_);
        Property(allocation, "members", &XccyMcBasketAllocation_::members_);
    }
    void ObservationTypes(py::module_& m) {
        auto source = py::class_<Script::SourceLocation_>(m, "XccyMcBasketSourceLocation_");
        Property(source, "offset", &Script::SourceLocation_::offset_);
        Property(source, "line", &Script::SourceLocation_::line_);
        Property(source, "column", &Script::SourceLocation_::column_);
        Property(source, "row", &Script::SourceLocation_::row_);
        Property(source, "event_date", &Script::SourceLocation_::eventDate_);
        auto use = py::class_<Script::ObservationUse_>(m, "XccyMcBasketObservationUse_");
        Property(use, "source", &Script::ObservationUse_::source_);
        Property(use, "event_id", &Script::ObservationUse_::eventId_);
        Property(use, "statement_id", &Script::ObservationUse_::statementId_);
        Property(use, "node_id", &Script::ObservationUse_::nodeId_);
        Property(use, "index_original", &Script::ObservationUse_::indexOriginal_);
        Property(use, "fixing_date", &Script::ObservationUse_::fixingDate_);
        Property(use, "legacy_spot", &Script::ObservationUse_::legacySpot_);
        auto slot = py::class_<Script::ModelObservation_>(m, "XccyMcBasketObservationSlot_");
        Property(slot, "sample_id", &Script::ModelObservation_::sampleId_);
        Property(slot, "output_id", &Script::ModelObservation_::outputId_);
        auto observation = py::class_<Script::ObservationRequest_>(m, "XccyMcBasketObservation_");
        observation.def_property_readonly("canonical_index", [](const Script::ObservationRequest_& v) { return Text(v.key_.canonicalIndex_); });
        observation.def_property_readonly("fixing_time", [](const Script::ObservationRequest_& v) { return v.key_.fixingTime_; });
        Property(observation, "uses", &Script::ObservationRequest_::uses_);
        Property(observation, "history_value_id", &Script::ObservationRequest_::historyValueId_);
        Property(observation, "model_slot", &Script::ObservationRequest_::modelSlot_);
        Property(observation, "historical", &Script::ObservationRequest_::historical_);
        auto plan = py::class_<Script::ObservationPlan_>(m, "XccyMcBasketObservationPlan_");
        plan.def_property_readonly("requests", [](const Script::ObservationPlan_& v) { return Output(v.Requests()); });
        plan.def_property_readonly("known_values", [](const Script::ObservationPlan_& v) { return Output(v.KnownValues()); });
        plan.def_property_readonly("sample_dates", [](const Script::ObservationPlan_& v) { return Output(v.SampleDates()); });
        plan.def_property_readonly("timeline", [](const Script::ObservationPlan_& v) { return Output(v.TimeLine()); });
        plan.def_property_readonly("event_to_sample", [](const Script::ObservationPlan_& v) { return Output(v.EventToSample()); });
        plan.def_property_readonly("live_event_ids", [](const Script::ObservationPlan_& v) { return Output(v.LiveEventIds()); });
        plan.def_property_readonly("model_binding_names", [](const Script::ObservationPlan_& v) { return Output(v.ModelBindingNames()); });
        auto request = py::class_<Script::XccyObservationRequest_>(m, "XccyMcBasketObservationRequest_");
        Property(request, "kind", &Script::XccyObservationRequest_::kind_);
        Property(request, "currency", &Script::XccyObservationRequest_::currency_);
        Property(request, "tenor", &Script::XccyObservationRequest_::tenor_);
        Property(request, "route", &Script::XccyObservationRequest_::route_);
        Property(request, "start", &Script::XccyObservationRequest_::start_);
        Property(request, "end", &Script::XccyObservationRequest_::end_);
        Property(request, "accrual", &Script::XccyObservationRequest_::accrual_);
    }
    void HeaderTypes(py::module_& m) {
        auto settings = py::class_<XccyMcSettings_>(m, "XccyMcBasketSettings_");
        Property(settings, "phase", &XccyMcSettings_::phase_);
        Property(settings, "batches", &XccyMcSettings_::batches_);
        Property(settings, "paths_per_batch", &XccyMcSettings_::pathsPerBatch_);
        Property(settings, "seed", &XccyMcSettings_::seed_);
        Property(settings, "stream_version", &XccyMcSettings_::streamVersion_);
        Property(settings, "compiled", &XccyMcSettings_::compiled_);
        Property(settings, "aad", &XccyMcSettings_::aad_);
        Property(settings, "retain_paths", &XccyMcSettings_::retainPaths_);
        Property(settings, "parameter_labels", &XccyMcSettings_::parameterLabels_);
        auto header = py::class_<XccyMcBasketExecutionHeader_>(m, "XccyMcBasketExecutionHeader_");
        Property(header, "version", &XccyMcBasketExecutionHeader_::version_);
        Property(header, "rule_id", &XccyMcBasketExecutionHeader_::ruleId_);
        Property(header, "plan_identity", &XccyMcBasketExecutionHeader_::planIdentity_);
        Property(header, "prepared_model_identity", &XccyMcBasketExecutionHeader_::preparedModelIdentity_);
        Property(header, "attempted_model_identity", &XccyMcBasketExecutionHeader_::attemptedModelIdentity_);
        Property(header, "native_map_identity", &XccyMcBasketExecutionHeader_::nativeMapIdentity_);
        Property(header, "derivative_method", &XccyMcBasketExecutionHeader_::derivativeMethod_);
        Property(header, "parameter_units", &XccyMcBasketExecutionHeader_::parameterUnits_);
        Property(header, "candidate_geometry_validated", &XccyMcBasketExecutionHeader_::candidateGeometryValidated_);
        Property(header, "settings", &XccyMcBasketExecutionHeader_::settings_);
    }
    void EstimateTypes(py::module_& m) {
        auto coefficients = py::class_<CalibrationAffineCoefficients_>(m, "XccyMcBasketCoefficients_");
        Property(coefficients, "a", &CalibrationAffineCoefficients_::a_);
        Property(coefficients, "b", &CalibrationAffineCoefficients_::b_);
        coefficients.def_property_readonly("da", [](const CalibrationAffineCoefficients_& v) { return v.da_.empty() ? py::none() : Output(v.da_); });
        coefficients.def_property_readonly("db", [](const CalibrationAffineCoefficients_& v) { return v.db_.empty() ? py::none() : Output(v.db_); });
        auto estimate = py::class_<XccyMcBasketMemberEstimate_>(m, "XccyMcBasketMemberEstimate_");
        Property(estimate, "coefficients", &XccyMcBasketMemberEstimate_::coefficients_);
        estimate.def_property_readonly("coefficient_covariance",
                                       [](const XccyMcBasketMemberEstimate_& v) { return Matrix(v.coefficientCovariance_); });
        estimate.def_property_readonly("mean", [](const XccyMcBasketMemberEstimate_& v) { return v.estimate_.mean_; });
        estimate.def_property_readonly("standard_error", [](const XccyMcBasketMemberEstimate_& v) { return v.estimate_.standardError_; });
        estimate.def_property_readonly("validation", [](const XccyMcBasketMemberEstimate_& v) { return v.estimate_.validation_; });
        estimate.def_property_readonly("held_out", [](const XccyMcBasketMemberEstimate_& v) { return v.estimate_.heldOut_; });
        estimate.def_property_readonly("instrument_identity",
                                       [](const XccyMcBasketMemberEstimate_& v) { return Text(v.estimate_.instrumentIdentity_); });
        estimate.def_property_readonly("context_identity", [](const XccyMcBasketMemberEstimate_& v) { return Text(v.estimate_.contextIdentity_); });
        estimate.def_property_readonly("mapping_identity", [](const XccyMcBasketMemberEstimate_& v) { return Text(v.estimate_.mappingIdentity_); });
        estimate.def_property_readonly("plan_identity", [](const XccyMcBasketMemberEstimate_& v) { return Text(v.estimate_.planIdentity_); });
        estimate.def_property_readonly("measure_id", [](const XccyMcBasketMemberEstimate_& v) { return Text(v.estimate_.measureId_); });
        estimate.def_property_readonly("annuity_standard_error", [](const XccyMcBasketMemberEstimate_& v) {
            return v.estimate_.affine_ ? Output(v.estimate_.affine_->annuityStandardError_) : py::none();
        });
        estimate.def_property_readonly("parameter_labels", [](const XccyMcBasketMemberEstimate_& v) { return Output(v.estimate_.parameterLabels_); });
        estimate.def_property_readonly("gradient", [](const XccyMcBasketMemberEstimate_& v) {
            return v.estimate_.parameterLabels_.empty() ? py::none() : Output(v.estimate_.gradient_);
        });
        estimate.def_property_readonly("gradient_standard_error", [](const XccyMcBasketMemberEstimate_& v) {
            return v.estimate_.parameterLabels_.empty() ? py::none() : Output(v.estimate_.gradientStandardError_);
        });
    }
    void ProgressTypes(py::module_& m) {
        auto batch = py::class_<XccyMcBasketCompletedBatch_>(m, "XccyMcBasketCompletedBatch_");
        Property(batch, "ordinal", &XccyMcBasketCompletedBatch_::ordinal_);
        Property(batch, "paths", &XccyMcBasketCompletedBatch_::paths_);
        Property(batch, "range", &XccyMcBasketCompletedBatch_::range_);
        Property(batch, "first_normals", &XccyMcBasketCompletedBatch_::firstNormals_);
        Property(batch, "row", &XccyMcBasketCompletedBatch_::row_);
        auto path = py::class_<XccyMcBasketRetainedPath_>(m, "XccyMcBasketRetainedPath_");
        Property(path, "path", &XccyMcBasketRetainedPath_::path_);
        Property(path, "first_coordinate", &XccyMcBasketRetainedPath_::firstCoordinate_);
        Property(path, "coefficients", &XccyMcBasketRetainedPath_::coefficients_);
        auto progress = py::class_<XccyMcBasketMemberProgress_>(m, "XccyMcBasketMemberProgress_");
        Property(progress, "member_id", &XccyMcBasketMemberProgress_::memberId_);
        Property(progress, "state", &XccyMcBasketMemberProgress_::state_);
        Property(progress, "batches", &XccyMcBasketMemberProgress_::batches_);
        Property(progress, "actual_ranges", &XccyMcBasketMemberProgress_::actualRanges_);
        Property(progress, "retained_paths", &XccyMcBasketMemberProgress_::retainedPaths_);
        Property(progress, "consumed_coordinates", &XccyMcBasketMemberProgress_::consumedCoordinates_);
        Property(progress, "completed_paths", &XccyMcBasketMemberProgress_::completedPaths_);
        Property(progress, "completed_batches", &XccyMcBasketMemberProgress_::completedBatches_);
        Property(progress, "completed_estimate", &XccyMcBasketMemberProgress_::completedEstimate_);
        Property(progress, "failure", &XccyMcBasketMemberProgress_::failure_);
    }
    void ResultTypes(py::module_& m) {
        py::class_<XccyMcBasketPartial_, std::shared_ptr<XccyMcBasketPartial_>>(m, "XccyMcBasketPartial_")
            .def_property_readonly("version", [](const XccyMcBasketPartial_& v) { return Text(v.Snapshot().version_); })
            .def_property_readonly("basket_complete", [](const XccyMcBasketPartial_& v) { return v.Snapshot().basketComplete_; })
            .def_property_readonly("header", [](const XccyMcBasketPartial_& v) { return v.Snapshot().header_; })
            .def_property_readonly("allocation", [](const XccyMcBasketPartial_& v) { return v.Snapshot().allocation_; })
            .def_property_readonly("members", [](const XccyMcBasketPartial_& v) { return Output(v.Snapshot().members_); })
            .def_property_readonly("completed_members", [](const XccyMcBasketPartial_& v) { return v.Snapshot().completedMembers_; })
            .def_property_readonly("consumed_coordinates", [](const XccyMcBasketPartial_& v) { return v.Snapshot().consumedCoordinates_; })
            .def_property_readonly("failure_location",
                                   [](const XccyMcBasketPartial_& v) {
                                       const auto& s = v.Snapshot();
                                       py::dict result;
                                       result["member"] = Output(s.failedMember_);
                                       result["batch"] = Output(s.failedBatch_);
                                       result["path"] = Output(s.failedPath_);
                                       result["observed_cursor"] = Output(s.observedCursor_);
                                       return result;
                                   })
            .def_property_readonly("report", [](const XccyMcBasketPartial_& v) { return Output(v.Report()); })
            .def_property_readonly("details_json", [](const std::shared_ptr<XccyMcBasketPartial_>& v) {
                return Text(DumpXccyMonteCarloBasketPartialJson(Handle_<XccyMcBasketPartial_>(v)));
            });
        auto result = py::class_<XccyMonteCarloBasketResult_>(m, "XccyMonteCarloBasketResult_");
        result.def_property_readonly("version", [](const XccyMonteCarloBasketResult_& v) { return Text(v.Snapshot().version_); });
        result.def_property_readonly("basket_complete", [](const XccyMonteCarloBasketResult_& v) { return v.Snapshot().basketComplete_; });
        result.def_property_readonly("header", [](const XccyMonteCarloBasketResult_& v) { return v.Snapshot().header_; });
        result.def_property_readonly("allocation", [](const XccyMonteCarloBasketResult_& v) { return v.Snapshot().allocation_; });
        result.def_property_readonly("members", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().members_); });
        result.def_property_readonly("coefficient_axes", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().coefficientAxes_); });
        result.def_property_readonly("output_axes", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().outputAxes_); });
        result.def_property_readonly("coefficient_rows", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().coefficientRows_); });
        result.def_property_readonly("coefficient_means",
                                     [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().coefficientMeans_); });
        result.def_property_readonly("coefficient_covariance",
                                     [](const XccyMonteCarloBasketResult_& v) { return Matrix(v.Snapshot().coefficientCovariance_); });
        result.def_property_readonly("mean_covariance", [](const XccyMonteCarloBasketResult_& v) { return Matrix(v.Snapshot().meanCovariance_); });
        result.def_property_readonly("output_jacobian", [](const XccyMonteCarloBasketResult_& v) { return Matrix(v.Snapshot().outputJacobian_); });
        result.def_property_readonly("output_covariance",
                                     [](const XccyMonteCarloBasketResult_& v) { return Matrix(v.Snapshot().outputCovariance_); });
        result.def_property_readonly("model", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Snapshot().model_); });
        result.def_property_readonly("report", [](const XccyMonteCarloBasketResult_& v) { return Output(v.Report()); });
        result.def_property_readonly("consumed_coordinates", [](const XccyMonteCarloBasketResult_& v) { return v.Snapshot().consumedCoordinates_; });
        result.def_property_readonly("details_json",
                                     [](const XccyMonteCarloBasketResult_& v) { return Text(DumpXccyMonteCarloBasketResultJson(v)); });
    }
    void PreparedType(py::module_& m) {
        auto prepared = py::class_<PreparedXccyMonteCarloBasket_>(m, "PreparedXccyMonteCarloBasket_");
        prepared.def_property_readonly("allocation", [](const PreparedXccyMonteCarloBasket_& v) { return v.Allocation(); });
        prepared.def_property_readonly("plan_identity", [](const PreparedXccyMonteCarloBasket_& v) { return Text(v.PlanIdentity()); });
        prepared.def_property_readonly("report", [](const PreparedXccyMonteCarloBasket_& v) { return Output(v.Report()); });
        prepared.def(
            "evaluate",
            [](const PreparedXccyMonteCarloBasket_& v, const py::object& candidate) {
                return Local([&] {
                    const auto plan = v;
                    const auto model = Nullable<XccyGaussianModelData_>(candidate, "/model");
                    py::gil_scoped_release release;
                    barrier.Wait(&barrierArmed, GIL_TIMEOUT);
                    return plan.Evaluate(model);
                });
            },
            py::arg("candidate") = py::none());
    }
} // namespace

void init_xccy_monte_carlo_basket(py::module_& m) {
    ErrorType(m);
    InputTypes(m);
    MappingTypes(m);
    ObservationTypes(m);
    AllocationTypes(m);
    HeaderTypes(m);
    EstimateTypes(m);
    ProgressTypes(m);
    ResultTypes(m);
    PreparedType(m);
    m.def(
        "XccyBasket_PrepareMonteCarlo",
        [](const py::object& members, const py::object& model, const py::object& request) {
            return Local([&] {
                const auto rows = Input_<Vector_<XccyMcBasketMember_>>::Read(members, "/members");
                const auto data = Nullable<XccyGaussianModelData_>(model, "/model");
                const auto input = Input_<XccyMonteCarloBasketRequest_>::Read(request, "/request");
                py::gil_scoped_release release;
                barrier.Wait(&barrierArmed, GIL_TIMEOUT);
                return PrepareXccyBasketMonteCarlo(rows, data, input);
            });
        },
        py::arg("members"), py::arg("model"), py::arg("request"));
#ifdef DAL_XCCY_TEST_BRIDGE
    init_xccy_mc_basket_test(m);
    m.def("_XccyBasketFailureForTesting", [](const std::string& fault, const py::object& members,
                                             const std::shared_ptr<XccyGaussianModelData_>& model, const XccyMonteCarloBasketRequest_& request) {
        Local([&] {
            const auto rows = Input_<Vector_<XccyMcBasketMember_>>::Read(members, "/members");
            const Handle_<XccyGaussianModelData_> data(model);
            const auto input = request;
            py::gil_scoped_release release;
            XccyBasketThrowForTesting(fault, rows, data, input);
        });
    });
    m.def("_XccyBasketGilBarrier_ArmForTesting", [] { barrier.Arm(&barrierArmed); });
    m.def("_XccyBasketGilBarrier_AwaitEntryForTesting", [] {
        py::gil_scoped_release release;
        return barrier.AwaitEntry();
    });
    m.def("_XccyBasketGilBarrier_ReleaseForTesting", [] { return barrier.Release(); });
    m.def("_XccyBasketGilBarrier_BlockWithGilForTesting", [] { barrier.Wait(&barrierArmed, GIL_TIMEOUT); });
#endif
}
