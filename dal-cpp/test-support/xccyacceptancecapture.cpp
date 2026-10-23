#include <chrono>
#include <fstream>
#include <iostream>

#include <dal/calibration/validation_internal.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/random/pseudorandom.hpp>
#include <dal/script/calibrationmc_internal.hpp>
#include <dal/storage/json.hpp>

#include "../tests/script/xccyacceptanceextendedfixture.hpp"
#include "../tests/script/xccyacceptancefixture.hpp"

namespace {
    using namespace Dal;
    using namespace Dal::CalibrationInternal;
    using namespace Dal::TestXccyMoments;
    using namespace Dal::TestXccyExecution;
    using namespace Dal::TestXccyMc;

    Json_ Numbers(const Vector_<>& values) {
        return Array(values, [](double x) { return Num(x); });
    }
    Json_ List(std::vector<Json_> values) {
        auto result = Json_::Array();
        result.array_ = std::move(values);
        return result;
    }
    Json_ Matrix(const Matrix_<>& values) {
        std::vector<Json_> rows;
        for (int i = 0; i < values.Rows(); ++i) {
            std::vector<Json_> row;
            for (int j = 0; j < values.Cols(); ++j)
                row.push_back(Num(values(i, j)));
            rows.push_back(List(std::move(row)));
        }
        return List(std::move(rows));
    }
    Json_ Dates(const Vector_<Date_>& dates) {
        return Array(dates, [](const auto& d) { return Str(Date::ToString(d)); });
    }
    Json_ Model(const XccyGaussianModelData_& data) {
        const auto& d = *data.dynamics_;
        const auto inputs = XccyGaussianMomentInputs(data);
        return Obj({{"identity", Str(data.Identity())},
                    {"valuation", Str(Date::ToString(data.market_->valuationTime_.Date()))},
                    {"spot", Num(data.market_->fxSpot_)},
                    {"a", Numbers({d.domesticA_, d.foreignA_})},
                    {"bucket_dates", Dates(d.bucketEndDates_)},
                    {"sigma", List({Numbers(d.domesticSigma_), Numbers(d.foreignSigma_), Numbers(d.fxSigma_)})},
                    {"correlation", Matrix(d.correlation_)},
                    {"curves", Array(inputs.curves_, [](const auto& c) {
                         return Obj({{"currency", Str(c->currency_)},
                                     {"route", Str(c->route_)},
                                     {"dates", Dates(c->nodeDates_)},
                                     {"logdf", Numbers(c->logDF_)},
                                     {"labels", Array(c->nativeLabels_, [](const auto& s) { return Str(s); })},
                                     {"values", Numbers(c->nativeValues_)},
                                     {"jacobian", Matrix(c->snapshotJacobian_)}});
                     })}});
    }
    Json_ Periods(const Vector_<XccyCouponPeriod_>& periods) {
        return Array(periods, [](const auto& p) {
            return Obj({{"start", Str(Date::ToString(p.schedule_.accrualStart_))},
                        {"end", Str(Date::ToString(p.schedule_.accrualEnd_))},
                        {"payment", Str(Date::ToString(p.schedule_.paymentDate_))},
                        {"fixing", Str(Date::ToString(p.rateFixingTime_.Date()))},
                        {"index", Str(p.rateIndexName_)},
                        {"alpha", Num(p.accrual_.dcf_)}});
        });
    }
    Json_ Coefficients(const CalibrationAffineCoefficients_& c) {
        return Obj({{"a", Num(c.a_)}, {"b", Num(c.b_)}, {"da", Numbers(c.da_)}, {"db", Numbers(c.db_)}});
    }
    Json_ Terms(const XccyGaussianMomentProgram_& program) {
        return Array(program.Terms(), [](const auto& t) {
            return Obj({{"weight", Num(t.weight_)},
                        {"annuity", Bool(t.annuity_)},
                        {"foreign_payment", Bool(t.foreignPayment_)},
                        {"payment", Str(Date::ToString(t.payment_))},
                        {"reset", t.reset_ ? Str(Date::ToString(*t.reset_)) : Json_{}},
                        {"growth", t.growth_ ? Periods({*t.growth_}) : Json_{}},
                        {"foreign_rate", Bool(t.foreignRate_)}});
        });
    }
    Json_ KernelControls() {
        std::vector<Json_> rows;
        for (const String_ scenario : {"ordinary", "tiny", "zero", "fx-only", "zero-a"}) {
            auto spec = ModelSpec(TestXccy::TODAY, scenario == "zero");
            if (scenario == "tiny")
                spec.dynamics_.domesticSigma_ = {1e-8};
            if (scenario == "fx-only")
                spec.dynamics_.domesticSigma_ = spec.dynamics_.foreignSigma_ = {0.};
            if (scenario == "zero-a")
                spec.dynamics_.domesticA_ = spec.dynamics_.foreignA_ = 0.;
            const auto& d = spec.dynamics_;
            const auto t = XccyGaussianDetail::ConstantTransition(
                d.domesticA_, d.foreignA_, std::array<double, 3>{d.domesticSigma_[0], d.foreignSigma_[0], d.fxSigma_[0]}, d.correlation_, 1.);
            Vector_<> shift(t.shift_.begin(), t.shift_.end());
            rows.push_back(Obj({{"scenario", Str(scenario)},
                                {"model", Model(*Data(spec))},
                                {"covariance", Matrix(t.covariance_)},
                                {"lower", Matrix(XccyGaussianDetail::ScaledFactor(t.covariance_))},
                                {"mean", Numbers(shift)}}));
        }
        return List(std::move(rows));
    }
    Json_ HistoryControls() {
        std::vector<Json_> rows;
        for (const Date_ valuation : {Date_(2027, 8, 4), Date_(2027, 11, 5), Date_(2027, 11, 9)})
            for (const bool foreign : {false, true}) {
                const auto spec = ModelSpec(valuation);
                const auto model = Data(spec);
                const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign);
                MarketFixingSnapshot_::values_t values;
                values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30))] = .031;
                values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 8, 2))] = .037;
                values["IR:EUR,LIBOR_6M_LCH"][DateTime_(Date_(2027, 4, 30))] = .029;
                values["FX[EUR/USD]"][DateTime_(Date_(2027, 8, 4))] = 1.17;
                const Handle_<MarketFixingSnapshot_> history(new MarketFixingSnapshot_(values));
                const auto prepared = Prepare(instrument, spec, "Price", history);
                const auto result = EvaluateXccyGaussianMoments<double>(prepared, *model);
                bool legacyRefused = false, parRefused = false;
                try {
                    static_cast<void>(Native(prepared, spec));
                } catch (const CalibrationError_&) {
                    legacyRefused = true;
                }
                try {
                    static_cast<void>(EvaluateXccyGaussianMoments<double>(Prepare(instrument, spec, "BasisSpread", history), *model)
                                          .Value(Prepare(instrument, spec, "BasisSpread", history)));
                } catch (const CalibrationError_&) {
                    parRefused = true;
                }
                auto settings = Settings(2, 3);
                const auto mc = PreparedXccyMc_(instrument, "Price", Context(spec, history), model, settings).Evaluate();
                rows.push_back(Obj({{"model", Model(*model)},
                                    {"foreign_spread", Bool(foreign)},
                                    {"receive", Bool(true)},
                                    {"mode", Str("MARK_TO_MARKET")},
                                    {"scenario", Str("history")},
                                    {"fast", Numbers({result.a_, result.b_, result.price_})},
                                    {"terms", Terms(LowerXccyGaussianMoments(prepared))},
                                    {"legacy_refused", Bool(legacyRefused)},
                                    {"par_refused", Bool(parRefused)},
                                    {"mc_price", Num(mc.estimate_.mean_)},
                                    {"source", Str(instrument->Definition()->SourceIdentity())}}));
            }
        return List(std::move(rows));
    }
    Vector_<> NativeGradient(AAD::Number_ output, const Vector_<AAD::Number_>& parameters, const Matrix_<>& chain, size_t size) {
        Vector_<> gradient(size, 0.);
        if (output.IsPassive())
            return gradient;
        AAD::ZeroAdjoints(*AAD::Tape());
        AAD::Adjoint(output) = 1.;
        AAD::PropagateToMark(*AAD::Tape());
        for (size_t i = 0; i < parameters.size(); ++i)
            for (size_t j = 0; j < size; ++j)
                gradient[j] += AAD::Adjoint(parameters[i]) * chain(static_cast<int>(i), static_cast<int>(j));
        return gradient;
    }
    Json_ CaptureGradients(const XccyGaussianMomentProgram_& program, const XccyGaussianModelData_& model, bool zero) {
        if (zero)
            return List({});
        const auto inputs = XccyGaussianMomentInputs(model);
        Vector_<String_> labels;
        const auto chain = XccyGaussianMomentNativeMap(model, &labels);
        std::vector<Json_> derivatives;
        auto* tape = AAD::Tape();
        Clear(*tape);
        {
            Vector_<AAD::Number_> parameters;
            for (double x : inputs.values_)
                parameters.push_back(AAD::Number_(x));
            for (auto& x : parameters)
                AAD::PutOnTape(x);
            AAD::Mark(*tape);
            {
                const auto active = EvaluateXccyGaussianMoments<AAD::Number_>(program, model, parameters);
                Vector_<AAD::Number_> outputs{active.a_, active.b_, active.price_};
                if (std::abs(Value(active.a_)) >= 1e-12)
                    outputs.push_back(-active.b_ / active.a_);
                for (auto output : outputs) {
                    derivatives.push_back(Numbers(NativeGradient(output, parameters, chain, labels.size())));
                }
            }
        }
        Clear(*tape);
        return List(std::move(derivatives));
    }
    Json_ CapturePrepared(const Handle_<CalibrationInstrument_>& instrument,
                          const Handle_<XccyGaussianModelData_>& model,
                          const CalibrationPreparationContext_& context,
                          const Json_& metadata,
                          bool zero = false) {
        const auto prepared = PrepareMoment(instrument, context);
        const auto program = LowerXccyGaussianMoments(prepared);
        const auto result = EvaluateXccyGaussianMoments<double>(program, *model);
        const auto& descriptor = **std::get_if<CalibrationXccy_>(&prepared.Descriptor());
        Vector_<String_> labels;
        static_cast<void>(XccyGaussianMomentNativeMap(*model, &labels));
        const auto derivatives = CaptureGradients(program, *model, zero);
        const Script::PreparedXccyScript_ script(instrument, context, model);
        auto settings = Settings(2, 3);
        settings.compiled_ = false;
        settings.retainPaths_ = true;
        const auto tree = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        settings.compiled_ = true;
        const auto compiled = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        REQUIRE(tree.estimate_.pathValues_ == compiled.estimate_.pathValues_, "tree/compiled per-path identity");
        auto output = Obj({{"model", Model(*model)},
                           {"source", Str(instrument->Definition()->SourceIdentity())},
                           {"instrument", Str(instrument->InstrumentIdentity())},
                           {"domestic_periods", Periods(descriptor.plan_.domesticPeriods_)},
                           {"foreign_periods", Periods(descriptor.plan_.foreignPeriods_)},
                           {"calendars", List({Str(descriptor.plan_.config_.convention_.domesticIndex_.fixingHolidays_.String()),
                                               Str(descriptor.plan_.config_.convention_.foreignIndex_.fixingHolidays_.String()),
                                               Str(descriptor.plan_.config_.convention_.domesticLeg_.paymentHolidays_.String()),
                                               Str(descriptor.plan_.config_.convention_.foreignLeg_.paymentHolidays_.String())})},
                           {"fast", std::abs(result.a_) >= 1e-12 ? Numbers({result.a_, result.b_, result.price_, -result.b_ / result.a_})
                                                                 : Numbers({result.a_, result.b_, result.price_})},
                           {"labels", Array(labels, [](const auto& s) { return Str(s); })},
                           {"fast_gradient", derivatives},
                           {"terms", Terms(program)},
                           {"dimension", Num(static_cast<double>(tree.mapping_.dimension_))},
                           {"mapping", Str(tree.mapping_.identity_)},
                           {"coordinates", Array(tree.mapping_.normalCoordinates_, [](const auto& s) { return Str(s); })},
                           {"grid", Dates(script.SampleDates())},
                           {"small_path_coefficients", Array(tree.pathCoefficients_, Coefficients)}});
        output.object_.insert(metadata.object_.begin(), metadata.object_.end());
        return output;
    }
    Json_ CaseMetadata(const String_& scenario, XccyNotionalMode_ mode, bool foreign, bool receive) {
        return Obj({{"id", Str(scenario + "/" + mode.String() + (foreign ? "/foreign" : "/domestic") + (receive ? "/receive" : "/pay"))},
                    {"scenario", Str(scenario)},
                    {"mode", Str(mode.String())},
                    {"foreign_spread", Bool(foreign)},
                    {"receive", Bool(receive)}});
    }
    Json_ Capture(const String_& scenario, XccyNotionalMode_ mode, bool foreign, bool receive) {
        const auto fixture = TestXccyAcceptance::Fixture(scenario, mode, foreign, receive);
        return CapturePrepared(fixture.instrument_, Data(fixture.spec_), Context(fixture.spec_, Empty()),
                               CaseMetadata(scenario, mode, foreign, receive), scenario == "zero");
    }
    Json_ Mc(const String_& phase, int factor) {
        const auto spec = ModelSpec();
        const auto model = Data(spec);
        const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
        const auto context = Context(spec, Empty());
        auto settings = Settings(16, (phase == "Fit" ? 4096 : 16384) * factor);
        settings.phase_ = ParseXccyMonteCarloPhase(phase);
        settings.aad_ = true;
        settings.parameterLabels_ = {
            "fxspot:EUR/USD",          "logdf:USD:OIS:2029-10-20", "logdf:EUR:OIS:2029-10-20", "logdf:USD:BASIS:EUR/USD:2029-10-20",
            "logdf:USD:3M:2029-10-20", "logdf:EUR:6M:2029-10-20",  "sigma:USD:2029-10-20",     "sigma:EUR:2029-10-20",
            "sigma:EUR/USD:2029-10-20"};
        const auto start = std::chrono::steady_clock::now();
        const PreparedXccyMc_ prepared(instrument, "Price", context, model, settings);
        const double prepareSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const auto result = prepared.Evaluate();
        const double evaluateSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() - prepareSeconds;
        REQUIRE(AAD::Tape()->nodes_.Size() == 0, "MC recording released");
        return Obj({{"model", Model(*model)},
                    {"source", Str(instrument->Definition()->SourceIdentity())},
                    {"instrument", Str(instrument->InstrumentIdentity())},
                    {"plan", Str(prepared.PlanIdentity())},
                    {"mapping", Str(result.mapping_.identity_)},
                    {"reservation", Str(result.reservation_.identity_)},
                    {"phase", Str(phase)},
                    {"factor", Num(factor)},
                    {"dimension", Num(static_cast<double>(result.mapping_.dimension_))},
                    {"labels", Array(settings.parameterLabels_, [](const auto& s) { return Str(s); })},
                    {"validation", Bool(result.estimate_.validation_)},
                    {"held_out", Bool(result.estimate_.heldOut_)},
                    {"mean", Num(result.estimate_.mean_)},
                    {"se", Num(result.estimate_.standardError_)},
                    {"gradient", Numbers(result.estimate_.gradient_)},
                    {"gradient_se", Numbers(result.estimate_.gradientStandardError_)},
                    {"coefficients", Coefficients(result.coefficients_)},
                    {"covariance", Matrix(result.coefficientCovariance_)},
                    {"batches", Array(result.estimate_.batches_, [](const auto& b) { return Coefficients(*b.coefficients_); })},
                    {"ranges", Array(result.reservation_.ranges_,
                                     [](const auto& r) {
                                         return Numbers({static_cast<double>(r.firstPath_), static_cast<double>(r.pathEnd_),
                                                         static_cast<double>(r.firstCoordinate_), static_cast<double>(r.coordinateEnd_)});
                                     })},
                    {"first_normals", Array(result.firstNormals_, Numbers)},
                    {"consumed", Num(static_cast<double>(result.consumedCoordinates_))},
                    {"prepare_seconds", Num(prepareSeconds)},
                    {"evaluate_seconds", Num(evaluateSeconds)}});
    }
    Json_ CrnControls() {
        const auto spec = ModelSpec();
        const auto model = Data(spec);
        const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET);
        const auto context = Context(spec, Empty());
        auto settings = Settings(4, 8);
        settings.aad_ = true;
        Vector_<String_> labels;
        static_cast<void>(XccyGaussianMomentNativeMap(*model, &labels));
        settings.parameterLabels_ = labels;
        const auto active = PreparedXccyMc_(instrument, "Price", context, model, settings).Evaluate();
        settings.aad_ = false;
        settings.parameterLabels_.clear();
        const PreparedXccyMc_ passive(instrument, "Price", context, model, settings);
        std::vector<Json_> rows;
        for (size_t column = 0; column < labels.size(); ++column) {
            std::vector<Json_> steps;
            const double scale = Scale(labels[column]);
            for (const double divisor : {1., 2., 4.}) {
                const double h = .001 * scale / divisor;
                const auto up = passive.Evaluate(Bump(*model, labels[column], h));
                const auto down = passive.Evaluate(Bump(*model, labels[column], -h));
                REQUIRE(up.reservation_.identity_ == active.reservation_.identity_ && down.reservation_.identity_ == active.reservation_.identity_,
                        "CRN native bumps keep complete mapping/range identity");
                Vector_<> gradients;
                for (size_t batch = 0; batch < active.estimate_.batches_.size(); ++batch)
                    gradients.push_back((up.estimate_.batches_[batch].value_ - down.estimate_.batches_[batch].value_) / (2 * h));
                steps.push_back(Obj({{"h", Num(h)},
                                     {"gradient", Num((up.estimate_.mean_ - down.estimate_.mean_) / (2 * h))},
                                     {"batch_gradients", Numbers(gradients)}}));
            }
            Vector_<> gradients;
            for (const auto& batch : active.estimate_.batches_)
                gradients.push_back(batch.derivatives_[column]);
            rows.push_back(Obj({{"label", Str(labels[column])},
                                {"scale", Num(scale)},
                                {"aad", Num(active.estimate_.gradient_[column])},
                                {"aad_se", Num(active.estimate_.gradientStandardError_[column])},
                                {"batch_aad", Numbers(gradients)},
                                {"steps", List(std::move(steps))}}));
        }
        REQUIRE(AAD::Tape()->nodes_.Size() == 0, "CRN tape released");
        auto rng = New(RNGType_("MRG32"), 1024, active.mapping_.dimension_, true);
        rng->SkipNormalTo(active.reservation_.ranges_.front().firstPath_);
        Vector_<> normals(active.mapping_.dimension_);
        std::vector<Json_> draws;
        for (size_t path = 0; path < 32; ++path) {
            rng->FillNormal(&normals);
            draws.push_back(Numbers(normals));
        }
        const Script::PreparedXccyScript_ script(instrument, context, model);
        return Obj({{"mapping", Str(active.mapping_.identity_)},
                    {"reservation", Str(active.reservation_.identity_)},
                    {"model", Model(*model)},
                    {"grid", Dates(script.SampleDates())},
                    {"price", Num(active.estimate_.mean_)},
                    {"normals", List(std::move(draws))},
                    {"rows", List(std::move(rows))}});
    }
    Json_ NamedMaturityQualification() {
        try {
            static_cast<void>(Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true, Date_(2028, 11, 6)));
        } catch (const CalibrationError_& error) {
            return Obj({{"requested", Str("2028-11-06")},
                        {"accepted", Bool(false)},
                        {"code", Str(error.Failure().diagnostic_.code_)},
                        {"field", Str(error.Failure().diagnostic_.field_.value_or(""))}});
        }
        return Obj({{"requested", Str("2028-11-06")}, {"accepted", Bool(true)}});
    }
    Json_ StageGeometry() {
        const auto spec = TestXccyAcceptance::StagedSpec({0., 0., 0.});
        const auto modelSpec = TestXccyAcceptance::StagedModel();
        const auto model = Data(modelSpec);
        std::vector<Json_> rows;
        for (const auto& observation : spec.observations_) {
            const auto instrument = observation->Instrument();
            const auto prepared = Prepare(instrument, modelSpec);
            const auto& descriptor = **std::get_if<CalibrationXccy_>(&prepared.Descriptor());
            auto row = CaseMetadata("staged", XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
            row.object_["id"] = Str(instrument->Id());
            row.object_["source"] = Str(instrument->Definition()->SourceIdentity());
            row.object_["model"] = Model(*model);
            row.object_["maturity"] = Str(Date::ToString(descriptor.plan_.maturity_));
            row.object_["domestic_periods"] = Periods(descriptor.plan_.domesticPeriods_);
            row.object_["foreign_periods"] = Periods(descriptor.plan_.foreignPeriods_);
            row.object_["payment_lag"] = Num(2.);
            rows.push_back(std::move(row));
        }
        return Obj({{"cases", List(std::move(rows))},
                    {"named_maturity_qualification", NamedMaturityQualification()},
                    {"knots", Dates(spec.knotDates_)},
                    {"target", Numbers(TestXccyAcceptance::StagedTarget())},
                    {"initial", Numbers(spec.solver_.initialGuessPerNode_)},
                    {"scale", Num(1e-4)}});
    }
    Vector_<> ReadQuotes(const char* path) {
        std::ifstream input(path);
        REQUIRE(input.good(), "independent quote input available");
        Vector_<> result;
        double value;
        while (input >> value)
            result.push_back(value);
        REQUIRE(input.eof() && result.size() == 3, "exactly three finite independent quotes");
        return result;
    }
    Json_ BasketAdmission(const XccyInstrumentCalibrationSpec_& spec, const Handle_<XccyGaussianModelData_>& model) {
        auto settings = Settings(16, 16384);
        settings.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
        CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const XccyScriptContext_>(spec.context_);
        Vector_<XccyMcReservation_> reservations;
        Vector_<> dimensions;
        for (const auto& row : spec.observations_) {
            const PreparedXccyMc_ prepared(row->Instrument(), "Price", context, model, settings);
            reservations.push_back(prepared.Reservation());
            dimensions.push_back(static_cast<double>(prepared.Mapping().dimension_));
        }
        try {
            RequireXccyMcIndependent(reservations[0], reservations[1]);
        } catch (const CalibrationError_& error) {
            return Obj({{"admitted", Bool(false)},
                        {"draws_started", Bool(false)},
                        {"dimensions", Numbers(dimensions)},
                        {"code", Str(error.Failure().diagnostic_.code_)},
                        {"field", Str(error.Failure().diagnostic_.field_.value_or(""))},
                        {"expected", Str(error.Failure().diagnostic_.expected_.value_or(""))}});
        }
        return Obj({{"admitted", Bool(true)}, {"draws_started", Bool(false)}, {"dimensions", Numbers(dimensions)}});
    }
    Json_ StageFit(const XccyInstrumentCalibrationSpec_& input, CurveSolveMode_ mode) {
        auto spec = input;
        spec.solver_.solveMode_ = mode;
        CurveInstrumentCalibrationOptions_ options;
        options.computeQuoteJacobian_ = true;
        options.jacobianMode_ = CalibrationDerivative_::Value_::AAD;
        const auto result = CalibrateXccyInstruments(spec, options);
        CalibrationPreparationContext_ context;
        context.xccy_ = std::make_shared<const XccyScriptContext_>(spec.context_);
        std::vector<Json_> cases;
        for (const auto& observation : spec.observations_) {
            auto metadata = CaseMetadata("staged", XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
            metadata.object_["id"] = Str(observation->Instrument()->Id());
            metadata.object_["payment_lag"] = Num(2.);
            const auto prepared = PrepareMoment(observation->Instrument(), context);
            const auto& descriptor = **std::get_if<CalibrationXccy_>(&prepared.Descriptor());
            metadata.object_["maturity"] = Str(Date::ToString(descriptor.plan_.maturity_));
            cases.push_back(CapturePrepared(observation->Instrument(), result.gaussianModel_, context, metadata));
        }
        return Obj({{"mode", Str(mode.String())},
                    {"parameters", Numbers(result.parameters_)},
                    {"initial", Numbers(result.initialParameters_)},
                    {"free_parameters",
                     Array(result.freeParameters_,
                           [](const auto& p) { return Obj({{"date", Str(Date::ToString(p.date_))}, {"component", Str(p.component_.String())}}); })},
                    {"row_ids", Array(result.axes_.observations_, [](const auto& row) { return Str(row->Id()); })},
                    {"row_scales", Array(result.axes_.observations_, [](const auto& row) { return Num(row->ErrorScale()); })},
                    {"raw_residuals", Numbers(result.rawResiduals_)},
                    {"scaled_norm", Num(result.scaledNorm_)},
                    {"max_abs_scaled", Num(result.maxAbsScaledResidual_)},
                    {"fit_within_tolerance", Bool(result.fitWithinTolerance_)},
                    {"raw_jacobian", Matrix(result.quoteJacobian_)},
                    {"model_measures", Numbers(result.modelMeasures_)},
                    {"cases", List(std::move(cases))},
                    {"held_out_basket", BasketAdmission(spec, result.gaussianModel_)}});
    }
    Json_ StagedRequest(int argc, char** argv) {
        REQUIRE(argc == 4, "staged capture requires independent quote file");
        const auto quotes = ReadQuotes(argv[3]);
        const auto spec = TestXccyAcceptance::StagedSpec(quotes);
        return Obj({{"quotes", Numbers(quotes)},
                    {"target", Numbers(TestXccyAcceptance::StagedTarget())},
                    {"fits", List({StageFit(spec, CurveSolveMode_::Value_::EXACT), StageFit(spec, CurveSolveMode_::Value_::APPROXIMATE)})}});
    }
    Json_ BroadCase(const String_& scenario, XccyNotionalMode_ mode, bool foreign, bool receive) {
        const auto fixture = TestXccyAcceptance::BroadFixture(scenario, mode, foreign, receive);
        const auto& spec = fixture.spec_;
        const auto& instrument = fixture.instrument_;
        auto metadata = CaseMetadata(scenario, mode, foreign, receive);
        const bool noLag = mode == XccyNotionalMode_::Value_::FIXED;
        metadata.object_["payment_lag"] = Num(noLag ? 0. : 2.);
        const auto prepared = Prepare(instrument, spec);
        metadata.object_["native_price"] = Num(Native(prepared, spec));
        metadata.object_["native_common_support"] = Bool(noLag);
        return CapturePrepared(instrument, Data(spec), Context(spec, Empty()), metadata);
    }
    Json_ BroadCapture() {
        std::vector<Json_> rows;
        for (const String_ scenario : {"rho-df", "rho-dx", "rho-fx", "negative-basis", "zero-basis", "own-projection", "nonflat"})
            for (const auto mode : XccyNotionalModeListAll())
                for (bool foreign : {false, true})
                    for (bool receive : {false, true})
                        rows.push_back(BroadCase(scenario, mode, foreign, receive));
        return List(std::move(rows));
    }
    TestXccyAcceptance::Fixture_ MatrixFixture(const String_& profile) {
        if (profile == "fixed")
            return TestXccyAcceptance::BroadFixture("rho-df", XccyNotionalMode_::Value_::FIXED, true, false);
        if (profile == "resettable")
            return TestXccyAcceptance::BroadFixture("negative-basis", XccyNotionalMode_::Value_::RESETTABLE, true, true);
        REQUIRE(profile == "nonflat", "fixed/resettable/nonflat matrix profile");
        return TestXccyAcceptance::BroadFixture("nonflat", XccyNotionalMode_::Value_::MARK_TO_MARKET, true, false);
    }
    Vector_<String_> MatrixLabels(const XccyGaussianModelData_& model) {
        Vector_<String_> labels, selected;
        static_cast<void>(XccyGaussianMomentNativeMap(model, &labels));
        for (const auto& label : labels)
            if (label == "fxspot:EUR/USD" || label.find("logdf:USD:BASIS:") == 0)
                selected.push_back(label);
        return selected;
    }
    Json_ MatrixRanges(const XccyMcReservation_& reservation) {
        return Array(reservation.ranges_, [](const auto& r) {
            return Numbers({static_cast<double>(r.firstPath_), static_cast<double>(r.pathEnd_), static_cast<double>(r.firstCoordinate_),
                            static_cast<double>(r.coordinateEnd_)});
        });
    }
    Json_ MatrixResult(const XccyMcResult_& result, const XccyMcSettings_& settings) {
        return Obj({{"mapping", Str(result.mapping_.identity_)},
                    {"reservation", Str(result.reservation_.identity_)},
                    {"phase", Str(settings.phase_.String())},
                    {"dimension", Num(static_cast<double>(result.mapping_.dimension_))},
                    {"coordinates", Array(result.mapping_.normalCoordinates_, [](const auto& s) { return Str(s); })},
                    {"labels", Array(settings.parameterLabels_, [](const auto& s) { return Str(s); })},
                    {"validation", Bool(result.estimate_.validation_)},
                    {"held_out", Bool(result.estimate_.heldOut_)},
                    {"mean", Num(result.estimate_.mean_)},
                    {"se", Num(result.estimate_.standardError_)},
                    {"gradient", Numbers(result.estimate_.gradient_)},
                    {"gradient_se", Numbers(result.estimate_.gradientStandardError_)},
                    {"coefficients", Coefficients(result.coefficients_)},
                    {"covariance", Matrix(result.coefficientCovariance_)},
                    {"batches", Array(result.estimate_.batches_, [](const auto& b) { return Coefficients(*b.coefficients_); })},
                    {"ranges", MatrixRanges(result.reservation_)},
                    {"first_normals", Array(result.firstNormals_, Numbers)},
                    {"consumed", Num(static_cast<double>(result.consumedCoordinates_))}});
    }
    Json_ MatrixCapture(int argc, char** argv) {
        REQUIRE(argc == 5, "matrix OUTPUT PROFILE FACTOR");
        const String_ profile(argv[3]), factor(argv[4]);
        REQUIRE(factor == "1" || factor == "2" || factor == "4", "frozen matrix prefixes");
        const auto fixture = MatrixFixture(profile);
        const auto model = Data(fixture.spec_);
        auto settings = Settings(16, 16384 * std::stoi(argv[4]));
        settings.phase_ = XccyMonteCarloPhase_::Value_::Validation;
        settings.aad_ = true;
        settings.parameterLabels_ = MatrixLabels(*model);
        const auto start = std::chrono::steady_clock::now();
        const PreparedXccyMc_ prepared(fixture.instrument_, "Price", Context(fixture.spec_, Empty()), model, settings);
        const double prepare = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const auto result = prepared.Evaluate();
        auto output = MatrixResult(result, settings);
        output.object_["profile"] = Str(profile);
        output.object_["factor"] = Num(std::stoi(argv[4]));
        output.object_["model"] = Model(*model);
        output.object_["source"] = Str(fixture.instrument_->Definition()->SourceIdentity());
        output.object_["prepare_seconds"] = Num(prepare);
        output.object_["evaluate_seconds"] = Num(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() - prepare);
        REQUIRE(AAD::Tape()->nodes_.Size() == 0, "matrix MC recording released");
        return output;
    }
    Json_ CaptureMetadata(Json_ data, const Handle_<CalibrationInstrument_>& instrument) {
        data.object_["instrument"] = Str(instrument->InstrumentIdentity());
        data.object_["binding"] = Str(instrument->BindingIdentity());
        data.object_["measure"] = Str("Price");
        return data;
    }
    Json_ CaptureMatrixRequest(int argc, char** argv) {
        auto data = MatrixCapture(argc, argv);
        return CaptureMetadata(std::move(data), MatrixFixture(argv[3]).instrument_);
    }
    Json_
    StatisticalContext(const String_& id, const TestXccyAcceptance::Fixture_& fixture, const Handle_<CalibrationInstrument_>& alias, bool allLabels) {
        REQUIRE(alias->Definition()->SourceIdentity() == fixture.instrument_->Definition()->SourceIdentity(), "same full economic source");
        REQUIRE(alias->BindingIdentity() == fixture.instrument_->BindingIdentity(), "same complete effective binding");
        const auto model = Data(fixture.spec_);
        const PreparedXccyMc_ prepared(fixture.instrument_, "Price", Context(fixture.spec_, Empty()), model, Settings(2, 3));
        Vector_<String_> labels;
        static_cast<void>(XccyGaussianMomentNativeMap(*model, &labels));
        return CaptureMetadata(
            Obj({{"id", Str(id)},
                 {"fixture_instrument", Str(fixture.instrument_->InstrumentIdentity())},
                 {"instrument_aliases", Array(Vector_<String_>{fixture.instrument_->InstrumentIdentity(), alias->InstrumentIdentity()},
                                              [](const auto& s) { return Str(s); })},
                 {"source", Str(fixture.instrument_->Definition()->SourceIdentity())},
                 {"model", Model(*model)},
                 {"mapping", Str(prepared.Mapping().identity_)},
                 {"dimension", Num(static_cast<double>(prepared.Mapping().dimension_))},
                 {"native_labels", Array(labels, [](const auto& s) { return Str(s); })},
                 {"selected_labels", Array(allLabels ? labels : MatrixLabels(*model), [](const auto& s) { return Str(s); })}}),
            fixture.instrument_);
    }
    Json_ StatisticalContexts() {
        const auto base = TestXccyAcceptance::Fixture("flat", XccyNotionalMode_::Value_::MARK_TO_MARKET, false, true);
        std::vector<Json_> rows{
            StatisticalContext("flat/MARK_TO_MARKET/domestic/receive", base, Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET), true)};
        const std::map<String_, String_> profiles{{"fixed", "rho-df/FIXED/foreign/pay"},
                                                  {"resettable", "negative-basis/RESETTABLE/foreign/receive"},
                                                  {"nonflat", "nonflat/MARK_TO_MARKET/foreign/pay"}};
        for (const auto& [profile, id] : profiles) {
            const auto fixture = MatrixFixture(profile);
            rows.push_back(StatisticalContext(id, fixture, fixture.instrument_, false));
        }
        return List(std::move(rows));
    }
    MarketFixingSnapshot_::values_t HistoricalValues() {
        MarketFixingSnapshot_::values_t values;
        values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 4, 30))] = .031;
        values["IR:USD,LIBOR_3M_LCH"][DateTime_(Date_(2027, 8, 2))] = .037;
        values["IR:EUR,LIBOR_6M_LCH"][DateTime_(Date_(2027, 4, 30))] = .029;
        values["FX[EUR/USD]"][DateTime_(Date_(2027, 8, 4))] = 1.17;
        return values;
    }
    Json_ HistoryInput(const MarketFixingSnapshot_::values_t& values) {
        auto result = Json_::Object();
        for (const auto& [index, dates] : values) {
            auto row = Json_::Object();
            for (const auto& [date, value] : dates)
                row.object_[Bytes(Date::ToString(date.Date()))] = Num(value);
            result.object_[Bytes(index)] = std::move(row);
        }
        return result;
    }
    Json_ HistoryCapture() {
        std::vector<Json_> rows;
        const auto values = HistoricalValues();
        const Handle_<MarketFixingSnapshot_> history(new MarketFixingSnapshot_(values));
        for (const Date_ date : {Date_(2027, 8, 4), Date_(2027, 11, 5), Date_(2027, 11, 9)})
            for (const bool foreign : {false, true}) {
                const auto spec = ModelSpec(date);
                const auto instrument = Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign);
                auto metadata = CaseMetadata("history", XccyNotionalMode_::Value_::MARK_TO_MARKET, foreign, true);
                metadata.object_["id"] = Str("history/" + Date::ToString(date) + (foreign ? "/foreign" : "/domestic"));
                metadata.object_["history"] = HistoryInput(values);
                metadata.object_["payment_lag"] = Num(2.);
                rows.push_back(CapturePrepared(instrument, Data(spec), Context(spec, history), metadata));
            }
        return List(std::move(rows));
    }
    Json_ CaptureAll() {
        std::vector<Json_> cases;
        for (const String_ scenario : {"flat", "zero", "nonflat", "no-lag", "zero-a"})
            for (const auto mode : XccyNotionalModeListAll())
                for (bool foreign : {false, true})
                    for (bool receive : {false, true})
                        cases.push_back(Capture(scenario, mode, foreign, receive));
        return List(std::move(cases));
    }
    Json_ CaptureMcRequest(int argc, char** argv) {
        REQUIRE(argc == 5, "explicit MC arguments");
        const String_ factor(argv[4]);
        REQUIRE(factor == "1" || factor == "2" || factor == "4", "frozen refinements");
        return CaptureMetadata(Mc(argv[3], std::stoi(argv[4])), Instrument(XccyNotionalMode_::Value_::MARK_TO_MARKET));
    }
    Json_ ExtendedRequest(const String_& action, int argc, char** argv) {
        if (action == "contexts")
            return StatisticalContexts();
        if (action == "geometry")
            return StageGeometry();
        if (action == "staged")
            return StagedRequest(argc, argv);
        if (action == "broad")
            return BroadCapture();
        if (action == "matrix")
            return CaptureMatrixRequest(argc, argv);
        REQUIRE(action == "historical", "known capture mode");
        return HistoryCapture();
    }
    Json_ CaptureRequest(int argc, char** argv) {
        const String_ action(argv[2]);
        if (action == "capture")
            return CaptureAll();
        if (action == "controls")
            return Obj({{"kernels", KernelControls()}, {"history", HistoryControls()}});
        if (action == "crn")
            return CrnControls();
        if (action == "mc")
            return CaptureMcRequest(argc, argv);
        return ExtendedRequest(action, argc, argv);
    }
} // namespace

int main(int argc, char** argv) try {
    REQUIRE(argc >= 3 && argc <= 5, "capture OUTPUT MODE [ARGUMENTS]");
    Dal::RegisterAll_::Init();
    const auto output = CaptureRequest(argc, argv);
    std::ofstream stream(argv[1]);
    stream << Jcs(output) << '\n';
    REQUIRE(stream.good(), "capture written");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
