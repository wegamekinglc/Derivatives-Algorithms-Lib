//
// Created by dal-implementer on 2026/7/12.
//

#include <dal/platform/strict.hpp>

#include <dal/math/interp/interpweights.hpp>
#include <dal/utilities/algorithms.hpp>

namespace Dal {
    namespace {
        Vector_<> SolveTriDiagonal(const Vector_<>& sub, const Vector_<>& diag, const Vector_<>& sup, const Vector_<>& rhs) {
            const int n = static_cast<int>(diag.size());
            REQUIRE(n > 0, "SolveTriDiagonal: diagonal must not be empty");
            REQUIRE(static_cast<int>(sub.size()) == n - 1 && static_cast<int>(sup.size()) == n - 1 && static_cast<int>(rhs.size()) == n,
                    "SolveTriDiagonal: inconsistent sub/diag/super/rhs sizes");
            if (n == 1)
                return {rhs[0] / diag[0]};

            Vector_<> modifiedSup(n - 1);
            Vector_<> modifiedDiag(n);
            Vector_<> forward(n);
            Vector_<> result(n);
            modifiedDiag[0] = diag[0];
            modifiedSup[0] = sup[0] / modifiedDiag[0];
            forward[0] = rhs[0] / modifiedDiag[0];
            for (int i = 1; i < n; ++i) {
                modifiedDiag[i] = diag[i] - sub[i - 1] * modifiedSup[i - 1];
                if (i < n - 1)
                    modifiedSup[i] = sup[i] / modifiedDiag[i];
                forward[i] = (rhs[i] - sub[i - 1] * forward[i - 1]) / modifiedDiag[i];
            }
            result[n - 1] = forward[n - 1];
            for (int i = n - 2; i >= 0; --i)
                result[i] = forward[i] - modifiedSup[i] * result[i + 1];
            return result;
        }

        Vector_<Vector_<>> NaturalSecondDerivativeWeights(const Vector_<>& x) {
            const int n = static_cast<int>(x.size());
            const int interiorCount = n - 2;
            Vector_<Vector_<>> result(n, Vector_<>(n, 0.0));
            Vector_<> widths(n - 1);
            for (int i = 0; i < n - 1; ++i)
                widths[i] = x[i + 1] - x[i];

            Vector_<> sub(std::max(0, interiorCount - 1));
            Vector_<> diag(interiorCount);
            Vector_<> sup(std::max(0, interiorCount - 1));
            for (int row = 0; row < interiorCount; ++row) {
                diag[row] = 2.0 * (widths[row] + widths[row + 1]);
                if (row > 0)
                    sub[row - 1] = widths[row];
                if (row < interiorCount - 1)
                    sup[row] = widths[row + 1];
            }

            for (int storageNode = 0; storageNode < n; ++storageNode) {
                Vector_<> rhs(interiorCount, 0.0);
                for (int row = 0; row < interiorCount; ++row) {
                    const int knot = row + 1;
                    if (storageNode == knot - 1)
                        rhs[row] += 6.0 / widths[knot - 1];
                    if (storageNode == knot)
                        rhs[row] -= 6.0 / widths[knot - 1] + 6.0 / widths[knot];
                    if (storageNode == knot + 1)
                        rhs[row] += 6.0 / widths[knot];
                }
                const Vector_<> solved = SolveTriDiagonal(sub, diag, sup, rhs);
                for (int row = 0; row < interiorCount; ++row)
                    result[row + 1][storageNode] = solved[row];
            }
            return result;
        }
    } // namespace

    namespace Interp {
        LinearWeightGeometry_::LinearWeightGeometry_(const Vector_<>& x) : x_(x) {
            REQUIRE(!x_.empty(), "LinearWeightGeometry_: abscissae must not be empty");
            REQUIRE(IsMonotonic(x_), "LinearWeightGeometry_: abscissae must be strictly increasing");
        }

        InterpWeights_ LinearWeightGeometry_::At(double x) const {
            const auto pGE = LowerBound(x_, x);
            if (pGE == x_.begin())
                return {{0, 1.0}};
            if (pGE == x_.end())
                return {{static_cast<int>(x_.size()) - 1, 1.0}};

            const int iGE = static_cast<int>(pGE - x_.begin());
            if (*pGE == x)
                return {{iGE, 1.0}};

            const int iLT = iGE - 1;
            const double upperWeight = (x - x_[iLT]) / (x_[iGE] - x_[iLT]);
            return {{iLT, 1.0 - upperWeight}, {iGE, upperWeight}};
        }

        NaturalCubicWeightGeometry_::NaturalCubicWeightGeometry_(const Vector_<>& x) : x_(x), secondDerivativeWeights_() {
            REQUIRE(x_.size() > 2, "NaturalCubicWeightGeometry_: need at least 3 abscissae");
            REQUIRE(IsMonotonic(x_), "NaturalCubicWeightGeometry_: abscissae must be strictly increasing");
            secondDerivativeWeights_ = NaturalSecondDerivativeWeights(x_);
        }

        InterpWeights_ NaturalCubicWeightGeometry_::At(double x) const {
            const auto pGE = LowerBound(x_, x);
            if (pGE != x_.end() && *pGE == x)
                return {{static_cast<int>(pGE - x_.begin()), 1.0}};

            const int n = static_cast<int>(x_.size());
            const int iGE = std::min(n - 1, std::max(1, static_cast<int>(pGE - x_.begin())));
            const int iLT = iGE - 1;
            const double width = x_[iGE] - x_[iLT];
            const double upperWeight = (x - x_[iLT]) / width;
            const double lowerWeight = 1.0 - upperWeight;
            const double factor = lowerWeight * upperWeight * width * width / 6.0;

            InterpWeights_ result;
            result.reserve(n);
            for (int storageNode = 0; storageNode < n; ++storageNode) {
                double weight = 0.0;
                if (storageNode == iLT)
                    weight += lowerWeight;
                if (storageNode == iGE)
                    weight += upperWeight;
                weight -= factor * ((1.0 + lowerWeight) * secondDerivativeWeights_[iLT][storageNode] +
                                    (1.0 + upperWeight) * secondDerivativeWeights_[iGE][storageNode]);
                result.emplace_back(storageNode, weight);
            }
            return result;
        }
    } // namespace Interp
} // namespace Dal
