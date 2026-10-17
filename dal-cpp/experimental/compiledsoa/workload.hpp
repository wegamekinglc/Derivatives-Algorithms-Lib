#pragma once

#include <cstring>

#include <dal/math/random/sobol.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/globals.hpp>

#include "soaevaluator.hpp"

namespace Dal::Script::SoaStudy {
    inline ScriptProduct_ Product(const std::string& name) {
        const Date_ start(2024, 1, 1), maturity(2025, 1, 1);
        Vector_<Cell_> dates{Cell_("STRIKE")};
        Vector_<String_> events{"100.0"};
        const bool barrier = name == "weekly" || name == "divergent";
        if (barrier) {
            dates.push_back(Cell_("BARRIER"));
            events.push_back(name == "divergent" ? "110.0" : "150.0");
            dates.push_back(Cell_(start));
            events.push_back("alive = 1");
            dates.push_back(Cell_("START: 2024-01-01 END: 2025-01-01 FREQ: 1W"));
            events.push_back("if spot() >= BARRIER then alive = 0 end");
        } else {
            REQUIRE(name == "vanilla", "unknown workload");
        }
        dates.push_back(Cell_(maturity));
        events.push_back(barrier ? "pay pays alive * MAX(spot() - STRIKE, 0.0)" : "pay pays MAX(spot() - STRIKE, 0.0)");
        auto dateScope = XGLOBAL::SetEvaluationDateInScope(start);
        ScriptProduct_ product(dates, events, "pay");
        product.PreProcess(false, false);
        return product;
    }

    inline Vector_<Vector_<>> Shocks(size_t paths, size_t dimensions) {
        Vector_<Vector_<>> result(paths, Vector_<>(dimensions));
        auto rng = NewSobol(static_cast<int>(dimensions), 0, false, false);
        for (auto& path : result)
            rng->FillNormal(&path);
        return result;
    }

    inline uint64_t ShockFingerprint(const Vector_<Vector_<>>& shocks) {
        uint64_t hash = 14695981039346656037ULL;
        for (const auto& path : shocks)
            for (const double value : path) {
                uint64_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                hash ^= bits;
                hash *= 1099511628211ULL;
            }
        return hash;
    }

    class BatchModel_ {
        Vector_<> drifts_, stds_, numeraires_;
        bool today_;
        double spot_, logSpot_;

    public:
        BatchModel_(const Vector_<>& timeline,
                    const Vector_<AAD::SampleDef_>& definitions,
                    double spot = 100.0,
                    double vol = 0.2,
                    double rate = 0.05,
                    double div = 0.02)
            : today_(!timeline.empty() && timeline[0] == 0.0), spot_(spot), logSpot_(std::log(spot)) {
            const AAD::BlackScholes_<double> validation(spot, vol, rate, div);
            validation.ValidateTimeline(timeline, definitions);
            REQUIRE(!timeline.empty(), "SoA BS: empty timeline");
            for (size_t e = 0; e < timeline.size(); ++e) {
                const auto& definition = definitions[e];
                REQUIRE(definition.indexNames_.empty() && definition.discountMats_.empty() && definition.liborDefs_.empty(),
                        "SoA BS: only legacy spot and numeraire samples are supported");
                numeraires_.push_back(definition.numeraire_ ? AAD::Detail::BlackScholesNumeraire(rate, timeline[e]) : 1.0);
                if (today_ && e == 0)
                    continue;
                const double previous = e == 0 ? 0.0 : timeline[e - 1];
                const auto coefficients = AAD::Detail::BlackScholesCoefficients(vol, rate - div, timeline[e] - previous);
                drifts_.push_back(coefficients.drift_);
                stds_.push_back(coefficients.std_);
            }
        }

        [[nodiscard]] const Vector_<>& Numeraires() const { return numeraires_; }

        template <size_t L_> void Generate(const Vector_<Vector_<>>& shocks, size_t first, size_t count, std::vector<Row_<L_>>* spots) const {
            REQUIRE(count > 0 && count <= L_ && first <= shocks.size() && count <= shocks.size() - first, "SoA BS: invalid block");
            REQUIRE(spots->size() == numeraires_.size(), "SoA BS: output size mismatch");
            for (size_t lane = 0; lane < count; ++lane)
                REQUIRE(shocks[first + lane].size() == drifts_.size(), "SoA BS: shock dimension mismatch");
            Row_<L_> logSpot;
            std::fill_n(logSpot.begin(), count, logSpot_);
            if (today_)
                std::fill_n((*spots)[0].begin(), count, spot_);
            for (size_t step = 0; step < drifts_.size(); ++step)
                for (size_t lane = 0; lane < count; ++lane) {
                    logSpot[lane] += drifts_[step] + stds_[step] * shocks[first + lane][step];
                    (*spots)[step + static_cast<size_t>(today_)][lane] = std::exp(logSpot[lane]);
                }
        }
    };
} // namespace Dal::Script::SoaStudy
