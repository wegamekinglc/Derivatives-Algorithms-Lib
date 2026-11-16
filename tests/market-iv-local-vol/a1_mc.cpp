#include "a1protocol.hpp"
#include "mcpayoff.hpp"

namespace A1 {
    using namespace Dal;
    using namespace DalTest;
    constexpr size_t MODELS = 8, GROUPS = 14;
    using Sums_ = std::array<Vector_<>, GROUPS>;

    Sums_ Zeros() {
        Sums_ result;
        for (auto& row : result)
            row = Vector_<>(WIDTH, 0.);
        return result;
    }

    struct Family_ {
        size_t ordinal_;
        Vector_<> maturities_;
        std::array<Vector_<>, 4> times_;
        std::array<Handle_<ModelData_>, MODELS> data_;
        std::vector<std::shared_ptr<const Payoff_>> payoffs_;

        Family_(const Json_& request, const Json_& fixture) : ordinal_(String_(fixture["id"].GetString()) == "F2_linear" ? 0 : 1) {
            const auto anchors = Axis(request["protocol"]["anchors"]);
            for (size_t level = 0; level < 4; ++level) {
                times_[level] = A1Timeline(anchors, level);
                REQUIRE(times_[level] == Axis(request["protocol"]["timelines"][level]), "Python/C++ A/1 timeline mismatch");
            }
            const auto fit = FitMarketIvSurface(Request(fixture));
            REQUIRE(fit.IsSuccess(), fit.Failure());
            for (size_t grid = 0; grid < 2; ++grid) {
                const auto& row = Find(request["grids"], grid == 0 ? "P7" : "P8");
                const auto surface = CalibrateMarketIvDupire(fit.Snapshot(), {Axis(row["spots"]), Axis(row["times"]), {}});
                REQUIRE(surface.IsSuccess(), surface.Failure());
                MarketIvLocalVolModelSettings_ settings;
                settings.index_ = "EQ[A]";
                settings.currency_ = "USD";
                settings.factor_ = "W_EQ";
                settings.horizon_ = 2.25;
                for (size_t level = 0; level < 4; ++level) {
                    settings.maxStep_ = 1. / (384 * (size_t{1} << level));
                    data_[grid * 4 + level] = Handle_<ModelData_>(BuildMarketIvLocalVolModelData("A1", surface, settings));
                }
            }
            for (const auto& term : fixture["mc_cases"].GetArray()) {
                maturities_.push_back(Number(term["time"]));
                payoffs_.push_back(std::make_shared<Payoff_>(data_[0], Axis(term["strikes"])));
            }
        }
    };

    double Align(const Handle_<ModelData_>& data) {
        const auto prepared = Prepared(data, "call PAYS MAX(spot()-100,0)");
        auto model = CreateModel<double>(data);
        model->Allocate(prepared.TimeLine(), prepared.DefLine());
        model->Init(prepared.TimeLine(), prepared.DefLine());
        auto ordinary = Script::CreateRNG("mrg32", model->SimDim(), false, {}, "Precise");
        auto scalar = New(RNGType_("MRG32"), 1024, 1, true);
        Vector_<> left(model->SimDim()), right(model->SimDim());
        AAD::Scenario_<> path;
        AAD::AllocatePath(prepared.DefLine(), path);
        AAD::InitializePath(path);
        auto evaluator = prepared.BuildEvalState<double>();
        double total = 0.;
        for (size_t p = 0; p < 64; ++p) {
            ordinary->FillNormal(&left);
            scalar->FillNormal(&right);
            REQUIRE(left == right, "A/1 default coordinate mismatch");
            model->GeneratePath(left, &path);
            const auto sample = path.front();
            model->GeneratePath(right, &path);
            REQUIRE(sample.spot_ == path.front().spot_ && sample.numeraire_ == path.front().numeraire_, "A/1 default path mismatch");
            prepared.CompiledProgram().Evaluate(path, evaluator);
            total += evaluator.VarVals()[prepared.PayOffIdx()];
        }
        auto* pool = ThreadPool_::GetInstance();
        const size_t previous = pool->NumThreads();
        pool->Start(1, true);
        const double reference = Script::MCSimulation<double>(prepared, data, 64, "mrg32", false, true).aggregated_;
        pool->Start(previous, true);
        REQUIRE(std::abs(total - reference) <= 1e-10, "A/1 default price mismatch");
        return total / 64;
    }

    void Accumulate(double value, double* sum, double* correction) {
        const double adjusted = value - *correction, next = *sum + adjusted;
        *correction = (next - *sum) - adjusted;
        *sum = next;
    }

    Sums_ Batch(const Family_& family, size_t batch, size_t from, size_t to) {
        std::vector<std::unique_ptr<Level_>> models;
        std::vector<BrownianMap_> maps;
        for (size_t j = 0; j < MODELS; ++j) {
            models.push_back(std::make_unique<Level_>(family.data_[j], family.times_[j % 4], family.maturities_, family.payoffs_));
            maps.emplace_back(family.times_[3], family.times_[j % 4]);
        }
        auto rng = New(RNGType_("MRG32"), 1024, 1, true);
        rng->SkipNormalTo(A1Address(family.ordinal_, batch, from));
        Sums_ sums = Zeros(), correction = Zeros();
        Vector_<> fine(8728), normal;
        std::array<Vector_<>, MODELS> values;
        for (size_t p = from; p < to; ++p) {
            rng->FillNormal(&fine);
            for (size_t j = 0; j < MODELS; ++j) {
                maps[j].Apply(fine, &normal);
                models[j]->Values(normal, &values[j]);
                for (size_t output = 0; output < WIDTH; ++output)
                    Accumulate(values[j][output], &sums[j][output], &correction[j][output]);
            }
            for (size_t grid = 0; grid < 2; ++grid)
                for (size_t pair = 0; pair < 3; ++pair) {
                    const size_t group = 8 + grid * 3 + pair, left = grid * 4 + pair;
                    for (size_t output = 0; output < WIDTH; ++output)
                        Accumulate(values[left][output] - values[left + 1][output], &sums[group][output], &correction[group][output]);
                }
        }
        return sums;
    }

    std::pair<double, double> Statistic(const std::array<Sums_, 16>& sums, size_t group, size_t output, size_t paths) {
        std::array<double, 16> means{};
        for (size_t b = 0; b < 16; ++b)
            means[b] = sums[b][group][output] / paths;
        return Estimate(means);
    }

    bool Sufficient(const std::array<Sums_, 16>& sums, size_t paths) {
        for (size_t group = 0; group < GROUPS; ++group)
            for (size_t output = 0; output < WIDTH; ++output)
                if (Statistic(sums, group, output, paths).second > (group < 8 ? .01 : .005))
                    return false;
        return true;
    }

    void Tier(Writer_* writer, const std::array<Sums_, 16>& sums, size_t paths, double seconds) {
        writer->StartObject();
        Number(writer, "paths_per_batch", paths);
        Number(writer, "seconds", seconds);
        writer->Key("se_sufficient");
        writer->Bool(Sufficient(sums, paths));
        writer->Key("groups");
        writer->StartArray();
        for (size_t group = 0; group < GROUPS; ++group) {
            writer->StartObject();
            writer->Key("estimates");
            writer->StartArray();
            for (size_t o = 0; o < WIDTH; ++o) {
                const auto estimate = Statistic(sums, group, o, paths);
                writer->StartArray();
                writer->Double(estimate.first);
                writer->Double(estimate.second);
                writer->EndArray();
            }
            writer->EndArray();
            writer->Key("batch_means");
            writer->StartArray();
            for (const auto& batch : sums) {
                writer->StartArray();
                for (double value : batch[group])
                    writer->Double(value / paths);
                writer->EndArray();
            }
            writer->EndArray();
            writer->EndObject();
        }
        writer->EndArray();
        writer->EndObject();
    }

    size_t Restore(const std::string& path, const Family_& family, size_t exponent, std::array<Sums_, 16>* sums) {
        if (path.empty()) {
            REQUIRE(exponent <= 16, "A/1 extension requires exact prior sums");
            return 0;
        }
        std::ifstream input(path);
        rapidjson::IStreamWrapper stream(input);
        rapidjson::Document checkpoint;
        checkpoint.ParseStream<rapidjson::kParseFullPrecisionFlag>(stream);
        REQUIRE(!checkpoint.HasParseError() && Number(checkpoint["family"]) == family.ordinal_, "A/1 checkpoint identity");
        const size_t previous = static_cast<size_t>(Number(checkpoint["paths_per_batch"]));
        REQUIRE(exponent > 16 && previous == (size_t{1} << (exponent - 1)), "A/1 checkpoint prefix mismatch");
        REQUIRE(checkpoint["sums"].Size() == 16, "A/1 checkpoint block count");
        for (size_t b = 0; b < 16; ++b) {
            REQUIRE(checkpoint["sums"][b].Size() == GROUPS, "A/1 checkpoint group count");
            for (size_t group = 0; group < GROUPS; ++group) {
                (*sums)[b][group] = Axis(checkpoint["sums"][b][group]);
                REQUIRE((*sums)[b][group].size() == WIDTH, "A/1 checkpoint output count");
            }
        }
        REQUIRE(!Sufficient(*sums, previous), "A/1 cannot extend sufficient SE");
        return previous;
    }

    void Checkpoint(const std::string& path, const Family_& family, const std::array<Sums_, 16>& sums, size_t paths) {
        std::ofstream output(path);
        rapidjson::OStreamWrapper stream(output);
        Writer_ writer(stream);
        writer.StartObject();
        Number(&writer, "family", family.ordinal_);
        Number(&writer, "paths_per_batch", paths);
        writer.Key("sums");
        writer.StartArray();
        for (const auto& batch : sums) {
            writer.StartArray();
            for (const auto& group : batch) {
                writer.StartArray();
                for (double value : group)
                    writer.Double(value);
                writer.EndArray();
            }
            writer.EndArray();
        }
        writer.EndArray();
        writer.EndObject();
    }

    void Run(Writer_* writer, const Family_& family, size_t exponent, const std::string& prior, const std::string& checkpoint) {
        std::array<Sums_, 16> sums;
        for (auto& batch : sums)
            batch = Zeros();
        const size_t previous = Restore(prior, family, exponent, &sums);
        writer->Key("tiers");
        writer->StartArray();
        const size_t paths = size_t{1} << exponent;
        const auto start = std::chrono::steady_clock::now();
        std::array<std::future<Sums_>, 16> tasks;
        for (size_t b = 0; b < 16; ++b)
            tasks[b] = std::async(std::launch::async, [&family, b, previous, paths] { return Batch(family, b, previous, paths); });
        for (size_t b = 0; b < 16; ++b) {
            const auto addition = tasks[b].get();
            for (size_t group = 0; group < GROUPS; ++group)
                for (size_t output = 0; output < WIDTH; ++output)
                    sums[b][group][output] += addition[group][output];
        }
        Tier(writer, sums, paths, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        Checkpoint(checkpoint, family, sums, paths);
        std::cerr << "A1 tier=" << exponent << " paths/batch=" << paths << " SE=" << Sufficient(sums, paths) << '\n';
        writer->EndArray();
    }
} // namespace A1

int main(int argc, char** argv) {
    if (argc != 5 && argc != 6)
        return 1;
    try {
        Dal::RegisterAll_::Init();
        std::ifstream input(argv[1]);
        rapidjson::IStreamWrapper stream(input);
        rapidjson::Document request;
        request.ParseStream<rapidjson::kParseFullPrecisionFlag>(stream);
        REQUIRE(!request.HasParseError(), "Invalid A/1 request");
        const size_t maximum = std::stoul(argv[4]);
        REQUIRE(maximum <= 20, "A/1 sample cap");
        const A1::Family_ family(request, DalTest::Find(request["fixtures"], argv[3]));
        std::ofstream output(argv[2]);
        rapidjson::OStreamWrapper destination(output);
        DalTest::Writer_ writer(destination);
        writer.StartObject();
        DalTest::Text(&writer, "schema", "dal.market-iv-a1-mc/1");
        DalTest::Text(&writer, "fixture", argv[3]);
        DalTest::Text(&writer, "sampling", maximum >= 16 ? "FORMAL" : "SMOKE_NOT_QUALIFIED");
        DalTest::Number(&writer, "dmax", 8728);
        DalTest::Number(&writer, "seed", 1024);
        for (size_t g = 0; g < 2; ++g)
            DalTest::Number(&writer, g == 0 ? "P7_default_alignment" : "P8_default_alignment", A1::Align(family.data_[g * 4]));
        A1::Run(&writer, family, maximum, argc == 6 ? argv[5] : "", std::string(argv[2]) + ".checkpoint.json");
        writer.EndObject();
        output << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
