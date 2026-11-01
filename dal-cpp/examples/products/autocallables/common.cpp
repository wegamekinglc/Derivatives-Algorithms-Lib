#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#include <dal/platform/platform.hpp>

#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/platform/initall.hpp>
#include <dal/script/event.hpp>
#include <dal/script/settings.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/globals.hpp>

#include "../../floatformat.hpp"
#include "common.hpp"

namespace Dal::Examples::Autocallables {
    String_ Literal(double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(12) << value;
        return String_(out.str());
    }

    String_ CouponOrKnockIn(const Case_& terms, double period) {
        if (!terms.phoenix_)
            return "if performance < " + Literal(terms.knockIn_) + " then is_ki = 1 end\n";
        const double coupon = terms.coupon_ * period;
        String_ script;
        if (terms.memory_)
            script += "unpaid = unpaid + " + Literal(coupon) + "\n";
        const String_ amount = terms.memory_ ? String_("unpaid") : Literal(coupon);
        String_ payment = "pay PAYS alive * " + amount;
        if (terms.memory_)
            payment += " unpaid = 0";
        return script + (terms.protected_ ? payment + "\n" : "if performance >= 0.8 then " + payment + " end\n");
    }

    String_ MaturityEvents(const Case_& terms, double accrued) {
        String_ script, redemption;
        if (terms.protected_) {
            redemption = "1";
        } else if (terms.phoenix_) {
            script = "is_ki = 0 if performance < " + Literal(terms.knockIn_) + " then is_ki = 1 end\n";
            redemption = "1 - is_ki + is_ki * MIN(performance, 1)";
        } else {
            const String_ downside = terms.airbag_ ? "MIN(performance / " + Literal(terms.knockIn_) + ", 1)" : String_("MIN(performance, 1)");
            redemption = "(1 - is_ki) * " + Literal(1.0 + terms.coupon_ * accrued) + " + is_ki * " + downside;
            if (terms.participation_ != 0.0)
                redemption += " + " + Literal(terms.participation_) + " * MAX(performance - 1, 0)";
        }
        return script + "pay PAYS alive * (" + redemption + ")\n";
    }

    Script::ScriptProductData_ Product(const Case_& terms) {
        const Date_ today(2026, 1, 2);
        Vector_<Cell_> dates = {Cell_(today)};
        Vector_<String_> events = {"alive = 1 is_ki = 0 unpaid = 0"};
        Date_ previous = today;
        for (int month = 1; month <= 12; ++month) {
            const Date_ observation(2026 + month / 12, month % 12 + 1, 2);
            const double accrued = (observation - today) / 365.0;
            const double period = (observation - previous) / 365.0;
            String_ script = "performance = ";
            script += terms.worstOf_ ? "MIN(FIX(EQ[A]) / 100, FIX(EQ[B]) / 120)\n" : "SPOT()\n";
            script += CouponOrKnockIn(terms, period);
            if (month >= terms.firstCall_) {
                const double barrier = std::max(0.85, terms.knockOut_ - terms.stepDown_ * (month - 1));
                const double redemption = terms.phoenix_ ? 1.0 : 1.0 + terms.coupon_ * accrued;
                script += "if performance >= " + Literal(barrier) + " then pay PAYS alive * " + Literal(redemption) + " alive = 0 end\n";
            }
            if (month == 12)
                script += MaturityEvents(terms, accrued);
            dates.emplace_back(observation);
            events.push_back(script);
            previous = observation;
        }
        return Script::ScriptProductData_(terms.title_, dates, events);
    }

    Handle_<ModelData_> Model(const Case_& terms) {
        if (!terms.worstOf_)
            return Handle_<ModelData_>(new BSModelData_("equity", 1.0, 0.20, 0.02, 0.0));
        CorrelatedBSSettings_ settings;
        settings.assets_ = {{"EQ[A]", 100.0, 0.20, 0.0}, {"EQ[B]", 120.0, 0.25, 0.0}};
        settings.rate_ = 0.02;
        settings.correlations_ = Matrix_<>(2, 2, 0.4);
        settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
        return Handle_<ModelData_>(new CorrelatedBSModelData_("two_equities", settings));
    }

    int Run(int argc, char* argv[], const Case_& terms) {
        try {
            REQUIRE(argc <= 2, "usage: autocallable_<type> [positive path count]");
            size_t paths = 16384;
            if (argc == 2) {
                const std::string text(argv[1]);
                REQUIRE(!text.empty() && text.find_first_not_of("0123456789") == std::string::npos, "path count must contain only decimal digits");
                const auto parsed = std::stoull(text);
                REQUIRE(parsed >= 1 && parsed <= static_cast<unsigned long long>(std::numeric_limits<int>::max()),
                        "path count must be in [1, 2147483647]");
                paths = static_cast<size_t>(parsed);
            }
            RegisterAll_::Init();
            Global::Dates_::SetEvaluationDate(Date_(2026, 1, 2));
            const auto product = Product(terms);
            const auto model = Model(terms);
            Script::MonteCarloSettings_ simulation;
            simulation.compiled_ = true;
            simulation.smooth_ = 0.001;
            const auto plain = Script::MCSimulation<double>(product, model, paths, {}, simulation);
            const auto aad = Script::MCSimulation<AAD::Number_>(product, model, paths, {}, simulation);
            const double pv = plain.aggregated_ / static_cast<double>(paths);
            const double aadPv = aad.aggregated_ / static_cast<double>(paths);
            REQUIRE(std::isfinite(pv) && std::isfinite(aadPv), "autocallable prices must be finite");
            std::cout << '\n'
                      << terms.title_ << "; notional 1; ACT/365F; " << paths << " Sobol paths\n"
                      << std::left << std::setw(42) << "Method / input derivative" << std::right << std::setw(20) << "PV / derivative" << '\n'
                      << std::string(62, '-') << '\n'
                      << std::left << std::setw(42) << "MC double" << std::right << std::setw(20) << ExampleFloat(pv, 6) << '\n'
                      << std::left << std::setw(42) << "MC AAD (smoothed barriers)" << std::right << std::setw(20) << ExampleFloat(aadPv, 6) << '\n';
            for (size_t i = 0; i < aad.names_.size(); ++i) {
                REQUIRE(std::isfinite(aad.risks_[i]), "autocallable derivatives must be finite");
                std::cout << std::left << std::setw(42) << aad.names_[i] << std::right << std::setw(20) << ExampleFloat(aad.risks_[i], 6) << '\n';
            }
            std::cout << std::string(62, '-') << "\nDerivatives hold contract terms fixed; rate/vol/div inputs are decimal units.\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
} // namespace Dal::Examples::Autocallables
