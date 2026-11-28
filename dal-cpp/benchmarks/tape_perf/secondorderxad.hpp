#pragma once

#include <algorithm>

#include <XAD/XAD.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/specialfunctions.hpp>

#include "aadcomparison.hpp"

namespace TapePerfXad {
    using namespace TapePerfComparison;
    using Forward_ = xad::FReal<double>;
    using Active_ = xad::AReal<Forward_>;
    using Tape_ = xad::Tape<Forward_>;

    struct NormalCdf_ {
        double operator()(double x) const { return Dal::NCDF(x); }
        double derivative(double x) const { return Dal::NPDF(x); }

        Forward_ operator()(const Forward_& x) const {
            const double z = xad::value(x);
            return {Dal::NCDF(z), Dal::NPDF(z) * xad::derivative(x)};
        }

        Forward_ derivative(const Forward_& x) const {
            const double z = xad::value(x), density = Dal::NPDF(z);
            return {density, (-z * density) * xad::derivative(x)};
        }
    };

    struct Result_ {
        double price_ = 0.0;
        Dal::Vector_<> gradient_, directional_;
        Dal::Matrix_<> products_;
        size_t maxLogicalBytes_ = 0;
    };

    using OwnedFunction_ = std::function<Active_(Dal::AAD::RecordingScope_*, const Dal::Vector_<Active_>&)>;

    struct OwnedExecution_ {
        Dal::String_ method_ = "XADForwardOverReverse";
        size_t callbackEvaluations_ = 0, recordings_ = 0, reverseSweeps_ = 0, numericPayloadBytes_ = 0;
        std::optional<size_t> peakTapeBytes_, cleanupReserveBytes_;
    };

    struct OwnedResult_ {
        Result_ risk_;
        Dal::Vector_<> point_;
        Dal::Matrix_<> directions_;
        OwnedExecution_ execution_;
    };

    class Workspace_ {
        Tape_ tape_{false};

        static void PackRow(int row, const Dal::Vector_<Active_>& x, const Active_& price, Result_* result, bool check) {
            if (row == 0) {
                result->price_ = xad::value(xad::value(price));
                for (int column = 0; column < static_cast<int>(x.size()); ++column) {
                    result->gradient_[column] = xad::value(xad::derivative(x[column]));
                    if (check) {
                        REQUIRE(std::isfinite(result->gradient_[column]), "XAD S0 nonfinite gradient");
                    }
                }
            }
            result->directional_[row] = xad::derivative(xad::value(price));
            for (int column = 0; column < static_cast<int>(x.size()); ++column) {
                result->products_(row, column) = xad::derivative(xad::derivative(x[column]));
                if (check) {
                    REQUIRE(std::isfinite(result->products_(row, column)), "XAD S0 nonfinite HVP");
                }
            }
        }

        static void CheckRecorded(int row, const Active_& price, const Result_& result, OwnedExecution_* work) {
            if (work) {
                REQUIRE(std::isfinite(result.price_) && std::isfinite(result.directional_[row]), "XAD S0 nonfinite price or directional derivative");
                REQUIRE(xad::value(xad::value(price)) == result.price_, "XAD S0 price changed across directions");
                ++work->recordings_;
                ++work->callbackEvaluations_;
                ++work->reverseSweeps_;
            }
        }

        template <class K_>
        Result_ Record(const K_& kernel,
                       const Dal::Vector_<>& point,
                       const Dal::Matrix_<>& directions,
                       bool measureMemory,
                       Counts_* counts,
                       OwnedExecution_* work) {
            const int count = directions.Rows(), inputs = directions.Cols();
            Span_ allocateOutput(Phase::REDUCE);
            Result_ result{0.0, Dal::Vector_<>(point.size()), Dal::Vector_<>(count), Dal::Matrix_<>(count, inputs)};
            allocateOutput.Finish();
            Span_ setup(Phase::PREPARE);
            Dal::Vector_<Active_> x;
            x.reserve(point.size());
            for (double coordinate : point)
                x.emplace_back(coordinate);
            tape_.registerInputs(x.begin(), x.end());
            setup.Finish();
            for (int row = 0; row < count; ++row) {
                Span_ direction(Phase::PREPARE);
                for (int column = 0; column < inputs; ++column)
                    xad::derivative(xad::value(x[column])) = directions(row, column);
                tape_.newRecording();
                direction.Finish();
                Span_ record(Phase::PATH_FORWARD);
                Active_ price = kernel(nullptr, x);
                record.Finish();
                Span_ seed(Phase::PREPARE);
                tape_.registerOutput(price);
                xad::derivative(price) = Forward_(1.0);
                seed.Finish();
                Span_ reverse(Phase::REVERSE);
                tape_.computeAdjoints();
                reverse.Finish();
                Span_ pack(Phase::REDUCE);
                PackRow(row, x, price, &result, work != nullptr);
                pack.Finish();
                CheckRecorded(row, price, result, work);
                if (measureMemory)
                    result.maxLogicalBytes_ = std::max(result.maxLogicalBytes_, tape_.getMemory());
                if (counts)
                    counts->Add(1, tape_.getNumStatements(), tape_.getNumOperations(), tape_.getMemory());
            }
            return result;
        }

    public:
        template <class K_>
        Result_ Evaluate(const K_& kernel,
                         const Dal::Vector_<>& point,
                         const Dal::Matrix_<>& directions,
                         bool measureMemory = false,
                         Counts_* counts = nullptr,
                         OwnedExecution_* work = nullptr) {
            Span_ activate(Phase::PREPARE);
            tape_.activate();
            activate.Finish();
            try {
                auto result = Record(kernel, point, directions, measureMemory, counts, work);
                const Span_ cleanup(Phase::PREPARE);
                // All active numbers have been destroyed before the tape is reset.
                tape_.clearAll();
                tape_.deactivate();
                return result;
            } catch (...) {
                tape_.deactivate();
                throw;
            }
        }

        OwnedResult_ EvaluateOwning(const OwnedFunction_& function,
                                    const Dal::Vector_<>& point,
                                    const Dal::AAD::ForwardOverReverseRequest_& request,
                                    bool logicalMemory = false) {
            const OwnedFunction_ fixedFunction = function;
            Dal::Vector_<> fixedPoint = point;
            auto fixedRequest = request;
            const auto& directions = fixedRequest.directions_;
            REQUIRE(fixedFunction && fixedPoint.size() <= static_cast<size_t>(std::numeric_limits<int>::max()) && directions.Rows() > 0 &&
                        directions.Cols() == static_cast<int>(fixedPoint.size()),
                    "XAD S0 invalid request shape");
            REQUIRE(!fixedRequest.recordingCapacityBudgetBytes_, "XAD S0 has no tape cap");
            OwnedExecution_ work;
            work.numericPayloadBytes_ = Dal::AAD::ForwardOverReversePayloadBytes(fixedPoint.size(), directions.Rows());
            REQUIRE(!fixedRequest.numericPayloadBudgetBytes_ || work.numericPayloadBytes_ <= *fixedRequest.numericPayloadBudgetBytes_,
                    "XAD S0 numeric payload budget exceeded");
            for (double value : fixedPoint)
                REQUIRE(std::isfinite(value), "XAD S0 nonfinite point");
            for (int row = 0; row < directions.Rows(); ++row)
                for (int column = 0; column < directions.Cols(); ++column)
                    REQUIRE(std::isfinite(directions(row, column)), "XAD S0 nonfinite direction");
            auto risk = Evaluate(fixedFunction, fixedPoint, directions, logicalMemory, nullptr, &work);
            return {std::move(risk), std::move(fixedPoint), std::move(fixedRequest.directions_), std::move(work)};
        }
    };

    class FirstOrderWorkspace_ {
        xad::Tape<double> tape_{false};

        template <class K_> FirstOrderResult_ Record(const K_& kernel, const Dal::Vector_<>& point, Counts_* counts, int sweeps) {
            Span_ setup(Phase::PREPARE);
            Dal::Vector_<xad::AReal<double>> inputs;
            inputs.reserve(point.size());
            for (double coordinate : point)
                inputs.emplace_back(coordinate);
            tape_.registerInputs(inputs.begin(), inputs.end());
            tape_.newRecording();
            setup.Finish();
            Span_ record(Phase::PATH_FORWARD);
            xad::AReal<double> price = kernel(nullptr, inputs);
            record.Finish();
            Span_ output(Phase::PREPARE);
            tape_.registerOutput(price);
            output.Finish();
            for (int pass = 0; pass < sweeps; ++pass) {
                Span_ seed(Phase::PREPARE);
                if (pass != 0)
                    tape_.clearDerivatives();
                xad::derivative(price) = 1.0;
                seed.Finish();
                const Span_ reverse(Phase::REVERSE);
                tape_.computeAdjoints();
            }
            if (counts)
                counts->Add(sweeps, tape_.getNumStatements(), tape_.getNumOperations(), tape_.getMemory());
            const Span_ pack(Phase::REDUCE);
            FirstOrderResult_ result{xad::value(price), Dal::Vector_<>(point.size())};
            for (size_t i = 0; i < point.size(); ++i)
                result.gradient_[i] = xad::derivative(inputs[i]);
            return result;
        }

    public:
        template <class K_> FirstOrderResult_ Evaluate(const K_& kernel, const Dal::Vector_<>& point, Counts_* counts = nullptr, int sweeps = 1) {
            Span_ activate(Phase::PREPARE);
            tape_.activate();
            activate.Finish();
            try {
                auto result = Record(kernel, point, counts, sweeps);
                const Span_ cleanup(Phase::PREPARE);
                tape_.clearAll();
                tape_.deactivate();
                return result;
            } catch (...) {
                tape_.deactivate();
                throw;
            }
        }
    };
} // namespace TapePerfXad

namespace xad {
    using TapePerfXad::Active_;
    using TapePerfXad::Forward_;
    using TapePerfXad::NormalCdf_;

    template <class T_> auto NCDF(const AReal<T_>& value) { return UnaryExpr<T_, NormalCdf_, ADVar<T_, 1>>(ADVar<T_, 1>(value)); }

    template <class T_, class E_> auto NCDF(const Expression<T_, E_, T_>& expression) { return UnaryExpr<T_, NormalCdf_, E_>(expression.derived()); }
} // namespace xad
