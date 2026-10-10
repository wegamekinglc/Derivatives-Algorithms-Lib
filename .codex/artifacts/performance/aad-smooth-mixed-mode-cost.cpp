//
// Created by Codex on 2026/10/10.
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

#if DAL_SMOOTH_COST_MODE
#include <dal/math/aad/forwardoverreverse.hpp>
#else
#include <dal/math/aad/bumpoveraad.hpp>
#endif
#include <dal/platform/platform.hpp>

using namespace Dal;
using namespace Dal::AAD;

namespace {
#if DAL_SMOOTH_COST_MODE
    using Active = ForwardOverReverseNumber_;
    using Request = ForwardOverReverseRequest_;
#else
    using Active = Number_;
    using Request = BumpOverAADRequest_;
#endif

    Active Polynomial(RecordingScope_*, const Vector_<Active>& x) { return x[0] * x[0] * x[0] * x[0]; }

    Active BlackScholes(RecordingScope_*, const Vector_<Active>& x) {
        const double rootTime = std::sqrt(1.7);
        const Active d1 = (log(x[0] / 100.0) + (0.02 + 0.5 * x[1] * x[1]) * 1.7) / (x[1] * rootTime);
        return x[0] * std::exp(-0.017) * NCDF(d1) - 100.0 * std::exp(-0.051) * NCDF(d1 - x[1] * rootTime);
    }

    struct Reference_ {
        double value_;
        Vector_<> gradient_;
        Matrix_<> hessian_;
    };

    Reference_ Reference(bool polynomial) {
        if (polynomial)
            return {16.0, {32.0}, Matrix_<>(1, 1, 48.0)};
        const double spot = 103.0, vol = 0.24, rootTime = std::sqrt(1.7), discount = std::exp(-0.017);
        const double d1 = (std::log(spot / 100.0) + (0.02 + 0.5 * vol * vol) * 1.7) / (vol * rootTime), d2 = d1 - vol * rootTime;
        const double density = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
        const double delta = discount * 0.5 * std::erfc(-d1 / std::sqrt(2.0)), vega = spot * discount * density * rootTime;
        Matrix_<> hessian(2, 2);
        hessian(0, 0) = discount * density / (spot * vol * rootTime);
        hessian(0, 1) = hessian(1, 0) = -discount * density * d2 / vol;
        hessian(1, 1) = vega * d1 * d2 / vol;
        return {spot * delta - 100.0 * std::exp(-0.051) * 0.5 * std::erfc(-d2 / std::sqrt(2.0)), {delta, vega}, std::move(hessian)};
    }

    Request MakeRequest(bool polynomial) {
        Request request;
        request.directions_ = Matrix_<>(polynomial ? 1 : 3, polynomial ? 1 : 2, 0.0);
        request.directions_(0, 0) = 1.0;
        if (!polynomial) {
            request.directions_(1, 1) = 1.0;
            request.directions_(2, 0) = -2.0;
            request.directions_(2, 1) = 0.5;
        }
#if !DAL_SMOOTH_COST_MODE
        request.steps_ = polynomial ? Vector_<>{0.01} : Vector_<>{1e-4, 1e-4, 1e-4};
#endif
        return request;
    }

    template <class R_> double Consume(const R_& result) {
        double sum = result.Value();
        for (double value : result.Gradient())
            sum += value;
        for (int row = 0; row < result.Directions().Rows(); ++row) {
#if DAL_SMOOTH_COST_MODE
            sum += result.DirectionalDerivatives()[row];
#else
            for (int column = 0; column < result.Directions().Cols(); ++column)
                sum += result.Gradient()[column] * result.Directions()(row, column);
#endif
            for (int column = 0; column < result.Directions().Cols(); ++column)
                sum += result.HessianProducts()(row, column);
        }
        return sum;
    }

    template <class R_> double Validate(const R_& result, bool polynomial) {
        const auto reference = Reference(polynomial);
        REQUIRE(std::abs(result.Value() - reference.value_) < 1e-10, "independent price mismatch");
        double maxError = 0.0;
        for (int column = 0; column < result.Directions().Cols(); ++column) {
            REQUIRE(std::abs(result.Gradient()[column] - reference.gradient_[column]) < 1e-10, "independent gradient mismatch");
            for (int row = 0; row < result.Directions().Rows(); ++row) {
                double expected = 0.0;
                for (int inner = 0; inner < result.Directions().Cols(); ++inner)
                    expected += reference.hessian_(column, inner) * result.Directions()(row, inner);
                maxError = std::max(maxError, std::abs(expected - result.HessianProducts()(row, column)));
            }
        }
#if DAL_SMOOTH_COST_MODE
        REQUIRE(maxError < 1e-9, "directional AD analytic mismatch");
#else
        REQUIRE(maxError < 0.01, "finite-step analytic mismatch");
#endif
        return maxError;
    }
} // namespace

int main(int argc, char** argv) {
    REQUIRE(argc == 3, "expected polynomial/blackscholes and repeat count");
    const std::string name = argv[1];
    REQUIRE(name == "polynomial" || name == "blackscholes", "unknown smooth cost case");
    const bool polynomial = name == "polynomial";
    const auto request = MakeRequest(polynomial);
    const Vector_<> point = polynomial ? Vector_<>{2.0} : Vector_<>{103.0, 0.24};
    const auto function = polynomial ? Polynomial : BlackScholes;
    const auto evaluate = [&] {
#if DAL_SMOOTH_COST_MODE
        return EvaluateForwardOverReverse(function, point, request);
#else
        return EvaluateBumpOverAAD(function, point, request);
#endif
    };
    const auto reference = evaluate();
    const double error = Validate(reference, polynomial);
    const int repeats = std::stoi(argv[2]);
    REQUIRE(repeats > 0, "positive repeat count required");
    const auto start = std::chrono::steady_clock::now();
    double checksum = 0.0;
    for (int i = 0; i < repeats; ++i)
        checksum += Consume(evaluate());
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
#if DAL_SMOOTH_COST_MODE
    const size_t recordings = reference.Execution().recordings_;
#else
    const size_t recordings = reference.Execution().gradientEvaluations_;
#endif
    REQUIRE(std::isfinite(checksum), "nonfinite consumed output");
    std::cout << std::setprecision(17) << "seconds " << seconds << " checksum " << checksum << " recordings " << recordings << " sweeps "
              << reference.Execution().reverseSweeps_ << " peak " << reference.Execution().peakTapeBytes_ << " cleanup "
              << reference.Execution().cleanupReserveBytes_ << " analytic_error " << error << '\n';
}
