#include <functional>
#include <cmath>
#include <dal/math/matrixarithmetic.hpp>
#include <dal/math/matrixs.hpp>

namespace Dal {
    namespace Matrix {
        Vector_<> Vols(const Matrix_<>& cov, Matrix_<>* corr) {
            const int n = cov.Rows();
            REQUIRE(cov.Cols() == n, "Covarinace matrix should be square matrix");
            Vector_<> retval(n);
            for(int i=0; i != n; ++ i) {
                retval[i] = sqrt(cov(i, i));
                if(corr) {
                    auto scale = std::bind(std::multiplies<double>(), std::placeholders::_1, 1. / Max(Dal::EPSILON, retval[i]));
                    auto r = corr->Row(i);
                    Transform(&r, scale);
                    auto c = corr->Col(i);
                    Transform(&c, scale);
                }
            }
            return retval;
        }
    }
}