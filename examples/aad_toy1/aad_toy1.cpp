//
// Created by wegam on 2020/12/5.
//

#include <iostream>
#include <dal/math/aad/toy1/aad.hpp>

using namespace std;
using namespace Dal;

template <class T_>
T_ f(T_ x[]) {
    T_ y1 = x[2] * (5.0 * x[0] + x[1]);
    T_ y2 = Log(y1);
    T_ y = (y1 + x[3] * y2) * (y1 + y2);
    return y;
}

int main() {

    Number_ x[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    Number_ y = f(x);

    y.SetOrder();
    y.LogProgram();
    cout << y.Evaluate() << endl;
    y.LogResult();

    x[0].SetVal(2.5);
    cout << y.Evaluate() << endl; // 2769.76

    y.PropagateAdjoints();

    for (size_t i = 0; i < 5; ++i) {
        cout << "a" << i << " = " << x[i].Adjoint() << endl;
    }

    return 0;
}

