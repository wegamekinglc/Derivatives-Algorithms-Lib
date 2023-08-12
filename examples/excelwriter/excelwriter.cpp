//
// Created by wegam on 2023/1/23.
//

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT

#include <dal/report/exceldriverlite.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/operators.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/algorithms.hpp>

using namespace Dal;

int main() {
    Dal::RegisterAll_::Init();

    // DON'T FORGET TO MODIFY EXCELIMPORTS.CPP for correct version of Excel.
    int N = 40;

    // Create abscissa x array
    double A = 0.0;
    double B = 3.0; // Interval_
    auto x = Vector::XRange(A, B, N);

    auto fun = [](double x) { return log(x + 0.01); };
    auto fun2 = [](double x) { return x * x; };
    auto fun3= [](double x) { return x * x * x; };
    auto fun4 = [](double x) { return exp(x); };
    auto fun5 = [](double x) { return x; };

    // Exx.Higher order functions in C++11 g = f op h, e.g. g = f - h
    auto vec1 = Apply(fun, x);
    auto vec2 = Apply(fun2, x);
    auto vec3 = Apply(fun3, x);
    auto vec4 = Apply(fun4, x);
    auto vec5 = Apply(fun5, x);

    // Now Excel output in one sheet for comparison purposes

    // Names of each vector
    Vector_<String_> labels{ "log(x+0.01)", "x^2", "x^3", "exp(x)","x" };

    // The list of Y values
    Vector_<Vector_<>> curves{ vec1, vec2, vec3, vec4, vec5 };

    std::cout << "Data has been created" << std::endl;
    ExcelDriver_ xl; xl.MakeVisible(true);
    xl.CreateChart(x, labels, curves, "Comparing Functions", "x", "y");

    return 0;
}
#else

int main() {
    return 0;
}

#endif