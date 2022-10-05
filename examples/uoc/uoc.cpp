//
// Created by wegam on 2022/9/25.
//

#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/math/aad/products/uoc.hpp>
#include <dal/math/aad/models/blackscholes.hpp>

using namespace Dal;
using namespace Dal::AAD;


auto UOCProducts(double strike, double barrier, const Schedule_& schedule, double smooth, bool callPut) {

    std::unique_ptr<Product_<>> prd = std::make_unique<UOC_<>>(
        strike, barrier, schedule, smooth, callPut);
    std::unique_ptr<Product_<Number_>> riskPrd = std::make_unique<UOC_<Number_>>(
        strike, barrier, schedule, smooth, callPut);
    return std::make_pair(std::move(prd), std::move(riskPrd));
}

auto Models(double spot, double vol, double rate, double div) {

    std::unique_ptr<Model_<>> mdl = std::make_unique<BlackScholes_<>>(
        spot, vol, true, rate, div);
    std::unique_ptr<Model_<Number_>> riskMdl = std::make_unique<BlackScholes_<Number_>>(
        spot, vol, true, rate, div);
    return std::make_pair(std::move(mdl), std::move(riskMdl));
}


int main() {
    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));

    double strike = 1.0;
    double barrier = 130.0;
    Date_ start(2022, 9, 25);
    Date_ maturity(2023, 9, 25);
    double spot = 1.0;
    double vol = 0.2;
    int seed = 1234;
    double rate = 0.03;
    double div = 0.06;
    size_t n_paths = 10000000;
    Handle_<Date::Increment_> tenor = Date::ParseIncrement("3M");
    Vector_<Date_> schedule = DateGenerate(start, maturity, tenor);

    auto products = UOCProducts(strike, barrier, schedule, 0.01, true);
    auto models = Models(spot, vol, rate, div);

    return 0;
}