#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <rapidjson/istreamwrapper.h>

#include <dal/model/factory.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/script/simulation.hpp>

#include "capturecommon.hpp"
#include "mcprotocol.hpp"

namespace DalTest {
    using namespace Dal;
    using Script::MonteCarloSettings_;
    using Script::ScriptValuationSettings_;
    constexpr size_t LEVELS = 4, CASES = 4, OUTPUTS = 20, WIDTH = CASES * OUTPUTS;

    const Json_& Find(const Json_& rows, const String_& id) {
        for (const auto& row : rows.GetArray())
            if (String_(row["id"].GetString()) == id)
                return row;
        THROW("Unknown qualification fixture/grid");
    }

    String_ Decimal(double value) {
        std::ostringstream result;
        result << std::setprecision(17) << value;
        return String_(result.str());
    }

    Vector_<String_> OutputNames() {
        Vector_<String_> result;
        for (const char* prefix : {"c", "p"})
            for (size_t i = 0; i < 5; ++i)
                result.push_back(String_(prefix) + String_(std::to_string(i)));
        result.Append(Vector_<String_>{"df", "forward"});
        for (size_t i = 0; i < 5; ++i)
            result.push_back("parity" + String_(std::to_string(i)));
        result.Append(Vector_<String_>{"mean", "fixed", "delayed"});
        return result;
    }

    String_ PayoffText(const Vector_<>& strikes) {
        String_ result;
        for (size_t i = 0; i < strikes.size(); ++i) {
            const String_ ordinal(std::to_string(i)), strike = Decimal(strikes[i]);
            result += "c" + ordinal + " PAYS MAX(spot()-" + strike + ",0) ";
            result += "p" + ordinal + " PAYS MAX(" + strike + "-spot(),0) ";
            result += "parity" + ordinal + "=c" + ordinal + "-p" + ordinal + " ";
        }
        return result + "df PAYS 1 forward PAYS spot() mean=spot() fixed PAYS 1 delayed PAYS 1 ON 2028-11-15";
    }

    Script::PreparedScript_ Prepared(const Handle_<ModelData_>& data, const String_& expression) {
        auto model = CreateModel<double>(data);
        ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Date_(2026, 11, 16);
        MonteCarloSettings_ simulation;
        simulation.rsg_ = "mrg32";
        simulation.normalPrecision_ = "Precise";
        simulation.useBb_ = false;
        simulation.compiled_ = true;
        const Script::ScriptProductData_ product("qualification", {Cell_(Date_(2027, 11, 16))}, {expression});
        return Script::PrepareScript(product, model.get(), valuation, simulation);
    }

    struct Payoff_ {
        Script::PreparedScript_ product_;
        Vector_<size_t> slots_;
        explicit Payoff_(const Handle_<ModelData_>& data, const Vector_<>& strikes) : product_(Prepared(data, PayoffText(strikes))) {
            const auto names = OutputNames();
            for (const auto& name : names) {
                const auto& vars = product_.Product().VarNames();
                const auto found = std::find(vars.begin(), vars.end(), name);
                REQUIRE(found != vars.end(), "Missing qualification payoff output");
                slots_.push_back(static_cast<size_t>(found - vars.begin()));
            }
        }
    };

    struct Level_ {
        Vector_<> times_;
        Vector_<AAD::SampleDef_> definitions_;
        std::unique_ptr<AAD::Model_<double>> model_;
        AAD::Scenario_<> path_, event_;
        Vector_<size_t> samples_;
        std::vector<Script::EvalState_<double>> evaluators_;
        const std::vector<std::shared_ptr<const Payoff_>>& payoffs_;

        Level_(const Handle_<ModelData_>& data,
               Vector_<> times,
               const Vector_<>& maturities,
               const std::vector<std::shared_ptr<const Payoff_>>& payoffs)
            : times_(std::move(times)), definitions_(times_.size()), model_(CreateModel<double>(data)), payoffs_(payoffs) {
            for (size_t i = 0; i < maturities.size(); ++i) {
                const auto found = std::find(times_.begin(), times_.end(), maturities[i]);
                REQUIRE(found != times_.end(), "Qualification maturity absent from timeline");
                const size_t sample = static_cast<size_t>(found - times_.begin());
                samples_.push_back(sample);
                definitions_[sample] = payoffs[i]->product_.DefLine().front();
                definitions_[sample].discountMats_ = {2.25};
                evaluators_.push_back(payoffs[i]->product_.BuildEvalState<double>());
            }
            model_->Allocate(times_, definitions_);
            model_->Init(times_, definitions_);
            REQUIRE(model_->SimDim() == times_.size() - 1, "Production introduced a non-nested internal step");
            AAD::AllocatePath(definitions_, path_);
            AAD::InitializePath(path_);
            AAD::AllocatePath(Vector_<AAD::SampleDef_>{definitions_[samples_.front()]}, event_);
            AAD::InitializePath(event_);
        }

        void Values(const Vector_<>& normals, Vector_<>* values) {
            model_->GeneratePath(normals, &path_);
            values->Resize(WIDTH);
            for (size_t i = 0; i < CASES; ++i) {
                const auto& sample = path_[samples_[i]];
                event_[0].spot_ = sample.spot_;
                event_[0].numeraire_ = sample.numeraire_;
                event_[0].observations_ = sample.observations_;
                event_[0].discounts_ = sample.discounts_;
                payoffs_[i]->product_.CompiledProgram().Evaluate(event_, evaluators_[i]);
                for (size_t output = 0; output < OUTPUTS; ++output)
                    (*values)[i * OUTPUTS + output] = evaluators_[i].VarVals()[payoffs_[i]->slots_[output]];
            }
        }
    };

    struct Family_ {
        Vector_<> anchors_, maturities_;
        std::array<Vector_<>, LEVELS> times_;
        std::array<Handle_<ModelData_>, LEVELS> data_;
        std::vector<std::shared_ptr<const Payoff_>> payoffs_;

        Family_(const Json_& fixture, const Json_& grid) {
            const auto fit = FitMarketIvSurface(Request(fixture));
            REQUIRE(fit.IsSuccess(), fit.Failure());
            const auto surface = CalibrateMarketIvDupire(fit.Snapshot(), {Axis(grid["spots"]), Axis(grid["times"]), {}});
            REQUIRE(surface.IsSuccess(), surface.Failure());
            MarketIvLocalVolModelSettings_ settings;
            settings.index_ = "EQ[A]";
            settings.currency_ = "USD";
            settings.factor_ = "W_EQ";
            settings.horizon_ = 2.25;
            for (size_t level = 0; level < LEVELS; ++level) {
                settings.maxStep_ = 1. / (48 * (size_t{1} << level));
                data_[level] = Handle_<ModelData_>(BuildMarketIvLocalVolModelData("qualification", surface, settings));
            }
            anchors_ = surface.Request().times_;
            anchors_.push_back(0.);
            for (const auto& term : fixture["mc_cases"].GetArray()) {
                maturities_.push_back(Number(term["time"]));
                payoffs_.push_back(std::make_shared<Payoff_>(data_.back(), Axis(term["strikes"])));
            }
            anchors_.Append(maturities_);
            const auto* market = dynamic_cast<const MarketIvLocalVolModelData_*>(data_[0].get());
            for (const auto& term : market->CarryTerms())
                if (term.time_ <= 2.25)
                    anchors_.push_back(term.time_);
            std::sort(anchors_.begin(), anchors_.end());
            anchors_.erase(std::unique(anchors_.begin(), anchors_.end()), anchors_.end());
            for (size_t level = 0; level < LEVELS; ++level)
                times_[level] = NestedTimeline(anchors_, level);
        }
    };

    double Align(const Family_& family) {
        const auto prepared = Prepared(family.data_[0], "call PAYS MAX(spot()-100,0)");
        auto model = CreateModel<double>(family.data_[0]);
        model->Allocate(prepared.TimeLine(), prepared.DefLine());
        model->Init(prepared.TimeLine(), prepared.DefLine());
        auto defaultRng = Script::CreateRNG("mrg32", model->SimDim(), false, {}, "Precise");
        auto scalar = New(RNGType_("MRG32"), 1024, 1, true);
        Vector_<> left(model->SimDim()), right(model->SimDim());
        AAD::Scenario_<> path;
        AAD::AllocatePath(prepared.DefLine(), path);
        AAD::InitializePath(path);
        auto evaluator = prepared.BuildEvalState<double>();
        double sum = 0.;
        for (size_t ordinal = 0; ordinal < 64; ++ordinal) {
            defaultRng->FillNormal(&left);
            scalar->FillNormal(&right);
            REQUIRE(left == right, "Default/scalar MRG32 normal path mismatch");
            model->GeneratePath(left, &path);
            const auto first = path.front();
            model->GeneratePath(right, &path);
            REQUIRE(path.front().spot_ == first.spot_ && path.front().numeraire_ == first.numeraire_, "Default/scalar model path mismatch");
            prepared.CompiledProgram().Evaluate(path, evaluator);
            sum += evaluator.VarVals()[prepared.PayOffIdx()];
        }
        auto* pool = ThreadPool_::GetInstance();
        const size_t previous = pool->NumThreads();
        pool->Start(1, true);
        const double ordinary = Script::MCSimulation<double>(prepared, family.data_[0], 64, "mrg32", false, true).aggregated_;
        pool->Start(previous, true);
        REQUIRE(std::abs(sum - ordinary) <= 1e-10, "Default/scalar passive price mismatch");
        return sum / 64;
    }

    using Sums_ = std::array<Vector_<>, LEVELS * 2 - 1>;

    Sums_ ZeroSums() {
        Sums_ result;
        for (auto& group : result)
            group = Vector_<>(WIDTH, 0.);
        return result;
    }

    void Accumulate(double value, double* sum, double* correction) {
        const double adjusted = value - *correction;
        const double next = *sum + adjusted;
        *correction = (next - *sum) - adjusted;
        *sum = next;
    }

    Sums_ Batch(const Family_& family, size_t batch, size_t from, size_t to) {
        std::vector<std::unique_ptr<Level_>> levels;
        std::vector<BrownianMap_> maps;
        for (size_t level = 0; level < LEVELS; ++level) {
            levels.push_back(std::make_unique<Level_>(family.data_[level], family.times_[level], family.maturities_, family.payoffs_));
            maps.emplace_back(family.times_.back(), family.times_[level]);
        }
        Sums_ sums = ZeroSums(), corrections = ZeroSums();
        auto rng = New(RNGType_("MRG32"), 1024, 1, true);
        const size_t dimension = family.times_.back().size() - 1;
        rng->SkipNormalTo(ScalarOffset(batch, from, dimension));
        Vector_<> fine(dimension), normal;
        std::array<Vector_<>, LEVELS> values;
        for (size_t path = from; path < to; ++path) {
            rng->FillNormal(&fine);
            for (size_t level = 0; level < LEVELS; ++level) {
                maps[level].Apply(fine, &normal);
                levels[level]->Values(normal, &values[level]);
                for (size_t output = 0; output < WIDTH; ++output)
                    Accumulate(values[level][output], &sums[level][output], &corrections[level][output]);
            }
            for (size_t pair = 0; pair + 1 < LEVELS; ++pair)
                for (size_t output = 0; output < WIDTH; ++output)
                    Accumulate(values[pair][output] - values[pair + 1][output], &sums[LEVELS + pair][output], &corrections[LEVELS + pair][output]);
        }
        return sums;
    }

    std::pair<double, double> Statistic(const std::array<Sums_, BATCHES>& sums, size_t group, size_t output, size_t paths) {
        std::array<double, BATCHES> means{};
        for (size_t batch = 0; batch < BATCHES; ++batch)
            means[batch] = sums[batch][group][output] / paths;
        return Estimate(means);
    }

    bool Sufficient(const std::array<Sums_, BATCHES>& sums, size_t paths) {
        for (size_t group = 0; group < LEVELS * 2 - 1; ++group)
            for (size_t output = 0; output < WIDTH; ++output) {
                const double limit = group < LEVELS ? .01 : .005;
                if (Statistic(sums, group, output, paths).second > limit)
                    return false;
            }
        return true;
    }

    void Tier(Writer_* writer, const std::array<Sums_, BATCHES>& sums, size_t paths, double seconds) {
        writer->StartObject();
        writer->Key("paths_per_batch");
        writer->Uint64(paths);
        Number(writer, "seconds", seconds);
        writer->Key("se_sufficient");
        writer->Bool(Sufficient(sums, paths));
        writer->Key("groups");
        writer->StartArray();
        for (size_t group = 0; group < LEVELS * 2 - 1; ++group) {
            writer->StartObject();
            writer->Key("estimates");
            writer->StartArray();
            for (size_t output = 0; output < WIDTH; ++output) {
                const auto value = Statistic(sums, group, output, paths);
                writer->StartArray();
                writer->Double(value.first);
                writer->Double(value.second);
                writer->EndArray();
            }
            writer->EndArray();
            writer->Key("batch_means");
            writer->StartArray();
            for (size_t batch = 0; batch < BATCHES; ++batch) {
                writer->StartArray();
                for (const double value : sums[batch][group])
                    writer->Double(value / paths);
                writer->EndArray();
            }
            writer->EndArray();
            writer->EndObject();
        }
        writer->EndArray();
        writer->EndObject();
    }

    void Extend(const Family_& family, size_t from, size_t to, std::array<Sums_, BATCHES>* sums) {
        std::array<std::future<Sums_>, BATCHES> tasks;
        for (size_t batch = 0; batch < BATCHES; ++batch)
            tasks[batch] = std::async(std::launch::async, [&family, batch, from, to] { return Batch(family, batch, from, to); });
        for (size_t batch = 0; batch < BATCHES; ++batch) {
            const auto increment = tasks[batch].get();
            for (size_t group = 0; group < LEVELS * 2 - 1; ++group)
                for (size_t output = 0; output < WIDTH; ++output)
                    (*sums)[batch][group][output] += increment[group][output];
        }
    }

    void Run(Writer_* writer, const Family_& family, size_t maxExponent) {
        std::array<Sums_, BATCHES> sums;
        for (auto& batch : sums)
            batch = ZeroSums();
        size_t previous = 0;
        writer->Key("tiers");
        writer->StartArray();
        for (size_t exponent = std::min(size_t{16}, maxExponent); exponent <= maxExponent; ++exponent) {
            const size_t paths = size_t{1} << exponent;
            const auto start = std::chrono::steady_clock::now();
            Extend(family, previous, paths, &sums);
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            Tier(writer, sums, paths, seconds);
            std::cerr << "tier=" << exponent << " paths/batch=" << paths << " seconds=" << seconds << " SE=" << Sufficient(sums, paths) << '\n';
            previous = paths;
            if (Sufficient(sums, paths))
                break;
        }
        writer->EndArray();
    }
} // namespace DalTest

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: dal_market_iv_mc request.json result.json fixture grid max-exponent\n";
        return 1;
    }
    try {
        Dal::RegisterAll_::Init();
        std::ifstream input(argv[1]);
        rapidjson::IStreamWrapper source(input);
        rapidjson::Document request;
        request.ParseStream<rapidjson::kParseFullPrecisionFlag>(source);
        REQUIRE(!request.HasParseError(), "Invalid MC qualification request");
        const size_t maximum = std::stoul(argv[5]);
        REQUIRE(maximum <= 20, "Qualification sample ceiling is 2^20 per batch");
        const DalTest::Family_ family(DalTest::Find(request["fixtures"], argv[3]), DalTest::Find(request["grids"], argv[4]));
        std::ofstream output(argv[2]);
        rapidjson::OStreamWrapper destination(output);
        DalTest::Writer_ writer(destination);
        writer.StartObject();
        DalTest::Text(&writer, "fixture", argv[3]);
        DalTest::Text(&writer, "grid", argv[4]);
        DalTest::Text(&writer, "sampling", maximum == 20 ? "FORMAL" : "SMOKE_NOT_QUALIFIED");
        DalTest::Number(&writer, "seed", 1024);
        DalTest::Number(&writer, "degrees_of_freedom", 15);
        DalTest::Number(&writer, "pmax", DalTest::MAX_PATHS);
        const size_t dimension = family.times_.back().size() - 1;
        DalTest::CheckedProduct(DalTest::BATCHES * DalTest::MAX_PATHS, dimension);
        DalTest::Number(&writer, "dmax", dimension);
        DalTest::Number(&writer, "default_alignment_price", DalTest::Align(family));
        DalTest::Vector(&writer, "anchors", family.anchors_);
        writer.Key("timelines");
        writer.StartArray();
        for (const auto& timeline : family.times_) {
            writer.StartArray();
            for (const double time : timeline)
                writer.Double(time);
            writer.EndArray();
        }
        writer.EndArray();
        DalTest::Run(&writer, family, maximum);
        writer.EndObject();
        output << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
