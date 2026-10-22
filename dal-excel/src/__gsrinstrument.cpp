#include "__gsrinstrument.hpp"
#include "__calibrationinstrumentinput.hpp"
#include "__scriptinput.hpp"

/*IF--------------------------------------------------------------------------
public GSRInstrumentCalibrationSettings_New
    Create owning GSR script calibration settings with an explicit valuation
&inputs
settings is cell[][]
    Exact field/value rows; valuation required; nested values are typed session handles
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRInstrumentCalibrationSettings_New", "settings");
&outputs
value is handle StorableGSRInstrumentCalibrationSettings
    Immutable session settings; no archive support
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCalibrationSolver_New
    Wrap native selected-g optimizer settings without changing legacy defaults
&inputs
settings is cell[][]
    Exact snake_case field/value rows; pricing is an optional typed handle
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRCalibrationSolver_New", "settings");
&outputs
value is handle StorableGSRCalibrationSolver
    Owning native solver settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRPricingSettings_New
    Wrap native Gaussian quadrature settings
&inputs
settings is cell[][]
    quadrature_order and estimate_error field/value rows
+const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
+argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRPricingSettings_New", "settings");
&outputs
value is handle StorableGSRPricingSettings
    Owning native pricing settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Calibrate_GSRInstruments
    Calibrate selected native g coordinates against bound script Price observations
&inputs
initial is handle ModelData
    Initial native multi-factor Gaussian model
observations is handle[]
    Ordered typed CalibrationObservation handles
parameters is cell[][]
    Exact factor,knot,lower,upper header and numeric rows
+const Excel::ScriptSettingsInput_ normalized_parameters(xl_parameters); xl_parameters = normalized_parameters.Get();
settings is handle StorableGSRInstrumentCalibrationSettings
    Required owning domain settings
&outputs
outcome is handle StorableCalibrationOutcome
    Succeeded, Candidate or Failed with an owning report and any available model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationOutcome_Get_Model
    Return an available GSR model candidate from a calibration outcome
&inputs
outcome is handle StorableCalibrationOutcome
    Owning calibration outcome
&outputs
model is handle ModelData
    Model candidate; unavailable is an error
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        using Excel::Calibration::Input_;
        using Excel::Calibration::Rows_;

        class GsrFields_ {
            Excel::Calibration::Fields_ fields_;
            const Matrix_<Cell_>& cells_;
            String_ function_;
            String_ field_;
            std::set<std::string> used_;

        public:
            GsrFields_(const Matrix_<Cell_>& cells, const String_& function) : fields_(cells), cells_(cells), function_(function) {
                std::map<String_, int> keys;
                for (int i = 0; i < cells.Rows() && cells.Cols() == 2; ++i) {
                    const auto key = Excel::Calibration::Text(cells(i, 0));
                    REQUIRE(keys.emplace(key, i).second, Location(key, i + 1) + "duplicate normalized field");
                }
            }
            String_ Location(const String_& key, int row = 0) const {
                if (!row)
                    for (int i = 0; i < cells_.Rows() && cells_.Cols() == 2; ++i)
                        if (Cell::IsString(cells_(i, 0)) && Cell::ToString(cells_(i, 0)) == key)
                            row = i + 1;
                return Excel::ScriptSettingLocation(function_, "settings", std::max(1, row), 2) + "field=" + key + "; ";
            }
            template <class T_> T_ Required(const char* key) {
                field_ = key;
                used_.insert(key);
                return fields_.Required<T_>(key);
            }
            template <class T_> T_ Default(const char* key, const T_& fallback) {
                field_ = key;
                used_.insert(key);
                return fields_.Default<T_>(key, fallback);
            }
            template <class T_> std::optional<T_> Maybe(const char* key) {
                field_ = key;
                used_.insert(key);
                return fields_.Maybe<T_>(key);
            }
            void Finish() {
                for (int i = 0; i < cells_.Rows() && cells_.Cols() == 2; ++i) {
                    const auto key = Excel::Calibration::Text(cells_(i, 0));
                    if (!used_.count(Excel::Calibration::Bytes(key))) {
                        field_ = key;
                        THROW(Location(key) + "unknown field");
                    }
                }
                fields_.Finish();
            }
            [[nodiscard]] String_ Failure(const Exception_& error) const {
                String_ field = field_;
                if (const auto* calibration = dynamic_cast<const CalibrationError_*>(&error)) {
                    const auto path = calibration->Failure().diagnostic_.field_.value_or("");
                    const auto slash = path.find('/', 10);
                    if (path.size() >= 10)
                        field = path.substr(10, slash == String_::npos ? String_::npos : slash - 10);
                }
                return Location(field) + error.what();
            }
        };

        GSRInstrumentCalibrationSettings_ ReadSettings(GsrFields_& f) {
            GSRInstrumentCalibrationSettings_ s;
            s.valuation_ = f.Required<Script::ScriptValuationSettings_>("valuation");
            s.solver_ = f.Default<GSRCalibrationSettings_>("solver", s.solver_);
            s.evaluation_ = f.Default<CalibrationEvaluationOptions_>("evaluation", s.evaluation_);
            s.fitSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("fit_sampling");
            s.validationSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("validation_sampling");
            s.heldOutSampling_ = f.Maybe<CalibrationMonteCarloSampling_>("held_out_sampling");
            s.heldOutObservations_ = f.Default<Vector_<Handle_<CalibrationObservation_>>>("held_out_observations", s.heldOutObservations_);
            s.maxPathsPerView_ = f.Default<int>("max_paths_per_view", s.maxPathsPerView_);
            s.maxBasketEvaluations_ = f.Default<int>("max_basket_evaluations", s.maxBasketEvaluations_);
            s.validationSigma_ = f.Default<double>("validation_sigma", s.validationSigma_);
            s.epsilonValue_ = f.Default<double>("epsilon_value", s.epsilonValue_);
            s.epsilonGradient_ = f.Default<double>("epsilon_gradient", s.epsilonGradient_);
            s.compiled_ = f.Default<bool>("compiled", s.compiled_);
            f.Finish();
            ValidateGSRInstrumentCalibrationSettings(s);
            return s;
        }
        Vector_<GSRCalibrationParameter_> Parameters(const Matrix_<Cell_>& table) {
            const Rows_ rows(table, {"factor", "knot", "lower", "upper"});
            Vector_<GSRCalibrationParameter_> result;
            for (int i = 0; i < rows.Size(); ++i) {
                const GSRCalibrationParameter_ p{rows.Get<int>(i, "factor"), rows.Get<int>(i, "knot"), rows.Get<double>(i, "lower"),
                                                 rows.Get<double>(i, "upper")};
                REQUIRE(p.factor_ >= 0 && p.knot_ >= 0 && p.lower_ >= 0. && p.upper_ > p.lower_,
                        "InvalidCalibrationInput: row=" + String::FromInt(i + 2) +
                            "; column=factor/knot/lower/upper; expected valid coordinates and bounds");
                result.push_back(p);
            }
            return result;
        }
        Vector_<Handle_<CalibrationObservation_>> Observations(const Vector_<Handle_<Storable_>>& handles) {
            Vector_<Handle_<CalibrationObservation_>> result;
            for (const auto& handle : handles) {
                const auto row = handle_cast<StorableCalibrationObservation_>(handle);
                REQUIRE(row, "InvalidCalibrationInput: observations row=" + String::FromInt(static_cast<int>(result.size()) + 1) +
                                 "; column=1; expected typed observation");
                result.push_back(row->val_);
            }
            return result;
        }
    } // namespace

    void GSRInstrumentCalibrationSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableGSRInstrumentCalibrationSettings_>* value) {
        try {
            GsrFields_ fields(settings, "GSRInstrumentCalibrationSettings_New");
            try {
                value->reset(new StorableGSRInstrumentCalibrationSettings_(ReadSettings(fields)));
            } catch (const Exception_& error) {
                THROW(fields.Failure(error));
            }
        } catch (const Exception_& error) {
            if (String_(error.what()).find("GSRInstrumentCalibrationSettings_New") != String_::npos)
                throw;
            THROW("GSRInstrumentCalibrationSettings_New: settings row=1 column=1; " + String_(error.what()));
        }
    }
    void GSRPricingSettings_New(const Matrix_<Cell_>& settings, Handle_<StorableGSRPricingSettings_>* value) {
        GsrFields_ f(settings, "GSRPricingSettings_New");
        try {
            GSRPricingSettings_ s;
            s.quadratureOrder_ = f.Default<int>("quadrature_order", s.quadratureOrder_);
            s.estimateError_ = f.Default<bool>("estimate_error", s.estimateError_);
            f.Finish();
            REQUIRE(s.quadratureOrder_ >= 2 && s.quadratureOrder_ <= 32, "quadrature_order must be 2 through 32");
            value->reset(new StorableGSRPricingSettings_(s));
        } catch (const Exception_& error) {
            THROW(f.Failure(error));
        }
    }
    void GSRCalibrationSolver_New(const Matrix_<Cell_>& settings, Handle_<StorableGSRCalibrationSolver_>* value) {
        GsrFields_ f(settings, "GSRCalibrationSolver_New");
        try {
            GSRCalibrationSettings_ s;
            s.maxIterations_ = f.Default<int>("max_iterations", s.maxIterations_);
            s.gradientTolerance_ = f.Default<double>("gradient_tolerance", s.gradientTolerance_);
            s.stepTolerance_ = f.Default<double>("step_tolerance", s.stepTolerance_);
            s.finiteDifferenceStep_ = f.Default<double>("finite_difference_step", s.finiteDifferenceStep_);
            s.parameterScale_ = f.Default<double>("parameter_scale", s.parameterScale_);
            s.priorWeight_ = f.Default<double>("prior_weight", s.priorWeight_);
            s.smoothingWeight_ = f.Default<double>("smoothing_weight", s.smoothingWeight_);
            s.numericalErrorFraction_ = f.Default<double>("numerical_error_fraction", s.numericalErrorFraction_);
            s.pricing_ = f.Default<GSRPricingSettings_>("pricing", s.pricing_);
            f.Finish();
            ValidateGSRInstrumentSolverSettings(s);
            value->reset(new StorableGSRCalibrationSolver_(s));
        } catch (const Exception_& error) {
            THROW(f.Failure(error));
        }
    }
    void Calibrate_GSRInstruments(const Handle_<ModelData_>& initial,
                                  const Vector_<Handle_<Storable_>>& observations,
                                  const Matrix_<Cell_>& parameters,
                                  const Handle_<StorableGSRInstrumentCalibrationSettings_>& settings,
                                  Handle_<StorableCalibrationOutcome_>* outcome) {
        REQUIRE(settings, "Calibrate_GSRInstruments: settings row=1 column=1; expected typed settings");
        Vector_<GSRCalibrationParameter_> coordinates;
        Vector_<Handle_<CalibrationObservation_>> rows;
        try {
            coordinates = Parameters(parameters);
            rows = Observations(observations);
        } catch (const Exception_& error) {
            THROW("Calibrate_GSRInstruments: " + String_(error.what()));
        }
        try {
            const auto r = CalibrateGSRInstruments(initial, rows, coordinates, settings->val_);
            outcome->reset(new StorableCalibrationOutcome_(r.report_->Data().summary_.status_, r.report_, {}, {}, Handle_<ModelData_>(r.model_)));
        } catch (const CalibrationError_& error) {
            const auto& failure = error.Failure();
            outcome->reset(new StorableCalibrationOutcome_("Failed", failure.report_, {}, {}, handle_cast<ModelData_>(failure.candidate_)));
        }
    }
    void CalibrationOutcome_Get_Model(const Handle_<StorableCalibrationOutcome_>& outcome, Handle_<ModelData_>* model) {
        REQUIRE(outcome && outcome->model_, "CalibrationCandidateUnavailable: no GSR model candidate");
        *model = outcome->model_;
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Calibrate_GSRInstruments_public.inc>
#include <dal-excel/auto/MG_CalibrationOutcome_Get_Model_public.inc>
#include <dal-excel/auto/MG_GSRCalibrationSolver_New_public.inc>
#include <dal-excel/auto/MG_GSRInstrumentCalibrationSettings_New_public.inc>
#include <dal-excel/auto/MG_GSRPricingSettings_New_public.inc>
#endif
} // namespace Dal
