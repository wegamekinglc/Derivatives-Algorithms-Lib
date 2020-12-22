//
// Created by wegam on 2020/12/21.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad2/AADNumber.h>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;

template <class T_>
T_ f(T_ x[]) {
    T_ y1 = x[2] * (5.0 * x[0] + x[1]);
    T_ y2 = log(y1);
    T_ y3 = (y1 + x[3] * y2) * (y1 + y2);
    T_ y4 = pow(y3, x[4] / 10.);
    T_ y = max(y4, x[5]);
    return y;
}


int main() {
    constexpr auto num_param = 6;

    size_t n_loops = 1;

    // Using automatic adjoint differentiation
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < n_loops; ++i) {
        Number x[num_param] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
        Number y = f(x);
        y.value();
        y.propagateToStart();
        Number::tape->rewind();
    }
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << "AAD aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";
    Number x[num_param] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    Number y = f(x);
    y.value();
    y.propagateToStart();
    cout << "y: " << setprecision(9) << y.value() << endl;
    for (size_t i = 0; i < num_param; ++i) {
        cout << "AAD a" << i << " = "
             << setprecision(9) << x[i].adjoint() << endl;
    }


    return 0;
}