//
// Created by wegam on 2020/12/6.
//

#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad/toy2/aad.hpp>

using namespace std;
using namespace Dal;
using namespace Dal::AAD_Toy2;

inline double Log(double x) {
    return std::log(x);
}

template <class T_>
T_ f(T_ x[]) {
    T_ y1 = x[2] * (5.0 * x[0] + x[1]);
    T_ y2 = Log(y1);
    T_ y = (y1 + x[3] * y2) * (y1 + y2);
    return y;
}

template <class T_>
void f_der(T_ x[], const Vector_<>& base_value, Vector_<>* ret_val, double eps=1.e-8) {
    for (size_t i = 0; i < 5; ++i) {
        x[i] = base_value[i] + eps;
        auto up_y = f(x);
        x[i] = base_value[i] - eps;
        auto down_y = f(x);
        (*ret_val)[i] = (up_y - down_y) / (2. * eps);
    }
}

int main() {

    Vector_<> base_value = {1.0, 2.0, 3.0, 4.0, 5.0};
    Number_ x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    size_t n_loops = 100000;
    Number_ y = f(x);

    // Using automatic adjoint differentiation
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i=0; i < n_loops; ++i) {
        y.PropagateAdjoints();
    }
    for (size_t i = 0; i < 5; ++i) {
        cout << "AAD a" << i << " = "
             << setprecision(9) << x[i].Adjoint() << endl;
    }
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << "AAD aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";

    // Using finite difference
    start = std::chrono::high_resolution_clock::now();
    Vector_<> ret_value(5);
    Vector_<> parameters = base_value;
    for (size_t i=0; i < n_loops; ++i)
        f_der(&parameters[0], base_value, &ret_value);
    for (size_t i = 0; i < 5; ++i) {
        cout << "Finite difference a" << i << " = "
             << setprecision(9) << ret_value[i] << endl;
    }
    finish = std::chrono::high_resolution_clock::now();
    std::cout << "Finite difference aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";

    return 0;
}

