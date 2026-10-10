//
// Created by Codex on 2026/10/11.
//

#include <algorithm>
#include <exception>

#include <dal-public/src/europeanpderisk.hpp>

#ifdef DAL_EXCEL_PDE_BOUNDARY
#include <dal-excel/src/__europeanpderisk.hpp>
#endif

namespace {
    using namespace Dal;

#ifndef DAL_EXCEL_PDE_BOUNDARY
    void CopyErrors(const Matrix_<>& errors, double* largest) {
        *largest = 0.0;
        for (double error : errors) {
            REQUIRE(std::isfinite(error) && error >= 0.0 && error <= 1e-12, "invalid native diagnostic");
            *largest = std::max(*largest, error);
        }
    }
#endif

#ifdef DAL_EXCEL_PDE_BOUNDARY
    void CopyWorksheetErrors(const Matrix_<Cell_>& errors, int steps, int channels, double* largest) {
        REQUIRE(errors.Rows() == steps + 1 && errors.Cols() == channels + 1, "invalid diagnostic spill shape");
        *largest = 0.0;
        for (int step = 0; step < steps; ++step) {
            REQUIRE(Cell::ToDouble(errors(step + 1, 0)) == step + 1, "invalid chronological step");
            for (int channel = 0; channel < channels; ++channel) {
                const double error = Cell::ToDouble(errors(step + 1, channel + 1));
                REQUIRE(std::isfinite(error) && error >= 0.0 && error <= 1e-12, "invalid worksheet diagnostic");
                *largest = std::max(*largest, error);
            }
        }
    }
#endif
    template <class P_, class J_> void CopyFinancialRows(P_ price, J_ derivative, double* output) {
        for (int layer = 0; layer < 2; ++layer) {
            output[layer] = price(layer);
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                output[2 + 3 * layer + coordinate] = derivative(layer, coordinate);
        }
    }

    void CopyFinancialRisk(int nodes, int intervals, double* output) {
#ifdef DAL_EXCEL_PDE_BOUNDARY
        Matrix_<Cell_> rows(2, 2);
        rows(0, 0) = "grid_points";
        rows(0, 1) = double(nodes);
        rows(1, 0) = "ordinary_steps";
        rows(1, 1) = double(intervals);
        Handle_<StorableEuropeanPdeRiskSettings_> settings;
        EuropeanPdeRiskSettings_New("cost", rows, &settings);
        Handle_<StorableEuropeanPdeRiskRequest_> request;
        EuropeanPdeRiskRequest_New("cost", 0.05, 0.20, 110.0, settings, &request);
        Handle_<StorableEuropeanPdeRiskResult_> result;
        EuropeanPdeRiskResult_New("cost", request, &result);
        Handle_<StorableEuropeanPdeRiskRequest_> retained;
        Handle_<StorableEuropeanPdeRiskSettings_> retainedSettings;
        EuropeanPdeRiskResult_Get_Request(result, &retained);
        EuropeanPdeRiskRequest_Get_Settings(retained, &retainedSettings);
        Matrix_<Cell_> configuration, point, prices, risks, grid, forward, transpose, execution;
        EuropeanPdeRiskSettings_Get_Configuration(retainedSettings, &configuration);
        EuropeanPdeRiskRequest_Get_Point(retained, &point);
        EuropeanPdeRiskResult_Get_Prices(result, &prices);
        EuropeanPdeRiskResult_Get_Jacobian(result, &risks);
        EuropeanPdeRiskResult_Get_Grid(result, &grid);
        EuropeanPdeRiskResult_Get_ForwardErrors(result, &forward);
        EuropeanPdeRiskResult_Get_TransposeErrors(result, &transpose);
        EuropeanPdeRiskResult_Get_Execution(result, &execution);
        REQUIRE(grid.Rows() == nodes + 1 && configuration.Rows() == 10 && point.Rows() == 3 && execution.Rows() == 6, "invalid passive spill");
        CopyFinancialRows([&](int layer) { return Cell::ToDouble(prices(layer + 1, 1)); },
                          [&](int layer, int coordinate) { return Cell::ToDouble(risks(1 + 3 * layer + coordinate, 3)); }, output);
        CopyWorksheetErrors(forward, intervals + 2, 2, output + 8);
        CopyWorksheetErrors(transpose, intervals + 2, 4, output + 9);
#else
        EuropeanPdeRiskRequest_ request;
        request.settings_.gridPoints_ = nodes;
        request.settings_.ordinarySteps_ = intervals;
        const auto result = EvaluateEuropeanPdeRisk(request);
        CopyFinancialRows([&](int layer) { return result.prices_[layer]; },
                          [&](int layer, int coordinate) { return result.jacobian_(layer, coordinate); }, output);
        CopyErrors(result.forwardBackwardErrors_, output + 8);
        CopyErrors(result.transposeBackwardErrors_, output + 9);
#endif
    }
} // namespace

extern "C" int DalEuropeanWorksheetRisk(int nodes, int intervals, double* output) {
    try {
        CopyFinancialRisk(nodes, intervals, output);
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
