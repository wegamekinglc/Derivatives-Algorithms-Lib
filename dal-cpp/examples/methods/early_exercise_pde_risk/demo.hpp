#pragma once

#include <iomanip>
#include <iostream>

#include <dal/math/pde/earlyexercisepderisk.hpp>

#include "../../floatformat.hpp"

namespace Dal::EarlyExerciseRiskExample {
    inline PDE::EarlyExercisePdeRiskRequest_ Request(bool american) {
        PDE::EarlyExercisePdeRiskRequest_ request;
        const Date_ today(2026, 10, 18), maturity(2027, 10, 18);
        request.valuation_.contract_ = {today, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_(american ? "AMERICAN" : "BERMUDAN"), {}};
        if (!american)
            request.valuation_.contract_.exerciseDates_ = {Date_(2027, 3, 4), maturity};
        request.valuation_.market_ = {100.0, .2, {{maturity, .05, 0.0}}};
        request.valuation_.settings_.upper_ = 400.0;
        return request;
    }

    inline void Print(const char* name, const PDE::EarlyExercisePdeRiskResult_& result) {
        std::cout << name << "  " << result.Status().String() << "  " << result.Method() << "\n";
        REQUIRE(result.PriceAvailable(), "example base price unavailable");
        std::cout << "Price per unit underlying: " << ExampleFloat(result.Price()) << "\n"
                  << std::left << std::setw(20) << "Risk" << std::setw(14) << "Value" << std::setw(46) << "Status" << "Unit\n"
                  << std::string(125, '-') << "\n";
        for (const auto& row : result.Risks()) {
            const bool available = row.status_ == EarlyExercisePdeRiskAvailability_("AVAILABLE") ||
                                   row.status_ == EarlyExercisePdeRiskAvailability_("AVAILABLE_EXERCISE_REGION");
            REQUIRE(available == row.value_.has_value(), "availability/value mismatch");
            std::cout << std::left << std::setw(20) << row.riskId_ << std::setw(14) << (available ? ExampleFloat(*row.value_) : "unavailable")
                      << std::setw(46) << row.status_.String() << row.unit_ << "\n";
        }
        std::cout << "Gamma: FINITE_SCALE_MODEL_APPROXIMATION; carry: NATIVE_MARKET_PARAMETER_PARTIAL\n"
                  << "Scenarios: " << result.Scenarios().size() << "  Solves: " << result.Execution().totalSolves_
                  << "  Work: " << result.Execution().workUnits_ << "\n\n";
    }

    template <class Evaluate_> int Run(Evaluate_ evaluate) {
        RegisterAll_::Init();
        for (const bool american : {false, true}) {
            const auto result = evaluate(Request(american));
            REQUIRE(result.IsComplete(), "smooth example must be complete");
            Print(american ? "American" : "Bermudan", result);
        }
        auto boundary = Request(true);
        const auto base = PDE::EvaluateEarlyExercisePde(boundary.valuation_);
        REQUIRE(base.IsSuccess() && !base.ExerciseLevels().empty() && base.ExerciseLevels().front().regionCount_ > 0, "missing exercise region");
        const auto& region = base.ExerciseRegions()[base.ExerciseLevels().front().regionOffset_];
        REQUIRE(!region.rightClipped_, "exercise edge clipped");
        boundary.valuation_.market_.spot_ = .5 * (base.Grid()[region.lastNode_] + base.Grid()[region.lastNode_ + 1]);
        const auto result = evaluate(boundary);
        REQUIRE(result.Status() == EarlyExercisePdeRiskStatus_("PARTIAL") &&
                    result.Risks()[0].status_ == EarlyExercisePdeRiskAvailability_("UNAVAILABLE_NEAR_EXERCISE_BOUNDARY"),
                "expected partial boundary risk");
        Print("Boundary", result);
        return 0;
    }
} // namespace Dal::EarlyExerciseRiskExample
