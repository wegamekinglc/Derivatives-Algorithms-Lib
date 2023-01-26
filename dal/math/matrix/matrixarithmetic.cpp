#include <cmath>
#include <dal/math/matrix/matrixarithmetic.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/functionals.hpp>
#include <dal/platform/strict.hpp>

namespace Dal::Matrix {
    Vector_<> Vols(const Matrix_<> &cov, Matrix_<> *corr) {
        const int n = cov.Rows();
        REQUIRE(cov.Cols() == n, "Covariance matrix should be square matrix");
        Vector_<> ret_val(n);
        if (corr)
            *corr = cov;
        for (int i = 0; i != n; ++i) {
            ret_val[i] = sqrt(cov(i, i));
            if (corr) {
                auto scale = [&ret_val, &i](double x) {
                    return x / max(Dal::EPSILON, ret_val[i]);
                };
                auto r = corr->Row(i);
                Transform(&r, scale);
                auto c = corr->Col(i);
                Transform(&c, scale);
            }
        }
        return ret_val;
    }

    namespace {
        void MultiplyAliasFree(const Matrix_<> &left, const Matrix_<> &right, Matrix_<> *result) {
            REQUIRE(result != &right, "result and right should not be same");
            result->Resize(left.Rows(), right.Cols());
            result->Fill(0.0);
            for (int ir = 0; ir < left.Rows(); ++ir) {
                auto dst = result->Row(ir);
                for (int jr = 0; jr < right.Rows(); ++jr)
                    Transform(&dst, right.Row(jr), LinearIncrement(left(ir, jr)));
            }
        }

        void MultiplyAliasFree(const Matrix_<> &left, const Vector_<> &right, Vector_<> *result) {
            REQUIRE(result != &right, "result and right should not be same");
            result->Resize(left.Rows());
            for (int ir = 0; ir < left.Rows(); ++ir)
                (*result)[ir] = InnerProduct(left.Row(ir), right);
        }

        void MultiplyAliasFree(const Vector_<> &left, const Matrix_<> &right, Vector_<> *result) {
            REQUIRE(result != &left, "result and right should not be same");
            result->Resize(right.Cols());
            result->Fill(0.0);
            REQUIRE(left.size() == right.Rows(), "left and right size is not compatible");
            for (int ir = 0; ir < right.Rows(); ++ir)
                Transform(result, right.Row(ir), LinearIncrement(left[ir]));
        }
    }    // leave local

    void Multiply(const Matrix_<> &left, const Matrix_<> &right, Matrix_<> *result) {
        REQUIRE(left.Cols() == right.Rows(), "left and right size is not compatible");
        if (result == &left)
            Multiply(Matrix_<>(left), right, result);
        else if (result == &right)
            Multiply(left, Matrix_<>(right), result);
        else
            MultiplyAliasFree(left, right, result);
    }

    void Multiply(const Matrix_<> &left, const Vector_<> &right, Vector_<> *result) {
        REQUIRE(left.Cols() == right.size(), "left and right size is not compatible");
        if (result == &right)    // aliased
            Multiply(left, Vector_<>(right), result);
        else
            MultiplyAliasFree(left, right, result);
    }

    void Multiply(const Vector_<> &left, const Matrix_<> &right, Vector_<> *result) {
        REQUIRE(left.size() == right.Rows(), "left and right size is not compatible");
        if (result == &left)    // aliased
            Multiply(Vector_<>(left), right, result);
        else
            MultiplyAliasFree(left, right, result);
    }

    double WeightedInnerProduct(const Vector_<> &left, const Matrix_<> &w, const Vector_<> &right) {
        REQUIRE(left.size() == w.Rows(), "left and w size is not compatible");
        REQUIRE(right.size() == w.Cols(), "right and w size is not compatible");
        double retval = 0.0;
        for (int ir = 0; ir < w.Rows(); ++ir)
            retval += left[ir] * InnerProduct(w.Row(ir), right);
        return retval;
    }


    void AddJSquaredToUpper(const Matrix_<> &a, Matrix_<> *h) {
        static const int CACHE_SIZE = 16;    // number of doubles in a cached row

        const int n = a.Rows(), nf = a.Cols();
        REQUIRE(h->Rows() == n && h->Cols() == n, "h and a size is not compatible");

        for (int ii = 0; ii < n; ++ii) {
            for (int jOuter = ii; jOuter < n; jOuter += CACHE_SIZE) {
                auto src = a.Row(ii).begin();
                const int jStop = min(n, jOuter + CACHE_SIZE);        // last j in this segment

                for (int kOuter = 0; kOuter < nf; kOuter += CACHE_SIZE, src += CACHE_SIZE) {
                    auto dst = h->Row(ii).begin() + jOuter;
                    const int nfHere = min(nf - kOuter, CACHE_SIZE);
                    auto srcStop = src + nfHere;

                    for (int jInner = jOuter; jInner < jStop; ++jInner, ++dst) {
                        *dst = inner_product(src, srcStop, a.Row(jInner).begin() + kOuter, *dst);
                    }
                }
            }
        }
    }
} // namespace Dal