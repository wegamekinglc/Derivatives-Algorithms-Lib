//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <cmath>
#include <map>

#include "__gsr_test_api.hpp"

namespace Dal::GSRWorksheet {
    inline int Integer(double value) {
        REQUIRE(std::isfinite(value) && value >= 0.0 && value <= std::numeric_limits<int>::max() && std::floor(value) == value,
                "InvalidGSRWorksheet: indices and integer settings must be nonnegative integers");
        return static_cast<int>(value);
    }

    template <class T_> Vector_<T_> Values(const Vector_<Handle_<Storable_>>& handles) {
        Vector_<T_> values;
        for (const auto& handle : handles) {
            const auto typed = handle_cast<GSRValueHandle_<T_>>(handle);
            REQUIRE(typed, "InvalidGSRWorksheet: wrong handle type");
            values.push_back(typed->value_);
        }
        return values;
    }

    template <class T_> Matrix_<Cell_> Column(const Vector_<T_>& values) {
        Matrix_<Cell_> result(values.size(), 1);
        for (size_t i = 0; i < values.size(); ++i)
            result(i, 0) = Cell_(static_cast<T_>(values[i]));
        return result;
    }

    inline Matrix_<Cell_> Cells(const Matrix_<>& values) {
        Matrix_<Cell_> result(values.Rows(), values.Cols());
        for (int row = 0; row < values.Rows(); ++row)
            for (int col = 0; col < values.Cols(); ++col)
                result(row, col) = Cell_(values(row, col));
        return result;
    }

    inline bool AssignSolver(GSRCalibrationSettings_* settings, const String_& key, const Cell_& value) {
        const std::map<String_, double GSRCalibrationSettings_::*> numeric{
            {"gradientTolerance", &GSRCalibrationSettings_::gradientTolerance_},
            {"stepTolerance", &GSRCalibrationSettings_::stepTolerance_},
            {"finiteDifferenceStep", &GSRCalibrationSettings_::finiteDifferenceStep_},
            {"priorWeight", &GSRCalibrationSettings_::priorWeight_},
            {"smoothingWeight", &GSRCalibrationSettings_::smoothingWeight_},
            {"numericalErrorFraction", &GSRCalibrationSettings_::numericalErrorFraction_}};
        const auto found = numeric.find(key);
        if (found != numeric.end())
            settings->*found->second = Cell::ToDouble(value);
        else if (key == "maxIterations")
            settings->maxIterations_ = Integer(Cell::ToDouble(value));
        else
            return false;
        return true;
    }
} // namespace Dal::GSRWorksheet
