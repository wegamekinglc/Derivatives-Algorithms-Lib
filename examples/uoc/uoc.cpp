//
// Created by wegam on 2022/9/25.
//

#include <dal/time/schedules.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/math/aad/products/uoc.hpp>
#include <dal/math/aad/products/european.hpp>
#include <dal/math/aad/models/blackscholes.hpp>
#include <dal/math/aad/models/dupire.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/aad/simulation.hpp>
#include <dal/utilities/timer.hpp>
#include <istream>
#include <iomanip>

using namespace Dal;
using namespace Dal::AAD;

auto EuropeanProducts(double strike, const Date_& maturity) {
    std::unique_ptr<Product_<>> prd = std::make_unique<European_<>>(strike, maturity, maturity);
    std::unique_ptr<Product_<Number_>> riskPrd = std::make_unique<European_<Number_>>(strike, maturity, maturity);
    return std::make_pair(std::move(prd), std::move(riskPrd));
}


auto UOCProducts(double strike, double barrier, const Schedule_& schedule, double smooth, bool callPut) {

    std::unique_ptr<Product_<>> prd = std::make_unique<UOC_<>>(
        strike, barrier, schedule, smooth, callPut);
    std::unique_ptr<Product_<Number_>> riskPrd = std::make_unique<UOC_<Number_>>(
        strike, barrier, schedule, smooth, callPut);
    return std::make_pair(std::move(prd), std::move(riskPrd));
}

auto BSModels(double spot, double vol, double rate, double div) {

    std::unique_ptr<Model_<>> mdl = std::make_unique<BlackScholes_<>>(
        spot, vol, false, rate, div);
    std::unique_ptr<Model_<Number_>> riskMdl = std::make_unique<BlackScholes_<Number_>>(
        spot, vol, false, rate, div);
    return std::make_pair(std::move(mdl), std::move(riskMdl));
}

auto DupireModels(double spot, double timeLow, double timeHigh, int timeSteps, double spotLow, double spotHigh, int spotSteps, double vol) {
    auto times = Vector::XRange(timeLow, timeHigh, timeSteps + 1);
    auto spots = Vector::XRange(spotLow, spotHigh, spotSteps + 1);

    std::unique_ptr<Model_<>> mdl = std::make_unique<Dupire_<>>(spot, spots, times, Matrix_<>(spots.size(), times.size(), 0.15), 10.0);
    std::unique_ptr<Model_<Number_>> riskMdl = std::make_unique<Dupire_<Number_>>(Number_(spot),
                                                                                  spots,
                                                                                  times,
                                                                                  Matrix_<Number_>(spots.size(), times.size(), Number_(0.15)),
                                                                                      10.0);
    return std::make_pair(std::move(mdl), std::move(riskMdl));
}


int main() {
    /*
     * some global evaluation settings
     */
    XGLOBAL::SetEvaluationDate(Date_(2022, 9, 25));
    const Date_ start = Global::Dates_().EvaluationDate();
    const double spot = 100.0;
    const double vol = 0.15;
    const double rate = 0.0;
    const double div = 0.0;
    const double strike = 120.0;
    const Date_ maturity(2025, 9, 25);
    int n;
    std::cout << "Plz input # of paths (power of 2): ";
    std::cin >> n;
    const int n_paths = Pow(2, n);

    double smooth;
    std::cout << "Plz input smooth factor: ";
    std::cin >> smooth;
    std::cout << "# of path: " << n_paths << " - smooth factor: " << std::setprecision(4) << smooth << std::endl;

    /*
     * European products and B-S models
     */
    auto products = EuropeanProducts(strike, maturity);
    Timer_ timer;
    
    // use a simple B-S model
    auto bsModels = BSModels(spot, vol, rate, div);
    
    timer.Reset();
    auto res = MCParallelSimulation(*products.first, *bsModels.first, "sobol", n_paths);
    auto sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    auto calculated = sum / static_cast<double>(res.Rows());
    std::cout << "\nEuropean       w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    
    timer.Reset();
    auto resAAD = MCParallelSimulationAAD(*products.second, *bsModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(resAAD.Rows());
    std::cout << "European (AAD) w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                     : delta " << std::setprecision(8) << resAAD.risks_[0] << std::endl;
    std::cout << "                     : vega  " << std::setprecision(8) << resAAD.risks_[1] << std::endl;
    std::cout << "                     : rho   " << std::setprecision(8) << resAAD.risks_[2] << std::endl;
    
    
    // use a flat dupire model
    auto dupireModels = DupireModels(spot, 0, 5, 60, 50, 200, 30, vol);
    timer.Reset();
    res = MCParallelSimulation(*products.first, *dupireModels.first, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    calculated = sum / static_cast<double>(res.Rows());
    std::cout << "\nEuropean       w. Dupire: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    timer.Reset();
    resAAD = MCParallelSimulationAAD(*products.second, *dupireModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(resAAD.Rows());
    std::cout << "European (AAD) w. Dupire: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                        : delta " << std::setprecision(8) << resAAD.risks_[0] << std::endl;
    // for flat dupire model, we have to aggregate all the vega risk together to get a whole picture.
    auto risk_sum = 0.0;
    for (int i = 1; i < resAAD.risks_.size(); ++i)
        risk_sum += resAAD.risks_[i];
    std::cout << "                        : vega  " << std::setprecision(8) << risk_sum << std::endl;

    /*
     * up and out call
     */
    auto tenor = Date::ParseIncrement("1W");
    const auto schedule = DateGenerate(start, maturity, tenor);
    const double barrier = 150;
    products = UOCProducts(strike, barrier, schedule, smooth, false);
    
    // use a simple B-S model
    timer.Reset();
    res = MCParallelSimulation(*products.first, *bsModels.first, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    calculated = sum / static_cast<double>(res.Rows());
    std::cout << "\nUOC       w. B-S: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    timer.Reset();
    resAAD = MCParallelSimulationAAD(*products.second, *bsModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(resAAD.Rows());
    std::cout << "UOC (AAD) w. B-S: price " << std::setprecision(8) << calculated
              << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                : delta " << std::setprecision(8) << resAAD.risks_[0] << std::endl;
    std::cout << "                : vega  " << std::setprecision(8) << resAAD.risks_[1] << std::endl;
    std::cout << "                : rho   " << std::setprecision(8) << resAAD.risks_[2] << std::endl;
    
    // use a flat dupire model
    timer.Reset();
    res = MCParallelSimulation(*products.first, *dupireModels.first, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < res.Rows(); ++row)
        sum += res(row, 0);
    calculated = sum / static_cast<double>(res.Rows());
    std::cout << "\nUOC       w. Dupire: price " << std::setprecision(8) << calculated << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    
    timer.Reset();
    resAAD = MCParallelSimulationAAD(*products.second, *dupireModels.second, "sobol", n_paths);
    sum = 0.0;
    for (auto row = 0; row < resAAD.Rows(); ++row)
        sum += resAAD.aggregated_[row];
    calculated = sum / static_cast<double>(resAAD.Rows());
    std::cout << "UOC (AAD) w. Dupire: price " << std::setprecision(8) << calculated
              << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;
    std::cout << "                   : delta " << std::setprecision(8) << resAAD.risks_[0] << std::endl;
    // for flat dupire model, we have to aggregate all the vega risk together to get a whole picture.
    risk_sum = 0.0;
    for (int i = 1; i < resAAD.risks_.size(); ++i)
        risk_sum += resAAD.risks_[i];
    std::cout << "                   : vega  " << std::setprecision(8) << risk_sum << std::endl;
    
    // for matrix of risk report for UOC under B-S
    for (int round = 10; round <= 25; ++round) {
        const int n_paths = Pow(2, round);
        resAAD = MCParallelSimulationAAD(*products.second, *bsModels.second, "sobol", n_paths);
        sum = 0.0;
        for (auto row = 0; row < resAAD.Rows(); ++row)
            sum += resAAD.aggregated_[row];
        auto price = sum / static_cast<double>(resAAD.Rows());
        auto delta = resAAD.risks_[0];
        auto vega = resAAD.risks_[1];
        auto rho = resAAD.risks_[2];
        std::cout << round << "," << price << "," << delta << "," << vega << "," << rho << std::endl;
    }

    return 0;
}