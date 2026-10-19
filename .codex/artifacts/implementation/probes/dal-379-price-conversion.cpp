#include <dal/platform/platform.hpp>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace {
    void Price(const char* name, double premium, bool call, double forward, double strike, double discount) {
        std::cout << "{\"kind\":\"price\",\"name\":\"" << name << "\",\"premium\":" << premium << ",\"call\":" << call
                  << ",\"forward\":" << forward << ",\"strike\":" << strike << ",\"discount\":" << discount << ",\"actual\":";
        try {
            std::cout << Dal::MarketIvDetail::NormalizePrice(premium, call, forward, strike, discount);
        } catch (const std::exception&) {
            std::cout << "null";
        }
        std::cout << "}\n";
    }
} // namespace

int main() {
    std::cout << std::setprecision(17) << std::boolalpha;
    std::cout << "{\"kind\":\"platform\",\"double_digits\":" << std::numeric_limits<double>::digits
              << ",\"long_double_digits\":" << std::numeric_limits<long double>::digits << "}\n";
    for (const int exponent : {0, 8, 24, 26, 27, 28, 29, 30, 32, 128, 512}) {
        const double discount = std::ldexp(1e-308, exponent);
        for (const double strike : {100000000., 100000000.1, 100000003.14159265})
            Price("scaled_itm_put", std::ldexp(1e-308 * strike, exponent), false, 1e-8, strike, discount);
        Price("scaled_call", std::ldexp(1e-317, exponent), true, 1e-8, 1e-8, discount);
        Price("scaled_otm_put", std::ldexp(1e-317, exponent), false, 1e-8, 5e-9, discount);
    }
    const double normal = std::numeric_limits<double>::min();
    for (const double discount : {std::nextafter(normal, 0.), normal, std::nextafter(normal, 1.)}) {
        for (const bool call : {true, false}) {
            Price("normal_boundary", discount / 4., call, 1., 1., discount);
            Price("zero_boundary", 0., call, 1., 1., discount);
        }
    }
    Price("extreme_put_integer", 1e16, false, 1., 1e16, 1.);
    Price("reciprocal_call", .3, true, 1e300, 1e300, 1e-300);
    Price("reciprocal_put", .3, false, 1e300, 1e300, 1e-300);
    Price("tiny_call", 1e-308, true, 1e-108, 1e-108, 1e-200);
}
