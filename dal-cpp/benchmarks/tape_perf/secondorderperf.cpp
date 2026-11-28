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
#include <dal/math/aad/tapecapacity.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/platform/platform.hpp>

#include "dupirereference.hpp"
#include "dupiresecondorder.hpp"
#include "secondorderperf.hpp"
#if defined(DAL_TAPE_PERF_WITH_XAD)
#include "secondorderxad.hpp"
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

    template <class T_> T_ BlackCall(const T_& forward, const T_& strike, const T_& stdDev) {
        const T_ d1 = log(forward / strike) / stdDev + 0.5 * stdDev;
        const T_ d2 = d1 - stdDev;
        return forward * NCDF(d1) - strike * NCDF(d2);
    }

    struct BlackScholes_ {
        template <class T_> T_ operator()(RecordingScope_*, const Vector_<T_>& x) const {
            const double rootTime = std::sqrt(BS_TIME);
            const auto d1 = (log(x[0] / BS_STRIKE) + (BS_RATE - BS_DIVIDEND + 0.5 * x[1] * x[1]) * BS_TIME) / (x[1] * rootTime);
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

    struct ConditionalBasket_ {
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
                const T_ price = node.linear_ ? T_(forwardB - strike) : BlackCall(forwardB, strike, stdDev);
                sum += node.weight_ * price;
            }
            return 0.5 * std::exp(-BASKET_RATE) * sum;
        }
    };

    ConditionalBasket_ MakeBasket(const Case_& shape, const Vector_<>& coordinateSteps) {
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
        ConditionalBasket_ result;
        result.nodes_.reserve(BASKET_NODES);
        for (int i = 0; i < BASKET_NODES; ++i) {
            const double z = quadrature.Abscissa()[i];
            // Bound the stock over the entire box before fixing the node's payoff branch.
            const double projectionLow = volLow * z, projectionHigh = volHigh * z;
            const auto projection = std::minmax(projectionLow, projectionHigh);
            const double stockLow = (x[0] - radius[0]) * std::exp(BASKET_RATE - 0.5 * volHigh * volHigh + projection.first);
            const double stockHigh = (x[0] + radius[0]) * std::exp(BASKET_RATE - 0.5 * volLow * volLow + projection.second);
            REQUIRE(stockHigh < 2.0 * BASKET_STRIKE || stockLow > 2.0 * BASKET_STRIKE,
                    "second-order basket stencil crosses a conditional strike zero");
            result.nodes_.push_back({z, quadrature.Weight()[i], stockLow > 2.0 * BASKET_STRIKE});
        }
        return result;
    }

    double BasketScalarPrice(const ConditionalBasket_& basket, const Vector_<>& x) {
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

    double MixedPriceDifference(const ConditionalBasket_& basket, const Case_& shape, int row, int column, double h, double k) {
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

    Reference_ BasketReference(const ConditionalBasket_& basket, const Case_& shape, const Vector_<>& coordinateSteps) {
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
    template <class P_, class B_, class X_>
    void MeasureTriplets(const Case_& shape, int directions, const P_& pointwise, const B_& centered, const X_& xad, bool reverseOrder) {
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
        const auto measure = [&](size_t route) {
            if (route == 0)
                return TimeBatch(pointwise, shape.callsPerBatch_);
            if (route == 1)
                return TimeBatch(centered, shape.callsPerBatch_);
            return TimeBatch(xad, shape.callsPerBatch_);
        };
        for (size_t i = 0; i < samples[0].size(); ++i)
            for (size_t route : order(i))
                samples[route][i] = measure(route);
        REQUIRE(TapeCapacity() == initialCapacity, "second-order benchmark tape grew after warmup");
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
                    bool reverseOrder) {
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
        if (!verifyOnly)
            MeasureTriplets(shape, directions.Rows(), pointwise, centered, xad, reverseOrder);
    }
#endif

    template <class K_>
    void Run(const Case_& shape, const K_& kernel, const Reference_& ref, bool verifyOnly, bool centeredFirst, [[maybe_unused]] bool withXad) {
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
            if (withXad) {
                RunWithXad(shape, ref, kernel, adRequest.directions_, pointwise, centeredWithDirectional, verifyOnly, centeredFirst);
                continue;
            }
#endif
            Clear(*Tape());
            const auto ad = pointwise();
            Clear(*Tape());
            if (shape.rollSteps_ != 0) {
                VerifyNativeRoutes(shape, ref, adRequest.directions_, ad, centeredWithDirectional());
                if (!verifyOnly)
                    Measure(shape, count, pointwise, centeredWithDirectional, centeredFirst);
                continue;
            }
            const auto bump = centered();
            Verify(shape, ref, count, ad, bump);
            if (!verifyOnly)
                Measure(shape, count, pointwise, centered, centeredFirst);
        }
    }

    void RunDupire(bool verifyOnly, bool centeredFirst, bool withXad) {
        namespace D = TapePerfDupire;
        const Case_ shape{"dupire_quote", D::QuoteSpreads(), D::Directions(), Vector_<>(3, D::QUOTE_STEP), 1, 2e-2, 1e-1, 2e-6, 2e-4,
                          true,           D::TIME_STEPS};
        Reference_ ref{D::Reference::PRICE, Matrix_<>(shape.directions_.Rows(), shape.directions_.Cols()), D::Reference::REFINEMENT,
                       Vector_<>(D::Reference::VEGA.begin(), D::Reference::VEGA.end())};
        for (int row = 0; row < ref.products_.Rows(); ++row)
            for (int column = 0; column < ref.products_.Cols(); ++column)
                ref.products_(row, column) = D::Reference::PRODUCTS[row][column];
        const D::Kernel_ kernel;
        D::Qualify(kernel, shape.point_, shape.directions_);
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
        Run(shape, kernel, ref, verifyOnly, centeredFirst, withXad);
    }
} // namespace

int RunSecondOrderBenchmarks(int argc, char** argv) {
    try {
        bool verifyOnly = false, centeredFirst = false, withXad = false, dupire = false;
        for (int arg = 1; arg < argc; ++arg) {
            const std::string option = argv[arg];
            if (option == "--verify-only" && !verifyOnly)
                verifyOnly = true;
            else if (option == "--centered-first" && !centeredFirst)
                centeredFirst = true;
            else if (option == "--with-xad" && !withXad)
                withXad = true;
            else if (option == "--dupire" && !dupire)
                dupire = true;
            else
                THROW("expected --second-order [--dupire] [--verify-only] [--centered-first] [--with-xad]");
        }
#if !defined(DAL_TAPE_PERF_WITH_XAD)
        REQUIRE(!withXad, "--with-xad requires DAL_CPP_XAD_BENCHMARK_SOURCE at configure time");
#endif
        RegisterAll_::Init();
        std::cout << std::setprecision(17) << "# hvp,case,directions,direction_index,coordinate_index,bump_step,reference,pointwise,centered\n"
                  << "# accuracy,case,directions,reference_price,reference_stencil_max_disagreement,pointwise_max_abs_error,centered_max_abs_error\n"
                  << "# "
                     "work,case,directions,pointwise_recordings,pointwise_sweeps,pointwise_peak_bytes,centered_recordings,centered_sweeps,centered_"
                     "peak_bytes\n"
                  << "# sample,case,directions,round,pair,first,calls_per_batch,pointwise_ns_per_call,centered_ns_per_call\n"
                  << "# summary,case,directions,pairs,warm_tape_bytes,pointwise_median_ns,pointwise_p95_ns,pointwise_min_ns,"
                     "centered_median_ns,centered_p95_ns,centered_min_ns,pointwise_over_centered_median\n";
        if (withXad || dupire)
            std::cout << "# metric,case,directions,route,price_error,gradient_max_abs_error,directional_max_abs_error,hvp_max_abs_error,"
                         "recordings,sweeps,tape_bytes,tape_byte_kind\n"
                      << "# output,case,directions,route,kind,row,column,reference,value\n"
                      << "# tri_sample,case,directions,round,pair,first,second,third,calls_per_batch,pointwise_ns,centered_ns,xad_ns\n"
                      << "# tri_summary,case,directions,route,median_ns,p95_ns,min_ns\n";
        if (dupire) {
            RunDupire(verifyOnly, centeredFirst, withXad);
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
        const auto conditional = MakeBasket(basket, coordinateSteps);
        const auto basketRef = BasketReference(conditional, basket, coordinateSteps);
        Run(small, BlackScholes_{}, BlackScholesReference(small), verifyOnly, centeredFirst, withXad);
        Run(basket, conditional, basketRef, verifyOnly, centeredFirst, withXad);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
