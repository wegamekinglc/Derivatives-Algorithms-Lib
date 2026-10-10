//
// Created by Codex on 2026/10/11.
//

#include <exception>

#include <dal-public/src/curvespec.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/platform/initall.hpp>
#include <dal/time/holidays.hpp>

#ifdef DAL_EXCEL_RATE_BOUNDARY
#include <dal-excel/src/__ratecurvature.hpp>
#endif

namespace {
    using namespace Dal;

    CurveCalibrationSpec_ Specification(int count) {
        REQUIRE(count == 1 || count == 8, "unsupported rate cost case");
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "rate-cost";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.tolerance_ = 1.0e-14;
        spec.initialGuess_ = 0.025;
        spec.knotDates_.push_back(spec.today_);
        RateIndexConvention_ index;
        index.dayBasis_ = DayBasis::Act365F();
        index.businessDayConvention_ = BizDayConvention_("Unadjusted");
        index.accrualHolidays_ = Holidays::None();
        for (int year = 1; year <= count; ++year) {
            const auto maturity = Date::AddMonths(spec.today_, 12 * year);
            spec.knotDates_.push_back(maturity);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, spec.today_, maturity, 0.025, index)));
        }
        return spec;
    }

    Vector_<RateTradeDefinition_> Trades(const CurveCalibrationSpec_& spec) {
        Vector_<RateTradeDefinition_> trades;
        for (const auto& instrument : spec.instruments_) {
            RateTradeDefinition_ trade;
            trade.instrumentId_ = "deposit";
            trade.instrumentType_ = RateInstrumentType_::Value_::DEPOSIT;
            trade.tradeDate_ = spec.today_;
            trade.startDate_ = spec.today_;
            trade.maturityDate_ = instrument->TimeSpan().second;
            trade.currencyOrPair_ = Ccy_("USD");
            DepositTradeTerms_ terms;
            terms.notional_ = 1.0;
            terms.contractRate_ = 0.028;
            terms.discountComponentKey_ = spec.curveName_;
            terms.index_ = static_cast<const Deposit_&>(*instrument).FloatConvention();
            trade.terms_ = terms;
            trades.push_back(trade);
        }
        return trades;
    }

    AAD::BumpOverAADRequest_ Request(int count) {
        AAD::BumpOverAADRequest_ request;
        request.directions_ = Matrix_<>(3, count, 0.0);
        request.directions_(0, 0) = 1.0;
        request.directions_(1, count - 1) = -2.0;
        for (int column = 0; column < count; ++column)
            request.directions_(2, column) = (column % 2 == 0 ? 1.0 : -1.0) / (column + 1);
        request.steps_ = {0.0001, 0.0002, 0.0001};
        return request;
    }

    Vector_<> Weights(int count) {
        Vector_<> weights;
        for (int column = 0; column < count; ++column)
            weights.push_back(column % 2 == 0 ? 1.0 : -0.5);
        return weights;
    }

    void CopyOutput(double value,
                    const Vector_<>& point,
                    const Vector_<>& gradient,
                    const Matrix_<>& products,
                    const RateQuoteCurvatureExecution_& execution,
                    double* output) {
        const int count = static_cast<int>(point.size());
        output[0] = value;
        for (int column = 0; column < count; ++column) {
            output[1 + column] = gradient[column];
            output[1 + 4 * count + column] = point[column];
            for (int row = 0; row < 3; ++row)
                output[1 + count + row * count + column] = products(row, column);
        }
        output[1 + 5 * count] = double(execution.quoteGradientEvaluations_);
        output[2 + 5 * count] = double(execution.calibrations_);
        output[3 + 5 * count] = double(execution.objectiveReverseSweeps_);
        output[4 + 5 * count] = double(execution.numericPayloadBytes_);
    }

#ifdef DAL_EXCEL_RATE_BOUNDARY
    Handle_<StorableBumpOverAADRequest_> WorksheetBumps(const AAD::BumpOverAADRequest_& native) {
        Matrix_<Cell_> directions(3, native.directions_.Cols()), steps(3, 1);
        for (int row = 0; row < 3; ++row) {
            steps(row, 0) = native.steps_[row];
            for (int column = 0; column < directions.Cols(); ++column)
                directions(row, column) = native.directions_(row, column);
        }
        Handle_<StorableBumpOverAADRequest_> bumps;
        BumpOverAADRequest_New("bumps", directions, steps, {}, &bumps);
        return bumps;
    }

    Vector_<> Numbers(const Matrix_<Cell_>& cells) {
        Vector_<> values;
        for (int row = 0; row < cells.Rows(); ++row)
            values.push_back(Cell::ToDouble(cells(row, 0)));
        return values;
    }

    void CopyWorksheet(const Handle_<StorableRateTradeQuoteCurvatureResult_>& result, double* output) {
        Matrix_<Cell_> point, gradient, products, directions, steps, shape, execution;
        RateTradeQuoteCurvatureResult_Get_Point(result, &point);
        RateTradeQuoteCurvatureResult_Get_Gradient(result, &gradient);
        RateTradeQuoteCurvatureResult_Get_HessianProducts(result, &products);
        RateTradeQuoteCurvatureResult_Get_Directions(result, &directions);
        RateTradeQuoteCurvatureResult_Get_Steps(result, &steps);
        RateTradeQuoteCurvatureResult_Get_Shape(result, &shape);
        RateTradeQuoteCurvatureResult_Get_Execution(result, &execution);
        REQUIRE(products.Rows() == 3 && directions.Rows() == 3 && steps.Rows() == 3 && shape.Cols() == 2 && execution.Rows() == 7,
                "invalid rate worksheet query shape");
        Matrix_<> values(products.Rows(), products.Cols());
        for (int row = 0; row < values.Rows(); ++row)
            for (int column = 0; column < values.Cols(); ++column)
                values(row, column) = Cell::ToDouble(products(row, column));
        double value = 0.0;
        String_ currency;
        RateTradeQuoteCurvatureResult_Get_Value(result, &value);
        RateTradeQuoteCurvatureResult_Get_Currency(result, &currency);
        REQUIRE(currency == "USD", "invalid rate PV currency");
        Handle_<StorableRateCalibrationSnapshot_> base;
        RateTradeQuoteCurvatureResult_Get_BaseCalibration(result, &base);
        Handle_<StorableCalibrationRiskPlan_> plan;
        RateCalibration_Get_QuotePlan(base, &plan);
        REQUIRE(plan->val_.CompleteInputAxis().size() == static_cast<size_t>(point.Rows()), "incomplete raw quote axis");
        CopyOutput(value, Numbers(point), Numbers(gradient), values, result->val_.Curvature().Execution(), output);
    }
#endif

    void Evaluate(int count, double* output) {
        const auto spec = Specification(count);
        const auto fitted = CalibrateSingleCurve(spec);
        REQUIRE(fitted.curve_, "initial rate calibration failed");
        const auto trades = Trades(spec);
        const auto native = Request(count);
        const auto weights = Weights(count);
        Vector_<> quotes(count, 0.025);
        quotes[0] += 0.0001;
#ifdef DAL_EXCEL_RATE_BOUNDARY
        const Handle_<Storable_> source(new StorableCurveCalibrationResult_(fitted, spec, {}));
        Handle_<StorableRateCalibrationSnapshot_> snapshot, rebuilt;
        RateCalibration_New("snapshot", source, &snapshot);
        Matrix_<Cell_> point(count, 1), weightCells(count, 1);
        Vector_<Handle_<Storable_>> handles;
        for (int column = 0; column < count; ++column) {
            point(column, 0) = quotes[column];
            weightCells(column, 0) = weights[column];
            handles.push_back(Handle_<Storable_>(new StorableRateTradeDefinition_(trades[column])));
        }
        RateCalibration_Recalibrate("replay", snapshot, point, &rebuilt);
        Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
        RateTradeQuoteCurvatureSettings_New("settings", weightCells, {}, &settings);
        Matrix_<Cell_> history;
        RateTradeQuoteCurvatureSettings_Get_Fixings(settings, &history);
        REQUIRE(!Cell::ToBool(history(0, 1)), "unexpected explicit history");
        Handle_<StorableRateTradeQuoteCurvatureResult_> result;
        RateTradeQuoteCurvatureResult_New("result", handles, rebuilt, WorksheetBumps(native), settings, &result);
        CopyWorksheet(result, output);
#else
        const auto snapshot = NewRateCalibration(spec);
        const auto rebuilt = RecalibrateRateWithRisk(snapshot, quotes);
        const auto result = EvaluateRateTradeQuoteCurvature(trades, rebuilt, native, {weights, {}});
        const auto& risk = result.Curvature();
        CopyOutput(risk.Value(), risk.Point(), risk.Gradient(), risk.HessianProducts(), risk.Execution(), output);
#endif
    }
} // namespace

extern "C" int DalRateWorksheetCurvature(int count, double* output) {
    try {
        Dal::RegisterAll_::Init();
        auto* pool = Dal::ThreadPool_::GetInstance();
        if (pool->NumThreads() != 1 || !pool->IsActive())
            pool->Start(1, true);
        Evaluate(count, output);
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
