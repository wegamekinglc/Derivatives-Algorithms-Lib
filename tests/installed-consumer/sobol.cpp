#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

#include <dal-public/src/random.hpp>

int main() {
    constexpr auto last = std::numeric_limits<uint32_t>::max() - 1;
    for (bool normal : {false, true}) {
        const auto get = normal ? Dal::GetSobolRSGNormal : Dal::GetSobolRSGUniform;
        const Dal::Handle_<Dal::SobolRSG_> generator(new Dal::SobolRSG_("installed retry", last, 3));
        Dal::Matrix_<> actual(1, 1, 123.);
        try {
            get(generator, 2, &actual);
            std::cerr << "Installed Sobol getter accepted an oversized batch\n";
            return 1;
        } catch (const Dal::Exception_& error) {
            if (std::string_view(error.what()).find("RandomSequenceExhausted: current=4294967294; maximum=4294967295") == std::string_view::npos) {
                std::cerr << "Unexpected installed Sobol diagnostic: " << error.what() << '\n';
                return 1;
            }
        }
        if (actual.Rows() != 1 || actual.Cols() != 1 || actual(0, 0) != 123.) {
            std::cerr << "Installed Sobol getter changed output after an oversized batch\n";
            return 1;
        }

        get(generator, 1, &actual);
        const Dal::Handle_<Dal::SobolRSG_> direct(new Dal::SobolRSG_("installed direct", last, 3));
        Dal::Matrix_<> expected;
        get(direct, 1, &expected);
        if (actual.Rows() != 1 || actual.Cols() != 3) {
            std::cerr << "Installed Sobol retry returned the wrong shape\n";
            return 1;
        }
        for (int col = 0; col < actual.Cols(); ++col) {
            if (actual(0, col) != expected(0, col)) {
                std::cerr << "Installed Sobol retry returned a different sample\n";
                return 1;
            }
        }
    }
    return 0;
}
