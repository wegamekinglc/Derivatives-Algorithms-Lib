//
// Created by Codex on 2026/10/11.
//

#include <exception>

#include <dal-public/src/dupirecurvature.hpp>
#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/script.hpp>
#include <dal/concurrency/threadpool.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/initall.hpp>

#ifdef DAL_EXCEL_CURVATURE_BOUNDARY
#include <dal-excel/src/__dupirecurvature.hpp>
#endif

namespace {
    using namespace Dal;

    class FlatIVS_ final : public AAD::IVS_ {
    public:
        FlatIVS_() : IVS_(100.0, 0.05, 0.02) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    DupireScriptCurvatureRequest_ Request(int paths) {
        DupireScriptCurvatureRequest_ request;
        request.risk_.numPaths_ = paths;
        request.risk_.valuation_.evaluationDate_ = Date_(2026, 9, 12);
        request.risk_.directBindings_ = {{0, "quote:3"}};
        request.bumps_.directions_ = Matrix_<>(3, 6, 0.0);
        request.bumps_.directions_(0, 3) = 1.0;
        request.bumps_.directions_(1, 3) = -2.0;
        request.bumps_.directions_(2, 0) = 1.0;
        request.bumps_.steps_ = {0.0002, 0.0001, 0.0002};
        return request;
    }

#ifdef DAL_EXCEL_CURVATURE_BOUNDARY
    void CheckFinancialShapes(const Matrix_<Cell_>& point, const Matrix_<Cell_>& gradient, const Matrix_<Cell_>& products) {
        REQUIRE(point.Rows() == 6 && gradient.Rows() == 6 && products.Rows() == 3 && products.Cols() == 6, "invalid raw financial shape");
    }

    void
    CheckQueryShapes(const Matrix_<Cell_>& directions, const Matrix_<Cell_>& steps, const Matrix_<Cell_>& shape, const Matrix_<Cell_>& execution) {
        REQUIRE(directions.Rows() == 3 && steps.Rows() == 3 && shape.Rows() == 1 && execution.Rows() == 4, "invalid passive query shape");
    }
#endif

    void Evaluate(int paths, double* output) {
        const auto source = CalibrateDupireWithRisk(
            FlatIVS_(), {{75.0, 105.0, 135.0}, {0.4, 1.2}, Matrix_<>(3, 2, 0.001), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5});
        const auto model = NewDupireModelData("model", source, "EQ[LOCAL]", "USD", "F_LOCAL", 0.25);
        const auto product = NewScriptProduct("quadratic", {Cell_("QUOTE"), Cell_(Date_(2027, 9, 12))}, {"0.001", "pay PAYS QUOTE * QUOTE"});
        const auto native = Request(paths);
#ifdef DAL_EXCEL_CURVATURE_BOUNDARY
        Matrix_<Cell_> directionCells(3, 6), stepCells(3, 1);
        for (int row = 0; row < 3; ++row) {
            stepCells(row, 0) = native.bumps_.steps_[row];
            for (int quote = 0; quote < 6; ++quote)
                directionCells(row, quote) = native.bumps_.directions_(row, quote);
        }
        Handle_<StorableBumpOverAADRequest_> bumps;
        BumpOverAADRequest_New("bumps", directionCells, stepCells, {}, &bumps);
        const Handle_<StorableDupireScriptRiskRequest_> risk(new StorableDupireScriptRiskRequest_("first", native.risk_));
        Handle_<StorableDupireScriptCurvatureRequest_> request;
        DupireScriptCurvatureRequest_New("request", risk, bumps, &request);
        const Handle_<StorableDupireCalibration_> calibration(new StorableDupireCalibration_("source", source));
        Handle_<StorableDupireScriptCurvaturePlan_> plan;
        DupireScriptCurvaturePlan_New("plan", product, model, calibration, "equity", request, &plan);
        Handle_<StorableDupireScriptCurvatureResult_> result;
        DupireScriptCurvatureResult_New("result", plan, &result);
        Handle_<StorableDupireScriptRiskResult_> base;
        Handle_<StorableCalibrationRiskPlan_> quotePlan;
        DupireScriptCurvatureResult_Get_Base(result, &base);
        DupireScriptCurvatureResult_Get_QuotePlan(result, &quotePlan);
        REQUIRE(quotePlan->val_.CompleteInputAxis().size() == 6, "incomplete raw quote axis");
        Matrix_<Cell_> point, gradient, directions, steps, products, shape, execution;
        DupireScriptCurvatureResult_Get_Point(result, &point);
        DupireScriptCurvatureResult_Get_Gradient(result, &gradient);
        DupireScriptCurvatureResult_Get_Directions(result, &directions);
        DupireScriptCurvatureResult_Get_Steps(result, &steps);
        DupireScriptCurvatureResult_Get_HessianProducts(result, &products);
        DupireScriptCurvatureResult_Get_Shape(result, &shape);
        DupireScriptCurvatureResult_Get_Execution(result, &execution);
        CheckFinancialShapes(point, gradient, products);
        CheckQueryShapes(directions, steps, shape, execution);
        output[0] = base->val_.Valuation().Values()[0];
        for (int quote = 0; quote < 6; ++quote) {
            output[1 + quote] = Cell::ToDouble(gradient(quote, 0));
            for (int row = 0; row < 3; ++row)
                output[7 + 6 * row + quote] = Cell::ToDouble(products(row, quote));
        }
        output[25] = Cell::ToDouble(execution(1, 1));
        output[26] = Cell::ToDouble(execution(2, 1));
        output[27] = Cell::ToDouble(execution(3, 1));
#else
        const auto result = ValueByMonteCarloWithDupireCurvature(PlanDupireScriptCurvature(product, model, source, "equity", native));
        output[0] = result.Base().Valuation().Values()[0];
        for (int quote = 0; quote < 6; ++quote) {
            output[1 + quote] = result.Gradient()[quote];
            for (int row = 0; row < 3; ++row)
                output[7 + 6 * row + quote] = result.HessianProducts()(row, quote);
        }
        output[25] = double(result.Execution().quoteGradientEvaluations_);
        output[26] = double(result.Execution().pathsPerEvaluation_);
        output[27] = double(result.Execution().numericPayloadBytes_);
#endif
    }
} // namespace

extern "C" int DalDupireWorksheetCurvature(int paths, double* output) {
    try {
        Dal::RegisterAll_::Init();
        auto* pool = Dal::ThreadPool_::GetInstance();
        if (pool->NumThreads() != 1 || !pool->IsActive())
            pool->Start(1, true);
        Evaluate(paths, output);
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
