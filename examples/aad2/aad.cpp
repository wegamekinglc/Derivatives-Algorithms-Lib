//
// Created by wegam on 2020/12/21.
//
#include <dal/math/aad2/AADNumber.h>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;

template <class T_>
T_ f(T_ x[]) {
    T_ y = x[0] * 2;
    return y;
}


int main() {
    constexpr auto num_param = 1;

    size_t n_loops = 1000000;

    // Using automatic adjoint differentiation
    auto start = std::chrono::high_resolution_clock::now();


    for (size_t i = 0; i < n_loops; ++i) {
        Number x[num_param] = {1.0};
        Number::tape->mark();
        Number y = f(x);
        y.value();
        y.propagateToMark();
    }
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << "AAD aprox. time: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / n_loops << "ns\n";
    Number x[num_param] = {1.0};
    Number::tape->mark();
    Number y = f(x);
    y.value();
    y.propagateToMark();
    cout << "y: " << setprecision(9) << y.value() << endl;
    for (size_t i = 0; i < num_param; ++i) {
        cout << "AAD a" << i << " = "
             << setprecision(9) << x[i].adjoint() << endl;
    }


    return 0;
}