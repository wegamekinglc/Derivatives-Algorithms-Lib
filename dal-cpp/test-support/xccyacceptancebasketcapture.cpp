#include <chrono>
#include <fstream>
#include <iostream>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/xccymcbasket.hpp>
#include <dal/storage/json.hpp>

#include "../tests/script/xccyacceptanceextendedfixture.hpp"

namespace {
    using namespace Dal;
    using namespace Dal::CalibrationInternal;

    Json_ Numbers(const Vector_<>& values) {
        return Array(values, [](double x) { return Num(x); });
    }
    Json_ Strings(const Vector_<String_>& values) {
        return Array(values, [](const auto& x) { return Str(x); });
    }
    Json_ Matrix(const Matrix_<>& values) {
        auto result = Json_::Array();
        for (int i = 0; i < values.Rows(); ++i) {
            Vector_<> row;
            for (int j = 0; j < values.Cols(); ++j)
                row.push_back(values(i, j));
            result.array_.push_back(Numbers(row));
        }
        return result;
    }
    Json_ Range(const XccyMcRange_& r) {
        return Numbers({double(r.firstPath_), double(r.pathEnd_), double(r.firstCoordinate_), double(r.coordinateEnd_)});
    }
    Json_ Allocation(const XccyMcBasketAllocation_& a) {
        return Obj({{"identity", Str(a.identity_)},
                    {"execution", Str(a.executionIdentity_)},
                    {"layout", Str(a.version_)},
                    {"stream", Str(a.streamVersion_)},
                    {"precision", Str(a.normalPrecision_)},
                    {"seek", Str(a.seekConvention_)},
                    {"seed", Num(a.seed_)},
                    {"phase", Str(a.phase_.String())},
                    {"batches", Num(a.batches_)},
                    {"n", Num(a.pathsPerBatch_)},
                    {"T", Num(double(a.reservedPaths_))},
                    {"end", Num(double(a.coordinateEnd_))},
                    {"expected_consumption", Num(double(a.expectedConsumption_))},
                    {"members", Array(a.members_, [](const auto& m) {
                         return Obj({{"id", Str(m.memberId_)},
                                     {"instrument", Str(m.instrumentIdentity_)},
                                     {"source", Str(m.sourceIdentity_)},
                                     {"binding", Str(m.bindingIdentity_)},
                                     {"context", Str(m.contextIdentity_)},
                                     {"measure", Str(m.measureId_)},
                                     {"unit", Str(m.measureUnit_)},
                                     {"mapping", Str(m.mapping_.identity_)},
                                     {"D", Num(double(m.mapping_.dimension_))},
                                     {"block", Numbers({double(m.firstCoordinate_), double(m.coordinateEnd_)})},
                                     {"coordinates", Strings(m.mapping_.normalCoordinates_)},
                                     {"grid", Array(m.grid_, [](const auto& d) { return Str(Date::ToString(d)); })},
                                     {"phases", Array(m.phaseRanges_, Range)},
                                     {"ranges", Array(m.ranges_, Range)}});
                     })}});
    }
    Json_ Estimate(const XccyMcBasketMemberEstimate_& m) {
        const auto& e = m.estimate_;
        return Obj({{"mean", Num(e.mean_)},
                    {"se", Num(e.standardError_)},
                    {"gradient", Numbers(e.gradient_)},
                    {"gradient_se", Numbers(e.gradientStandardError_)},
                    {"validation", Bool(e.validation_)},
                    {"held_out", Bool(e.heldOut_)},
                    {"a", Num(m.coefficients_.a_)},
                    {"b", Num(m.coefficients_.b_)},
                    {"da", Numbers(m.coefficients_.da_)},
                    {"db", Numbers(m.coefficients_.db_)},
                    {"covariance", Matrix(m.coefficientCovariance_)}});
    }
    Json_ Axes(const Vector_<XccyMcBasketAxis_>& axes) {
        return Array(axes, [](const auto& a) { return Strings({a.memberId_, a.label_, a.unit_}); });
    }
    Json_ Snapshot(const XccyMcBasketSnapshot_& s) {
        return Obj({{"allocation", Allocation(s.allocation_)},
                    {"complete", Bool(s.basketComplete_)},
                    {"model", Str(s.model_->Identity())},
                    {"plan", Str(s.header_.planIdentity_)},
                    {"native_map", Str(s.header_.nativeMapIdentity_.value_or(""))},
                    {"candidate_geometry_validated", Bool(s.header_.candidateGeometryValidated_)},
                    {"labels", Strings(s.header_.settings_.parameterLabels_)},
                    {"units", Strings(s.header_.parameterUnits_)},
                    {"consumed", Num(double(s.consumedCoordinates_))},
                    {"rows", Array(s.coefficientRows_, Numbers)},
                    {"means", Numbers(s.coefficientMeans_)},
                    {"coefficient_axes", Axes(s.coefficientAxes_)},
                    {"output_axes", Axes(s.outputAxes_)},
                    {"C_batch", Matrix(s.coefficientCovariance_)},
                    {"C_mean", Matrix(s.meanCovariance_)},
                    {"J", Matrix(s.outputJacobian_)},
                    {"C_output", Matrix(s.outputCovariance_)},
                    {"members", Array(s.members_, [](const auto& m) {
                         return Obj({{"id", Str(m.memberId_)},
                                     {"state", Str(m.state_)},
                                     {"consumed", Num(double(m.consumedCoordinates_))},
                                     {"paths", Num(double(m.completedPaths_))},
                                     {"batches", Num(double(m.completedBatches_))},
                                     {"actual_ranges", Array(m.actualRanges_, Range)},
                                     {"estimate", Estimate(*m.completedEstimate_)},
                                     {"rows", Array(m.batches_, [](const auto& b) { return Numbers(b.row_); })},
                                     {"first_normals", Array(m.batches_, [](const auto& b) { return Numbers(b.firstNormals_); })}});
                     })}});
    }
    Vector_<> Gradient(AAD::Number_ output, const Vector_<AAD::Number_>& parameters, const Matrix_<>& chain) {
        Vector_<> result(chain.Cols(), 0.);
        if (output.IsPassive())
            return result;
        AAD::ZeroAdjoints(*AAD::Tape());
        AAD::Adjoint(output) = 1.;
        AAD::PropagateToMark(*AAD::Tape());
        for (size_t i = 0; i < parameters.size(); ++i)
            for (int j = 0; j < chain.Cols(); ++j)
                result[j] += AAD::Adjoint(parameters[i]) * chain(static_cast<int>(i), j);
        return result;
    }
    Json_ Direct(const XccyMcBasketMember_& member, const CalibrationPreparationContext_& context, const XccyGaussianModelData_& model) {
        const auto prepared = PrepareCalibrationInstrument(member.instrument_, "Price", context);
        const auto values = XccyGaussianMomentInputs(model);
        Vector_<String_> labels;
        const auto chain = XccyGaussianMomentNativeMap(model, &labels);
        AAD::Clear(*AAD::Tape());
        Vector_<AAD::Number_> parameters;
        for (double v : values.values_)
            parameters.push_back(AAD::Number_(v));
        for (auto& p : parameters)
            AAD::PutOnTape(p);
        AAD::Mark(*AAD::Tape());
        const auto c = EvaluateXccyGaussianMoments<AAD::Number_>(prepared, model, parameters);
        Vector_<AAD::Number_> outputs{c.a_, c.b_, c.price_, -c.b_ / c.a_};
        auto fast = Json_::Array(), gradients = Json_::Array();
        for (const auto& output : outputs) {
            fast.array_.push_back(Num(Value(output)));
            gradients.array_.push_back(Numbers(Gradient(output, parameters, chain)));
        }
        AAD::Clear(*AAD::Tape());
        return Obj({{"fast", fast}, {"fast_gradient", gradients}, {"labels", Strings(labels)}, {"native_chain", Matrix(chain)}});
    }
    void Write(const char* path, const Json_& value) {
        std::ofstream output(path);
        output << Jcs(value) << '\n';
        REQUIRE(output.good(), "basket capture written");
    }
    Vector_<String_> SelectedLabels(const XccyGaussianModelData_& model) {
        Vector_<String_> nativeLabels, selected;
        static_cast<void>(XccyGaussianMomentNativeMap(model, &nativeLabels));
        for (const auto& label : nativeLabels)
            if (label == "fxspot:EUR/USD" || label.find("logdf:USD:BASIS:") == 0 || label.find("sigma:") == 0)
                selected.push_back(label);
        return selected;
    }
    Json_ CrnRows(const Vector_<XccyMcBasketMember_>& members,
                  const CalibrationPreparationContext_& context,
                  const Handle_<XccyGaussianModelData_>& model,
                  XccyMcSettings_ settings) {
        auto rows = Json_::Array();
        settings.aad_ = false;
        settings.parameterLabels_.clear();
        const PreparedXccyMcBasket_ crn(members, context, model, settings);
        for (const String_ label : {"fxspot:EUR/USD", "logdf:USD:BASIS:EUR/USD:2028-11-08", "sigma:EUR/USD:2029-10-20"})
            for (double divisor : {1., 2., 4.}) {
                const double h = TestXccyMc::Scale(label) * .001 / divisor;
                const auto plus = crn.Evaluate(TestXccyMc::Bump(*model, label, h));
                const auto minus = crn.Evaluate(TestXccyMc::Bump(*model, label, -h));
                rows.array_.push_back(
                    Obj({{"label", Str(label)}, {"h", Num(h)}, {"plus", Snapshot(plus.Snapshot())}, {"minus", Snapshot(minus.Snapshot())}}));
            }
        return rows;
    }
    Json_ Capture(const String_& mode, int factor, bool controls) {
        auto spec = TestXccyAcceptance::StagedSpec({-.06705259701832679, -.054512806033648581, -.049912191545990019});
        spec.solver_.solveMode_ = CurveSolveMode_(mode);
        CurveInstrumentCalibrationOptions_ options;
        options.computeQuoteJacobian_ = true;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto fit = CalibrateXccyInstruments(spec, options);
        REQUIRE(fit.fitWithinTolerance_, "original staged fit qualified");
        const auto model = fit.gaussianModel_;
        CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const XccyScriptContext_>(spec.context_);
        Vector_<XccyMcBasketMember_> members;
        for (const auto& o : spec.observations_)
            members.push_back({o->Id(), o->Instrument(), "Price"});
        auto settings = TestXccyMc::Settings(controls ? 2 : 16, controls ? 3 : 16384 * std::max(1, factor));
        settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
        settings.aad_ = true;
        settings.parameterLabels_ = SelectedLabels(*model);
        const PreparedXccyMcBasket_ plan(members, context, model, settings);
        auto output = Obj({{"mode", Str(mode)},
                           {"factor", Num(factor)},
                           {"parameters", Numbers(fit.parameters_)},
                           {"model_archive", Str(JSON::WriteString(*model))},
                           {"allocation", Allocation(plan.Allocation())},
                           {"direct", Array(members, [&](const auto& m) { return Direct(m, context, *model); })}});
        if (factor == 0 && !controls)
            return output;
        const auto start = std::chrono::steady_clock::now();
        const auto result = plan.Evaluate();
        output.object_["result"] = Snapshot(result.Snapshot());
        output.object_["evaluate_seconds"] = Num(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        if (controls)
            output.object_["crn"] = CrnRows(members, context, model, settings);
        REQUIRE(AAD::Tape()->nodes_.Size() == 0, "basket caller tape restored");
        return output;
    }
} // namespace

int main(int argc, char** argv) try {
    REQUIRE(argc == 4 || argc == 5, "OUTPUT EXACT|APPROXIMATE 0|1|2|4 [controls]");
    Dal::RegisterAll_::Init();
    const String_ mode(argv[2]);
    const int factor = std::stoi(argv[3]);
    REQUIRE((mode == "EXACT" || mode == "APPROXIMATE") && (factor == 0 || factor == 1 || factor == 2 || factor == 4), "frozen request");
    Write(argv[1], Capture(mode, factor, argc == 5 && std::string(argv[4]) == "controls"));
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
