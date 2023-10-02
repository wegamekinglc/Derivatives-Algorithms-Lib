//
// Created by wegam on 2023/3/18.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/pde/meshers/concentrating1dmesher.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/utilities/exceptions.hpp>


namespace Dal {
    Concentrating1dMesher_::Concentrating1dMesher_(double start, double end, int size, const std::pair<double, double>& cPoints, bool requireCPoint)
        : FDM1DMesher_(size) {
        const auto cPoint = cPoints.first;
        const auto density = cPoints.second * (end - start);

        REQUIRE(cPoint >= start && cPoint <= end, "cPoint must be between start and end");
        REQUIRE(density > 0.0, "density > 0 required");

        const auto dx = 1.0 / (size - 1);

        Vector_<> u, z;
        const auto c1 = std::asinh((start - cPoint) / density);
        const auto c2 = std::asinh((end - cPoint) / density);
        std::shared_ptr<Interp1_> transform;

        if (requireCPoint) {
            u.push_back(0.0);
            z.push_back(0.0);
            if (!IsClose(cPoint, start) && !IsClose(cPoint, end)) {
                const auto z0 = -c1 / (c2 - c1);
                const auto u0 = std::max(std::min(std::lround(z0 * (size - 1)), static_cast<long>(size) - 2), 1L) / ((double)(size - 1));
                u.push_back(u0);
                z.push_back(z0);
            }
            u.push_back(1.0);
            z.push_back(1.0);

            Interp::NewLinear("intep", u, z);
            transform = std::shared_ptr<Interp1_>(Interp::NewLinear("intep", u, z));
        }

        for (int i = 1; i < size - 1; ++i) {
            const auto li = requireCPoint ? (*transform)(i*dx) : i*dx;
            locations_[i] = cPoint + density*std::sinh(c1 * (1.0 - li) + c2 * li);
        }

        locations_.front() = start;
        locations_.back() = end;

        for (int i = 0; i < size - 1; ++i) {
            dplus_[i] = dminus_[i + 1] = locations_[i + 1] - locations_[i];
        }
        dplus_.back() = dminus_.front() = Null_<double>();
    }
}