//
// Created by wegam on 2020/12/27.
//

#include <dal/math/random/sobol.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/timer.hpp>
#include <iostream>
#include <iomanip>
#include <memory>

using namespace Dal;
using namespace std;

int main() {
    Dal::RegisterAll_::Init();

    Timer_ timer;
    Vector_<int> widths = {20, 14, 14, 14};

    std::cout << std::setw(widths[0]) << std::right << "# of paths"
              << std::setw(widths[1]) << std::right << "# of dims"
              << std::setw(widths[2]) << std::right << "Uniform"
              << std::setw(widths[3]) << std::right << "Normal"
              << std::endl;

    Vector_<int> pNumPaths = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    const int numDims  = 5;
    for (auto i: pNumPaths) {
        Vector_<> dst;
        unique_ptr<SequenceSet_> rsg(NewSobol(numDims, 1000));
        const auto numPaths = static_cast<size_t>(std::pow(2, i));

        timer.Reset();
        for (auto j = 0; j < numPaths; ++j)
            rsg->FillUniform(&dst);
        auto uniformDuration = int(timer.Elapsed<milliseconds>());

        timer.Reset();
        for (auto j = 0; j < numPaths; ++j)
            rsg->FillNormal(&dst);
        auto normalDuration = int(timer.Elapsed<milliseconds>());


        std::cout << std::fixed
                  << std::setprecision(6)
                  << std::setw(widths[0]) << std::right << numPaths
                  << std::setw(widths[1]) << std::right << numDims
                  << std::setw(widths[2]) << std::right << uniformDuration
                  << std::setw(widths[3]) << std::right << normalDuration
                  << std::endl;

    }
    return 0;
}