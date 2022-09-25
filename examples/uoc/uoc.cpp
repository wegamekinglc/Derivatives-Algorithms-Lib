//
// Created by wegam on 2022/9/25.
//

#include <dal/math/aad/products/uoc.hpp>
#include <dal/math/aad/models/blackscholes.hpp>

using namespace Dal;
using namespace Dal::AAD;


auto UOCProducts(double strike, double barrier, const Date_& maturity, Time_ monitorFreq, double smooth, bool callPut) {

    std::unique_ptr<Product_<>> prd = std::make_unique<UOC_<>>(
        strike, barrier, maturity, monitorFreq, smooth, callPut);
    std::unique_ptr<Product_<Number_>> riskPrd = std::make_unique<UOC_<Number_>>(
        strike, barrier, maturity, monitorFreq, smooth, callPut);
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
    Date_ maturity = Date_(2023, 9, 25);
    double spot = 1.0;
    double vol = 0.2;
    int seed = 1234;
    double rate = 0.03;
    double div = 0.06;
    size_t n_paths = 10000000;

    auto products = UOCProducts(strike, barrier, Date_(2023, 9, 25), 30./365, 0.01, true);
    auto models = Models(spot, vol, rate, div);

    return 0;
}