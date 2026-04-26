//
// Created by wegam on 2026/4/26.
//

#include <cmath>

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/integral/quadrature.hpp>
#include <dal/math/vectors.hpp>

namespace Dal {

    Quad1DBase_::~Quad1DBase_() = default;

    namespace {
        constexpr int MAX_NEWTON_ITERATIONS = 20;
        constexpr double ROOT_TOLERANCE = 1.0e-14;

        struct HermiteRoot_ {
            double root_;
            double derivative_;
        };

        double InitialHermiteRoot(int i, int n, double previousRoot, const Vector_<>& x) {
            if (i == 0)
                return sqrt(2.0 * n + 1.0) - 1.85575 * pow(2.0 * n + 1.0, -1.0 / 6.0);
            if (i == 1)
                return previousRoot - 1.14 * pow(static_cast<double>(n), 0.426) / previousRoot;
            if (i == 2)
                return 1.86 * previousRoot - 0.86 * x[n - 1] / M_SQRT_2;
            if (i == 3)
                return 1.91 * previousRoot - 0.91 * x[n - 2] / M_SQRT_2;
            return 2.0 * previousRoot - x[n - i + 1] / M_SQRT_2;
        }

        HermiteRoot_ FindHermiteRoot(int n, double initialRoot, double invPiQuarter, double rootTwoN) {
            double root = initialRoot;
            double derivative = 0.0;
            for (int its = 0; its < MAX_NEWTON_ITERATIONS; ++its) {
                double p1 = invPiQuarter;
                double p2 = 0.0;
                for (int j = 1; j <= n; ++j) {
                    const double p3 = p2;
                    p2 = p1;
                    p1 = root * sqrt(2.0 / j) * p2 - sqrt(static_cast<double>(j - 1) / j) * p3;
                }
                derivative = rootTwoN * p2;
                const double previousRoot = root;
                root -= p1 / derivative;
                if (std::abs(root - previousRoot) <= ROOT_TOLERANCE)
                    return {root, derivative};
            }
            THROW("Too many iterations in Gauss-Hermite root search");
        }

        void SetNormalHermitePair(int i, int n, double root, double derivative, double invRootPi, Vector_<>* x, Vector_<>* w) {
            const int left = i;
            const int right = n - 1 - i;
            const double xValue = M_SQRT_2 * root;
            const double wValue = 2.0 * invRootPi / (derivative * derivative);
            (*x)[left] = -xValue;
            (*x)[right] = xValue;
            (*w)[left] = wValue;
            (*w)[right] = wValue;
        }
    } // namespace

    void Quadrature::NCDFGaussHermiteWeights(Vector_<>* x, Vector_<>* w) {
        const int n = static_cast<int>(x->size());
        x->Resize(n);
        w->Resize(n);
        if (n == 0)
            return;

        const int m = (n + 1) / 2;
        const double invPiQuarter = 1.0 / sqrt(sqrt(PI));
        const double invRootPi = 1.0 / sqrt(PI);
        const double rootTwoN = sqrt(2.0 * n);
        double z = 0.0;
        for (int i = 0; i < m; ++i) {
            // Compute positive Hermite roots, then map symmetric pairs to standard-normal coordinates.
            z = InitialHermiteRoot(i, n, z, *x);
            const HermiteRoot_ root = FindHermiteRoot(n, z, invPiQuarter, rootTwoN);
            z = root.root_;
            SetNormalHermitePair(i, n, root.root_, root.derivative_, invRootPi, x, w);
        }
    }

    void Quadrature::SimpsonWeights(int n, double lo, double hi, Vector_<>* x, Vector_<>* w) {
        n |= 1;
        const double dx = (hi - lo) / (n - 1);
        for (int ii = 0; ii < n; ++ii) {
            (*x)[ii] = lo + ii * dx;
            (*w)[ii] = (ii & 1 ? 4 : 2) * dx / 3.0;
        }
        w->front() = w->back() = dx / 3.0;
    }
} // namespace Dal
