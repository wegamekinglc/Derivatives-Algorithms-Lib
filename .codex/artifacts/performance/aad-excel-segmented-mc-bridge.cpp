//
// Created by Codex on 2026/10/11.
//

#include <algorithm>
#include <exception>
#include <limits>

#include <dal-public/src/global.hpp>
#include <dal-public/src/montecarlocurvature.hpp>
#include <dal/platform/initall.hpp>

#ifdef DAL_EXCEL_MC_BOUNDARY
#include <dal-excel/src/__mccurvature.hpp>
#endif

namespace {
    using namespace Dal;

    Handle_<ScriptProductData_> Product(int selected) {
        Vector_<Cell_> dates{Cell_("SCALE"), Cell_(Date_(2027, 1, 2))};
        Vector_<String_> events{"2", "pay PAYS SCALE * FIX(EQ[EXCEL_MC_COST]) ^ 2"};
        if (selected == 1) {
            dates.push_back(Cell_(Date_(2028, 1, 2)));
            events = {"2", "APPEND(v, SCALE * FIX(EQ[EXCEL_MC_COST]) ^ 2)", "APPEND(v, SCALE * FIX(EQ[EXCEL_MC_COST]) ^ 2) pay PAYS SUM(v)"};
        }
        return Handle_<ScriptProductData_>(new ScriptProductData_("cost", dates, events));
    }

    Script::ScriptValuationSettings_ Valuation() {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 1, 2);
        return valuation;
    }

#ifndef DAL_EXCEL_MC_BOUNDARY
    Script::SegmentedMonteCarloSettings_ Settings() {
        Script::SegmentedMonteCarloSettings_ settings;
        settings.firstPath_ = 7;
        settings.path_.segmentSteps_ = 1;
        return settings;
    }
#endif

    AAD::BumpOverAADRequest_ Bumps() {
        AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Matrix_<>(2, 5, 0.0);
        const double rows[2][5] = {{1, .3, -.1, .2, .5}, {-2, -.6, .2, -.4, -1}};
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 5; ++column)
                bumps.directions_(row, column) = rows[row][column];
        bumps.steps_ = {2e-4, 1e-4};
        return bumps;
    }

#ifndef DAL_EXCEL_MC_BOUNDARY
    void CopyExecution(const Script::SegmentedMonteCarloExecution_& execution, double* output) {
        output[0] = double(execution.pathCount_);
        output[1] = double(execution.batchSize_);
        output[2] = double(execution.batches_);
        output[3] = double(execution.segmentSteps_);
    }
#endif

#ifdef DAL_EXCEL_MC_BOUNDARY
    Matrix_<Cell_> Cells(const Matrix_<>& values) {
        Matrix_<Cell_> cells(values.Rows(), values.Cols());
        for (int row = 0; row < values.Rows(); ++row)
            for (int column = 0; column < values.Cols(); ++column)
                cells(row, column) = values(row, column);
        return cells;
    }

    struct Context_ {
        Handle_<StorableBlackScholesMonteCarloPlan_> plan_;
        Handle_<StorableSegmentedMonteCarloSettings_> settings_;
        Handle_<StorableBumpOverAADRequest_> bumps_;
        int selected_;
        explicit Context_(int selected) : selected_(selected) {
            const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("cost", Valuation()));
            BlackScholesMonteCarloPlan_New("cost", Product(selected), valuation, .01, &plan_);
            Matrix_<Cell_> settings(2, 2);
            settings(0, 0) = "first_path";
            settings(0, 1) = 7.0;
            settings(1, 0) = "segment_steps";
            settings(1, 1) = 1.0;
            SegmentedMonteCarloSettings_New("cost", settings, &settings_);
            const auto request = Bumps();
            Matrix_<Cell_> steps(2, 1);
            for (int row = 0; row < 2; ++row)
                steps(row, 0) = request.steps_[row];
            BumpOverAADRequest_New("cost", Cells(request.directions_), steps, {}, &bumps_);
        }
    };

    void CopyMean(const Handle_<StorableBlackScholesMonteCarloResult_>& mean, double* output) {
        BlackScholesMonteCarloResult_Get_Value(mean, output);
        Matrix_<Cell_> gradient, execution;
        BlackScholesMonteCarloResult_Get_Gradient(mean, &gradient);
        for (int column = 0; column < 5; ++column)
            output[column + 1] = Cell::ToDouble(gradient(column, 0));
        BlackScholesMonteCarloResult_Get_Execution(mean, &execution);
        const int rows[]{0, 2, 3, 5};
        for (int column = 0; column < 4; ++column)
            output[16 + column] = Cell::ToDouble(execution(rows[column], 1));
    }

    void Evaluate(const Context_& context, const double* point, double* output) {
        Matrix_<Cell_> cells(1, 5);
        for (int column = 0; column < 5; ++column)
            cells(0, column) = point[column];
        Handle_<StorableBlackScholesMonteCarloResult_> mean;
        if (context.selected_ == 0) {
            BlackScholesMonteCarloResult_New("cost", context.plan_, cells, 1, context.settings_, &mean);
            output[20] = 1;
            output[21] = 0;
        } else {
            Handle_<StorableBlackScholesMonteCarloCurvatureResult_> curvature;
            BlackScholesMonteCarloCurvatureResult_New("cost", context.plan_, cells, 33, context.bumps_, context.settings_, &curvature);
            BlackScholesMonteCarloCurvatureResult_Get_Base(curvature, &mean);
            Matrix_<Cell_> products, execution;
            BlackScholesMonteCarloCurvatureResult_Get_HessianProducts(curvature, &products);
            for (int row = 0; row < 2; ++row)
                for (int column = 0; column < 5; ++column)
                    output[6 + row * 5 + column] = Cell::ToDouble(products(row, column));
            BlackScholesMonteCarloCurvatureResult_Get_Execution(curvature, &execution);
            output[20] = Cell::ToDouble(execution(1, 1));
            output[21] = Cell::ToDouble(execution(2, 1));
        }
        CopyMean(mean, output);
    }
#else
    struct Context_ {
        BlackScholesMonteCarloPlan_ plan_;
        Script::SegmentedMonteCarloSettings_ settings_ = Settings();
        AAD::BumpOverAADRequest_ bumps_ = Bumps();
        int selected_;
        explicit Context_(int selected) : plan_(PlanBlackScholesMonteCarlo(Product(selected), Valuation())), selected_(selected) {}
    };

    void CopyMean(const Script::SegmentedMonteCarloResult_& mean, double* output) {
        output[0] = mean.MeanValue();
        std::copy(mean.MeanGradient().begin(), mean.MeanGradient().end(), output + 1);
        CopyExecution(mean.Execution(), output + 16);
    }

    void Evaluate(const Context_& context, const double* parameters, double* output) {
        const Vector_<> point(parameters, parameters + 5);
        if (context.selected_ == 0) {
            const auto mean = ValueByBlackScholesSegmentedMonteCarlo(context.plan_, point, 1, context.settings_);
            CopyMean(mean.Mean(), output);
            output[20] = 1;
            output[21] = 0;
        } else {
            const auto result = ValueByBlackScholesMonteCarloWithCurvature(context.plan_, point, 33, context.bumps_, context.settings_);
            const auto& curvature = result.Curvature();
            CopyMean(curvature.Base(), output);
            std::copy(curvature.HessianProducts().begin(), curvature.HessianProducts().end(), output + 6);
            output[20] = double(curvature.Execution().gradientEvaluations_);
            output[21] = double(curvature.Execution().numericPayloadBytes_);
        }
    }
#endif
} // namespace

extern "C" {
void* PrepareMonteCarloCost(int selected) {
    try {
        REQUIRE(selected == 0 || selected == 1, "unsupported MC cost case");
        Dal::RegisterAll_::Init();
        Dal::InitGlobalData(1);
        return new Context_(selected);
    } catch (const std::exception&) {
        return nullptr;
    }
}

void DestroyMonteCarloCost(void* context) { delete static_cast<Context_*>(context); }

int EvaluateMonteCarloCost(void* context, const double* point, double* output) {
    try {
        std::fill(output, output + 22, 0.0);
        Evaluate(*static_cast<Context_*>(context), point, output);
        return 0;
    } catch (const std::exception&) {
        output[0] = std::numeric_limits<double>::quiet_NaN();
        return 1;
    }
}
}
