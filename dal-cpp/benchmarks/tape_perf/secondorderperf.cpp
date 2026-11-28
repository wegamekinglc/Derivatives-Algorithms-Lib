#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <functional>
#include <future>
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
#include <dal/math/aad/sharedsecondorder.hpp>
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/analytics/detail/conditionalbasketcallrisk.hpp>
#include <dal/math/analytics/detail/spotcallrisk.hpp>
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
        bool s0Shared_ = false;
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

        template <bool Trace_ = false, class T_>
        T_ Evaluate(const Vector_<T_>& x, ConditionalBasketDetail::FixedConditionalBasketCall_::Trace_* trace = nullptr) const {
            // One-year maturity with both dividend yields zero.
            const T_ driftA = BASKET_RATE - 0.5 * x[2] * x[2];
            const T_ projectionB = x[3] * x[4];
            const T_ driftB = BASKET_RATE - 0.5 * projectionB * projectionB;
            const T_ stdDev = x[3] * sqrt(1.0 - x[4] * x[4]);
            T_ sum(0.0);
            for (size_t i = 0; i < nodes_.size(); ++i) {
                const auto& node = nodes_[i];
                const T_ strike = 2.0 * BASKET_STRIKE - x[0] * exp(driftA + x[2] * node.z_);
                const T_ forwardB = x[1] * exp(driftB + projectionB * node.z_);
                const T_ price = node.linear_ ? T_(forwardB - strike) : BlackCall<Smooth_>(forwardB, strike, stdDev);
                if constexpr (Trace_)
                    trace->prices_[i] = SecondOrderDetail::Access_::Value(price);
                sum += node.weight_ * price;
            }
            return 0.5 * std::exp(-BASKET_RATE) * sum;
        }

        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const { return Evaluate(x); }
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

    void VerifyProducts(
        const Case_& shape, const Reference_& ref, size_t n, const ForwardOverReverseResult_& pointwise, const BumpOverAADResult_& centered) {
        const auto& pointwiseWork = pointwise.Execution();
        const auto& centeredWork = centered.Execution();
        const auto& products = pointwise.HessianProducts();
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

    bool HasShape(const Matrix_<>& matrix, int rows, int cols) { return matrix.Rows() == rows && matrix.Cols() == cols; }

    void
    Verify(const Case_& shape, const Reference_& ref, int count, const ForwardOverReverseResult_& pointwise, const BumpOverAADResult_& centered) {
        const auto& pointwiseWork = pointwise.Execution();
        const auto& centeredWork = centered.Execution();
        const auto& products = pointwise.HessianProducts();
        const auto n = static_cast<size_t>(count);
        REQUIRE(HasShape(products, count, ref.products_.Cols()) && HasShape(centered.HessianProducts(), count, ref.products_.Cols()),
                "second-order benchmark unexpected HVP shape");
        REQUIRE(pointwiseWork.recordings_ == n && pointwiseWork.reverseSweeps_ == n + 1 && centeredWork.gradientEvaluations_ == 1 + 2 * n &&
                    centeredWork.reverseSweeps_ == 1 + 2 * n,
                "second-order benchmark unexpected tape work");
        REQUIRE(std::abs(ref.price_ - pointwise.Value()) <= shape.priceTolerance_ && std::abs(ref.price_ - centered.Value()) <= shape.priceTolerance_,
                "second-order benchmark price differs from independent reference");
        REQUIRE(std::abs(ref.products_(0, 1)) > 1e-6, "second-order benchmark single direction needs a nonzero cross term");
        VerifyProducts(shape, ref, n, pointwise, centered);
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

    bool SmoothNormalMatches(double value, double slope, double direction, double product, double expectedValue, double density, double hvp) {
        const auto near = [](double actual, double expected) { return std::abs(actual - expected) <= 2e-13 * std::abs(expected); };
        return near(value, expectedValue) && near(slope, density) && near(direction, density) && near(product, hvp);
    }

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
                REQUIRE(SmoothNormalMatches(native.Value(), native.Gradient()[0], native.DirectionalDerivatives()[0], native.HessianProducts()(0, 0),
                                            value, density, -z * density) &&
                            SmoothNormalMatches(other.risk_.price_, other.risk_.gradient_[0], other.risk_.directional_[0],
                                                other.risk_.products_(0, 0), value, density, -z * density),
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

    template <class R_, class F_> decltype(auto) VisitS0Risk(const R_& result, const F_& inspect) {
        if constexpr (std::is_same_v<R_, DupireQuoteSecondOrderResult_>) {
            if (const auto* shared = result.Risk().Shared())
                return inspect(*shared);
            return inspect(*result.Risk().Pointwise());
        } else {
            return inspect(result);
        }
    }

    template <class D_>
    void S0Allocation(const Case_& shape, const D_& evaluate, const ForwardOverReverseRequest_& request, S0_ mode,
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
                    const auto result = evaluate();
                    REQUIRE(!NativeThread::retainedTape, "cold second order initialized a first-order tape");
                    Bench::DoNotOptimize(&result);
                    observeResult();
                    return VisitS0Risk(result, [](const auto& risk) {
                        const auto& work = risk.Execution();
                        if constexpr (std::is_same_v<std::decay_t<decltype(risk)>, SharedSecondOrderResult_>)
                            return RiskMemory_{SharedSecondOrderDetail::LastLogicalPeak(), work.cleanupReserveBytes_, "shared_logical_tape_peak",
                                               work.peakTapeBytes_, work.retainedTapeBytes_};
                        else
                            return RiskMemory_{SecondOrderDetail::LastSecondOrderLogicalPeak(), work.cleanupReserveBytes_, "dal_logical_tape_peak",
                                               work.peakTapeBytes_, work.retainedTapeBytes_};
                    });
                });
            }
        });
    }

    template <class F_>
    void CheckS0Dal(
        const Case_& shape, const Reference_& ref, const ForwardOverReverseRequest_& request, const F_& dal, const SharedSecondOrderResult_& ad) {
        const auto& dw = ad.Execution();
        const auto n = static_cast<size_t>(request.directions_.Rows());
        const auto blocks = n / 3 + n % 3;
        REQUIRE(dw.callbackEvaluations_ == 1 && dw.sharedRecordings_ == 1 && dw.sharedTangentSweeps_ == blocks && dw.sharedReverseSweeps_ == blocks &&
                    !dw.replayDirections_ && !dw.pointwiseRecordings_,
                "S0 shared graph replayed or did unexpected work");
        const SecondOrderFunction_ pointwise = dal;
        REQUIRE(ad.Value() == EvaluateSecondOrder(pointwise, shape.point_, request.directions_).Value(),
                "S0 shared and pointwise price differs at the fixed point");
        VerifyRoute(shape, ref, request.directions_, "shared_s0", ad.Value(), ad.Gradient(), ad.DirectionalDerivatives(), ad.HessianProducts(),
                    shape.pointwiseTolerance_, {dw.sharedRecordings_, dw.sharedReverseSweeps_, dw.peakTapeBytes_, "shared_tape_capacity"});
    }

    template <class F_>
    void CheckS0Dal(const Case_& shape, const Reference_& ref, const ForwardOverReverseRequest_& request, const F_&, const SecondOrderResult_& ad) {
        const auto& dw = ad.Execution();
        const auto n = static_cast<size_t>(request.directions_.Rows());
        REQUIRE(dw.callbackEvaluations_ == n && dw.recordings_ == n && dw.reverseSweeps_ == n && dw.safePathActivations_ == 0,
                std::string("S0 pointwise execution fields do not match the fixed request. Protection: ") +
                    SecondOrderDetail::LastSecondOrderProtection());
        VerifyRoute(shape, ref, request.directions_, "dal_s0", ad.Value(), ad.Gradient(), ad.DirectionalDerivatives(), ad.HessianProducts(),
                    shape.pointwiseTolerance_, {dw.recordings_, dw.reverseSweeps_, dw.peakTapeBytes_, "dal_tape_capacity"});
    }

    template <class Work_> void CheckS0Xad(const Work_& work, size_t n, size_t payload) {
        REQUIRE(work.callbackEvaluations_ == n && work.recordings_ == n && work.reverseSweeps_ == n && work.numericPayloadBytes_ == payload &&
                    !work.peakTapeBytes_ && !work.cleanupReserveBytes_,
                "S0 XAD execution fields do not match the fixed request");
    }

    template <class F_, class E_>
    void VerifyS0Directions(const Case_& shape,
                            const Reference_& ref,
                            const F_& dal,
                            const E_& engine,
                            const ForwardOverReverseRequest_& request,
                            const TapePerfXad::OwnedFunction_& xad,
                            TapePerfXad::Workspace_& workspace) {
        const auto count = request.directions_.Rows();
        const auto withExpiredSources = [&](const auto& function, const auto& engine) {
            auto sourcePoint = shape.point_;
            auto sourceRequest = request;
            auto sourceFunction = function;
            auto result = engine(sourceFunction, sourcePoint, sourceRequest);
            sourcePoint.Fill(0.0);
            sourceRequest.directions_.Fill(0.0);
            return result;
        };
        const auto dalResult = withExpiredSources(dal, engine);
        const auto xv = withExpiredSources(xad, [&](const auto& f, const auto& p, const auto& r) { return workspace.EvaluateOwning(f, p, r); });
        const auto& xw = xv.execution_;
        const auto& expected = request.directions_;
        const auto sameDirections = [&](const Matrix_<>& actual) {
            return HasShape(actual, expected.Rows(), expected.Cols()) &&
                   std::equal(expected.Data(), expected.Data() + expected.Rows() * expected.Cols(), actual.Data());
        };
        VisitS0Risk(dalResult, [&](const auto& ad) {
            const auto& dw = ad.Execution();
            REQUIRE(ad.Point() == shape.point_ && xv.point_ == shape.point_ && sameDirections(ad.Directions()) && sameDirections(xv.directions_),
                    "S0 point and directions must remain owning after source destruction");
            const auto n = static_cast<size_t>(count);
            CheckS0Xad(xw, n, dw.numericPayloadBytes_);
            CheckS0Dal(shape, ref, request, dal, ad);
        });
        VerifyRoute(shape, ref, request.directions_, "xad_s0", xv.risk_.price_, xv.risk_.gradient_, xv.risk_.directional_, xv.risk_.products_,
                    shape.pointwiseTolerance_, {xw.recordings_, xw.reverseSweeps_, 0, "unavailable"});
    }

    template <class F_, class E_>
    void RunS0Directions(const Case_& shape,
                         const Reference_& ref,
                         const Options_& options,
                         int count,
                         const F_& dal,
                         const E_& engine,
                         const TapePerfXad::OwnedFunction_& xad) {
        ForwardOverReverseRequest_ request;
        request.directions_ = Prefix(shape.directions_, count);
        const auto native = [&] { return engine(dal, shape.point_, request); };
        if (S0Memory(options.s0_)) {
            S0Allocation(shape, native, request, options.s0_, xad);
            return;
        }
        TapePerfXad::Workspace_ workspace;
        const auto other = [&] { return workspace.EvaluateOwning(xad, shape.point_, request); };
        if (!options.verifyOnly_) {
            if (options.s0_ == S0_::AA) {
                TapePerfXad::Workspace_ alternate;
                const auto replica = [&] { return alternate.EvaluateOwning(xad, shape.point_, request); };
                MeasureS0(shape, count, other, replica, true, options.centeredFirst_);
            } else {
                MeasureS0(shape, count, native, other, false, options.centeredFirst_);
            }
            return;
        }
        VerifyS0Directions(shape, ref, dal, engine, request, xad, workspace);
    }

    template <class Reject_>
    void CheckCoreBasketCaps(const Case_& shape,
                             const ConditionalBasketDetail::FixedConditionalBasketCall_& plan,
                             const Matrix_<>& one,
                             const Matrix_<>& stress,
                             const Reject_& reject) {
        for (const auto* direction : {&one, static_cast<const Matrix_<>*>(&stress)}) {
            const auto val = [&](const SecondOrderOptions_& options = {}) { return plan.Evaluate(shape.point_, *direction, options); };
            const auto cold = std::async(std::launch::async, [&] { return val().Execution().peakTapeBytes_; }).get();
            const auto hot = std::async(std::launch::async, [&] {
                                 (void)reject([&] { (void)val({{}, cold - 1}); }, SecondOrderErrorReason_::Value_::BUDGET_EXCEEDED, 0);
                                 REQUIRE(val({{}, cold}).Execution().peakTapeBytes_ == cold, "fixed basket cold exact cap");
                                 const auto cap = val().Execution().peakTapeBytes_;
                                 REQUIRE(val({{}, cap}).Execution().peakTapeBytes_ == cap, "fixed basket hot exact cap");
                                 (void)reject([&] { (void)val({{}, cap - 1}); }, SecondOrderErrorReason_::Value_::BUDGET_EXCEEDED, 0);
                                 REQUIRE(val().Execution().peakTapeBytes_ == cold, "fixed basket failed cap did not release tape");
                                 return cap;
                             }).get();
            std::cout << "basket_core_cap," << (direction == &one ? "ordinary" : "protected") << ',' << cold << ',' << hot << '\n';
        }
    }

    template <class SameBits_>
    Matrix_<> CheckCoreBasketReplay(const Case_& shape,
                                    const ConditionalBasket_<true>& kernel,
                                    const ConditionalBasketDetail::FixedConditionalBasketCall_& plan,
                                    const SameBits_& sameBits) {
        Matrix_<> stress(1, 5, 0.0);
        stress(0, 0) = 0x1p40;
        const auto checkReplay = [&](const Matrix_<>& direction, const char* reason) {
            const auto wide = EvaluateSecondOrder(
                [&](const auto& x) {
                    if (!SecondOrderDetail::activeArena->wide_)
                        SecondOrderDetail::Promote("force original wide basket for verification");
                    return kernel(nullptr, x);
                },
                shape.point_, direction);
            const auto protectedResult = plan.Evaluate(shape.point_, direction);
            const auto& work = protectedResult.Execution();
            REQUIRE(work.method_ == "NativeBasketScalarReplay" && work.recordings_ == 2 && work.reverseSweeps_ == 1 && work.safePathActivations_ == 1,
                    "fixed basket protection must replay the scalar kernel");
            REQUIRE(sameBits(wide.Value(), protectedResult.Value()) && wide.Gradient() == protectedResult.Gradient() &&
                        wide.DirectionalDerivatives() == protectedResult.DirectionalDerivatives() &&
                        std::equal(wide.HessianProducts().begin(), wide.HessianProducts().end(), protectedResult.HessianProducts().begin()),
                    "fixed basket replay must equal the original wide scalar output");
            REQUIRE(std::strcmp(SecondOrderDetail::LastSecondOrderProtection(), reason) == 0, "fixed basket replay reason");
            std::cout << "basket_core_protection," << reason << ',' << work.recordings_ << ',' << work.reverseSweeps_ << ','
                      << work.safePathActivations_ << '\n';
        };
        checkReplay(stress, "Black local derivative range");
        Matrix_<> cancel(1, 5, 0.0);
        cancel(0, 0) = -shape.point_[0] * (kernel.nodes_[0].z_ - shape.point_[2]);
        cancel(0, 2) = 1.0;
        checkReplay(cancel, "Black local derivative cancellation");
        return stress;
    }

    void CheckCoreBasketProducts(const Case_& shape, const Reference_& ref, const ConditionalBasketDetail::FixedConditionalBasketCall_& plan) {
        double worst = 0.0;
        for (int row = 0; row < shape.directions_.Rows(); ++row) {
            Matrix_<> direction(1, 5);
            std::copy(shape.directions_.Row(row).begin(), shape.directions_.Row(row).end(), direction.Row(0).begin());
            const auto result = plan.Evaluate(shape.point_, direction);
            REQUIRE(result.Execution().method_ == "NativeBasketComposedAAD" && !result.Execution().safePathActivations_, "fixed basket route");
            double slope = 0.0;
            for (int col = 0; col < 5; ++col) {
                slope += direction(0, col) * ref.gradient_[col];
                REQUIRE(std::abs(result.Gradient()[col] - ref.gradient_[col]) <= 2e-11, "fixed basket full Vega and correlation gradient");
                worst = std::max(worst, std::abs(result.HessianProducts()(0, col) - ref.products_(row, col)));
                std::cout << "basket_core_hvp," << row << ',' << col << ',' << result.HessianProducts()(0, col) << ',' << ref.products_(row, col)
                          << '\n';
            }
            REQUIRE(std::abs(result.DirectionalDerivatives()[0] - slope) <= 2e-11, "fixed basket directional derivative");
        }
        REQUIRE(worst <= 2e-11, "fixed basket full HVP");
        std::cout << "basket_core_hvp_error," << worst << '\n';
    }

    void CheckCoreBasket(const Case_& shape, const Reference_& ref, const ConditionalBasket_<true>& kernel,
                         const ConditionalBasketDetail::FixedConditionalBasketCall_& plan) {
        using Plan = ConditionalBasketDetail::FixedConditionalBasketCall_;
        const auto sameBits = [](double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; };
        const auto before = SecondOrderDetail::LastSecondOrderLogicalPeak();
        const auto reject = [](const auto& run, SecondOrderErrorReason_::Value_ reason, std::optional<size_t> row) {
            try {
                run();
            } catch (const SecondOrderError_& error) {
                REQUIRE(error.Reason().Switch() == reason && error.DirectionIndex() == row, "fixed basket error type or direction");
                return error.Stage();
            }
            THROW("expected fixed basket request to fail");
        };
        auto different = shape.point_;
        different[0] = std::nextafter(different[0], std::numeric_limits<double>::infinity());
        const auto one = Prefix(shape.directions_, 1);
        REQUIRE(reject([&] { (void)plan.Evaluate(different, one); }, SecondOrderErrorReason_::Value_::OUTSIDE_DOMAIN, {}) ==
                    SecondOrderStage_::Value_::VALIDATION, "uncertified basket point must fail before recording");
        REQUIRE(reject([&] { (void)plan.Evaluate(shape.point_, shape.directions_); }, SecondOrderErrorReason_::Value_::INVALID_INPUT, {}) ==
                    SecondOrderStage_::Value_::VALIDATION, "only one direction is eligible for the private basket plan");
        REQUIRE(SecondOrderDetail::LastSecondOrderLogicalPeak() == before, "basket validation changed the active ledger");

        Plan::Trace_ original{}, composed{};
        const auto originalPrice = EvaluateSecondOrder([&](const auto& x) { return kernel.template Evaluate<true>(x, &original); },
                                                       shape.point_, one);
        const auto core = plan.Evaluate(shape.point_, one, {}, &composed);
        REQUIRE(sameBits(originalPrice.Value(), core.Value()), "fixed basket primal bits");
        REQUIRE(composed.statements_ == 53 && composed.edges_ == 377, "fixed basket core graph size");
        std::cout << "basket_core_graph," << composed.statements_ << ',' << composed.edges_ << '\n';
        for (size_t i = 0; i < original.prices_.size(); ++i) {
            REQUIRE(sameBits(original.prices_[i], composed.prices_[i]), "fixed basket per-node primal bits");
            std::cout << "basket_core_node," << i << ',' << kernel.nodes_[i].linear_ << ',' << std::hexfloat << original.prices_[i] << ','
                      << composed.prices_[i] << std::defaultfloat << '\n';
        }

        CheckCoreBasketProducts(shape, ref, plan);
        const auto stress = CheckCoreBasketReplay(shape, kernel, plan, sameBits);
        CheckCoreBasketCaps(shape, plan, one, stress, reject);
    }

    void RunS0Basket(const Case_& shape,
                     const ConditionalBasket_<true>& kernel,
                     const Reference_& ref,
                     const Options_& options,
                     const SecondOrderFunction_& dal,
                     const TapePerfXad::OwnedFunction_& xad) {
        ConditionalBasketDetail::FixedConditionalBasketCall_::Nodes_ nodes{};
        REQUIRE(kernel.nodes_.size() == nodes.size(), "expected the frozen conditional basket quadrature");
        std::transform(kernel.nodes_.begin(), kernel.nodes_.end(), nodes.begin(),
                       [](const auto& node) { return ConditionalBasketDetail::Node_{node.z_, node.weight_, node.linear_}; });
        const ConditionalBasketDetail::FixedConditionalBasketCall_ plan(BASKET_STRIKE, BASKET_RATE, nodes, shape.point_);
        if (options.verifyOnly_)
            CheckCoreBasket(shape, ref, kernel, plan);
        const auto evaluate = [&](const auto&, const auto& point, const auto& request) { return plan.Evaluate(point, request.directions_); };
        RunS0Directions(shape, ref, options, 1, dal, evaluate, xad);
    }

    bool SelectDirections(const Options_& options, int count) { return !options.s0Directions_ || options.s0Directions_ == count; }

    void RunS0Spot(
        const Case_& shape, const Reference_& ref, const Options_& options, const SecondOrderFunction_& dal, const TapePerfXad::OwnedFunction_& xad) {
        const SpotCallDetail::FixedSpotCall_ plan(BS_STRIKE, BS_TIME, BS_RATE, BS_DIVIDEND);
        const auto evaluate = [&](const auto&, const auto& point, const auto& request) { return plan.Evaluate(point, request.directions_); };
        for (int count : {1, shape.directions_.Rows()})
            if (SelectDirections(options, count))
                RunS0Directions(shape, ref, options, count, dal, evaluate, xad);
    }

    template <class K_> constexpr bool SupportsS0Shared = std::is_same_v<K_, ConditionalBasket_<true>> || std::is_same_v<K_, TapePerfDupire::Kernel_>;

    bool UseS0Shared(const Options_& options, int count) { return options.s0Shared_ && count >= 2; }

    template <class K_, class F_>
    void RunS0OtherDirections(const Case_& shape,
                              const K_& kernel,
                              const Reference_& ref,
                              const Options_& options,
                              const F_& scalar,
                              const SecondOrderFunction_& dal,
                              const TapePerfXad::OwnedFunction_& xad) {
        for (int count : {1, shape.directions_.Rows()}) {
            if (!SelectDirections(options, count))
                continue;
            if constexpr (std::is_same_v<K_, ConditionalBasket_<true>>) {
                if (count == 1) {
                    RunS0Basket(shape, kernel, ref, options, dal, xad);
                    continue;
                }
            }
            if constexpr (SupportsS0Shared<K_>) {
                if (UseS0Shared(options, count)) {
                    RunS0Directions(shape, ref, options, count, scalar,
                                    [](const auto& f, const auto& p, const auto& r) { return EvaluateSharedSecondOrder(f, p, r.directions_); },
                                    xad);
                    continue;
                }
            }
            RunS0Directions(shape, ref, options, count, dal,
                            [](const auto& f, const auto& p, const auto& r) { return EvaluateSecondOrder(f, p, r.directions_); }, xad);
        }
    }

    template <class K_> void RunS0(const Case_& shape, const K_& kernel, const Reference_& ref, const Options_& options) {
        if (!options.s0Case_.empty() && options.s0Case_ != shape.name_)
            return;
        const auto scalar = [plan = &kernel](const auto& x) { return (*plan)(nullptr, x); };
        const SecondOrderFunction_ dal = scalar;
        const TapePerfXad::OwnedFunction_ xad = [plan = &kernel](RecordingScope_* scope, const auto& x) { return (*plan)(scope, x); };
        if constexpr (std::is_same_v<K_, BlackScholes_<true>>) {
            RunS0Spot(shape, ref, options, dal, xad);
            return;
        }
        RunS0OtherDirections(shape, kernel, ref, options, scalar, dal, xad);
    }

    void RunS0DupireProduct(const Case_& shape, const Reference_& reference, const Options_& options,
                            const DupireQuoteSecondOrderPlan_& plan) {
        if (!options.s0Case_.empty() && options.s0Case_ != shape.name_)
            return;
        const auto& kernel = DupireSecondOrderDetail::PlanAccess_::Kernel(plan);
        const auto scalar = [plan = &kernel](const auto& x) { return (*plan)(nullptr, x); };
        const TapePerfXad::OwnedFunction_ xad = [plan = &kernel](RecordingScope_* scope, const auto& x) { return (*plan)(scope, x); };
        const auto axis = plan.InputAxis();
        const auto evaluate = [&](const auto&, const auto&, const auto& request) {
            const auto result = EvaluateDupireQuoteSecondOrder(plan, axis, request.directions_);
            if (options.verifyOnly_) {
                REQUIRE(result.Risk().Value() == DupireSecondOrderDetail::PlanAccess_::Margins(plan).price_,
                        "public Dupire price differs from the certified binary64 price");
            }
            return result;
        };
        if (options.verifyOnly_) {
            const auto& m = DupireSecondOrderDetail::PlanAccess_::Margins(plan);
            std::cout << "# dupire_fixed_certificate,iv,stddev,ckk,numerator,variance,left_boundary,right_boundary,dominance,pivot,"
                         "pivot_lower,normalized_margin,price\n";
            std::cout << "dupire_fixed_certificate," << m.ivLower_ << ',' << m.stdDevLower_ << ',' << m.ckkLower_ << ','
                      << m.numeratorLower_ << ',' << m.varianceLower_ << ',' << m.leftBoundary_ << ',' << m.rightBoundary_ << ','
                      << m.diagonalMargin_ << ',' << m.pivot_ << ',' << m.pivotLower_ << ',' << m.normalizedMargin_ << ',' << m.price_ << '\n';
        }
        for (int count : {1, shape.directions_.Rows()})
            if (SelectDirections(options, count))
                RunS0Directions(shape, reference, options, count, scalar, evaluate, xad);
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

    template <class D_, class X_, class DT_, class XT_>
    void ProfileFirst(const Case_& shape, bool reverseOrder, const D_& dal, const X_& xad, const DT_& dalTwice, const XT_& xadTwice) {
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
    }

    template <class R_> bool SameFirstRisk(const R_& actual, const R_& expected) {
        return actual.price_ == expected.price_ && actual.gradient_ == expected.gradient_;
    }

    bool SameGraphSize(const Counts_& before, const Counts_& replay) {
        return before.statements_ == replay.statements_ && before.edges_ == replay.edges_;
    }

    template <class K_, class D_, class X_, class DR_, class XR_>
    void DiagnoseFirst(const Case_& shape,
                       const K_& kernel,
                       TapePerfXad::FirstOrderWorkspace_& workspace,
                       bool reverseOrder,
                       const D_& dal,
                       const X_& xad,
                       const DR_& dalCheck,
                       const XR_& xadCheck,
                       const Counts_& dalCounts,
                       const Counts_& xadCounts) {
        Counts_ dalTwiceCounts, xadTwiceCounts;
        const auto dalTwice = [&]() { return TapePerfComparison::DalFirst(kernel, shape.point_, nullptr, 2); };
        const auto xadTwice = [&]() { return workspace.Evaluate(kernel, shape.point_, nullptr, 2); };
        const auto dalRepeated = TapePerfComparison::DalFirst(kernel, shape.point_, &dalTwiceCounts, 2);
        const auto xadRepeated = workspace.Evaluate(kernel, shape.point_, &xadTwiceCounts, 2);
        REQUIRE(SameFirstRisk(dalRepeated, dalCheck) && SameFirstRisk(xadRepeated, xadCheck), "AAD comparison repeat sweep changed its result");
        REQUIRE(SameGraphSize(dalCounts, dalTwiceCounts) && SameGraphSize(xadCounts, xadTwiceCounts),
                "AAD comparison repeat sweep changed recording size");
        PrintCounts(shape, 0, "dal_repeat", dalTwiceCounts, "dal_allocated_peak");
        PrintCounts(shape, 0, "xad_repeat", xadTwiceCounts, "xad_logical_after_sweep");
        ProfileFirst(shape, reverseOrder, dal, xad, dalTwice, xadTwice);
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
            DiagnoseFirst(shape, kernel, workspace, reverseOrder, dal, xad, dalCheck, xadCheck, dalCounts, xadCounts);
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

    template <class Order_>
    void PrintTriplets(const Case_& shape,
                       int directions,
                       size_t initialCapacity,
                       const std::array<Samples_, 3>& samples,
                       const std::array<const char*, 3>& NAMES,
                       const Order_& order) {
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

    std::array<size_t, 3> TripletOrder(size_t index, bool reverseOrder) {
        const size_t first = index / 2 % 3, second = (first + 1) % 3, third = (first + 2) % 3;
        return (reverseOrder != (index % 2 != 0)) ? std::array{third, second, first} : std::array{first, second, third};
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
        const auto order = [reverseOrder](size_t index) { return TripletOrder(index, reverseOrder); };
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
        PrintTriplets(shape, directions, initialCapacity, samples, NAMES, order);
    }

#if defined(DAL_ENABLE_AAD_PROFILING)
    template <class K_, class P_, class B_>
    void DiagnoseSecond(const Case_& shape,
                        const K_& kernel,
                        const Matrix_<>& directions,
                        const P_& pointwise,
                        const B_& centered,
                        TapePerfXad::Workspace_& workspace,
                        const ForwardOverReverseResult_& pointwiseValue,
                        const CenteredOutput_& centeredValue) {
        const auto n = static_cast<size_t>(directions.Rows());
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
        if (diagnostics)
            DiagnoseSecond(shape, kernel, directions, pointwise, centered, workspace, pointwiseValue, centeredValue);
#endif
        if (!verifyOnly)
            MeasureTriplets(shape, directions.Rows(), pointwise, centered, xad, reverseOrder, diagnostics);
    }
#endif

    template <class K_> void RunLegacy(const Case_& shape, const K_& kernel, const Reference_& ref, const Options_& options) {
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
        RunLegacy(shape, kernel, ref, options);
    }

    void CheckDupireGradientSteps(const Case_& shape, const Reference_& ref, const TapePerfDupire::Kernel_& kernel) {
        namespace D = TapePerfDupire;
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
    }

    Reference_ DupireReference(const Case_& shape) {
        namespace D = TapePerfDupire;
        Reference_ ref{D::Reference::PRICE, Matrix_<>(shape.directions_.Rows(), shape.directions_.Cols()), D::Reference::REFINEMENT,
                       Vector_<>(D::Reference::VEGA.begin(), D::Reference::VEGA.end())};
        for (int row = 0; row < ref.products_.Rows(); ++row)
            for (int column = 0; column < ref.products_.Cols(); ++column)
                ref.products_(row, column) = D::Reference::PRODUCTS[row][column];
        return ref;
    }

    void VerifyDupireSetup(const Case_& shape, const TapePerfDupire::Kernel_& kernel, bool s0, bool verifyOnly) {
        namespace D = TapePerfDupire;
        if (!s0 || verifyOnly)
            D::Qualify(kernel, shape.point_, shape.directions_);
        if (s0 && verifyOnly) {
            REQUIRE(D::Reference::REFINEMENT <= 2e-6, "S0 Dupire reference did not stabilize");
            std::cout << "s0_dupire_spread" << std::hexfloat;
            for (double spread : shape.point_)
                std::cout << ',' << spread;
            std::cout << std::defaultfloat << '\n';
        }
    }

    bool CheckLegacyDupireSteps(const Options_& options, bool s0) { return !options.firstOrder_ && !s0; }

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
        const auto ref = DupireReference(shape);
        const D::Kernel_ kernel;
        VerifyDupireSetup(shape, kernel, s0, options.verifyOnly_);
#if defined(DAL_TAPE_PERF_WITH_XAD)
        if (s0 && options.s0Shared_) {
            const auto plan = PrepareDupireEuropeanCallQuoteRisk(D::Snapshot(shape.point_), {D::STRIKE, 1.0, "EQ", Ccy_("USD")},
                                                                 {D::HALF_WIDTH, D::SPACE_NODES, D::TIME_STEPS});
            RunS0DupireProduct(shape, ref, options, plan);
            return;
        }
#endif
        if (CheckLegacyDupireSteps(options, s0))
            CheckDupireGradientSteps(shape, ref, kernel);
        Run(shape, kernel, ref, options);
    }

    bool ReadFlag(Options_& options, const std::string& option) {
        using Flag = std::pair<const char*, bool Options_::*>;
        static constexpr std::array flags{Flag{"--verify-only", &Options_::verifyOnly_}, Flag{"--centered-first", &Options_::centeredFirst_},
                                          Flag{"--with-xad", &Options_::withXad_}, Flag{"--dupire", &Options_::dupire_},
                                          Flag{"--diagnostics", &Options_::diagnostics_}};
        for (const auto& [name, flag] : flags) {
            if (option == name && !(options.*flag)) {
                options.*flag = true;
                return true;
            }
        }
        return false;
    }

    bool ReadS0Mode(Options_& options, const std::string& option) {
        if (options.s0_ != S0_::NONE)
            return false;
        using Mode = std::pair<const char*, S0_>;
        static constexpr std::array modes{Mode{"--s0", S0_::PAIR},
                                          Mode{"--s0-aa", S0_::AA},
                                          Mode{"--s0-dal-cold", S0_::DAL_COLD},
                                          Mode{"--s0-dal-hot", S0_::DAL_HOT},
                                          Mode{"--s0-xad-cold", S0_::XAD_COLD},
                                          Mode{"--s0-xad-hot", S0_::XAD_HOT}};
        for (const auto& [name, mode] : modes) {
            if (option == name) {
                options.s0_ = mode;
                return true;
            }
        }
        return false;
    }

    bool ReadS0Selector(Options_& options, const std::string& option) {
        if (option == "--s0-shared") {
            options.s0Shared_ = true;
        } else if (option.rfind("--s0-case=", 0) == 0 && options.s0Case_.empty()) {
            options.s0Case_ = option.substr(10);
        } else if (option.rfind("--s0-directions=", 0) == 0 && options.s0Directions_ == 0) {
            const auto n = option.substr(16);
            REQUIRE(n == "1" || n == "3" || n == "6", "expected S0 directions 1, 3, or 6");
            options.s0Directions_ = std::stoi(n);
        } else {
            return false;
        }
        return true;
    }

    Options_ ReadComparisonOptions(int argc, char** argv, bool firstOrder) {
        Options_ options;
        options.firstOrder_ = firstOrder;
        for (int arg = 1; arg < argc; ++arg) {
            const std::string option = argv[arg];
            if (!ReadFlag(options, option) && !ReadS0Mode(options, option) && !ReadS0Selector(options, option))
                THROW("expected --first-order or --second-order [--dupire] [--verify-only] [--centered-first] [--with-xad] [--diagnostics] [--s0 "
                      "mode]");
        }
        return options;
    }

#if defined(DAL_TAPE_PERF_WITH_XAD)
    void ValidateS0Case(Options_& options, bool selected) {
        const auto& name = options.s0Case_;
        if (name == "dupire_quote")
            options.dupire_ = true;
        if (!selected)
            return;
        REQUIRE(name == "dupire_quote" || name == "black_scholes" || name == "conditional_basket", "unknown S0 case");
        const auto allowed = name == "conditional_basket" ? 6 : 3;
        REQUIRE(options.dupire_ == (name == "dupire_quote") && (options.s0Directions_ <= 1 || options.s0Directions_ == allowed),
                "S0 case and direction mismatch");
    }

    void ValidateS0Timing(const Options_& options, bool s0) {
        REQUIRE(!s0 || (!options.firstOrder_ && !options.diagnostics_ && !ProfilingAvailable()), "S0 requires second order, profiling OFF");
    }

    void ValidateS0Options(Options_& options, bool s0) {
        ValidateS0Timing(options, s0);
        const bool complete = !options.verifyOnly_ && !options.s0Case_.empty() && options.s0Directions_ != 0;
        REQUIRE(!S0Memory(options.s0_) || complete, "S0 memory requires exactly one case and direction count");
        ValidateS0Case(options, s0 && (!options.s0Case_.empty() || options.s0Directions_ != 0));
    }
#endif

    bool HasS0Selectors(const Options_& options) { return !options.s0Case_.empty() || options.s0Directions_ != 0 || options.s0Shared_; }

    void ValidateDiagnostics(const Options_& options) {
        const bool canDiagnose = ProfilingAvailable() && options.withXad_;
        REQUIRE(!options.diagnostics_ || canDiagnose, "--diagnostics requires DAL_ENABLE_AAD_PROFILING=ON and XAD");
    }

    void ValidateComparisonOptions(Options_& options, bool s0) {
        REQUIRE(s0 || !HasS0Selectors(options), "S0 selectors require an S0 mode");
#if !defined(DAL_TAPE_PERF_WITH_XAD)
        REQUIRE(!options.withXad_ && !options.firstOrder_ && !s0, "XAD comparison requires DAL_CPP_XAD_BENCHMARK_SOURCE at configure time");
#else
        ValidateS0Options(options, s0);
#endif
        options.withXad_ = options.withXad_ || options.firstOrder_ || s0;
        ValidateDiagnostics(options);
    }

    void PrintRiskHeaders(bool firstOrder) {
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
    }

    void PrintComparisonHeaders(const Options_& options) {
        const bool firstOrder = options.firstOrder_;
        if (options.diagnostics_ || firstOrder)
            std::cout << "# "
                         "tape_counts,case,directions,route,recordings,sweeps,stored_statements,stored_edges,statement_visits,edge_slots,bytes,"
                         "byte_kind\n";
        if (options.diagnostics_ && !options.verifyOnly_)
            std::cout << "# phase_sample,case,directions,route,batch,calls_per_batch,price_record_ns,reverse_ns,setup_cleanup_ns,pack_ns,total_ns,"
                         "unattributed_ns\n";
        PrintRiskHeaders(firstOrder);
        if (!firstOrder && (options.withXad_ || options.dupire_))
            std::cout << "# metric,case,directions,route,price_error,gradient_max_abs_error,directional_max_abs_error,hvp_max_abs_error,"
                         "recordings,sweeps,tape_bytes,tape_byte_kind\n"
                      << "# output,case,directions,route,kind,row,column,reference,value\n"
                      << "# tri_sample,case,directions,round,pair,first,second,third,calls_per_batch,pointwise_ns,centered_ns,xad_ns\n"
                      << "# tri_summary,case,directions,route,median_ns,p95_ns,min_ns\n";
    }

    void RunVanillaCases(const Options_& options, bool s0) {
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
    }

    int RunComparison(int argc, char** argv, bool firstOrder) {
        try {
            auto options = ReadComparisonOptions(argc, argv, firstOrder);
            const bool s0 = options.s0_ != S0_::NONE;
            ValidateComparisonOptions(options, s0);
            RegisterAll_::Init();
            std::cout << std::setprecision(17);
#if defined(DAL_TAPE_PERF_WITH_XAD)
            if (s0 && options.verifyOnly_ && !options.dupire_)
                VerifySmoothNormal();
#endif
            PrintComparisonHeaders(options);
            if (options.dupire_) {
                RunDupire(options);
                return 0;
            }
            RunVanillaCases(options, s0);
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 2;
        }
    }
} // namespace

int RunSecondOrderBenchmarks(int argc, char** argv) { return RunComparison(argc, argv, false); }
int RunFirstOrderBenchmarks(int argc, char** argv) { return RunComparison(argc, argv, true); }
