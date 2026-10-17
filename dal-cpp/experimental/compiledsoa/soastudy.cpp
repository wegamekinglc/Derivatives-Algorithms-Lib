#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#include <dal/benchmarks/bench.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/platform.hpp>

#include "workload.hpp"

namespace Dal::Script::SoaStudy {
    struct Options_ {
        std::string workload_ = "weekly", engine_ = "soa", phase_ = "eval";
        size_t paths_ = 100000, lanes_ = 32, iterations_ = 1;
        bool fallback_ = true;
    };

    size_t ReadCount(const std::string& value) {
        size_t count = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
        REQUIRE(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && count > 0, "invalid count");
        return count;
    }

    void ReadOption(const std::string& key, const std::string& value, Options_* result) {
        const std::map<std::string, std::string Options_::*> strings{
            {"--workload", &Options_::workload_}, {"--engine", &Options_::engine_}, {"--phase", &Options_::phase_}};
        const std::map<std::string, size_t Options_::*> counts{
            {"--paths", &Options_::paths_}, {"--lanes", &Options_::lanes_}, {"--iterations", &Options_::iterations_}};
        if (const auto found = strings.find(key); found != strings.end())
            result->*(found->second) = value;
        else if (key == "--fallback") {
            REQUIRE(value == "true" || value == "false", "invalid fallback flag");
            result->fallback_ = value == "true";
        } else {
            const auto found = counts.find(key);
            REQUIRE(found != counts.end(), "unknown option " + key);
            result->*(found->second) = ReadCount(value);
        }
    }

    void ValidateOptions(const Options_& result) {
        REQUIRE(result.paths_ <= 1000000 && result.iterations_ <= 1000000, "request too large");
        REQUIRE(result.engine_ == "scalar" || result.engine_ == "soa", "invalid engine");
        REQUIRE(result.phase_ == "eval" || result.phase_ == "model" || result.phase_ == "pipeline" || result.phase_ == "end_to_end", "invalid phase");
    }

    Options_ ReadOptions(int argc, char** argv) {
        REQUIRE(argc % 2 == 1, "expected option/value pairs");
        Options_ result;
        for (int i = 1; i < argc; i += 2)
            ReadOption(argv[i], argv[i + 1], &result);
        ValidateOptions(result);
        return result;
    }

    template <size_t L_> class Study_ {
        const Options_ options_;
        const ScriptProduct_ product_;
        const ScriptCompiled_ compiled_;
        const Plan_ plan_;
        typename Plan_::State_<L_> state_;
        EvalState_<double> scalarState_;
        const BatchModel_ batch_;
        AAD::BlackScholes_<double> model_;
        Vector_<Vector_<>> shocks_;
        AAD::Scenario_<double> path_;
        std::vector<std::vector<Row_<L_>>> inputs_;
        Vector_<AAD::Scenario_<double>> scalarInputs_;
        std::vector<Row_<L_>> generated_;
        Vector_<Vector_<>> randomBlock_;
        Vector_<> randomPath_;

        void PrepareInputs() {
            inputs_.resize((options_.paths_ + L_ - 1) / L_, std::vector<Row_<L_>>(path_.size()));
            if (options_.phase_ == "eval")
                scalarInputs_.Resize(options_.paths_);
            for (size_t p = 0; p < options_.paths_; ++p) {
                model_.GeneratePath(shocks_[p], &path_);
                if (options_.phase_ == "eval")
                    scalarInputs_[p] = path_;
                for (size_t e = 0; e < path_.size(); ++e)
                    inputs_[p / L_][e][p % L_] = path_[e].spot_;
            }
        }

        double ScalarPayoff(size_t p, bool generate, const Vector_<>& normal) {
            if (generate)
                model_.GeneratePath(normal, &path_);
            if (options_.phase_ == "model") {
                Bench::DoNotOptimize(path_.data());
                return path_.back().spot_;
            }
            compiled_.Evaluate(generate ? path_ : scalarInputs_[p], scalarState_);
            return scalarState_.VarVals()[product_.PayOffIdx()];
        }

        double ScalarRequest(SequenceSet_* rng) {
            double sum = 0.0;
            for (size_t p = 0; p < options_.paths_; ++p) {
                if (rng)
                    rng->FillNormal(&randomPath_);
                sum += ScalarPayoff(p, options_.phase_ != "eval", rng ? randomPath_ : shocks_[p]);
            }
            return sum;
        }

        void AccumulateBatch(const std::vector<Row_<L_>>& spots, size_t count, double* sum) {
            if (options_.phase_ == "model") {
                Bench::DoNotOptimize(spots.data());
                for (size_t lane = 0; lane < count; ++lane)
                    *sum += spots.back()[lane];
            } else {
                plan_.Evaluate(spots, batch_.Numeraires(), count, options_.fallback_, &state_, &counters_);
                for (size_t lane = 0; lane < count; ++lane)
                    *sum += state_.variables_[product_.PayOffIdx()][lane];
            }
        }

        double BatchRequest(SequenceSet_* rng) {
            double sum = 0.0;
            for (size_t block = 0; block < inputs_.size(); ++block) {
                const size_t first = block * L_, count = std::min(L_, options_.paths_ - first);
                if (rng)
                    for (size_t lane = 0; lane < count; ++lane)
                        rng->FillNormal(&randomBlock_[lane]);
                if (options_.phase_ != "eval")
                    batch_.Generate(rng ? randomBlock_ : shocks_, rng ? 0 : first, count, &generated_);
                AccumulateBatch(options_.phase_ == "eval" ? inputs_[block] : generated_, count, &sum);
            }
            return sum;
        }

    public:
        Counters_ counters_;

        explicit Study_(const Options_& options)
            : options_(options), product_(Product(options.workload_)), compiled_(product_.Compile()),
              plan_(compiled_, product_.VarValues(), product_.ConstVarValues()), state_(plan_), scalarState_(product_.BuildEvalState<double>()),
              batch_(product_.TimeLine(), product_.DefLine()), model_(100.0, 0.2, 0.05, 0.02) {
            model_.Allocate(product_.TimeLine(), product_.DefLine());
            model_.Init(product_.TimeLine(), product_.DefLine());
            shocks_ = Shocks(options.paths_, model_.SimDim());
            AAD::AllocatePath(product_.DefLine(), path_);
            AAD::InitializePath(path_);
            PrepareInputs();
            generated_.resize(path_.size());
            randomBlock_ = Vector_<Vector_<>>(L_, Vector_<>(model_.SimDim()));
            randomPath_.Resize(model_.SimDim());
        }

        double Request() {
            const bool random = options_.phase_ == "end_to_end";
            auto rng = random ? NewSobol(static_cast<int>(model_.SimDim()), 0, false, false) : nullptr;
            const bool soa = options_.engine_ == "soa" && options_.paths_ >= 8;
            return soa ? BatchRequest(rng.get()) : ScalarRequest(rng.get());
        }

        void Print(double seconds, double checksum) const {
            std::cout << std::setprecision(17) << "{\"workload\":\"" << options_.workload_ << "\",\"engine\":\"" << options_.engine_
                      << "\",\"phase\":\"" << options_.phase_ << "\",\"paths\":" << options_.paths_ << ",\"lanes\":" << L_
                      << ",\"iterations\":" << options_.iterations_ << ",\"fallback\":" << (options_.fallback_ ? "true" : "false")
                      << ",\"seconds\":" << seconds << ",\"pv\":" << checksum / options_.paths_ << ",\"shock_fingerprint\":\"" << std::hex
                      << ShockFingerprint(shocks_) << std::dec << "\",\"events\":" << path_.size() << ",\"mixed\":" << counters_.mixed_
                      << ",\"fallback_lanes\":" << counters_.fallbackLanes_ << "}\n";
        }
    };

    template <size_t L_> int Run(const Options_& options) {
        Study_<L_> study(options);
        double checksum = study.Request();
        study.counters_ = {};
        const auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < options.iterations_; ++i) {
            checksum = study.Request();
            Bench::DoNotOptimize(&checksum);
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / options.iterations_;
        study.Print(seconds, checksum);
        return 0;
    }
} // namespace Dal::Script::SoaStudy

int main(int argc, char** argv) {
    try {
        Dal::RegisterAll_::Init(1);
        const auto options = Dal::Script::SoaStudy::ReadOptions(argc, argv);
        switch (options.lanes_) {
        case 8:
            return Dal::Script::SoaStudy::Run<8>(options);
        case 16:
            return Dal::Script::SoaStudy::Run<16>(options);
        case 32:
            return Dal::Script::SoaStudy::Run<32>(options);
        case 64:
            return Dal::Script::SoaStudy::Run<64>(options);
        default:
            THROW("invalid lane width");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
