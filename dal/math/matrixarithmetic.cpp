#include <dal/math/matrixarithmetic.hpp>
#include <dal/math/matrix.hpp>

namespace Dal {
    namespace Matrix {
        Vector_<> Vols(const Matrix<>& cov, Matrix<>* corr) {
            const int n = cov.Rows();
            REQUIRE(cov.Cols() == n, "Covarinace matrix should be square matrix");
            Vector_<> retval(n);
            for(int i=0; i != n; ++ i) {
                retval[i] = sqrt(cov(i, i));
                if(corr) {
                    auto scale = std::bind2nd(std::multiplies<double>(), 1. / Max(Dal::EPSILON, retval[i]));
                    auto r = corr->Row(i);
                    Transform(&r, scale);
                    auto c = corr->Col(i);
                    Transform(&c, scale)
                }
            }
            return retval;
        }
    }
}