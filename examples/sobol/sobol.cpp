//
// Created by wegam on 2020/12/27.
//

#include <dal/math/random/sobol.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/vectors.hpp>
#include <iostream>
#include <iomanip>
#include <memory>

using namespace Dal;
using namespace std;

int main() {
    Dal::RegisterAll_::Init();

    unique_ptr<SequenceSet_> rsg(NewSobol(10, 1000));

    Vector_<> dst;
    rsg->FillNormal(&dst);

    auto func = [](double x){ cout << setw(10) << setprecision(6) << x; };
    for_each(dst.begin(), dst.end(), func);
    return 0;
}