#include <fstream>
#include <iomanip>
#include <iostream>

#include <dal/model/surface/lvmodel.hpp>

namespace {
    void ReadVector(std::istream& input, Dal::Vector_<>* values) {
        for (auto& value : *values)
            input >> value;
    }

    Dal::Matrix_<> ReadVols(std::istream& input, size_t ns, size_t nt) {
        Dal::Matrix_<> vols(ns, nt);
        for (size_t s = 0; s < ns; ++s)
            for (size_t t = 0; t < nt; ++t)
                input >> vols(s, t);
        return vols;
    }

    void Probe(std::istream& input, size_t ns, size_t nt, size_t queries) {
        Dal::Vector_<> spots(ns), times(nt), queryTimes(queries), querySpots(queries);
        ReadVector(input, &spots);
        ReadVector(input, &times);
        const auto vols = ReadVols(input, ns, nt);
        for (size_t q = 0; q < queries; ++q)
            input >> queryTimes[q] >> querySpots[q];
        const Dal::LocalVolSurfaceData_ data("A1-probe", spots, times, vols);
        Dal::AAD::LocalVolSurface_<double> surface(data);
        surface.Prepare(queryTimes);
        for (size_t q = 0; q < queries; ++q)
            std::cout << surface.Vol(queryTimes[q], querySpots[q]) << ' ' << surface.VolAtLogSpot(q, std::log(querySpots[q])) << '\n';
    }
} // namespace

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    std::ifstream input(argv[1]);
    std::cout << std::setprecision(17);
    size_t ns, nt, queries;
    while (input >> ns >> nt >> queries)
        Probe(input, ns, nt, queries);
}
