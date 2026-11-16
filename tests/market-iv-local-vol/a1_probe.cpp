#include <fstream>
#include <iomanip>
#include <iostream>

#include <dal/model/surface/lvmodel.hpp>

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    std::ifstream input(argv[1]);
    std::cout << std::setprecision(17);
    size_t ns, nt, queries;
    while (input >> ns >> nt >> queries) {
        Dal::Vector_<> spots(ns), times(nt), queryTimes(queries), querySpots(queries);
        Dal::Matrix_<> vols(ns, nt);
        for (auto& value : spots)
            input >> value;
        for (auto& value : times)
            input >> value;
        for (size_t s = 0; s < ns; ++s)
            for (size_t t = 0; t < nt; ++t)
                input >> vols(s, t);
        for (size_t q = 0; q < queries; ++q)
            input >> queryTimes[q] >> querySpots[q];
        const Dal::LocalVolSurfaceData_ data("A1-probe", spots, times, vols);
        Dal::AAD::LocalVolSurface_<double> surface(data);
        surface.Prepare(queryTimes);
        for (size_t q = 0; q < queries; ++q)
            std::cout << surface.Vol(queryTimes[q], querySpots[q]) << ' ' << surface.VolAtLogSpot(q, std::log(querySpots[q])) << '\n';
    }
}
