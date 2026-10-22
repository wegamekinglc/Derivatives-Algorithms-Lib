#include <gtest/gtest.h>

#include <dal-excel/src/__gsrinstrument.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/global.hpp>

using namespace Dal;

namespace {
    template <class T_> String_ Store(const Handle_<T_>& value) { return Excel::ScriptTestStoreRepository(handle_cast<Storable_>(value)); }
    Matrix_<Cell_> Fields(std::initializer_list<std::pair<const char*, Cell_>> fields) {
        Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
        int row = 0;
        for (const auto& field : fields) {
            result(row, 0) = field.first;
            result(row++, 1) = field.second;
        }
        return result;
    }
    struct Fixture_ {
        const Date_ today_{2026, 10, 21}, expiry_{2027, 10, 21}, maturity_{2028, 10, 21};
        Handle_<ModelData_> model_;
        Handle_<StorableScriptValuationSettings_> valuation_;
        Vector_<Handle_<Storable_>> rows_;
        Matrix_<Cell_> parameters_{2, 4};
        Fixture_() {
            InitGlobalData();
            ScriptValuationSettings_ v;
            v.evaluationDate_ = today_;
            v.fixings_.reset(new MarketFixingSnapshot_());
            valuation_.reset(new StorableScriptValuationSettings_("", v));
            Handle_<GSRCurveData_> curve(new GSRCurveData_("curve", today_, "USD", {today_, maturity_}, {0., -.08}, {}, {}));
            Handle_<MultiFactorGSRVolData_> vol(
                new MultiFactorGSRVolData_("vol", {"level"}, {today_}, Matrix_<>(1, 1, .02), {today_}, Matrix_<>(1, 1, 1.), Matrix_<>(1, 1, 1.)));
            model_.reset(new MultiFactorGSRModelData_("model", curve, vol));
            const BondOption_ option{expiry_, maturity_, .96, OptionType_("Call")};
            const auto instrument = NewEuropeanRateOptionScriptInstrument(option, {"bond", Ccy_("USD"), 100., "USD.OIS"});
            const auto quote =
                NewCalibrationMarketQuote("quote", 100. * PriceGSREuropeanOption(model_, option).price_, CalibrationValueUnit_::Parse("Price:USD"));
            rows_ = {Handle_<Storable_>(new StorableCalibrationObservation_(NewCalibrationObservation("obs", instrument, "Price", quote, .1)))};
            const char* columns[] = {"factor", "knot", "lower", "upper"};
            const double values[] = {0., 0., .001, .1};
            for (int j = 0; j < 4; ++j) {
                parameters_(0, j) = columns[j];
                parameters_(1, j) = values[j];
            }
        }
        Matrix_<Cell_> Settings() const { return Fields({{"valuation", Cell_(Store(valuation_))}}); }
        Handle_<StorableGSRInstrumentCalibrationSettings_> Mc(bool fail = false) const {
            Handle_<StorableGSRCalibrationSolver_> solver;
            GSRCalibrationSolver_New(Fields({{"max_iterations", Cell_(1.)}}), &solver);
            Handle_<StorableCalibrationEvaluationOptions_> evaluation;
            CalibrationEvaluationOptions_New(Fields({{"evaluation", Cell_("ScriptMC")}, {"allow_monte_carlo_calibration", Cell_(true)}}),
                                             &evaluation);
            Handle_<StorableCalibrationMonteCarloSampling_> fit, validation;
            const auto sampling = [](double paths) {
                return Fields({{"paths_per_batch", Cell_(paths)},
                               {"batches", Cell_(2.)},
                               {"seed", Cell_(1024.)},
                               {"stream_version", Cell_("dal.mrg32-normal-paths/1")}});
            };
            CalibrationMonteCarloSampling_New(sampling(16.), &fit);
            CalibrationMonteCarloSampling_New(sampling(32.), &validation);
            Handle_<StorableGSRInstrumentCalibrationSettings_> result;
            GSRInstrumentCalibrationSettings_New(Fields({{"valuation", Cell_(Store(valuation_))},
                                                         {"solver", Cell_(Store(solver))},
                                                         {"evaluation", Cell_(Store(evaluation))},
                                                         {"fit_sampling", Cell_(Store(fit))},
                                                         {"validation_sampling", Cell_(Store(validation))},
                                                         {"max_paths_per_view", Cell_(96.)},
                                                         {"max_basket_evaluations", Cell_(fail ? 1. : 1000.)}}),
                                                 &result);
            return result;
        }
    };
} // namespace

TEST(ExcelGsrInstrumentTest, TestSettingsMissingValuationIsWorksheetInputError) {
    Dal::Handle_<Dal::StorableGSRInstrumentCalibrationSettings_> value;
    try {
        Dal::GSRInstrumentCalibrationSettings_New({}, &value);
        FAIL() << "Expected missing valuation error";
    } catch (const Dal::Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("GSRInstrumentCalibrationSettings_New"), std::string::npos);
        ASSERT_NE(message.find("valuation"), std::string::npos);
        ASSERT_NE(message.find("row="), std::string::npos);
    }
    ASSERT_FALSE(value);
}

TEST(ExcelGsrInstrumentTest, TestSolverSemanticErrorUsesItsActualFieldRow) {
    Handle_<StorableGSRCalibrationSolver_> value;
    try {
        GSRCalibrationSolver_New(Fields({{"parameter_scale", Cell_(.01)}, {"max_iterations", Cell_(0.)}}), &value);
        FAIL() << "Expected invalid iteration limit";
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("GSRCalibrationSolver_New"), std::string::npos);
        ASSERT_NE(message.find("settings row=2 column=2; field=max_iterations;"), std::string::npos) << message;
    }
    ASSERT_FALSE(value);
}

TEST(ExcelGsrInstrumentTest, TestPricingSemanticErrorUsesItsActualFieldRow) {
    Handle_<StorableGSRPricingSettings_> value;
    try {
        GSRPricingSettings_New(Fields({{"estimate_error", Cell_(true)}, {"quadrature_order", Cell_(1.)}}), &value);
        FAIL() << "Expected invalid quadrature order";
    } catch (const Exception_& error) {
        const std::string message = error.what();
        ASSERT_NE(message.find("GSRPricingSettings_New"), std::string::npos);
        ASSERT_NE(message.find("settings row=2 column=2; field=quadrature_order;"), std::string::npos) << message;
    }
    ASSERT_FALSE(value);
}

TEST(ExcelGsrInstrumentTest, TestStrictFieldsAndParameterRows) {
    const Fixture_ f;
    Handle_<StorableGSRInstrumentCalibrationSettings_> settings;
    try {
        GSRInstrumentCalibrationSettings_New(Fields({{"valuation", Cell_(Store(f.valuation_))}, {"valuation", Cell_(Store(f.valuation_))}}),
                                             &settings);
        FAIL() << "Expected duplicate at the second field row";
    } catch (const Exception_& error) {
        ASSERT_NE(std::string(error.what()).find("row=2 column=1"), std::string::npos);
    }
    for (const auto& rows : {Fields({{"valuation", Cell_(Store(f.valuation_))}, {"Valuation", Cell_(Store(f.valuation_))}}),
                             Fields({{"valuation", Cell_(Store(f.valuation_))}, {"unknown", Cell_(1.)}}),
                             Fields({{"valuation", Cell_(Store(f.valuation_))}, {"compiled", Cell_(1.)}}),
                             Fields({{"valuation", Cell_(Store(f.valuation_))}, {"max_paths_per_view", Cell_(true)}}),
                             Fields({{"valuation", Cell_(Store(f.valuation_))}, {"max_basket_evaluations", Cell_(1.5)}}),
                             Fields({{"valuation", Cell_(Store(f.valuation_))}, {"solver", Cell_(Store(f.valuation_))}})}) {
        ASSERT_THROW(GSRInstrumentCalibrationSettings_New(rows, &settings), Exception_);
        ASSERT_FALSE(settings);
    }
    GSRInstrumentCalibrationSettings_New(f.Settings(), &settings);
    auto parameters = f.parameters_;
    Handle_<StorableCalibrationOutcome_> outcome;
    for (const auto& cell : {Cell_(true), Cell_(.5), Cell_("0"), Cell_()}) {
        parameters(1, 0) = cell;
        try {
            Calibrate_GSRInstruments(f.model_, f.rows_, parameters, settings, &outcome);
            FAIL() << "Expected strict parameter input error";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("row=2; column=factor"), std::string::npos);
        }
        ASSERT_FALSE(outcome);
    }
}

TEST(ExcelGsrInstrumentTest, TestFastParityAndOwnedModelReportGetters) {
    const Fixture_ f;
    Handle_<StorableGSRInstrumentCalibrationSettings_> settings;
    GSRInstrumentCalibrationSettings_New(f.Settings(), &settings);
    Handle_<StorableCalibrationOutcome_> outcome;
    Calibrate_GSRInstruments(f.model_, f.rows_, f.parameters_, settings, &outcome);
    ASSERT_EQ(outcome->status_, "Succeeded");
    Handle_<ModelData_> model;
    CalibrationOutcome_Get_Model(outcome, &model);
    ASSERT_TRUE(handle_cast<MultiFactorGSRModelData_>(model));
    const auto nativeRows = Vector_<Handle_<CalibrationObservation_>>{handle_cast<StorableCalibrationObservation_>(f.rows_[0])->val_};
    const auto native = CalibrateGSRInstruments(f.model_, nativeRows, {{0, 0, .001, .1}}, settings->val_);
    ASSERT_EQ(DumpCalibrationReportJson(outcome->report_), DumpCalibrationReportJson(native.report_));
    Matrix_<Cell_> table;
    CalibrationReport_Get_Observations(outcome->report_, &table);
    ASSERT_EQ(table.Cols(), 22);
    ASSERT_EQ(Cell::ToString(table(1, 19)), "FastGaussian");
    ASSERT_EQ(Cell::ToString(table(1, 20)), "FiniteDifference");
    Matrix_<> matrix;
    CalibrationReport_Get_QuoteJacobian(outcome->report_, &matrix);
    ASSERT_DOUBLE_EQ(matrix(0, 0), native.quoteJacobian_(0, 0));
}

TEST(ExcelGsrInstrumentTest, TestMcCandidateAndFailedKeepIndependentFlagsAndCandidate) {
    Handle_<StorableCalibrationOutcome_> outcome, failed;
    {
        const Fixture_ f;
        Calibrate_GSRInstruments(f.model_, f.rows_, f.parameters_, f.Mc(), &outcome);
        Calibrate_GSRInstruments(f.model_, f.rows_, f.parameters_, f.Mc(true), &failed);
    }
    ASSERT_NE(outcome->status_, "Failed");
    ASSERT_EQ(outcome->report_->Data().observations_[0].backend_, "ScriptMC");
    ASSERT_TRUE(outcome->report_->Data().observations_[0].standardError_);
    ASSERT_EQ(failed->status_, "Failed");
    ASSERT_EQ(failed->report_->Data().summary_.evaluations_, 1);
    ASSERT_EQ(failed->report_->Errors()[0].code_, "CalibrationBudgetExceeded");
    Handle_<ModelData_> model;
    CalibrationOutcome_Get_Model(failed, &model);
    ASSERT_TRUE(handle_cast<MultiFactorGSRModelData_>(model));
    Matrix_<Cell_> errors;
    CalibrationReport_Get_Errors(failed->report_, &errors);
    ASSERT_EQ(errors.Cols(), 14);
    Matrix_<> unavailable;
    ASSERT_THROW(CalibrationReport_Get_QuoteJacobian(failed->report_, &unavailable), CalibrationError_);
    const Handle_<StorableCalibrationOutcome_> empty(new StorableCalibrationOutcome_("Failed", failed->report_));
    ASSERT_THROW(CalibrationOutcome_Get_Model(empty, &model), Exception_);
}
