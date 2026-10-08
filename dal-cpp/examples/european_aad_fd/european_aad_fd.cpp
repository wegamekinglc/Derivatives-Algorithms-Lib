//
// Created by Codex on 2026/10/8.
//

#include <algorithm>
#include <array>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include <dal/math/aad/native.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

#include "../../test-support/europeanaadfd.hpp"
#include "../floatformat.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    struct Result_ {
        std::array<double, 2> prices_;
        Matrix_<> risks_;
        double forwardError_, transposeError_;
        size_t steps_;
    };

    Result_ Calculate() {
        Clear(*Tape());
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        std::array<Number_, 3> parameters;
        const std::array<double, 3> values = {0.05, 0.20, 110.0};
        for (size_t coordinate = 0; coordinate < parameters.size(); ++coordinate)
            scope.RegisterInput(parameters[coordinate], values[coordinate]);
        scope.StartRecording();
        auto recording = Example::RecordEuropeanOptions(&scope, {61, 120}, parameters);
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t layer = 0; layer < 2; ++layer)
            NativeOperations_::SetSeed(recording.prices_[layer], 1.0, layer);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        Result_ result{{Value(recording.prices_[0]), Value(recording.prices_[1])},
                       Matrix_<>(2, 3),
                       recording.largestForwardError_,
                       0.0,
                       reports.Entries().size()};
        for (int layer = 0; layer < 2; ++layer)
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                result.risks_(layer, coordinate) = NativeOperations_::ReadAdjoint(parameters[coordinate], layer);
        for (const auto& event : recording.events_)
            for (double error : reports.Report(event).transposeBackwardErrors_)
                result.transposeError_ = std::max(result.transposeError_, error);
        scope.Close();
        Clear(*Tape());
        return result;
    }

    void Print(const Result_& result) {
        std::cout << '\n'
                  << std::string(70, '=') << "\n  Fixed-grid European option prices and native AAD risks\n"
                  << std::string(70, '=') << "\n\n"
                  << "S=100, K=110, r=0.05, q=0.02, sigma=0.20, T=1\n"
                  << "61 space nodes, 120 ordinary time steps, four implicit half steps\n\n";
        std::cout << std::left << std::setw(10) << "Option" << std::right << std::setw(16) << "Price" << std::setw(16) << "dPrice/dr" << std::setw(16)
                  << "dPrice/dSigma" << std::setw(16) << "dPrice/dK" << '\n';
        std::cout << std::string(74, '-') << '\n';
        const std::array<const char*, 2> names = {"Call", "Put"};
        for (int layer = 0; layer < 2; ++layer) {
            std::cout << std::left << std::setw(10) << names[layer] << std::right << std::setw(16) << ExampleFloat(result.prices_[layer], 6);
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                std::cout << std::setw(16) << ExampleFloat(result.risks_(layer, coordinate), 6);
            std::cout << '\n';
        }
        std::cout << std::string(74, '-') << "\n\n"
                  << "Checked steps: " << result.steps_ << "\nLargest physical forward error: " << ExampleFloat(result.forwardError_, 6)
                  << "\nLargest physical transpose error: " << ExampleFloat(result.transposeError_, 6) << "\n\n";
    }
} // namespace

int main() {
    RegisterAll_::Init();
    try {
        Print(Calculate());
        return 0;
    } catch (const std::exception& error) {
        Clear(*Tape());
        std::cerr << "European AAD example: " << error.what() << '\n';
        return 1;
    }
}
