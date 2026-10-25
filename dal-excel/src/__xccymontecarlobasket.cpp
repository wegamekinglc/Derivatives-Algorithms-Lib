#include "__xccymontecarlobasket.hpp"
#include "__platform.hpp"
#include "__script_test_api.hpp"
#include "__xccyinput.hpp"

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketMembers_New
    Own an ordered XCCY basket member table
&inputs
members is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyMonteCarloBasketMembers_New", "members", 3)
    Complete member_id, instrument, measure_id columns
&outputs
value is handle StorableXccyMonteCarloBasketMembers
    Immutable typed member list
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketRequest_New
    Create an explicit XCCY basket request
&inputs
phase is cell or XccyBasketPhase->false
    Fit, Validation or HeldOut
sampling is handle StorableCalibrationMonteCarloSampling
    Explicit typed sampling
fixings is handle StorableMarketFixingSnapshot
    Required snapshot, including explicit empty history
&optional
settings is cell[][]+ or Xccy->Excel::XccyTableInput_("XccyMonteCarloBasketRequest_New", "settings", 2)
    Optional simulation, derivative_method and parameter_labels
&outputs
value is handle StorableXccyMonteCarloBasketRequest
    Immutable request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyBasket_PrepareMonteCarlo
    Prepare all basket members before drawing
&inputs
members is handle StorableXccyMonteCarloBasketMembers
    Ordered members
model is handle XccyGaussianModelData
    Explicit immutable model
request is handle StorableXccyMonteCarloBasketRequest
    Explicit phase, sampling and snapshot
&outputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Prepared or Failed outcome
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyBasket_EvaluateMonteCarlo
    Evaluate a prepared basket with checked numeric refresh
&inputs
prepared is handle StorablePreparedXccyMonteCarloBasket
    Prepared basket
&optional
candidate is handle XccyGaussianModelData
    Optional same geometry model
&outputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Estimate or Failed outcome
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Status
    Inspect basket outcome status
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Owning outcome
&outputs
status is string
    Prepared, Estimate or Failed
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Report
    Inspect basket report
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Owning outcome
&outputs
report is handle CalibrationReport
    Original owning report
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Prepared
    Extract a successfully prepared basket
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Prepared outcome
&outputs
prepared is handle StorablePreparedXccyMonteCarloBasket
    Independent session owner
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Model
    Extract the geometry validated model
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Owning outcome
&outputs
model is handle XccyGaussianModelData
    Actual validated model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Result
    Extract a complete basket estimate
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Estimate outcome
&outputs
result is handle StorableXccyMonteCarloBasketResult
    Independent complete result owner
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Partial
    Extract owned progress from a failed basket
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Failed outcome
&outputs
partial is handle StorableXccyMonteCarloBasketPartial
    Independent shared partial owner
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketOutcome_Get_Json
    Export basket outcome details
&inputs
outcome is handle StorableXccyMonteCarloBasketOutcome
    Owning outcome
&outputs
json is string[]
    Concatenate chunks without separators
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public XccyMonteCarloBasketPartial_Get_Json
    Export owned failed progress
&inputs
partial is handle StorableXccyMonteCarloBasketPartial
    Owned partial independent of outcome
&outputs
json is string[]
    Concatenate chunks without separators
-IF-------------------------------------------------------------------------*/

#ifdef _WIN32
namespace Dal::Excel {
    inline void ValidateXccyBasketPhaseInput(const OPER_* input) {
        const String_ function = "XccyMonteCarloBasketRequest_New; phase; ";
        if (input->xltype == xltypeMulti)
            REQUIRE(input->val.array.rows == 1 && input->val.array.columns == 1 && input->val.array.lparray, function + "expected scalar phase");
        const auto* cell = ScriptScalarInput(input);
        if (ScriptInputBlank(cell))
            CalibrationInputFailure("InvalidCalibrationPlan", "/phase", function + "Fit, Validation or HeldOut");
        REQUIRE(cell->xltype == xltypeStr, function + "expected text Fit, Validation or HeldOut");
    }
    inline Cell_ ToCell_orXccyBasketPhase(const OPER_* input, bool) {
        ValidateXccyBasketPhaseInput(input);
        return ToCell(input);
    }
} // namespace Dal::Excel
#endif

namespace Dal {
    namespace {
        using namespace Excel::Calibration;
        template <class T_> const T_& Value(const Handle_<StorableCalibrationValue_<T_>>& input, const char* field) {
            if (!input)
                CalibrationInputFailure("InvalidCalibrationPlan", field, "nonnull typed basket session handle");
            return input->val_;
        }
        template <class T_> Handle_<T_> Available(const Handle_<T_>& input, const String_& field) {
            if (!input)
                CalibrationInputFailure("CalibrationCandidateUnavailable", field, "available owned basket object");
            return input;
        }
        String_ Location(int row, int column) {
            return Excel::ScriptSettingLocation("XccyMonteCarloBasketMembers_New", "members", row + 1, column + 1);
        }
        XccyMonteCarloPhase_ Phase(const Cell_& phase) {
            const String_ function = "XccyMonteCarloBasketRequest_New; phase; ";
            if (!Cell::IsEmpty(phase) && !Cell::IsString(phase))
                CalibrationInputFailure("InvalidCalibrationPlan", "/phase", function + "text Fit, Validation or HeldOut");
            try {
                return ParseXccyMonteCarloPhase(Cell::IsEmpty(phase) ? String_() : Input_<String_>::Read(phase));
            } catch (const CalibrationError_& error) {
                auto failure = std::make_shared<CalibrationFailure_>(error.Failure());
                failure->diagnostic_.message_ = function + failure->diagnostic_.message_;
                failure->report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Input, {}, {failure->diagnostic_}));
                throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
            }
        }
        String_ JsonText(const String_& value) {
            String_ result = "\"";
            constexpr char HEX[] = "0123456789abcdef";
            for (const unsigned char c : value) {
                if (c == '"' || c == '\\') {
                    result += '\\';
                    result += static_cast<char>(c);
                } else if (c < 32) {
                    result += "\\u00";
                    result += HEX[c >> 4];
                    result += HEX[c & 15];
                } else
                    result += static_cast<char>(c);
            }
            return result + "\"";
        }
        template <class T_, class F_> String_ OptionalJson(const std::optional<T_>& value, F_ convert) {
            return value ? JsonText(convert(*value)) : String_("null");
        }
        String_ DiagnosticJson(const std::optional<CalibrationDiagnostic_>& diagnostic) {
            if (!diagnostic)
                return "null";
            const auto& d = *diagnostic;
            const auto text = [](const String_& x) { return x; };
            const auto integer = [](int x) { return String_(std::to_string(x)); };
            return "{\"code\":" + JsonText(d.code_) + ",\"message\":" + JsonText(d.message_) + ",\"stage\":" + JsonText(d.stage_.String()) +
                   ",\"field\":" + OptionalJson(d.field_, text) + ",\"observation_id\":" + OptionalJson(d.observationId_, text) +
                   ",\"instrument_id\":" + OptionalJson(d.instrumentId_, text) + ",\"measure_id\":" + OptionalJson(d.measureId_, text) +
                   ",\"quote_id\":" + OptionalJson(d.quoteId_, text) + ",\"parameter_key\":" + OptionalJson(d.parameterKey_, text) +
                   ",\"source_row\":" + OptionalJson(d.sourceRow_, integer) + ",\"statement\":" + OptionalJson(d.statement_, integer) +
                   ",\"event_date\":" + OptionalJson(d.eventDate_, [](const auto& x) { return Date::ToString(x); }) +
                   ",\"actual\":" + OptionalJson(d.actual_, text) + ",\"expected\":" + OptionalJson(d.expected_, text) + "}";
        }
        template <class T_> T_ TableValue(const Matrix_<Cell_>& cells, int row, int column) {
            try {
                return Input_<T_>::Read(cells(row, column));
            } catch (const CalibrationError_&) {
                throw;
            } catch (const Exception_& error) {
                THROW(Location(row, column) + error.what());
            }
        }
        std::map<std::string, int> Columns(const Matrix_<Cell_>& cells) {
            REQUIRE(cells.Rows() >= 2 && cells.Cols() == 3, Location(0, 0) + "expected three named columns and nonempty members");
            const std::set<std::string> allowed{"member_id", "instrument", "measure_id"};
            std::set<String_> normalized;
            std::map<std::string, int> result;
            for (int col = 0; col < 3; ++col) {
                const auto key = TableValue<String_>(cells, 0, col);
                REQUIRE(normalized.insert(key).second, Location(0, col) + "normalized duplicate column " + key);
                REQUIRE(allowed.count(Bytes(key)) != 0, Location(0, col) + "unknown column " + key);
                result.emplace(Bytes(key), col);
            }
            return result;
        }
        XccyMonteCarloBasketOutcome_ Failure(const CalibrationError_& error, const Handle_<XccyMcBasketPartial_>& partial) {
            XccyMonteCarloBasketOutcome_ result;
            result.status_ = "Failed";
            result.diagnostic_ = error.Failure().diagnostic_;
            result.report_ = error.Failure().report_;
            result.model_ = handle_cast<XccyGaussianModelData_>(error.Failure().candidate_);
            result.partial_ = partial;
            return result;
        }
        template <class F_> void Outcome(F_ operation, Handle_<StorableXccyMonteCarloBasketOutcome_>* output) {
            XccyMonteCarloBasketOutcome_ result;
            try {
                result = operation();
            } catch (const XccyMcBasketError_& error) {
                result = Failure(error, error.Partial());
            } catch (const CalibrationError_& error) {
                result = Failure(error, {});
            }
            output->reset(new StorableXccyMonteCarloBasketOutcome_(result));
        }
    } // namespace

    void XccyMonteCarloBasketMembers_New(const Matrix_<Cell_>& members, Handle_<StorableXccyMonteCarloBasketMembers_>* value) {
        const auto columns = Columns(members);
        Vector_<XccyMcBasketMember_> rows;
        for (int row = 1; row < members.Rows(); ++row)
            rows.push_back({TableValue<String_>(members, row, columns.at("member_id")),
                            TableValue<Handle_<CalibrationInstrument_>>(members, row, columns.at("instrument")),
                            TableValue<String_>(members, row, columns.at("measure_id"))});
        value->reset(new StorableXccyMonteCarloBasketMembers_(rows));
    }
    void XccyMonteCarloBasketRequest_New(const Cell_& phase,
                                         const Handle_<StorableCalibrationMonteCarloSampling_>& sampling,
                                         const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                         const Matrix_<Cell_>& settings,
                                         Handle_<StorableXccyMonteCarloBasketRequest_>* value) {
        XccyFields_ fields(settings, "XccyMonteCarloBasketRequest_New", {"simulation", "derivative_method", "parameter_labels"}, true);
        XccyMonteCarloBasketRequest_ r;
        r.phase_ = Phase(phase);
        r.sampling_ = Value(sampling, "/sampling");
        if (!fixings || !fixings->val_)
            CalibrationInputFailure("InvalidCalibrationPlan", "/context/fixings", "XccyMonteCarloBasketRequest_New; fixings; nonnull typed snapshot");
        r.fixings_ = fixings->val_;
        r.simulation_ = fields.Default<CalibrationSimulationSettings_>("simulation", r.simulation_);
        r.derivativeMethod_ = fields.Default<CalibrationDerivative_>("derivative_method", r.derivativeMethod_);
        r.parameterLabels_ = fields.Default<Vector_<String_>>("parameter_labels", {});
        fields.Finish();
        value->reset(new StorableXccyMonteCarloBasketRequest_(r));
    }
    void XccyBasket_PrepareMonteCarlo(const Handle_<StorableXccyMonteCarloBasketMembers_>& members,
                                      const Handle_<XccyGaussianModelData_>& model,
                                      const Handle_<StorableXccyMonteCarloBasketRequest_>& request,
                                      Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome) {
        Outcome(
            [&] {
                const auto prepared = PrepareXccyBasketMonteCarlo(Value(members, "/members"), model, Value(request, "/request"));
                XccyMonteCarloBasketOutcome_ result;
                result.status_ = "Prepared";
                result.prepared_.reset(new StorablePreparedXccyMonteCarloBasket_(prepared));
                result.report_ = prepared.Report();
                result.model_ = prepared.Model();
                return result;
            },
            outcome);
    }
    void XccyBasket_EvaluateMonteCarlo(const Handle_<StorablePreparedXccyMonteCarloBasket_>& prepared,
                                       const Handle_<XccyGaussianModelData_>& candidate,
                                       Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome) {
        Outcome(
            [&] {
                const auto value = Value(prepared, "/prepared").Evaluate(candidate);
                XccyMonteCarloBasketOutcome_ result;
                result.status_ = "Estimate";
                result.result_.reset(new StorableXccyMonteCarloBasketResult_(value));
                result.report_ = value.Report();
                result.model_ = value.Snapshot().model_;
                return result;
            },
            outcome);
    }
    void XccyMonteCarloBasketOutcome_Get_Status(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, String_* status) {
        *status = Value(outcome, "/outcome").status_;
    }
    void XccyMonteCarloBasketOutcome_Get_Report(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, Handle_<CalibrationReport_>* report) {
        *report = Value(outcome, "/outcome").report_;
    }
    void XccyMonteCarloBasketOutcome_Get_Prepared(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                  Handle_<StorablePreparedXccyMonteCarloBasket_>* prepared) {
        *prepared = Available(Value(outcome, "/outcome").prepared_, "/outcome/prepared");
    }
    void XccyMonteCarloBasketOutcome_Get_Model(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, Handle_<XccyGaussianModelData_>* model) {
        *model = Available(Value(outcome, "/outcome").model_, "/outcome/model");
    }
    void XccyMonteCarloBasketOutcome_Get_Result(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                Handle_<StorableXccyMonteCarloBasketResult_>* result) {
        *result = Available(Value(outcome, "/outcome").result_, "/outcome/result");
    }
    void XccyMonteCarloBasketOutcome_Get_Partial(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                 Handle_<StorableXccyMonteCarloBasketPartial_>* partial) {
        const auto owner = Available(Value(outcome, "/outcome").partial_, "/outcome/partial");
        partial->reset(new StorableXccyMonteCarloBasketPartial_(owner));
    }
    void XccyMonteCarloBasketOutcome_Get_Json(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, Vector_<String_>* json) {
        const auto& o = Value(outcome, "/outcome");
        const auto envelope = "{\"schema\":\"dal.xccy-gaussian-public-basket-outcome/1\",\"status\":" + JsonText(o.status_) +
                              ",\"diagnostic\":" + DiagnosticJson(o.diagnostic_) + ",\"report\":" + DumpCalibrationReportJson(o.report_) +
                              ",\"prepared_plan_identity\":" + (o.prepared_ ? JsonText(o.prepared_->val_.PlanIdentity()) : String_("null")) +
                              ",\"validated_model_identity\":" + (o.model_ ? JsonText(o.model_->Identity()) : String_("null")) +
                              ",\"result\":" + (o.result_ ? DumpXccyMonteCarloBasketResultJson(o.result_->val_) : String_("null")) +
                              ",\"partial\":" + (o.partial_ ? DumpXccyMonteCarloBasketPartialJson(o.partial_) : String_("null")) + "}";
        *json = Excel::ScriptDiagnosticChunks(envelope, "XccyMonteCarloBasketOutcome_Get_Json");
    }
    void XccyMonteCarloBasketPartial_Get_Json(const Handle_<StorableXccyMonteCarloBasketPartial_>& partial, Vector_<String_>* json) {
        const auto owner = Available(partial, "/outcome/partial");
        *json = Excel::ScriptDiagnosticChunks(DumpXccyMonteCarloBasketPartialJson(owner->val_), "XccyMonteCarloBasketPartial_Get_Json");
    }
#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_TEST_API_IMPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
    void XccyBasketFailureForTesting(const XccyMcBasketError_& error, Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome) {
        Outcome([&]() -> XccyMonteCarloBasketOutcome_ { throw error; }, outcome);
    }
#endif
} // namespace Dal

#ifdef _WIN32
using namespace Dal;
#include <dal-excel/auto/MG_XccyBasket_EvaluateMonteCarlo_public.inc>
#include <dal-excel/auto/MG_XccyBasket_PrepareMonteCarlo_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketMembers_New_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Json_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Model_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Partial_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Prepared_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Report_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Result_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketOutcome_Get_Status_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketPartial_Get_Json_public.inc>
#include <dal-excel/auto/MG_XccyMonteCarloBasketRequest_New_public.inc>
#endif
