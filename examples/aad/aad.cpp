//
// Created by wegam on 2020/12/21.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad/number.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace Dal;

inline double Log(double x) {
    return std::log(x);
}

inline double Pow(double x, double y) {
    return std::pow(x, y);
}

template <class T_>
T_ f(const Vector_<T_>& x) {
    T_ y1 = x[2] * (5.0 * x[0] + x[1]);
    T_ y2 = Log(y1);
    T_ y3 = (y1 + x[3] * y2) * (y1 + y2);
    T_ y4 = Pow(y3, x[4] / 10.);
    T_ y = Max(y4, x[5]);
    return y;
}

template <class T_>
void f_der(Vector_<T_>& x, const Vector_<>& base_value, Vector_<>* ret_val, double eps=1.e-8, int num_params=5) {
    for (size_t i = 0; i < num_params; ++i) {
        x[i] = base_value[i] + eps;
        auto up_y = f(x);
        x[i] = base_value[i] - eps;
        auto down_y = f(x);
        (*ret_val)[i] = (up_y - down_y) / (2. * eps);
    }
}

int main() {
    constexpr auto num_param = 10;
    Vector_<> base_value = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.};

    size_t n_loops = 1000000;

    // Using automatic adjoint differentiation
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n_loops; ++i) {
        Vector_<Number_> x{Number_(1.0), Number_(2.0), Number_(3.0), Number_(4.0), Number_(5.0), Number_(6.0), Number_(7.0), Number_(8.0), Number_(9.0), Number_(10.)};
        Number_ y = f(x);
        y.Value();
        y.PropagateToStart();
        Number_::tape_->Rewind();
    }
    Vector_<Number_> x{Number_(1.0), Number_(2.0), Number_(3.0), Number_(4.0), Number_(5.0), Number_(6.0), Number_(7.0), Number_(8.0), Number_(9.0), Number_(10.)};
    Number_ y = f(x);
    y.Value();
    y.PropagateToStart();
    cout << "y: " << setprecision(9) << y.Value() << endl;
    for (size_t i = 0; i < num_param; ++i) {
        cout << "AAD a" << i << " = "
             << setprecision(9) << x[i].Adjoint() << endl;
    }
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << "AAD aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";

    // Using finite difference
    start = std::chrono::high_resolution_clock::now();
    Vector_<> ret_value(num_param);
    Vector_<> parameters = base_value;
    cout << "y: " << setprecision(9) << f(parameters) << endl;
    for (size_t i=0; i < n_loops; ++i)
        f_der(parameters, base_value, &ret_value, 1e-8, num_param);
    for (size_t i = 0; i < num_param; ++i) {
        cout << "Finite difference a" << i << " = "
             << setprecision(9) << ret_value[i] << endl;
    }
    finish = std::chrono::high_resolution_clock::now();
    std::cout << "Finite difference aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";

    return 0;
}