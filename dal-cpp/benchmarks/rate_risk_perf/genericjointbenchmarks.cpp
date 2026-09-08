// Post-construction generic joint quote risk versus native nodes plus one dense transform.

#include "genericjointbenchmarks.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>

#include <dal/benchmarks/bench.hpp>
#include <dal/curve/ratecashflowpricing_internal.hpp>
#include <dal/math/matrix/matrixarithmetic.hpp>

// Keep the numerical oracle and performance workloads on the same curve fixtures.
#include "../../tests/curve/jointquoteriskfixtures.hpp"

namespace Dal::RateRiskPerf {
    namespace {
        struct Workload_ {
            RatePricingMarket_ market_;
            Vector_<RateTradeDefinition_> trades_;
            Vector_<RateQuoteRiskProvenance_> provenances_;
        };

        Workload_ Workload(int width, int tradeCount) {
            using namespace JointQuoteRiskFixtures;
            const auto spec = Spec(width, 3, CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD, true);
            JointMultiCurveCalibrationOptions_ options;
            options.computeEffJacobianInverse_ = true;
            const auto result = CalibrateJointMultiCurve(spec, options);
            Workload_ workload;
            workload.market_ = Market(spec, result);
            workload.provenances_.push_back(BuildJointMultiCurveQuoteRiskProvenance(spec, result, options, workload.market_, Config(3)));
            for (int index = 0; index < tradeCount; ++index) {
                auto trade = Irs(spec, 1, index % 3 == 0 ? -250000.0 : 1000000.0);
                trade.instrumentId_ = "joint-bench:" + String::FromInt(index);
                workload.trades_.push_back(trade);
            }
            return workload;
        }

        Vector_<> Reference(const Workload_& workload) {
            const auto cells =
                RateCashflowPricingInternal::JointNodeSensitivitiesBatch(workload.trades_, workload.market_, {"curve:0", "curve:1", "curve:2"});
            const auto& provenance = workload.provenances_.front();
            const auto& inverse = provenance.EffectiveInverse();
            Vector_<> gradient(inverse.Rows(), 0.0);
            REQUIRE(cells.size() == 3 * workload.trades_.size(), "Joint benchmark reference lost a node cell");
            for (int index = 0; index < static_cast<int>(cells.size()); ++index) {
                const auto& cell = cells[index];
                if (index % 3 == 2) {
                    REQUIRE(!cell.result_.eligible_ && cell.result_.reason_ == "TRADE_DOES_NOT_DEPEND_ON_COMPONENT",
                            "Joint benchmark structural zero changed");
                    continue;
                }
                const auto& range = provenance.Axis().parameterRanges_[index % 3];
                REQUIRE(cell.result_.eligible_ && static_cast<int>(cell.result_.gradient_.size()) == range.size_,
                        "Joint benchmark reference lost an eligible node cell");
                for (int parameter = 0; parameter < range.size_; ++parameter)
                    gradient[range.offset_ + parameter] += cell.result_.gradient_[parameter];
            }
            Vector_<> quotes;
            Matrix::Multiply(gradient, inverse, &quotes);
            for (double& quote : quotes)
                quote /= provenance.Tolerance();
            return quotes;
        }

        Vector_<> Aggregate(const Workload_& workload) {
            const auto result = AggregateRatePortfolioQuoteRisk(workload.trades_, workload.market_, workload.provenances_);
            REQUIRE(result.provenanceFailures_.empty() && result.meta_.size() == workload.trades_.size(),
                    "Joint benchmark provenance or trade failed");
            for (const auto& meta : result.meta_)
                REQUIRE(meta.eligible_, "Joint benchmark lost an eligible trade");
            Vector_<> quotes;
            for (const auto& bucket : result.buckets_) {
                REQUIRE(bucket.actualPvCcy_ == Ccy_("USD") && bucket.dv01_ == bucket.dPvDDecimalQuote_ * 1.0e-4, "Joint benchmark units changed");
                quotes.push_back(bucket.dPvDDecimalQuote_);
            }
            return quotes;
        }

        struct Counters_ {
            int calibrations_ = CurveCalibrationInvocationCount();
            int provenance_ = RateCashflowPricingInternal::g_quoteRiskProvenancePreparationCount.load();
            int nodes_ = RateCashflowPricingInternal::g_nodeSensitivityPreparationCount.load();
            int sweeps_ = RateCashflowPricingInternal::g_nodeSensitivitySweepCount.load();
        };

        void WriteObservation(int width, int trades, int sample, bool reference, int64_t ns, const Counters_& before, const Counters_& after) {
            const int calibrations = after.calibrations_ - before.calibrations_;
            const int preparations = after.provenance_ - before.provenance_;
            const int nodes = after.nodes_ - before.nodes_;
            const int sweeps = after.sweeps_ - before.sweeps_;
            REQUIRE(calibrations == 0 && preparations == (reference ? 0 : 1),
                    "Joint benchmark recalibration or provenance preparation count drifted");
            REQUIRE(nodes == 3 && sweeps == 2 * trades, "Joint benchmark prepared or swept an unexpected number of native blocks");
            if (sample < 0)
                return;
            if (const char* path = std::getenv("DAL_JOINT_QUOTE_RISK_BENCHMARK_FILE")) {
                std::ofstream output(path, std::ios::app);
                REQUIRE(output.is_open(), "Cannot open generic joint benchmark evidence");
                if (output.tellp() == 0)
                    output << "width,trades,layered,mode,sample,side,nanoseconds,calibrations,provenance_preparations,node_preparations,sweeps\n";
                output << width << ',' << trades << ",1,ANALYTIC," << sample << ',' << (reference ? "reference" : "aggregate") << ',' << ns << ','
                       << calibrations << ',' << preparations << ',' << nodes << ',' << sweeps << '\n';
                REQUIRE(output.good(), "Cannot write generic joint benchmark evidence");
            }
        }

        int64_t Observe(const Workload_& workload, int width, int trades, int sample, bool reference, const Vector_<>& expected) {
            const Counters_ before;
            const auto start = std::chrono::steady_clock::now();
            const auto actual = reference ? Reference(workload) : Aggregate(workload);
            const auto stop = std::chrono::steady_clock::now();
            const Counters_ after;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
            REQUIRE(actual.size() == expected.size(), "Joint benchmark quote width changed");
            for (int quote = 0; quote < static_cast<int>(actual.size()); ++quote)
                REQUIRE(std::isfinite(actual[quote]) &&
                            std::abs(actual[quote] - expected[quote]) <= 1.0e-10 * std::max(1.0, std::abs(expected[quote])),
                        "Joint benchmark node transform differs from quote risk");
            Bench::DoNotOptimize(&actual);
            WriteObservation(width, trades, sample, reference, ns, before, after);
            return ns;
        }

        Bench::Result_ Summary(const std::string& name, std::vector<int64_t> times) {
            std::sort(times.begin(), times.end());
            return {name, times[times.size() / 2], times.front(), times.back(), static_cast<int>(times.size())};
        }

        void RunCase(int width, int trades) {
            const auto workload = Workload(width, trades);
            const auto expected = Reference(workload);
            std::vector<int64_t> aggregateTimes, referenceTimes;
            for (int sample = -3; sample < 10; ++sample) {
                const bool referenceFirst = sample % 2 == 0;
                const auto first = Observe(workload, width, trades, sample, referenceFirst, expected);
                const auto second = Observe(workload, width, trades, sample, !referenceFirst, expected);
                if (sample >= 0) {
                    referenceTimes.push_back(referenceFirst ? first : second);
                    aggregateTimes.push_back(referenceFirst ? second : first);
                }
            }
            const auto suffix = " (" + std::to_string(trades) + " IRS x N=" + std::to_string(width) + ")";
            const auto aggregate = Summary("Quote risk generic joint" + suffix, aggregateTimes);
            const auto reference = Summary("Quote risk generic joint node reference" + suffix, referenceTimes);
            Bench::Print(aggregate);
            Bench::Print(reference);
            const double overhead = 100.0 * (static_cast<double>(aggregate.minNs) / static_cast<double>(reference.minNs) - 1.0);
            std::fprintf(stderr, "Generic joint N=%d trades=%d: overhead=%.3f%% calibrations=0 provenance_preparations=1 sweeps=%d\n", width, trades,
                         overhead, 2 * trades);
            REQUIRE(overhead <= 20.0, "Generic joint steady-state overhead exceeds 20 percent");
        }
    } // namespace

    void RunGenericJointBenchmarks() {
        for (int width : {5, 10, 16})
            for (int trades : {100, 1000})
                RunCase(width, trades);
    }
} // namespace Dal::RateRiskPerf
