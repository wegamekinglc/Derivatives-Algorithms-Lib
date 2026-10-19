#include <dal/platform/platform.hpp>

#include <limits>
#include <dal/model/surface/marketivsurfaceinternal.hpp>

using Dal::Vector_;

namespace {
    void Array(const Vector_<>& values) {
        std::cout << '[';
        for (size_t i = 0; i < values.size(); ++i) {
            if (i)
                std::cout << ',';
            std::cout << values[i];
        }
        std::cout << ']';
    }
    void Curve(const char* name, const Vector_<>& times, const Vector_<>& values) {
        const auto c = Dal::MarketIvDetail::NewThetaCurve(times, values);
        std::cout << "{\"kind\":\"curve\",\"name\":\"" << name << "\",\"certified\":" << c.certified_ << ",\"times\":";
        Array(times);
        std::cout << ",\"values\":";
        Array(values);
        std::cout << ",\"slopes\":";
        Array(c.slopes_);
        std::cout << ",\"bounds\":";
        Array(c.lowerBounds_);
        std::cout << ",\"evaluations\":[";
        bool first = true;
        if (c.certified_) {
            Vector_<> queries = times;
            for (size_t i = 1; i < times.size(); ++i) {
                const double middle = times[i - 1] + (times[i] - times[i - 1]) / 2.;
                if (middle > times[i - 1] && middle < times[i])
                    queries.push_back(middle);
            }
            queries.push_back(std::nextafter(times.back(), std::numeric_limits<double>::infinity()));
            queries.push_back(times.back() + 1.);
            for (const double time : queries) {
                if (!first)
                    std::cout << ',';
                first = false;
                const auto point = Dal::MarketIvDetail::ThetaAt(c, time);
                std::cout << '[' << time << ',' << point.first << ',' << point.second << ']';
            }
        }
        std::cout << "]}\n";
    }
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
    Curve("plateau", {0., 1., 2., 3.}, {0., .04, .04, .04});
    Curve("valley", {0., 1., 2., 3.}, {0., .08, .12, .20});
    Curve("tiny", {0., 1., 2.}, {0., 1e-310, 2e-310});
    Curve("huge", {0., 10., 11.}, {0., 1e308, 1.1e308});
    Curve("two_nodes", {0., 1.}, {0., .04});
    Curve("wide_times", {0., 1e-12, 1., 1e12}, {0., 1e-12, .04, .08});
    Curve("zero_then_rising", {0., .1, .10000000000000002, .5, 2.}, {0., .04, .04, .04, .08});
    Curve("decreasing_rejected", {0., 1., 2.}, {0., .04, .03});
    for (int direction : {-1, 0, 1}) {
        double middle = .04;
        if (direction != 0)
            middle = std::nextafter(middle, direction < 0 ? 0. : 1.);
        Curve(direction < 0 ? "ulp_below" : direction == 0 ? "ulp_equal" : "ulp_above", {0., 1., 2., 3.}, {0., .04, middle, .08});
    }
    Curve("time_ulp", {0., 1., std::nextafter(1., 2.), 2.}, {0., .04, std::nextafter(.04, 1.), .08});
    for (const auto box : {std::pair<double, double>{.999, 1e-8}, {.25, .99}, {std::nextafter(1., 0.), 1e-8},
                           {.1, std::nextafter(1., 0.)}, {1e-100, 1e-100}}) {
        Dal::MarketIvFitSettings_ s;
        s.rhoLimit_ = box.first;
        s.constraintMargin_ = box.second;
        const double cap = Dal::MarketIvDetail::EtaMax(s);
        const auto bounds = Dal::MarketIvDetail::GlobalBounds(cap, s.rhoLimit_);
        std::cout << "{\"kind\":\"cap\",\"rho_limit\":" << box.first << ",\"margin\":" << box.second << ",\"eta\":" << cap
                  << ",\"butterfly_bound\":" << bounds.first << ",\"wing_bound\":" << bounds.second
                  << ",\"accepted\":" << Dal::MarketIvDetail::ParametersCertified(s, s.rhoLimit_, cap)
                  << ",\"eta_up_accepted\":" << Dal::MarketIvDetail::ParametersCertified(s, 0., std::nextafter(cap, 3.))
                  << ",\"rho_up_accepted\":" << Dal::MarketIvDetail::ParametersCertified(s, std::nextafter(s.rhoLimit_, 1.), cap) << "}\n";
    }
    Price("extreme_put_integer", 1e16, false, 1., 1e16, 1.);
    Price("reciprocal_scale", .3, true, 1e300, 1e300, 1e-300);
    Price("tiny_call", 1e-308, true, 1e-108, 1e-108, 1e-200);
    for (double strike : {100000000., 100000000.1, 100000003.14159265})
        Price("subnormal_df_put", 1e-308 * strike, false, 1e-8, strike, 1e-308);
}
