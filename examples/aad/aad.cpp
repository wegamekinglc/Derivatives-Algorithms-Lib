//
// Created by wegam on 2020/12/21.
//

#include <dal/math/aad/operators.hpp>
#include <dal/math/aad/number.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/timer.hpp>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace Dal;
using namespace Dal::AAD;

template <class T_>
T_ f(const Vector_<T_>& x) {
    T_ y1 = x[2] * (5.0 * x[0] + x[1]);
    T_ y2 = Log(y1);
    T_ y3 = (y1 + x[3] * y2) * (y1 + y2);
    T_ y4 = Pow(y3, x[4] / 10.);
    T_ y5 = Max(y4, x[5]);
    T_ y6 = y5 - x[6] + x[7];
    T_ y = y6 * x[8] / x[9];
    return y;
}

template <class T_>
void f_der(Vector_<T_>& x, const Vector_<>& base_value, Vector_<>* ret_val, int num_params, double eps=1.e-8) {
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
    Tape_ new_tape;
    Number_::tape_ = &new_tape;
    Vector_<> base_value = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.};
    Vector_<> parameters = base_value;
    Timer_ timer;

    size_t n_loops = 10000000;
    // simple primitive double calculation
    double y_raw = 0.;
    for (size_t i = 0; i < n_loops; ++i) {
        // add small randomness to avoid compiler optimization
        parameters[9] = base_value[9] + static_cast<double>(i) * 1e-14;
        y_raw = f(parameters);
    }
    cout << "y: " << setprecision(9) << y_raw << endl;
    std::cout << "Primitive aprox. time: "
              << timer.Elapsed<nanoseconds>() / n_loops << " ns\n";

    // Using automatic adjoint differentiation
    timer.Reset();
    Number_ y;
    Vector_<Number_> x(num_param);
    for (size_t i = 0; i < n_loops; ++i) {
        for(auto k = 0; k < num_param; ++k)
            x[k] = base_value[k];
        y = f(x);
        y.Value();
        y.PropagateToStart();
        Number_::tape_->Rewind();
    }
    cout << "y: " << setprecision(9) << y.Value() << endl;
    for (size_t i = 0; i < num_param; ++i) {
        cout << "AAD a" << i << " = "
             << setprecision(9) << x[i].Adjoint() << endl;
    }
    std::cout << "AAD aprox. time: "
              << timer.Elapsed<nanoseconds>() / n_loops << " ns\n";

    // Using finite difference
    timer.Reset();
    Vector_<> ret_value(num_param);
    parameters = base_value;
    cout << "y: " << setprecision(9) << f(parameters) << endl;
    for (size_t i=0; i < n_loops; ++i)
        f_der(parameters, base_value, &ret_value, num_param, 1e-8);
    for (size_t i = 0; i < num_param; ++i) {
        cout << "Finite difference a" << i << " = "
             << setprecision(9) << ret_value[i] << endl;
    }
    std::cout << "Finite difference aprox. time: "
              << timer.Elapsed<nanoseconds>() / n_loops << " ns\n";

    return 0;
}