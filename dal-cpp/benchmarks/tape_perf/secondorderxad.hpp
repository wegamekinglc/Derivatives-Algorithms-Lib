#pragma once

#include <algorithm>

#include <XAD/XAD.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/specialfunctions.hpp>

namespace TapePerfXad {
    using Forward_ = xad::FReal<double>;
    using Active_ = xad::AReal<Forward_>;
    using Tape_ = xad::Tape<Forward_>;

    struct NormalCdf_ {
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

        template <class K_> Result_ Record(const K_& kernel, const Dal::Vector_<>& point, const Dal::Matrix_<>& directions, bool measureMemory) {
            const int count = directions.Rows(), inputs = directions.Cols();
            Result_ result{0.0, Dal::Vector_<>(point.size()), Dal::Vector_<>(count), Dal::Matrix_<>(count, inputs)};
            Dal::Vector_<Active_> x;
            x.reserve(point.size());
            for (double coordinate : point)
                x.emplace_back(coordinate);
            tape_.registerInputs(x.begin(), x.end());
            for (int row = 0; row < count; ++row) {
                for (int column = 0; column < inputs; ++column)
                    xad::derivative(xad::value(x[column])) = directions(row, column);
                tape_.newRecording();
                Active_ price = kernel(nullptr, x);
                tape_.registerOutput(price);
                xad::derivative(price) = Forward_(1.0);
                tape_.computeAdjoints();
                if (row == 0) {
                    result.price_ = xad::value(xad::value(price));
                    for (int column = 0; column < inputs; ++column)
                        result.gradient_[column] = xad::value(xad::derivative(x[column]));
                }
                result.directional_[row] = xad::derivative(xad::value(price));
                for (int column = 0; column < inputs; ++column)
                    result.products_(row, column) = xad::derivative(xad::derivative(x[column]));
                if (measureMemory)
                    result.maxLogicalBytes_ = std::max(result.maxLogicalBytes_, tape_.getMemory());
            }
            return result;
        }

    public:
        template <class K_>
        Result_ Evaluate(const K_& kernel, const Dal::Vector_<>& point, const Dal::Matrix_<>& directions, bool measureMemory = false) {
            tape_.activate();
            try {
                auto result = Record(kernel, point, directions, measureMemory);
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
} // namespace TapePerfXad

namespace xad {
    using TapePerfXad::Active_;
    using TapePerfXad::Forward_;
    using TapePerfXad::NormalCdf_;

    inline auto NCDF(const Active_& value) { return UnaryExpr<Forward_, NormalCdf_, ADVar<Forward_, 1>>(ADVar<Forward_, 1>(value)); }

    template <class E_> auto NCDF(const Expression<Forward_, E_, Forward_>& expression) {
        return UnaryExpr<Forward_, NormalCdf_, E_>(expression.derived());
    }
} // namespace xad
