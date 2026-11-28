#pragma once

#include <algorithm>

#include <XAD/XAD.hpp>
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

    class Workspace_ {
        Tape_ tape_{false};

        template <class K_>
        Result_ Record(const K_& kernel, const Dal::Vector_<>& point, const Dal::Matrix_<>& directions, bool measureMemory, Counts_* counts) {
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
                if (row == 0) {
                    result.price_ = xad::value(xad::value(price));
                    for (int column = 0; column < inputs; ++column)
                        result.gradient_[column] = xad::value(xad::derivative(x[column]));
                }
                result.directional_[row] = xad::derivative(xad::value(price));
                for (int column = 0; column < inputs; ++column)
                    result.products_(row, column) = xad::derivative(xad::derivative(x[column]));
                pack.Finish();
                if (measureMemory)
                    result.maxLogicalBytes_ = std::max(result.maxLogicalBytes_, tape_.getMemory());
                if (counts)
                    counts->Add(1, tape_.getNumStatements(), tape_.getNumOperations(), tape_.getMemory());
            }
            return result;
        }

    public:
        template <class K_>
        Result_ Evaluate(
            const K_& kernel, const Dal::Vector_<>& point, const Dal::Matrix_<>& directions, bool measureMemory = false, Counts_* counts = nullptr) {
            Span_ activate(Phase::PREPARE);
            tape_.activate();
            activate.Finish();
            try {
                auto result = Record(kernel, point, directions, measureMemory, counts);
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
