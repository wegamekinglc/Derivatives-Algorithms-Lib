//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <dal-excel/src/__dupireinput.hpp>
#include <dal-excel/src/__dupirerisk.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/models.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/dupire.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    struct RejectGetterWork_ {
        Script::TestSupport::RejectFixingReads_ history_;
        Script::TestSupport::RejectSubmissions_ workers_;
        Detail::FixingReadObserver_* previousHistory_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_* previousWorkers_ = Excel::ScriptTestSimulationObserver();
        RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = &history_;
            Excel::ScriptTestSimulationObserver() = &workers_;
        }
        ~RejectGetterWork_() {
            Excel::ScriptTestFixingObserver() = previousHistory_;
            Excel::ScriptTestSimulationObserver() = previousWorkers_;
        }
    };

    template <class F_> void AssertError(F_ action, const char* field) {
        try {
            action();
            FAIL() << "expected error for " << field;
        } catch (const std::exception& error) {
            ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }
    struct SingleWorker_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        std::pair<size_t, bool> excelState_;
        SingleWorker_() {
            RegisterAll_::Init();
            Excel::ScriptTestInitialize(1);
            excelState_ = Excel::ScriptTestStartWorkers(1);
            pool_->Start(1, true);
        }
        ~SingleWorker_() {
            Excel::ScriptTestRestoreWorkers(excelState_);
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        double ImpliedVol(double, double) const override { return 0.2; }
    };

    DupireRiskInputs_ Inputs() { return {{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5}; }

    Handle_<StorableDupireRiskInputs_> InputHandle() {
        const auto config = Inputs();
        Handle_<StorableDupireGrid_> grid;
        DupireGrid_New("grid", config.inclusionSpots_, config.maxSpotSpacing_, config.inclusionTimes_, config.maxTimeSpacing_, &grid);
        Handle_<StorableDupireRiskInputs_> inputs;
        DupireRiskInputs_New("quotes", config.quoteStrikes_, config.quoteMaturities_, config.quoteSpreads_, grid, &inputs);
        return inputs;
    }

    Matrix_<Cell_> MertonSettings() {
        const Vector_<String_> keys{"spot", "vol", "intensity", "average_jump", "jump_std"};
        const Vector_<> values{100.0, 0.2, 0.08, -0.1, 0.15};
        Matrix_<Cell_> result(5, 2);
        for (int row = 0; row < 5; ++row) {
            result(row, 0) = keys[static_cast<size_t>(row)];
            result(row, 1) = values[static_cast<size_t>(row)];
        }
        return result;
    }

    Handle_<StorableDupireCalibration_> Calibration(bool merton) {
        Handle_<Storable_> base;
        if (merton) {
            Handle_<StorableMertonIVS_> value;
            MertonIVS_New("merton", MertonSettings(), &value);
            base = handle_cast<Storable_>(value);
        } else
            base = handle_cast<Storable_>(NewBSModelData("base", 100.0, 0.2, 0.05, 0.02));
        Handle_<StorableDupireCalibration_> result;
        DupireCalibration_New("surface", base, InputHandle(), &result);
        return result;
    }

    Handle_<ScriptProductData_> Product(double quote = 0.0) {
        std::ostringstream value;
        value << std::setprecision(17) << quote;
        Handle_<ScriptProductData_> product;
        Product_New("smooth", {Cell_("QUOTE"), Cell_(double(Date::ToExcel(Date_(2027, 9, 12))))},
                    {String_(value.str()), "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 3 * QUOTE"}, &product);
        return product;
    }

    ScriptValuationSettings_ Valuation() {
        ScriptValuationSettings_ result;
        result.evaluationDate_ = Date_(2026, 9, 12);
        return result;
    }

    double LegacyPrice(const AAD::IVS_& base, const Matrix_<>& spreads, bool compiled) {
        const auto inputs = Inputs();
        AAD::RiskView_<double> quotes(inputs.quoteStrikes_, inputs.quoteMaturities_);
        for (int row = 0; row < spreads.Rows(); ++row)
            for (int column = 0; column < spreads.Cols(); ++column)
                quotes.Bump(row, column, spreads(row, column));
        const auto numeric =
            AAD::DupireCalib(base, inputs.inclusionSpots_, inputs.maxSpotSpacing_, inputs.inclusionTimes_, inputs.maxTimeSpacing_, quotes);
        const auto surface = NewLocalVolSurfaceData("legacy", numeric.spots_, numeric.times_, numeric.lVols_);
        const BSModelData_ carry("carry", base.Spot(), 0.2, base.Rate(), base.DividendYield());
        const auto model = NewBSLocalVolModelData("legacy", "EQ[LOCAL]", "USD", "F_LOCAL", carry, surface, 0.25);
        MonteCarloSettings_ simulation;
        simulation.compiled_ = compiled;
        return ValueByMonteCarlo(Product(spreads(0, 0)), model, 257, Valuation(), simulation).at("PV");
    }

    Matrix_<> QuoteDirection(int coordinate) {
        Matrix_<> direction(3, 2, 0.0);
        if (coordinate < 6)
            direction(coordinate / 2, coordinate % 2) = 1.0;
        else
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 2; ++column)
                    direction(row, column) = std::cos(double(2 * row + column + 1));
        return direction;
    }

    double DirectionalAdjoint(const Matrix_<>& adjoints, const Matrix_<>& direction) {
        double result = 0.0;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column)
                result += adjoints(row, column) * direction(row, column);
        return result;
    }

    Matrix_<> ScaledDirection(const Matrix_<>& direction, double scale) {
        auto result = direction;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column)
                result(row, column) *= scale;
        return result;
    }

    void CheckDirection(const AAD::IVS_& base, bool merton, bool compiled, int coordinate, const Matrix_<>& adjoints) {
        const auto direction = QuoteDirection(coordinate);
        const double aad = DirectionalAdjoint(adjoints, direction);
        bool previous = false, adjacent = false;
        for (const double step : {2e-4, 1e-4, 5e-5}) {
            const auto positive = ScaledDirection(direction, step);
            const auto negative = ScaledDirection(direction, -step);
            const double fd = (LegacyPrice(base, positive, compiled) - LegacyPrice(base, negative, compiled)) / (2.0 * step);
            const double tolerance = 1e-3 + 1e-3 * std::abs(fd);
            const bool pass = std::isfinite(fd) && std::abs(aad - fd) <= tolerance;
            std::cout << std::setprecision(17) << "ExcelQuoteOracle," << merton << ',' << compiled << ',' << coordinate << ',' << step << ',' << aad
                      << ',' << fd << ',' << tolerance << ',' << pass << '\n';
            adjacent = adjacent || (previous && pass);
            previous = pass;
        }
        ASSERT_TRUE(adjacent) << "coordinate=" << coordinate;
    }

    void CheckChain(bool merton, bool compiled) {
        const SingleWorker_ workers;
        const auto calibration = Calibration(merton);
        Handle_<ModelData_> model;
        DupireModelData_New("local", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
        MonteCarloSettings_ execution;
        execution.enableAad_ = true;
        execution.compiled_ = compiled;
        const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("execution", execution));
        const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", Valuation()));
        Handle_<StorableRiskResult_> source;
        MonteCarlo_ValueWithRisk(Product(), model, 257, {}, valuation, simulation, &source);
        String_ ids, scales;
        for (auto it = source->val_.InputAxis().rbegin(); it != source->val_.InputAxis().rend(); ++it) {
            if (!ids.empty()) {
                ids += ";";
                scales += ";";
            }
            ids += it->id_;
            scales += "0.01";
        }
        Matrix_<Cell_> selection(2, 2);
        selection(0, 0) = "inputs";
        selection(0, 1) = ids;
        selection(1, 0) = "report_factors";
        selection(1, 1) = scales;
        Handle_<StorableRiskRequest_> request;
        RiskRequest_New("reversed", selection, &request);
        Handle_<StorableRiskResult_> selected;
        MonteCarlo_ValueWithRisk(Product(), model, 257, request, valuation, simulation, &selected);
        Matrix_<> directMatrix(3, 2, 0.0);
        directMatrix(0, 0) = 3.0 * std::exp(-calibration->val_.Rate());
        Handle_<StorableDupireDirectQuoteAdjoints_> direct;
        DupireDirectQuoteAdjoints_New("direct", calibration, directMatrix, &direct);
        Handle_<StorableDupireScriptQuoteRisk_> combined;
        DupireScriptQuoteRisk_New("combined", selected, calibration, "equity", direct, &combined);
        Handle_<StorableDupireQuoteRisk_> result;
        DupireScriptQuoteRisk_Get_QuoteRisk(combined, &result);
        Matrix_<> adjoints;
        DupireQuoteRisk_Get_Adjoints(result, "total", &adjoints);
        Handle_<StorableDupireParameterAdjoints_> parameters;
        DupireParameterAdjoints_FromRisk("parameters", selected, calibration, "equity", &parameters);
        Handle_<StorableDupireQuoteRisk_> separate;
        DupireQuoteRisk_New("separate", calibration, parameters, direct, &separate);
        ASSERT_EQ(result->val_.TotalAdjoints()(0, 0), separate->val_.TotalAdjoints()(0, 0));
        ASSERT_EQ(combined->val_.Method(), "NativeAADThenNativeAADCalibrationVJP");
        const FlatIVS_ flat;
        const AAD::MertonIVS_ jump(100.0, 0.2, 0.08, -0.1, 0.15);
        const AAD::IVS_& base = merton ? static_cast<const AAD::IVS_&>(jump) : static_cast<const AAD::IVS_&>(flat);
        ASSERT_EQ(source->val_.Values()[0], LegacyPrice(base, Matrix_<>(3, 2, 0.0), compiled));
        for (int coordinate = 0; coordinate < 7; ++coordinate)
            ASSERT_NO_FATAL_FAILURE(CheckDirection(base, merton, compiled, coordinate, adjoints));
    }
} // namespace

TEST(ExcelDupireRiskTest, TestCalibrationAndZeroPullbackAreAvailableThroughHandles) {
    Handle_<StorableDupireGrid_> grid;
    DupireGrid_New("grid", {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5, &grid);
    Handle_<StorableDupireRiskInputs_> inputs;
    DupireRiskInputs_New("quotes", {75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.0), grid, &inputs);
    Handle_<StorableDupireCalibration_> calibration;
    DupireCalibration_New("surface", handle_cast<Storable_>(NewBSModelData("base", 100.0, 0.2, 0.05, 0.02)), inputs, &calibration);
    ASSERT_EQ(calibration->val_.Surface()->vols_.Rows(), 9);
    Handle_<StorableDupireParameterAdjoints_> seeds;
    DupireParameterAdjoints_New("zero", calibration, Matrix_<>(9, 2, 0.0), &seeds);
    Handle_<StorableDupireQuoteRisk_> result;
    DupireQuoteRisk_New("risk", calibration, seeds, {}, &result);
    Matrix_<> adjoints;
    DupireQuoteRisk_Get_Adjoints(result, "total", &adjoints);
    ASSERT_EQ(adjoints.Rows(), 3);
    ASSERT_EQ(adjoints.Cols(), 2);
    for (const auto value : adjoints)
        ASSERT_EQ(value, 0.0);
    ASSERT_NO_THROW(DupireQuoteRisk_Get_Adjoints(result, "", &adjoints));
}

TEST(ExcelDupireRiskTest, TestFlatTreeCompleteQuoteChain) { ASSERT_NO_FATAL_FAILURE(CheckChain(false, false)); }
TEST(ExcelDupireRiskTest, TestFlatCompiledCompleteQuoteChain) { ASSERT_NO_FATAL_FAILURE(CheckChain(false, true)); }
TEST(ExcelDupireRiskTest, TestMertonTreeCompleteQuoteChain) { ASSERT_NO_FATAL_FAILURE(CheckChain(true, false)); }
TEST(ExcelDupireRiskTest, TestMertonCompiledCompleteQuoteChain) { ASSERT_NO_FATAL_FAILURE(CheckChain(true, true)); }

TEST(ExcelDupireRiskTest, TestCopiedGettersNegativeSeedsAndOutputPreservation) {
    const auto calibration = Calibration(false);
    Handle_<LocalVolSurfaceData_> surface;
    DupireCalibration_Get_Surface(calibration, &surface);
    ASSERT_NE(surface.get(), calibration->val_.Surface().get());
    const auto original = calibration->val_.Surface()->vols_(0, 0);
    const_cast<LocalVolSurfaceData_*>(surface.get())->vols_(0, 0) = 0.9;
    ASSERT_EQ(calibration->val_.Surface()->vols_(0, 0), original);
    Vector_<> spots, times;
    Matrix_<> vols;
    Matrix_<Cell_> quotes, provenance;
    DupireCalibration_Get_Spots(calibration, &spots);
    DupireCalibration_Get_Times(calibration, &times);
    DupireCalibration_Get_Vols(calibration, &vols);
    DupireCalibration_Get_Quotes(calibration, &quotes);
    DupireCalibration_Get_Provenance(calibration, &provenance);
    ASSERT_EQ(spots.size(), 9);
    ASSERT_EQ(times, Vector_<>({0.5, 1.0}));
    ASSERT_EQ(quotes.Rows(), 7);
    ASSERT_EQ(Cell::ToDouble(quotes(2, 0)), 75.0);
    ASSERT_EQ(Cell::ToDouble(quotes(2, 1)), 1.2);
    Matrix_<> seeds(9, 2, -0.5);
    Handle_<StorableDupireParameterAdjoints_> parameters;
    DupireParameterAdjoints_New("negative", calibration, seeds, &parameters);
    seeds(0, 0) = 99.0;
    DupireParameterAdjoints_Get_Adjoints(parameters, &vols);
    ASSERT_EQ(vols(0, 0), -0.5);
    Handle_<StorableDupireDirectQuoteAdjoints_> direct;
    DupireDirectQuoteAdjoints_New("direct", calibration, Matrix_<>(3, 2, 0.25), &direct);
    DupireDirectQuoteAdjoints_Get_Adjoints(direct, &vols);
    ASSERT_EQ(vols(0, 0), 0.25);
    Handle_<StorableDupireQuoteRisk_> result;
    DupireQuoteRisk_New("risk", calibration, parameters, direct, &result);
    Matrix_<> adjoints;
    DupireQuoteRisk_Get_Adjoints(result, "calibration", &adjoints);
    double sum = 0.0;
    for (const auto value : adjoints)
        sum += value;
    ASSERT_NEAR(sum, -9.0, 3e-5);
    DupireQuoteRisk_Get_Provenance(result, &provenance);
    ASSERT_EQ(Cell::ToString(provenance(1, 1)), "decimal-vol");
    const auto previous = result;
    ASSERT_THROW(DupireQuoteRisk_New("bad", {}, parameters, {}, &result), Exception_);
    ASSERT_EQ(result, previous);
    const auto saved = adjoints(0, 0);
    ASSERT_THROW(DupireQuoteRisk_Get_Adjoints(result, "typo", &adjoints), Exception_);
    ASSERT_EQ(adjoints(0, 0), saved);
    ASSERT_THROW(JSON::WriteString(*result), Exception_);
    auto writable = calibration;
    ASSERT_THROW(DupireCalibration_New("bad", handle_cast<Storable_>(InputHandle()), InputHandle(), &writable), Exception_);
    ASSERT_EQ(writable, calibration);
}

TEST(ExcelDupireRiskTest, TestStrictMertonSettingsAndErrorContextRetainPreviousHandle) {
    Handle_<StorableMertonIVS_> base;
    MertonIVS_New("valid", MertonSettings(), &base);
    const auto previous = base;
    for (const Cell_ bad : {Cell_(true), Cell_("0.2"), Cell_(std::numeric_limits<double>::infinity()), Cell_(-0.2)}) {
        auto settings = MertonSettings();
        settings(1, 1) = bad;
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { MertonIVS_New("bad", settings, &base); }, "settings row=2 column=2"));
        ASSERT_EQ(base, previous);
    }
    auto duplicate = MertonSettings();
    duplicate(4, 0) = "VOL";
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { MertonIVS_New("bad", duplicate, &base); }, "duplicate key"));
    auto unknown = MertonSettings();
    unknown(4, 0) = "typo";
    ASSERT_THROW(MertonIVS_New("bad", unknown, &base), Exception_);
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { MertonIVS_New("bad", {}, &base); }, "missing required key=spot"));
    ASSERT_THROW(MertonIVS_New("bad", Matrix_<Cell_>(1, 3), &base), Exception_);
    ASSERT_EQ(base, previous);
    ASSERT_NO_THROW(MertonIVS_New("recovered", MertonSettings(), &base));
}

TEST(ExcelDupireRiskTest, TestSeedMismatchFailureAndRecoveryPreserveResults) {
    const auto calibration = Calibration(false);
    Handle_<StorableDupireParameterAdjoints_> parameters;
    DupireParameterAdjoints_New("one", calibration, Matrix_<>(9, 2, 1.0), &parameters);
    const auto previous = parameters;
    ASSERT_THROW(DupireParameterAdjoints_New("bad", calibration, Matrix_<>(9, 1), &parameters), Exception_);
    ASSERT_EQ(parameters, previous);
    Matrix_<> invalid(9, 2, 1.0);
    invalid(0, 1) = std::numeric_limits<double>::quiet_NaN();
    ASSERT_THROW(DupireParameterAdjoints_New("bad", calibration, invalid, &parameters), Exception_);
    ASSERT_EQ(parameters, previous);
    Handle_<StorableDupireQuoteRisk_> result;
    DupireQuoteRisk_New("one", calibration, parameters, {}, &result);
    const auto saved = result;
    auto inputs = Inputs();
    inputs.quoteSpreads_(0, 0) = 0.001;
    const FlatIVS_ flat;
    const Handle_<StorableDupireCalibration_> other(new StorableDupireCalibration_("other", CalibrateDupireWithRisk(flat, inputs)));
    ASSERT_THROW(DupireQuoteRisk_New("bad", other, parameters, {}, &result), Exception_);
    ASSERT_EQ(result, saved);
    Handle_<StorableDupireDirectQuoteAdjoints_> direct;
    DupireDirectQuoteAdjoints_New("other", other, Matrix_<>(3, 2, 1.0), &direct);
    ASSERT_THROW(DupireQuoteRisk_New("bad", calibration, parameters, direct, &result), Exception_);
    ASSERT_EQ(result, saved);
    DupireQuoteRisk_New("recovered", calibration, parameters, {}, &result);
    ASSERT_EQ(result->val_.TotalAdjoints()(0, 0), saved->val_.TotalAdjoints()(0, 0));
    Handle_<ModelData_> model;
    DupireModelData_New("valid", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
    const auto oldModel = model;
    ASSERT_THROW(DupireModelData_New("bad", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.0, &model), Exception_);
    ASSERT_EQ(model, oldModel);
    const String_ nul(std::string("bad\0factor", 10));
    ASSERT_THROW(DupireModelData_New("bad", calibration, "EQ[LOCAL]", "USD", nul, 0.25, &model), Exception_);
    ASSERT_EQ(model, oldModel);
}

TEST(ExcelDupireRiskTest, TestCompositeGettersAndNullErrorsDoNoHistoryOrValuationWork) {
    const SingleWorker_ workers;
    const auto calibration = Calibration(false);
    Handle_<ModelData_> model;
    DupireModelData_New("local", calibration, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25, &model);
    const Handle_<StorableScriptValuationSettings_> settings(new StorableScriptValuationSettings_("valuation", Valuation()));
    Handle_<StorableRiskResult_> source;
    MonteCarlo_ValueWithRisk(Product(), model, 257, {}, settings, {}, &source);
    const RejectGetterWork_ reject;
    Handle_<StorableDupireScriptQuoteRisk_> result;
    DupireScriptQuoteRisk_New("result", source, calibration, "equity", {}, &result);
    const auto saved = result;
    ASSERT_THROW(DupireScriptQuoteRisk_New("bad", source, calibration, "missing", {}, &result), Exception_);
    ASSERT_EQ(result, saved);
    Handle_<StorableRiskResult_> copy;
    DupireScriptQuoteRisk_Get_Valuation(result, &copy);
    ASSERT_EQ(copy->val_.Values(), source->val_.Values());
    ASSERT_NE(copy.get(), source.get());
    Matrix_<Cell_> provenance;
    DupireScriptQuoteRisk_Get_Provenance(result, &provenance);
    ASSERT_EQ(Cell::ToString(provenance(1, 1)), "equity");
    Handle_<StorableDupireQuoteRisk_> quotes;
    DupireScriptQuoteRisk_Get_QuoteRisk(result, &quotes);
    Matrix_<> adjoints;
    DupireQuoteRisk_Get_Adjoints(quotes, "total", &adjoints);
    ASSERT_THROW(DupireScriptQuoteRisk_Get_Valuation({}, &copy), Exception_);
    ASSERT_THROW(DupireScriptQuoteRisk_Get_QuoteRisk({}, &quotes), Exception_);
    ASSERT_THROW(DupireScriptQuoteRisk_Get_Provenance({}, &provenance), Exception_);
    ASSERT_THROW(DupireParameterAdjoints_Get_Adjoints({}, &adjoints), Exception_);
    ASSERT_THROW(DupireDirectQuoteAdjoints_Get_Adjoints({}, &adjoints), Exception_);
    ASSERT_THROW(DupireCalibration_Get_Vols({}, &adjoints), Exception_);
    auto writable = calibration;
    ASSERT_THROW(DupireCalibration_New("null", {}, InputHandle(), &writable), Exception_);
    ASSERT_EQ(writable, calibration);
    ASSERT_EQ(reject.history_.historyCalls_, 0);
    ASSERT_EQ(reject.workers_.calls_, 0);
}

#ifdef _WIN32
TEST(ExcelDupireRiskTest, TestWorksheetIntegerNormalizationAndEmbeddedNulValidation) {
    OPER_ cells[2]{};
    cells[0].xltype = xltypeInt;
    cells[0].val.w = 100;
    cells[1].xltype = xltypeNum;
    cells[1].val.num = 0.2;
    OPER_ range{};
    range.xltype = xltypeMulti;
    range.val.array = {cells, 1, 2};
    const Excel::ScriptSettingsInput_ input(&range);
    ASSERT_EQ(input.Get()->val.array.lparray[0].xltype, xltypeNum);
    ASSERT_EQ(input.Get()->val.array.lparray[0].val.num, 100.0);
    ASSERT_EQ(cells[0].xltype, xltypeInt);
    wchar_t text[]{3, L'a', L'\0', L'b'};
    OPER_ string{};
    string.xltype = xltypeStr;
    string.val.str = text;
    ASSERT_THROW(Excel::ValidateDupireTextInput(&string, "component"), Exception_);
}
#endif
