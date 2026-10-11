//
// Created by Codex on 2026/10/11.
//

#include <algorithm>
#include <exception>
#include <limits>

#include <dal-public/src/global.hpp>
#include <dal-public/src/lsmccurvature.hpp>
#include <dal-public/src/script.hpp>
#include <dal/platform/initall.hpp>

#ifdef DAL_EXCEL_LSMC_BOUNDARY
#include <dal-excel/src/__lsmccurvature.hpp>
#endif

namespace {
    using namespace Dal;

    Handle_<Script::ScriptProductData_> Product() {
        return Handle_<Script::ScriptProductData_>(new Script::ScriptProductData_("cost", {Cell_("SCALE"), Cell_(Date_(2027, 1, 2))},
                                                                                  {"2", "EXERCISE SCALE * FIX(EQ[EXCEL_LSMC_COST]) ^ 2"}));
    }

    Script::ScriptValuationSettings_ Valuation() {
        Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 1, 2);
        return valuation;
    }

    Script::MonteCarloSettings_ Simulation(int selected) {
        auto simulation = DefaultRiskMonteCarloSettings();
        simulation.compiled_ = true;
        simulation.lsmcTrainingPaths_ = 64;
        simulation.lsmcPolicyRiskMode_ = selected == 0 ? "Frozen" : "RetrainedBump";
        simulation.lsmcPolicyBumpRelative_ = .002;
        return simulation;
    }

    AAD::BumpOverAADRequest_ Bumps() {
        AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Matrix_<>(2, 5, 0.0);
        const double rows[2][5] = {{1, .01, .002, -.003, .5}, {-2, -.02, -.004, .006, -1}};
        for (int row = 0; row < 2; ++row)
            for (int coordinate = 0; coordinate < 5; ++coordinate)
                bumps.directions_(row, coordinate) = rows[row][coordinate];
        bumps.steps_ = {.01, .005};
        return bumps;
    }

#ifdef DAL_EXCEL_LSMC_BOUNDARY
    struct Context_ {
        Handle_<StorableBlackScholesLsmcPlan_> plan_;
        Handle_<StorableBumpOverAADRequest_> bumps_;
        explicit Context_(int selected) {
            const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("cost", Valuation()));
            const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("cost", Simulation(selected)));
            BlackScholesLsmcPlan_New("cost", Product(), valuation, simulation, &plan_);
            bumps_.reset(new StorableBumpOverAADRequest_("cost", Bumps()));
        }
    };

    void Evaluate(const Context_& context, const double* point, double* output) {
        Matrix_<Cell_> cells(1, 5);
        for (int coordinate = 0; coordinate < 5; ++coordinate)
            cells(0, coordinate) = point[coordinate];
        Handle_<StorableBlackScholesLsmcCurvatureResult_> result;
        BlackScholesLsmcCurvatureResult_New("cost", context.plan_, cells, 33, context.bumps_, &result);
        BlackScholesLsmcCurvatureResult_Get_Value(result, output);
        Matrix_<Cell_> gradient, products, execution;
        BlackScholesLsmcCurvatureResult_Get_Gradient(result, &gradient);
        BlackScholesLsmcCurvatureResult_Get_HessianProducts(result, &products);
        BlackScholesLsmcCurvatureResult_Get_Execution(result, &execution);
        for (int coordinate = 0; coordinate < 5; ++coordinate)
            output[1 + coordinate] = Cell::ToDouble(gradient(coordinate, 0));
        for (int row = 0; row < 2; ++row)
            for (int coordinate = 0; coordinate < 5; ++coordinate)
                output[6 + row * 5 + coordinate] = Cell::ToDouble(products(row, coordinate));
        const int rows[]{2, 3, 4, 5, 1, 6};
        for (int field = 0; field < 6; ++field)
            output[16 + field] = Cell::ToDouble(execution(rows[field], 1));
    }
#else
    struct Context_ {
        BlackScholesLsmcPlan_ plan_;
        AAD::BumpOverAADRequest_ bumps_ = Bumps();
        explicit Context_(int selected) : plan_(PlanBlackScholesLsmc(Product(), Simulation(selected), Valuation())) {}
    };

    void Evaluate(const Context_& context, const double* parameters, double* output) {
        const auto result = ValueByBlackScholesLsmcWithCurvature(context.plan_, Vector_<>(parameters, parameters + 5), 33, context.bumps_);
        const auto& curvature = result.Curvature();
        output[0] = curvature.Value();
        std::copy(curvature.Gradient().begin(), curvature.Gradient().end(), output + 1);
        std::copy(curvature.HessianProducts().begin(), curvature.HessianProducts().end(), output + 6);
        const auto& execution = curvature.Execution();
        output[16] = double(execution.pathsPerReplicate_);
        output[17] = double(execution.trainingPaths_);
        output[18] = double(execution.validationPaths_);
        output[19] = double(execution.pricingReplicates_);
        output[20] = double(execution.gradientEvaluations_);
        output[21] = double(execution.numericPayloadBytes_);
    }
#endif
} // namespace

extern "C" {
int CopyLsmcPricingNormals(double* output) {
    try {
        auto random = Dal::Script::CreateRNG("sobol", 1, false);
        random->SkipTo(64);
        Dal::Vector_<> normal(1);
        for (int path = 0; path < 33; ++path) {
            random->FillNormal(&normal);
            output[path] = normal[0];
        }
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}

void* PrepareLsmcCost(int selected) {
    try {
        REQUIRE(selected == 0 || selected == 1, "unsupported LSMC cost case");
        Dal::RegisterAll_::Init();
        Dal::InitGlobalData(1);
        return new Context_(selected);
    } catch (const std::exception&) {
        return nullptr;
    }
}

void DestroyLsmcCost(void* context) { delete static_cast<Context_*>(context); }

int EvaluateLsmcCost(void* context, const double* point, double* output) {
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
