#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/aad/bumpoveraad.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/aad/profiling.hpp>
#include <dal/math/aad/secondorder.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/platform/platform.hpp>

#include "dupirereference.hpp"
#include "dupiresecondorder.hpp"
#include "secondorderperf.hpp"
#include "s0/basketreference.hpp"
#if defined(DAL_TAPE_PERF_WITH_XAD)
#include "secondorderxad.hpp"
#include "s0/memory.hpp"
#endif

using namespace Dal;
using namespace Dal::AAD;

namespace {
    constexpr int BASKET_NODES = 48;
    constexpr double BASKET_RATE = 0.05;
    constexpr double BASKET_STRIKE = 110.0;
    constexpr double BS_TIME = 1.7;
    constexpr double BS_RATE = 0.03;
    constexpr double BS_DIVIDEND = 0.01;
    constexpr double BS_STRIKE = 100.0;
    constexpr int REFERENCE_STEP_MULTIPLIER = 4;
    constexpr int WARMUP_PAIRS = 3;
    constexpr int ROUNDS = 2;
    constexpr int PAIRS_PER_ROUND = 10;

    enum class S0_ { NONE, PAIR, AA, DAL_COLD, DAL_HOT, XAD_COLD, XAD_HOT };

    struct Options_ {
        bool verifyOnly_ = false, centeredFirst_ = false, withXad_ = false, diagnostics_ = false, firstOrder_ = false, dupire_ = false;
        S0_ s0_ = S0_::NONE;
        std::string s0Case_;
        int s0Directions_ = 0;
    };

    struct Case_ {
        const char* name_;
        Vector_<> point_;
        Matrix_<> directions_;
        Vector_<> steps_;
        int callsPerBatch_;
        double pointwiseTolerance_;
        double centeredTolerance_;
        double priceTolerance_ = 1e-11;
        double firstOrderTolerance_ = 5e-8;
        bool absoluteDerivatives_ = false;
        int rollSteps_ = 0;
    };

    struct Reference_ {
        double price_;
        Matrix_<> products_;
        double stencilDisagreement_ = 0.0;
        Vector_<> gradient_{};
    };

    Matrix_<> Directions(std::initializer_list<double> mixed) {
        const auto n = static_cast<int>(mixed.size());
        Matrix_<> result(n + 1, n, 0.0);
        for (int column = 0; column < n; ++column) {
            result(column, column) = 1.0;
            result(n, column) = mixed.begin()[column];
        }
        return result;
    }

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    template <class T_> T_ SmoothCdf(const T_& z) {
        using std::erfc;
        return 0.5 * erfc(-0.7071067811865475244 * z);
    }

    template <bool Smooth_, class T_> T_ BlackCall(const T_& forward, const T_& strike, const T_& stdDev) {
        const T_ d1 = log(forward / strike) / stdDev + 0.5 * stdDev;
        const T_ d2 = d1 - stdDev;
        if constexpr (Smooth_)
            return forward * SmoothCdf(d1) - strike * SmoothCdf(d2);
        return forward * NCDF(d1) - strike * NCDF(d2);
    }

    template <bool Smooth_> struct BlackScholes_ {
        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const {
            const double rootTime = std::sqrt(BS_TIME);
            const auto d1 = (log(x[0] / BS_STRIKE) + (BS_RATE - BS_DIVIDEND + 0.5 * x[1] * x[1]) * BS_TIME) / (x[1] * rootTime);
            if constexpr (Smooth_) {
                const T_ z = d1, d2 = z - x[1] * rootTime;
                return x[0] * std::exp(-BS_DIVIDEND * BS_TIME) * SmoothCdf(z) - BS_STRIKE * std::exp(-BS_RATE * BS_TIME) * SmoothCdf(d2);
            }
            return x[0] * std::exp(-BS_DIVIDEND * BS_TIME) * NCDF(d1) - BS_STRIKE * std::exp(-BS_RATE * BS_TIME) * NCDF(d1 - x[1] * rootTime);
        }
    };

    Reference_ BlackScholesReference(const Case_& shape) {
        const double spot = shape.point_[0], vol = shape.point_[1], rootTime = std::sqrt(BS_TIME);
        const double dividend = std::exp(-BS_DIVIDEND * BS_TIME);
        const double d1 = (std::log(spot / BS_STRIKE) + (BS_RATE - BS_DIVIDEND + 0.5 * vol * vol) * BS_TIME) / (vol * rootTime);
        const double d2 = d1 - vol * rootTime;
        const double density = std::exp(-0.5 * d1 * d1) / std::sqrt(2.0 * std::acos(-1.0));
        const double gamma = dividend * density / (spot * vol * rootTime);
        const double vanna = -dividend * density * d2 / vol;
        const double volga = spot * dividend * density * rootTime * d1 * d2 / vol;
        const double price = spot * dividend * NormalCdf(d1) - BS_STRIKE * std::exp(-BS_RATE * BS_TIME) * NormalCdf(d2);
        Reference_ result{price, Matrix_<>(shape.directions_.Rows(), 2)};
        result.gradient_ = {dividend * NormalCdf(d1), spot * dividend * density * rootTime};
        for (int row = 0; row < result.products_.Rows(); ++row) {
            const double v0 = shape.directions_(row, 0), v1 = shape.directions_(row, 1);
            result.products_(row, 0) = gamma * v0 + vanna * v1;
            result.products_(row, 1) = vanna * v0 + volga * v1;
        }
        return result;
    }

    struct BasketNode_ {
        double z_;
        double weight_;
        bool linear_;
    };

    template <bool Smooth_> struct ConditionalBasket_ {
        Vector_<BasketNode_> nodes_;

        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const {
            // One-year maturity with both dividend yields zero.
            const T_ driftA = BASKET_RATE - 0.5 * x[2] * x[2];
            const T_ projectionB = x[3] * x[4];
            const T_ driftB = BASKET_RATE - 0.5 * projectionB * projectionB;
            const T_ stdDev = x[3] * sqrt(1.0 - x[4] * x[4]);
            T_ sum(0.0);
            for (const auto& node : nodes_) {
                const T_ strike = 2.0 * BASKET_STRIKE - x[0] * exp(driftA + x[2] * node.z_);
                const T_ forwardB = x[1] * exp(driftB + projectionB * node.z_);
                const T_ price = node.linear_ ? T_(forwardB - strike) : BlackCall<Smooth_>(forwardB, strike, stdDev);
                sum += node.weight_ * price;
            }
            return 0.5 * std::exp(-BASKET_RATE) * sum;
        }
    };

    template <bool Smooth_> ConditionalBasket_<Smooth_> MakeBasket(const Case_& shape, const Vector_<>& coordinateSteps) {
        Vector_<> radius(shape.point_.size(), 0.0);
        for (int row = 0; row < shape.directions_.Rows(); ++row)
            for (int column = 0; column < shape.directions_.Cols(); ++column)
                radius[column] = std::max(radius[column], coordinateSteps[column] + REFERENCE_STEP_MULTIPLIER * shape.steps_[row] *
                                                                                        std::abs(shape.directions_(row, column)));
        const auto& x = shape.point_;
        for (size_t column = 0; column < 4; ++column)
            REQUIRE(x[column] > radius[column], "second-order basket requires positive spots and vols throughout the stencil");
        REQUIRE(std::abs(x[4]) + radius[4] < 1.0, "second-order basket requires nonsingular correlation throughout the stencil");
        const double volLow = x[2] - radius[2], volHigh = x[2] + radius[2];
        const NormalExpectation_<> quadrature(BASKET_NODES);
        ConditionalBasket_<Smooth_> result;
        result.nodes_.reserve(BASKET_NODES);
        for (int i = 0; i < BASKET_NODES; ++i) {
            const double z = Smooth_ ? TapePerfS0::BASKET_NODES[i][0] : quadrature.Abscissa()[i];
            // Bound the stock over the entire box before fixing the node's payoff branch.
            const double projectionLow = volLow * z, projectionHigh = volHigh * z;
            const auto projection = std::minmax(projectionLow, projectionHigh);
            const double stockLow = (x[0] - radius[0]) * std::exp(BASKET_RATE - 0.5 * volHigh * volHigh + projection.first);
            const double stockHigh = (x[0] + radius[0]) * std::exp(BASKET_RATE - 0.5 * volLow * volLow + projection.second);
            REQUIRE(stockHigh < 2.0 * BASKET_STRIKE || stockLow > 2.0 * BASKET_STRIKE,
                    "second-order basket stencil crosses a conditional strike zero");
            const double weight = Smooth_ ? TapePerfS0::BASKET_NODES[i][1] : quadrature.Weight()[i];
            result.nodes_.push_back({z, weight, stockLow > 2.0 * BASKET_STRIKE});
        }
        return result;
    }

    double BasketScalarPrice(const ConditionalBasket_<false>& basket, const Vector_<>& x) {
        const double conditionalStdDev = x[3] * std::sqrt((1.0 - x[4]) * (1.0 + x[4]));
        double price = 0.0;
        for (const auto& node : basket.nodes_) {
            const double terminalA = x[0] * std::exp(BASKET_RATE + x[2] * (node.z_ - 0.5 * x[2]));
            const double conditionalLogB = std::log(x[1]) + BASKET_RATE - 0.5 * x[3] * x[3] + x[3] * x[4] * node.z_;
            const double meanB = std::exp(conditionalLogB + 0.5 * conditionalStdDev * conditionalStdDev);
            const double strike = 2.0 * BASKET_STRIKE - terminalA;
            double call = meanB - strike;
            if (strike > 0.0) {
                const double d2 = (conditionalLogB - std::log(strike)) / conditionalStdDev;
                call = meanB * NormalCdf(d2 + conditionalStdDev) - strike * NormalCdf(d2);
            }
            price += node.weight_ * call;
        }
        return 0.5 * std::exp(-BASKET_RATE) * price;
    }

    double MixedPriceDifference(const ConditionalBasket_<false>& basket, const Case_& shape, int row, int column, double h, double k) {
        Vector_<> upper = shape.point_, lower = shape.point_;
        for (int i = 0; i < shape.directions_.Cols(); ++i) {
            upper[i] += h * shape.directions_(row, i);
            lower[i] -= h * shape.directions_(row, i);
        }
        upper[column] += k;
        lower[column] += k;
        const double upperPlus = BasketScalarPrice(basket, upper), lowerPlus = BasketScalarPrice(basket, lower);
        upper[column] -= 2.0 * k;
        lower[column] -= 2.0 * k;
        return ((upperPlus - BasketScalarPrice(basket, upper)) - (lowerPlus - BasketScalarPrice(basket, lower))) / (4.0 * h * k);
    }

    Reference_ BasketReference(const ConditionalBasket_<false>& basket, const Case_& shape, const Vector_<>& coordinateSteps) {
        Reference_ result{BasketScalarPrice(basket, shape.point_), Matrix_<>(shape.directions_.Rows(), shape.directions_.Cols())};
        result.gradient_.Resize(shape.point_.size());
        for (size_t column = 0; column < shape.point_.size(); ++column) {
            const auto slope = [&](double step) {
                auto plus = shape.point_, minus = shape.point_;
                plus[column] += step;
                minus[column] -= step;
                return (BasketScalarPrice(basket, plus) - BasketScalarPrice(basket, minus)) / (2.0 * step);
            };
            const double h = coordinateSteps[column];
            result.gradient_[column] = (4.0 * slope(h / 2.0) - slope(h)) / 3.0;
        }
        for (int row = 0; row < result.products_.Rows(); ++row) {
            for (int column = 0; column < result.products_.Cols(); ++column) {
                const double h = REFERENCE_STEP_MULTIPLIER * shape.steps_[row], k = coordinateSteps[column];
                const double full = MixedPriceDifference(basket, shape, row, column, h, k);
                const double half = MixedPriceDifference(basket, shape, row, column, h / 2.0, k / 2.0);
                const double quarter = MixedPriceDifference(basket, shape, row, column, h / 4.0, k / 4.0);
                // Halving both axes cancels the leading squared-step error.
                const double coarse = (4.0 * half - full) / 3.0, refined = (4.0 * quarter - half) / 3.0;
                const double disagreement = std::abs(refined - coarse);
                REQUIRE(disagreement <= 0.5 * shape.pointwiseTolerance_ * std::max(1.0, std::abs(refined)),
                        "second-order basket scalar-price HVP reference did not stabilize");
                result.stencilDisagreement_ = std::max(result.stencilDisagreement_, disagreement);
                result.products_(row, column) = refined;
            }
        }
        return result;
    }

    Reference_ S0BasketReference(const Case_& shape) {
        using namespace TapePerfS0;
        Reference_ ref{BASKET_PRICE, Matrix_<>(shape.directions_.Rows(), 5, 0.0), 0.0, Vector_<>(BASKET_GRADIENT.begin(), BASKET_GRADIENT.end())};
        for (int row = 0; row < ref.products_.Rows(); ++row)
            for (int col = 0; col < ref.products_.Cols(); ++col)
                for (int axis = 0; axis < ref.products_.Cols(); ++axis)
                    ref.products_(row, col) += BASKET_HESSIAN[col][axis] * shape.directions_(row, axis);
        return ref;
    }

    Matrix_<> Prefix(const Matrix_<>& full, int rows) {
        Matrix_<> result(rows, full.Cols());
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < full.Cols(); ++column)
                result(row, column) = full(row, column);
        return result;
    }

    double Tolerance(const Case_& shape, double tolerance, double expected) {
        return shape.absoluteDerivatives_ ? tolerance : tolerance * std::max(1.0, std::abs(expected));
    }

    void
    Verify(const Case_& shape, const Reference_& ref, int count, const ForwardOverReverseResult_& pointwise, const BumpOverAADResult_& centered) {
        const auto& pointwiseWork = pointwise.Execution();
        const auto& centeredWork = centered.Execution();
        const auto& products = pointwise.HessianProducts();
        const auto n = static_cast<size_t>(count);
        REQUIRE(products.Rows() == count && centered.HessianProducts().Rows() == count && products.Cols() == ref.products_.Cols() &&
                    centered.HessianProducts().Cols() == ref.products_.Cols(),
                "second-order benchmark unexpected HVP shape");
        REQUIRE(pointwiseWork.recordings_ == n && pointwiseWork.reverseSweeps_ == n + 1 && centeredWork.gradientEvaluations_ == 1 + 2 * n &&
                    centeredWork.reverseSweeps_ == 1 + 2 * n,
                "second-order benchmark unexpected tape work");
        REQUIRE(std::abs(ref.price_ - pointwise.Value()) <= shape.priceTolerance_ && std::abs(ref.price_ - centered.Value()) <= shape.priceTolerance_,
                "second-order benchmark price differs from independent reference");
        REQUIRE(std::abs(ref.products_(0, 1)) > 1e-6, "second-order benchmark single direction needs a nonzero cross term");
        double pointwiseError = 0.0, centeredError = 0.0;
        for (int row = 0; row < products.Rows(); ++row) {
            for (int column = 0; column < products.Cols(); ++column) {
                const double expected = ref.products_(row, column);
                const double ad = products(row, column), bump = centered.HessianProducts()(row, column);
                const double adError = std::abs(ad - expected), bumpError = std::abs(bump - expected);
                std::cout << "hvp," << shape.name_ << ',' << n << ',' << row << ',' << column << ',' << centered.Steps()[row] << ',' << expected
                          << ',' << ad << ',' << bump << '\n';
                REQUIRE(adError <= Tolerance(shape, shape.pointwiseTolerance_, expected),
                        "second-order benchmark pointwise HVP differs from reference");
                REQUIRE(bumpError <= Tolerance(shape, shape.centeredTolerance_, expected),
                        "second-order benchmark centered HVP differs from reference");
                pointwiseError = std::max(pointwiseError, adError);
                centeredError = std::max(centeredError, bumpError);
            }
        }
        std::cout << "accuracy," << shape.name_ << ',' << n << ',' << ref.price_ << ',' << ref.stencilDisagreement_ << ',' << pointwiseError << ','
                  << centeredError << '\n'
                  << "work," << shape.name_ << ',' << n << ',' << pointwiseWork.recordings_ << ',' << pointwiseWork.reverseSweeps_ << ','
                  << pointwiseWork.peakTapeBytes_ << ',' << centeredWork.gradientEvaluations_ << ',' << centeredWork.reverseSweeps_ << ','
                  << centeredWork.peakTapeBytes_ << '\n';
    }

    size_t TapeCapacity() {
        TapeCapacityBudget_ budget(std::numeric_limits<size_t>::max());
        TapeCapacityScope_ scope(&budget);
        return budget.CapacityBytes();
    }

    template <class F_> double TimeBatch(const F_& evaluate, int calls) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < calls; ++i) {
            const auto result = evaluate();
            Bench::DoNotOptimize(&result);
        }
        return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / calls;
    }

    using Samples_ = std::array<double, ROUNDS * PAIRS_PER_ROUND>;

    std::array<double, 3> Summary(Samples_ samples) {
        std::sort(samples.begin(), samples.end());
        const size_t middle = samples.size() / 2;
        const size_t rank95 = (95 * samples.size() + 99) / 100;
        return {0.5 * (samples[middle - 1] + samples[middle]), samples[rank95 - 1], samples.front()};
    }

    template <class P_, class B_> void Measure(const Case_& shape, int directions, const P_& pointwise, const B_& centered, bool centeredFirst) {
        Clear(*Tape());
        for (int i = 0; i < WARMUP_PAIRS; ++i) {
            (void)TimeBatch(pointwise, shape.callsPerBatch_);
            (void)TimeBatch(centered, shape.callsPerBatch_);
        }
        // Both drivers rewind to an empty graph and retain the same warmed tape blocks.
        const size_t initialCapacity = TapeCapacity();
        Samples_ pointwiseSamples{}, centeredSamples{};
        for (size_t i = 0; i < pointwiseSamples.size(); ++i) {
            const bool bumpFirst = centeredFirst != (i % 2 != 0);
            if (bumpFirst) {
                centeredSamples[i] = TimeBatch(centered, shape.callsPerBatch_);
                pointwiseSamples[i] = TimeBatch(pointwise, shape.callsPerBatch_);
            } else {
                pointwiseSamples[i] = TimeBatch(pointwise, shape.callsPerBatch_);
                centeredSamples[i] = TimeBatch(centered, shape.callsPerBatch_);
            }
        }
        REQUIRE(TapeCapacity() == initialCapacity, "second-order benchmark tape grew after warmup");
        for (size_t i = 0; i < pointwiseSamples.size(); ++i)
            std::cout << "sample," << shape.name_ << ',' << directions << ',' << 1 + i / PAIRS_PER_ROUND << ',' << 1 + i % PAIRS_PER_ROUND << ','
                      << ((centeredFirst != (i % 2 != 0)) ? "centered" : "pointwise") << ',' << shape.callsPerBatch_ << ',' << pointwiseSamples[i]
                      << ',' << centeredSamples[i] << '\n';
        const auto [adMedian, adP95, adMin] = Summary(pointwiseSamples);
        const auto [bumpMedian, bumpP95, bumpMin] = Summary(centeredSamples);
        std::cout << "summary," << shape.name_ << ',' << directions << ',' << pointwiseSamples.size() << ',' << initialCapacity << ',' << adMedian
                  << ',' << adP95 << ',' << adMin << ',' << bumpMedian << ',' << bumpP95 << ',' << bumpMin << ',' << adMedian / bumpMedian << '\n';
    }

    Vector_<> ProjectGradient(const Vector_<>& gradient, const Matrix_<>& directions) {
        Vector_<> slopes(directions.Rows(), 0.0);
        for (int row = 0; row < directions.Rows(); ++row)
            for (int column = 0; column < directions.Cols(); ++column)
                slopes[row] += directions(row, column) * gradient[column];
        return slopes;
    }

    struct CenteredOutput_ {
        BumpOverAADResult_ native_;
        Vector_<> directional_;
    };

    struct TapeWork_ {
        size_t recordings_;
        size_t sweeps_;
        size_t bytes_;
        const char* byteKind_;
    };

    void VerifyRoute(const Case_& shape,
                     const Reference_& ref,
                     const Matrix_<>& directions,
                     const char* name,
                     double price,
                     const Vector_<>& gradient,
                     const Vector_<>& directional,
                     const Matrix_<>& products,
                     double hvpTolerance,
                     const TapeWork_& work) {
        const auto expectedSlopes = ProjectGradient(ref.gradient_, directions);
        REQUIRE(gradient.size() == ref.gradient_.size() && directional.size() == expectedSlopes.size() && products.Rows() == directions.Rows() &&
                    products.Cols() == directions.Cols(),
                "second-order route output shape mismatch");
        const double priceError = std::abs(price - ref.price_);
        REQUIRE(priceError <= shape.priceTolerance_, "second-order route price mismatch");
        const auto check = [&](double actual, double expected, double tolerance) {
            const double error = std::abs(actual - expected);
            REQUIRE(error <= Tolerance(shape, tolerance, expected), "second-order route derivative differs from its independent reference");
            return error;
        };
        const auto printOutput = [&](const char* kind, int row, int column, double expected, double value) {
            std::cout << "output," << shape.name_ << ',' << directions.Rows() << ',' << name << ',' << kind << ',' << row << ',' << column << ','
                      << expected << ',' << value << '\n';
        };
        printOutput("price", 0, 0, ref.price_, price);
        double gradientError = 0.0, directionalError = 0.0, productError = 0.0;
        for (int column = 0; column < directions.Cols(); ++column) {
            gradientError = std::max(gradientError, check(gradient[column], ref.gradient_[column], shape.firstOrderTolerance_));
            printOutput("gradient", 0, column, ref.gradient_[column], gradient[column]);
        }
        for (int row = 0; row < directions.Rows(); ++row) {
            directionalError = std::max(directionalError, check(directional[row], expectedSlopes[row], shape.firstOrderTolerance_));
            printOutput("directional", row, 0, expectedSlopes[row], directional[row]);
            for (int column = 0; column < directions.Cols(); ++column) {
                productError = std::max(productError, check(products(row, column), ref.products_(row, column), hvpTolerance));
                printOutput("hvp", row, column, ref.products_(row, column), products(row, column));
            }
        }
        std::cout << "metric," << shape.name_ << ',' << directions.Rows() << ',' << name << ',' << priceError << ',' << gradientError << ','
                  << directionalError << ',' << productError << ',' << work.recordings_ << ',' << work.sweeps_ << ',' << work.bytes_ << ','
                  << work.byteKind_ << '\n';
        if (shape.rollSteps_ != 0)
            std::cout << "dupire_work," << directions.Rows() << ',' << name << ',' << work.recordings_ << ',' << work.recordings_ << ','
                      << work.recordings_ * shape.rollSteps_ << '\n';
    }

    void VerifyNativeRoutes(const Case_& shape,
                            const Reference_& ref,
                            const Matrix_<>& directions,
                            const ForwardOverReverseResult_& pointwise,
                            const CenteredOutput_& centered) {
        const auto& adWork = pointwise.Execution();
        const auto& bumpWork = centered.native_.Execution();
        Verify(shape, ref, directions.Rows(), pointwise, centered.native_);
        VerifyRoute(shape, ref, directions, "pointwise", pointwise.Value(), pointwise.Gradient(), pointwise.DirectionalDerivatives(),
                    pointwise.HessianProducts(), shape.pointwiseTolerance_,
                    {adWork.recordings_, adWork.reverseSweeps_, adWork.peakTapeBytes_, "dal_allocated_peak"});
        VerifyRoute(shape, ref, directions, "centered", centered.native_.Value(), centered.native_.Gradient(), centered.directional_,
                    centered.native_.HessianProducts(), shape.centeredTolerance_,
                    {bumpWork.gradientEvaluations_, bumpWork.reverseSweeps_, bumpWork.peakTapeBytes_, "dal_allocated_peak"});
    }

#if defined(DAL_TAPE_PERF_WITH_XAD)
    bool S0Memory(S0_ mode) { return mode != S0_::PAIR && mode != S0_::AA && mode != S0_::NONE; }

    void VerifySmoothNormal() {
        const SecondOrderFunction_ dal = [](const auto& x) { return SmoothCdf(x[0]); };
        const TapePerfXad::OwnedFunction_ xad = [](RecordingScope_*, const auto& x) { return SmoothCdf(x[0]); };
        TapePerfXad::Workspace_ workspace;
        ForwardOverReverseRequest_ request;
        request.directions_ = Matrix_<>(1, 1, 1.0);
        for (double magnitude : {std::nextafter(10.0, 0.0), 10.0, std::nextafter(10.0, 20.0), 12.0, 20.0}) {
            for (double z : {-magnitude, magnitude}) {
                const auto native = EvaluateSecondOrder(dal, {z}, request.directions_);
                const auto other = workspace.EvaluateOwning(xad, {z}, request);
                const long double s = z;
                const double value = 0.5L * std::erfc(-s / std::sqrt(2.0L));
                const double density = std::exp(-s * s / 2.0L) / std::sqrt(2.0L * std::acos(-1.0L));
                const auto near = [](double actual, double expected) { return std::abs(actual - expected) <= 2e-13 * std::abs(expected); };
                REQUIRE(near(native.Value(), value) && near(other.risk_.price_, value) && near(native.Gradient()[0], density) &&
                            near(other.risk_.gradient_[0], density) && near(native.DirectionalDerivatives()[0], density) &&
                            near(other.risk_.directional_[0], density) && near(native.HessianProducts()(0, 0), -z * density) &&
                            near(other.risk_.products_(0, 0), -z * density),
                        "S0 smooth normal value or weight mismatch");
                std::cout << "s0_normal," << z << ',' << value << ',' << density << ',' << -z * density << '\n';
            }
        }
    }

    template <class D_, class X_> void MeasureS0(const Case_& shape, int directions, const D_& first, const X_& second, bool aa, bool secondFirst) {
        for (int i = 0; i < WARMUP_PAIRS; ++i) {
            (void)TimeBatch(first, shape.callsPerBatch_);
            (void)TimeBatch(second, shape.callsPerBatch_);
        }
        Samples_ left{}, right{};
        for (size_t i = 0; i < left.size(); ++i) {
            if (secondFirst != (i % 2 != 0)) {
                right[i] = TimeBatch(second, shape.callsPerBatch_);
                left[i] = TimeBatch(first, shape.callsPerBatch_);
            } else {
                left[i] = TimeBatch(first, shape.callsPerBatch_);
                right[i] = TimeBatch(second, shape.callsPerBatch_);
            }
        }
        for (size_t i = 0; i < left.size(); ++i)
            std::cout << "s0_pair," << (aa ? "aa" : "comparison") << ',' << shape.name_ << ',' << directions << ',' << i + 1 << ','
                      << ((secondFirst != (i % 2 != 0)) ? "B" : "A") << ',' << shape.callsPerBatch_ << ',' << left[i] << ',' << right[i] << '\n';
    }

    void
    S0Allocation(const Case_& shape, const SecondOrderFunction_& dal, const ForwardOverReverseRequest_& request, S0_ mode,
                 const TapePerfXad::OwnedFunction_& xad) {
        using namespace TapePerfS0;
        const MemoryRequest_ memory{mode == S0_::XAD_COLD || mode == S0_::XAD_HOT, mode == S0_::DAL_HOT || mode == S0_::XAD_HOT};
        MeasureMemory(shape.name_, request.directions_.Rows(), memory, [&](const auto& use) {
            REQUIRE(!NativeThread::retainedTape, "S0 worker must start without a native tape");
            if (memory.xad_) {
                TapePerfXad::Workspace_ workspace;
                use([&](const auto& observeResult) {
                    const auto result = workspace.EvaluateOwning(xad, shape.point_, request, true);
                    Bench::DoNotOptimize(&result);
                    observeResult();
                    return RiskMemory_{result.risk_.maxLogicalBytes_, std::nullopt, "xad_logical_after_sweep", {}, {}};
                });
                REQUIRE(!NativeThread::retainedTape, "S0 XAD allocation process initialized a native tape");
            } else {
                use([&](const auto& observeResult) {
                    const auto result = EvaluateSecondOrder(dal, shape.point_, request.directions_);
                    REQUIRE(!NativeThread::retainedTape, "cold second order initialized a first-order tape");
                    const auto& work = result.Execution();
                    Bench::DoNotOptimize(&result);
                    observeResult();
                    return RiskMemory_{SecondOrderDetail::LastSecondOrderLogicalPeak(), work.cleanupReserveBytes_, "dal_logical_tape_peak",
                                       work.peakTapeBytes_, work.retainedTapeBytes_};
                });
            }
        });
    }

    template <class K_> void RunS0(const Case_& shape, const K_& kernel, const Reference_& ref, const Options_& options) {
        if (!options.s0Case_.empty() && options.s0Case_ != shape.name_)
            return;
        const SecondOrderFunction_ dal = [plan = &kernel](const auto& x) { return (*plan)(nullptr, x); };
        const TapePerfXad::OwnedFunction_ xad = [plan = &kernel](RecordingScope_* scope, const auto& x) { return (*plan)(scope, x); };
        for (int count : {1, shape.directions_.Rows()}) {
            if (options.s0Directions_ && options.s0Directions_ != count)
                continue;
            ForwardOverReverseRequest_ request;
            request.directions_ = Prefix(shape.directions_, count);
            if (S0Memory(options.s0_)) {
                S0Allocation(shape, dal, request, options.s0_, xad);
                continue;
            }
            TapePerfXad::Workspace_ workspace;
            const auto native = [&] { return EvaluateSecondOrder(dal, shape.point_, request.directions_); };
            const auto other = [&] { return workspace.EvaluateOwning(xad, shape.point_, request); };
            if (!options.verifyOnly_) {
                if (options.s0_ == S0_::AA) {
                    TapePerfXad::Workspace_ alternate;
                    const auto replica = [&] { return alternate.EvaluateOwning(xad, shape.point_, request); };
                    MeasureS0(shape, count, other, replica, true, options.centeredFirst_);
                } else {
                    MeasureS0(shape, count, native, other, false, options.centeredFirst_);
                }
                continue;
            }
            const auto withExpiredSources = [&](const auto& function, const auto& engine) {
                auto sourcePoint = shape.point_;
                auto sourceRequest = request;
                auto sourceFunction = function;
                auto result = engine(sourceFunction, sourcePoint, sourceRequest);
                sourcePoint.Fill(0.0);
                sourceRequest.directions_.Fill(0.0);
                sourceFunction = {};
                return result;
            };
            const auto ad = withExpiredSources(dal, [](const auto& f, const auto& p, const auto& r) { return EvaluateSecondOrder(f, p, r.directions_); });
            const auto xv = withExpiredSources(xad, [&](const auto& f, const auto& p, const auto& r) { return workspace.EvaluateOwning(f, p, r); });
            const auto& dw = ad.Execution();
            const auto& xw = xv.execution_;
            const auto& expected = request.directions_;
            const auto sameDirections = [&](const Matrix_<>& actual) {
                return actual.Rows() == expected.Rows() && actual.Cols() == expected.Cols() &&
                       std::equal(expected.Data(), expected.Data() + expected.Rows() * expected.Cols(), actual.Data());
            };
            REQUIRE(ad.Point() == shape.point_ && xv.point_ == shape.point_ && sameDirections(ad.Directions()) && sameDirections(xv.directions_),
                    "S0 point and directions must remain owning after source destruction");
            const auto n = static_cast<size_t>(count);
            REQUIRE(dw.callbackEvaluations_ == n && dw.recordings_ == n && dw.reverseSweeps_ == n && dw.safePathActivations_ == 0 &&
                        xw.callbackEvaluations_ == n &&
                        xw.recordings_ == n && xw.reverseSweeps_ == n && xw.numericPayloadBytes_ == dw.numericPayloadBytes_ && !xw.peakTapeBytes_ &&
                        !xw.cleanupReserveBytes_,
                    std::string("S0 execution fields do not match the fixed request. Protection: ") +
                        SecondOrderDetail::LastSecondOrderProtection());
            VerifyRoute(shape, ref, request.directions_, "dal_s0", ad.Value(), ad.Gradient(), ad.DirectionalDerivatives(), ad.HessianProducts(),
                        shape.pointwiseTolerance_, {dw.recordings_, dw.reverseSweeps_, dw.peakTapeBytes_, "dal_tape_capacity"});
            VerifyRoute(shape, ref, request.directions_, "xad_s0", xv.risk_.price_, xv.risk_.gradient_, xv.risk_.directional_, xv.risk_.products_,
                        shape.pointwiseTolerance_, {xw.recordings_, xw.reverseSweeps_, 0, "unavailable"});
            std::cout << "s0_owned," << shape.name_ << ',' << count << ',' << dw.numericPayloadBytes_ << ',' << dw.method_ << ',' << xw.method_
                      << '\n';
        }
    }

    using TapePerfComparison::Counts_;
    using TapePerfComparison::FirstOrderResult_;
    using TapePerfComparison::Phase;
    using TapePerfComparison::Span_;

    template <class C_> void PrintCounts(const Case_& shape, int directions, const char* route, const C_& counts, const char* byteKind) {
        std::cout << "tape_counts," << shape.name_ << ',' << directions << ',' << route << ',' << counts.recordings_ << ',' << counts.sweeps_ << ','
                  << counts.statements_ << ',' << counts.edges_ << ',' << counts.statementVisits_ << ',' << counts.edgeSlots_ << ','
                  << counts.maxBytes_ << ',' << byteKind << '\n';
    }

#if defined(DAL_ENABLE_AAD_PROFILING)
    template <class F_> ProfilingReverseGraphStatistics_ CountReverseGraphs(const F_& evaluate) {
        Clear(*Tape());
        ProfilingData_ data;
        {
            ProfilingScope_ profile(&data);
            data.reverseGraphs_.emplace();
            (void)evaluate();
        }
        REQUIRE(data.complete_ && data.reverseGraphs_->sweeps_ == data.Phase(Phase::REVERSE).calls_,
                "AAD comparison reverse graph counts must cover every actual sweep");
        return *data.reverseGraphs_;
    }
#endif

    template <class F_> void ProfileBatch(const Case_& shape, int directions, const char* route, size_t sample, const F_& evaluate) {
        ProfilingData_ data;
        double total;
        {
            ProfilingScope_ profile(&data);
            total = TimeBatch(evaluate, shape.callsPerBatch_);
        }
        REQUIRE(data.complete_ && !data.invalidMeasurement_, "AAD comparison phase profiling did not complete");
        const auto phase = [&](Phase kind) { return double(data.Phase(kind).selfWallNanoseconds_) / shape.callsPerBatch_; };
        const double record = phase(Phase::PATH_FORWARD), reverse = phase(Phase::REVERSE), manage = phase(Phase::PREPARE),
                     pack = phase(Phase::REDUCE);
        REQUIRE(record > 0.0 && reverse > 0.0 && manage > 0.0 && pack > 0.0, "AAD comparison phase was not measured");
        std::cout << "phase_sample," << shape.name_ << ',' << directions << ',' << route << ',' << sample + 1 << ',' << shape.callsPerBatch_ << ','
                  << record << ',' << reverse << ',' << manage << ',' << pack << ',' << total << ',' << total - record - reverse - manage - pack
                  << '\n';
    }

    void VerifyFirst(const Case_& shape, const Reference_& ref, const FirstOrderResult_& result, const char* route) {
        REQUIRE(std::abs(result.price_ - ref.price_) <= shape.priceTolerance_ && result.gradient_.size() == ref.gradient_.size(),
                "first-order benchmark price or gradient size differs from reference");
        double error = 0.0;
        std::cout << "first_output," << shape.name_ << ',' << route << ",price,-1," << ref.price_ << ',' << result.price_ << '\n';
        for (size_t i = 0; i < ref.gradient_.size(); ++i) {
            const double actual = result.gradient_[i], expected = ref.gradient_[i];
            const double absError = std::abs(actual - expected);
            REQUIRE(absError <= Tolerance(shape, shape.firstOrderTolerance_, expected), "first-order benchmark gradient differs from reference");
            error = std::max(error, absError);
            std::cout << "first_output," << shape.name_ << ',' << route << ",gradient," << i << ',' << expected << ',' << actual << '\n';
        }
        std::cout << "first_metric," << shape.name_ << ',' << route << ',' << std::abs(ref.price_ - result.price_) << ',' << error << '\n';
    }

    template <class K_>
    void RunFirst(const Case_& shape, const K_& kernel, const Reference_& ref, bool verifyOnly, bool reverseOrder, bool diagnostics) {
        TapePerfXad::FirstOrderWorkspace_ workspace;
        Counts_ dalCounts, xadCounts;
        Clear(*Tape());
        const auto dalCheck = TapePerfComparison::DalFirst(kernel, shape.point_, &dalCounts);
        const auto xadCheck = workspace.Evaluate(kernel, shape.point_, &xadCounts);
        VerifyFirst(shape, ref, dalCheck, "dal_first");
        VerifyFirst(shape, ref, xadCheck, "xad_first");
        PrintCounts(shape, 0, "dal_first", dalCounts, "dal_allocated_peak");
        PrintCounts(shape, 0, "xad_first", xadCounts, "xad_logical_after_sweep");
        if (verifyOnly)
            return;
        const auto dal = [&]() { return TapePerfComparison::DalFirst(kernel, shape.point_); };
        const auto xad = [&]() { return workspace.Evaluate(kernel, shape.point_); };
        for (int i = 0; i < WARMUP_PAIRS; ++i) {
            (void)TimeBatch(dal, shape.callsPerBatch_);
            (void)TimeBatch(xad, shape.callsPerBatch_);
        }
        if (diagnostics) {
            Counts_ dalTwiceCounts, xadTwiceCounts;
            const auto dalTwice = [&]() { return TapePerfComparison::DalFirst(kernel, shape.point_, nullptr, 2); };
            const auto xadTwice = [&]() { return workspace.Evaluate(kernel, shape.point_, nullptr, 2); };
            const auto dalRepeated = TapePerfComparison::DalFirst(kernel, shape.point_, &dalTwiceCounts, 2);
            const auto xadRepeated = workspace.Evaluate(kernel, shape.point_, &xadTwiceCounts, 2);
            REQUIRE(dalRepeated.price_ == dalCheck.price_ && dalRepeated.gradient_ == dalCheck.gradient_ && xadRepeated.price_ == xadCheck.price_ &&
                        xadRepeated.gradient_ == xadCheck.gradient_,
                    "AAD comparison repeat sweep changed its result");
            REQUIRE(dalCounts.statements_ == dalTwiceCounts.statements_ && dalCounts.edges_ == dalTwiceCounts.edges_ &&
                        xadCounts.statements_ == xadTwiceCounts.statements_ && xadCounts.edges_ == xadTwiceCounts.edges_,
                    "AAD comparison repeat sweep changed recording size");
            PrintCounts(shape, 0, "dal_repeat", dalTwiceCounts, "dal_allocated_peak");
            PrintCounts(shape, 0, "xad_repeat", xadTwiceCounts, "xad_logical_after_sweep");
            for (size_t i = 0; i < Samples_{}.size(); ++i) {
                for (size_t j = 0; j < 4; ++j) {
                    const size_t route = (i / 2 + ((reverseOrder != (i % 2 != 0)) ? 3 - j : j)) % 4;
                    if (route == 0)
                        ProfileBatch(shape, 0, "dal_first", i, dal);
                    else if (route == 1)
                        ProfileBatch(shape, 0, "xad_first", i, xad);
                    else if (route == 2)
                        ProfileBatch(shape, 0, "dal_repeat", i, dalTwice);
                    else
                        ProfileBatch(shape, 0, "xad_repeat", i, xadTwice);
                }
            }
            return;
        }
        const size_t initialCapacity = TapeCapacity();
        Samples_ dalSamples{}, xadSamples{};
        for (size_t i = 0; i < dalSamples.size(); ++i) {
            if (reverseOrder != (i % 2 != 0)) {
                xadSamples[i] = TimeBatch(xad, shape.callsPerBatch_);
                dalSamples[i] = TimeBatch(dal, shape.callsPerBatch_);
            } else {
                dalSamples[i] = TimeBatch(dal, shape.callsPerBatch_);
                xadSamples[i] = TimeBatch(xad, shape.callsPerBatch_);
            }
        }
        REQUIRE(TapeCapacity() == initialCapacity, "first-order benchmark tape grew after warmup");
        for (size_t i = 0; i < dalSamples.size(); ++i)
            std::cout << "first_sample," << shape.name_ << ',' << 1 + i / PAIRS_PER_ROUND << ',' << 1 + i % PAIRS_PER_ROUND << ','
                      << ((reverseOrder != (i % 2 != 0)) ? "xad_first" : "dal_first") << ',' << shape.callsPerBatch_ << ',' << dalSamples[i] << ','
                      << xadSamples[i] << '\n';
    }

    template <class P_, class B_, class X_>
    void
    MeasureTriplets(const Case_& shape, int directions, const P_& pointwise, const B_& centered, const X_& xad, bool reverseOrder, bool diagnostics) {
        Clear(*Tape());
        for (int i = 0; i < WARMUP_PAIRS; ++i) {
            (void)TimeBatch(pointwise, shape.callsPerBatch_);
            (void)TimeBatch(centered, shape.callsPerBatch_);
            (void)TimeBatch(xad, shape.callsPerBatch_);
        }
        const size_t initialCapacity = TapeCapacity();
        std::array<Samples_, 3> samples{};
        constexpr std::array<const char*, 3> NAMES{"pointwise", "centered", "xad"};
        const auto order = [reverseOrder](size_t index) {
            const size_t first = index / 2 % 3, second = (first + 1) % 3, third = (first + 2) % 3;
            return (reverseOrder != (index % 2 != 0)) ? std::array{third, second, first} : std::array{first, second, third};
        };
        const auto measure = [&](size_t route, size_t sample) {
            const auto batch = [&](const auto& f) {
                if (!diagnostics)
                    return TimeBatch(f, shape.callsPerBatch_);
                ProfileBatch(shape, directions, NAMES[route], sample, f);
                return 0.0;
            };
            if (route == 0)
                return batch(pointwise);
            if (route == 1)
                return batch(centered);
            return batch(xad);
        };
        for (size_t i = 0; i < samples[0].size(); ++i)
            for (size_t route : order(i))
                samples[route][i] = measure(route, i);
        REQUIRE(TapeCapacity() == initialCapacity, "second-order benchmark tape grew after warmup");
        if (diagnostics)
            return;
        if (shape.rollSteps_ != 0)
            std::cout << "dupire_warm_dal_tape_bytes," << directions << ',' << initialCapacity << '\n';
        for (size_t i = 0; i < samples[0].size(); ++i) {
            const auto ordering = order(i);
            std::cout << "tri_sample," << shape.name_ << ',' << directions << ',' << 1 + i / PAIRS_PER_ROUND << ',' << 1 + i % PAIRS_PER_ROUND << ','
                      << NAMES[ordering[0]] << ',' << NAMES[ordering[1]] << ',' << NAMES[ordering[2]] << ',' << shape.callsPerBatch_ << ','
                      << samples[0][i] << ',' << samples[1][i] << ',' << samples[2][i] << '\n';
        }
        for (size_t route = 0; route < samples.size(); ++route) {
            const auto [median, p95, minimum] = Summary(samples[route]);
            std::cout << "tri_summary," << shape.name_ << ',' << directions << ',' << NAMES[route] << ',' << median << ',' << p95 << ',' << minimum
                      << '\n';
        }
    }

    template <class K_, class P_, class B_>
    void RunWithXad(const Case_& shape,
                    const Reference_& ref,
                    const K_& kernel,
                    const Matrix_<>& directions,
                    const P_& pointwise,
                    const B_& centered,
                    bool verifyOnly,
                    bool reverseOrder,
                    bool diagnostics) {
        TapePerfXad::Workspace_ workspace;
        const auto xad = [&]() { return workspace.Evaluate(kernel, shape.point_, directions); };
        Clear(*Tape());
        const auto pointwiseValue = pointwise();
        Clear(*Tape());
        const auto centeredValue = centered();
        const auto xadValue = workspace.Evaluate(kernel, shape.point_, directions, true);
        const size_t n = static_cast<size_t>(directions.Rows());
        VerifyNativeRoutes(shape, ref, directions, pointwiseValue, centeredValue);
        VerifyRoute(shape, ref, directions, "xad", xadValue.price_, xadValue.gradient_, xadValue.directional_, xadValue.products_,
                    shape.pointwiseTolerance_, {n, n, xadValue.maxLogicalBytes_, "xad_max_logical_after_sweep"});
#if defined(DAL_ENABLE_AAD_PROFILING)
        if (diagnostics) {
            const auto dalCounts = CountReverseGraphs(pointwise);
            const auto centeredCounts = CountReverseGraphs(centered);
            Counts_ xadCounts;
            (void)workspace.Evaluate(kernel, shape.point_, directions, false, &xadCounts);
            const auto& adWork = pointwiseValue.Execution();
            const auto& bumpWork = centeredValue.native_.Execution();
            REQUIRE(dalCounts.recordings_ == adWork.recordings_ && dalCounts.sweeps_ == adWork.reverseSweeps_ &&
                        centeredCounts.recordings_ == bumpWork.gradientEvaluations_ && centeredCounts.sweeps_ == bumpWork.reverseSweeps_ &&
                        xadCounts.recordings_ == n && xadCounts.sweeps_ == n,
                    "AAD comparison unexpected diagnostic tape work");
            PrintCounts(shape, directions.Rows(), "pointwise", dalCounts, "dal_allocated_after_active_root");
            PrintCounts(shape, directions.Rows(), "centered", centeredCounts, "dal_allocated_after_active_root");
            PrintCounts(shape, directions.Rows(), "xad", xadCounts, "xad_logical_after_sweep");
        }
#endif
        if (!verifyOnly)
            MeasureTriplets(shape, directions.Rows(), pointwise, centered, xad, reverseOrder, diagnostics);
    }
#endif

    template <class K_> void Run(const Case_& shape, const K_& kernel, const Reference_& ref, const Options_& options) {
#if defined(DAL_TAPE_PERF_WITH_XAD)
        if (options.s0_ != S0_::NONE) {
            RunS0(shape, kernel, ref, options);
            return;
        }
        if (options.firstOrder_) {
            RunFirst(shape, kernel, ref, options.verifyOnly_, options.centeredFirst_, options.diagnostics_);
            return;
        }
#endif
        const NativeDirectionalFunction_ pointwiseKernel = std::cref(kernel);
        const NativeScalarFunction_ centeredKernel = std::cref(kernel);
        for (int count : {1, shape.directions_.Rows()}) {
            ForwardOverReverseRequest_ adRequest;
            adRequest.directions_ = Prefix(shape.directions_, count);
            BumpOverAADRequest_ bumpRequest;
            bumpRequest.directions_ = adRequest.directions_;
            bumpRequest.steps_ = Vector_<>(shape.steps_.begin(), shape.steps_.begin() + count);
            const auto pointwise = [&]() { return EvaluateForwardOverReverse(pointwiseKernel, shape.point_, adRequest); };
            const auto centered = [&]() { return EvaluateBumpOverAAD(centeredKernel, shape.point_, bumpRequest); };
            const auto centeredWithDirectional = [&]() {
                auto native = centered();
                auto directional = ProjectGradient(native.Gradient(), adRequest.directions_);
                return CenteredOutput_{std::move(native), std::move(directional)};
            };
#if defined(DAL_TAPE_PERF_WITH_XAD)
            if (options.withXad_) {
                RunWithXad(shape, ref, kernel, adRequest.directions_, pointwise, centeredWithDirectional, options.verifyOnly_, options.centeredFirst_,
                           options.diagnostics_);
                continue;
            }
#endif
            Clear(*Tape());
            const auto ad = pointwise();
            Clear(*Tape());
            if (shape.rollSteps_ != 0) {
                VerifyNativeRoutes(shape, ref, adRequest.directions_, ad, centeredWithDirectional());
                if (!options.verifyOnly_)
                    Measure(shape, count, pointwise, centeredWithDirectional, options.centeredFirst_);
                continue;
            }
            const auto bump = centered();
            Verify(shape, ref, count, ad, bump);
            if (!options.verifyOnly_)
                Measure(shape, count, pointwise, centered, options.centeredFirst_);
        }
    }

    void RunDupire(const Options_& options) {
        namespace D = TapePerfDupire;
        const bool s0 = options.s0_ != S0_::NONE;
        const Case_ shape{"dupire_quote",
                          D::QuoteSpreads(),
                          D::Directions(),
                          Vector_<>(3, D::QUOTE_STEP),
                          1,
                          s0 ? 1e-5 : 2e-2,
                          1e-1,
                          s0 ? 2e-7 : 2e-6,
                          s0 ? 2e-6 : 2e-4,
                          true,
                          D::TIME_STEPS};
        Reference_ ref{D::Reference::PRICE, Matrix_<>(shape.directions_.Rows(), shape.directions_.Cols()), D::Reference::REFINEMENT,
                       Vector_<>(D::Reference::VEGA.begin(), D::Reference::VEGA.end())};
        for (int row = 0; row < ref.products_.Rows(); ++row)
            for (int column = 0; column < ref.products_.Cols(); ++column)
                ref.products_(row, column) = D::Reference::PRODUCTS[row][column];
        const D::Kernel_ kernel;
        if (!s0 || options.verifyOnly_)
            D::Qualify(kernel, shape.point_, shape.directions_);
        if (s0 && options.verifyOnly_) {
            REQUIRE(D::Reference::REFINEMENT <= 2e-6, "S0 Dupire reference did not stabilize");
            std::cout << "s0_dupire_spread" << std::hexfloat;
            for (double spread : shape.point_)
                std::cout << ',' << spread;
            std::cout << std::defaultfloat << '\n';
        }
        if (options.firstOrder_ || s0) {
            Run(shape, kernel, ref, options);
            return;
        }
        const NativeScalarFunction_ scalarKernel = std::cref(kernel);
        std::cout << "# dupire_gradient_step,step,max_abs_hvp_error,volga,volga_abs_error\n"
                  << "# dupire_work,directions,route,calibrations,rollbacks,rollback_steps\n";
        for (double step : {2.0 * D::QUOTE_STEP, D::QUOTE_STEP, 0.5 * D::QUOTE_STEP}) {
            BumpOverAADRequest_ request;
            request.directions_ = shape.directions_;
            request.steps_ = Vector_<>(shape.directions_.Rows(), step);
            const auto result = EvaluateBumpOverAAD(scalarKernel, shape.point_, request);
            double maxError = 0.0;
            for (int row = 0; row < ref.products_.Rows(); ++row)
                for (int column = 0; column < ref.products_.Cols(); ++column) {
                    const double error = std::abs(ref.products_(row, column) - result.HessianProducts()(row, column));
                    REQUIRE(std::isfinite(error), "Dupire benchmark step study requires finite HVPs");
                    maxError = std::max(maxError, error);
                }
            const double volga = result.HessianProducts()(0, 3);
            std::cout << "dupire_gradient_step," << step << ',' << maxError << ',' << volga << ',' << std::abs(volga - ref.products_(0, 3)) << '\n';
        }
        Run(shape, kernel, ref, options);
    }

    int RunComparison(int argc, char** argv, bool firstOrder) {
        try {
            Options_ options;
            options.firstOrder_ = firstOrder;
            for (int arg = 1; arg < argc; ++arg) {
                const std::string option = argv[arg];
                if (option == "--verify-only" && !options.verifyOnly_)
                    options.verifyOnly_ = true;
                else if (option == "--centered-first" && !options.centeredFirst_)
                    options.centeredFirst_ = true;
                else if (option == "--with-xad" && !options.withXad_)
                    options.withXad_ = true;
                else if (option == "--dupire" && !options.dupire_)
                    options.dupire_ = true;
                else if (option == "--diagnostics" && !options.diagnostics_)
                    options.diagnostics_ = true;
                else if (option == "--s0" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::PAIR;
                else if (option == "--s0-aa" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::AA;
                else if (option == "--s0-dal-cold" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::DAL_COLD;
                else if (option == "--s0-dal-hot" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::DAL_HOT;
                else if (option == "--s0-xad-cold" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::XAD_COLD;
                else if (option == "--s0-xad-hot" && options.s0_ == S0_::NONE)
                    options.s0_ = S0_::XAD_HOT;
                else if (option.rfind("--s0-case=", 0) == 0 && options.s0Case_.empty())
                    options.s0Case_ = option.substr(10);
                else if (option.rfind("--s0-directions=", 0) == 0 && options.s0Directions_ == 0) {
                    const auto n = option.substr(16);
                    REQUIRE(n == "1" || n == "3" || n == "6", "expected S0 directions 1, 3, or 6");
                    options.s0Directions_ = std::stoi(n);
                } else
                    THROW("expected --first-order or --second-order [--dupire] [--verify-only] [--centered-first] [--with-xad] [--diagnostics] [--s0 "
                          "mode]");
            }
            const bool s0 = options.s0_ != S0_::NONE;
            REQUIRE(s0 || (options.s0Case_.empty() && options.s0Directions_ == 0), "S0 selectors require an S0 mode");
#if !defined(DAL_TAPE_PERF_WITH_XAD)
            REQUIRE(!options.withXad_ && !firstOrder && !s0, "XAD comparison requires DAL_CPP_XAD_BENCHMARK_SOURCE at configure time");
#else
            REQUIRE(!s0 || (!firstOrder && !options.diagnostics_ && !ProfilingAvailable()), "S0 requires second order, profiling OFF");
            const bool memory = S0Memory(options.s0_);
            REQUIRE(!memory || (!options.verifyOnly_ && !options.s0Case_.empty() && options.s0Directions_ != 0),
                    "S0 memory requires exactly one case and direction count");
            if (options.s0Case_ == "dupire_quote")
                options.dupire_ = true;
            if (s0 && (!options.s0Case_.empty() || options.s0Directions_ != 0)) {
                REQUIRE(options.s0Case_ == "dupire_quote" || options.s0Case_ == "black_scholes" || options.s0Case_ == "conditional_basket",
                        "unknown S0 case");
                REQUIRE(options.dupire_ == (options.s0Case_ == "dupire_quote") &&
                            (options.s0Directions_ <= 1 || options.s0Directions_ == (options.s0Case_ == "conditional_basket" ? 6 : 3)),
                        "S0 case and direction mismatch");
            }
#endif
            options.withXad_ = options.withXad_ || firstOrder || s0;
            REQUIRE(!options.diagnostics_ || (ProfilingAvailable() && options.withXad_),
                    "--diagnostics requires DAL_ENABLE_AAD_PROFILING=ON and XAD");
            RegisterAll_::Init();
            std::cout << std::setprecision(17);
#if defined(DAL_TAPE_PERF_WITH_XAD)
            if (s0 && options.verifyOnly_ && !options.dupire_)
                VerifySmoothNormal();
#endif
            if (options.diagnostics_ || firstOrder)
                std::cout << "# "
                             "tape_counts,case,directions,route,recordings,sweeps,stored_statements,stored_edges,statement_visits,edge_slots,bytes,"
                             "byte_kind\n";
            if (options.diagnostics_ && !options.verifyOnly_)
                std::cout
                    << "# phase_sample,case,directions,route,batch,calls_per_batch,price_record_ns,reverse_ns,setup_cleanup_ns,pack_ns,total_ns,"
                       "unattributed_ns\n";
            if (firstOrder)
                std::cout << "# first_output,case,route,kind,coordinate,reference,value\n"
                          << "# first_metric,case,route,price_error,gradient_max_abs_error\n"
                          << "# first_sample,case,round,pair,first,calls_per_batch,dal_ns,xad_ns\n";
            else
                std::cout
                    << "# hvp,case,directions,direction_index,coordinate_index,bump_step,reference,pointwise,centered\n"
                    << "# "
                       "accuracy,case,directions,reference_price,reference_stencil_max_disagreement,pointwise_max_abs_error,centered_max_abs_error\n"
                    << "# "
                       "work,case,directions,pointwise_recordings,pointwise_sweeps,pointwise_peak_bytes,centered_recordings,centered_sweeps,centered_"
                       "peak_bytes\n"
                    << "# sample,case,directions,round,pair,first,calls_per_batch,pointwise_ns_per_call,centered_ns_per_call\n"
                    << "# summary,case,directions,pairs,warm_tape_bytes,pointwise_median_ns,pointwise_p95_ns,pointwise_min_ns,"
                       "centered_median_ns,centered_p95_ns,centered_min_ns,pointwise_over_centered_median\n";
            if (!firstOrder && (options.withXad_ || options.dupire_))
                std::cout << "# metric,case,directions,route,price_error,gradient_max_abs_error,directional_max_abs_error,hvp_max_abs_error,"
                             "recordings,sweeps,tape_bytes,tape_byte_kind\n"
                          << "# output,case,directions,route,kind,row,column,reference,value\n"
                          << "# tri_sample,case,directions,round,pair,first,second,third,calls_per_batch,pointwise_ns,centered_ns,xad_ns\n"
                          << "# tri_summary,case,directions,route,median_ns,p95_ns,min_ns\n";
            if (options.dupire_) {
                RunDupire(options);
                return 0;
            }
            const Case_ small{"black_scholes", {103.0, 0.24}, Directions({-2.0, 0.5}), {0.1, 0.0005, 0.0005}, 256, 1e-10, 2.5e-5};
            const Case_ basket{"conditional_basket",
                               {100.0, 120.0, 0.20, 0.30, 0.40},
                               Directions({-2.0, 0.75, 0.20, -0.15, 0.35}),
                               {0.05, 0.05, 0.0002, 0.0002, 0.0002, 0.0005},
                               16,
                               2e-6,
                               5e-5};
            const Vector_<> coordinateSteps{0.2, 0.2, 0.002, 0.002, 0.002};
            const auto bsRef = BlackScholesReference(small);
            if (s0) {
                Run(small, BlackScholes_<true>{}, bsRef, options);
                Run(basket, MakeBasket<true>(basket, coordinateSteps), S0BasketReference(basket), options);
            } else {
                const auto conditional = MakeBasket<false>(basket, coordinateSteps);
                Run(small, BlackScholes_<false>{}, bsRef, options);
                Run(basket, conditional, BasketReference(conditional, basket, coordinateSteps), options);
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 2;
        }
    }
} // namespace

int RunSecondOrderBenchmarks(int argc, char** argv) { return RunComparison(argc, argv, false); }
int RunFirstOrderBenchmarks(int argc, char** argv) { return RunComparison(argc, argv, true); }
