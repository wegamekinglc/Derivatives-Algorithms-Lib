#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <dal/model/blackscholes.hpp>
#include <dal/model/hybriddata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/simulation.hpp>
#include <dal/time/dateincrement.hpp>

#include "lsmctrainingstudy.hpp"

using namespace Dal;
using namespace Dal::Script;

namespace {
    using Writer_ = rapidjson::Writer<rapidjson::StringBuffer>;
    using ProductFactory_ = ScriptProductData_ (*)(const String_&, bool);

    struct SampleOptions_ {
        std::string case_ = "positive_control";
        std::string engine_ = "compiled";
        std::string mode_ = "frozen";
        std::string precision_ = "Default";
        std::optional<size_t> training_;
        std::optional<double> volatility_;
        size_t pricing_ = 1024;
        size_t validation_ = 0;
        size_t trainingSeed_ = 17;
        size_t pricingSeed_ = 29;
        size_t replicates_ = 2;
        size_t threads_ = 4;
        bool diagnostics_ = false;
        bool bridge_ = false;
    };

    template <class T_> bool ParseNumber(const std::string& text, T_* value) {
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), *value);
        return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
    }

    bool ReadTraining(const std::string& text, std::optional<size_t>* training) {
        if (text == "default") {
            training->reset();
            return true;
        }
        size_t count = 0;
        if (!ParseNumber(text, &count) || count == 0 || count > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        *training = count;
        return true;
    }

    bool ReadText(const std::string& text, std::string* value) {
        *value = text;
        return true;
    }

    bool ReadBool(const std::string& text, bool* value) {
        if (text != "true" && text != "false")
            return false;
        *value = text == "true";
        return true;
    }

    bool ReadVolatility(const std::string& text, std::optional<double>* value) {
        double volatility = 0.0;
        if (!ParseNumber(text, &volatility) || !std::isfinite(volatility) || volatility < 0.0)
            return false;
        *value = volatility;
        return true;
    }

    template <class T_, class F_>
    bool ReadMapped(const std::map<std::string, T_*>& fields, const std::string& name, const std::string& text, const F_& parse) {
        const auto found = fields.find(name);
        return found != fields.end() && parse(text, found->second);
    }

    bool ReadOption(const std::string& name, const std::string& text, SampleOptions_* options) {
        const std::map<std::string, std::string*> strings{
            {"--case", &options->case_}, {"--engine", &options->engine_}, {"--mode", &options->mode_}, {"--normal-precision", &options->precision_}};
        const std::map<std::string, size_t*> counts{
            {"--pricing", &options->pricing_},          {"--validation", &options->validation_}, {"--training-seed", &options->trainingSeed_},
            {"--pricing-seed", &options->pricingSeed_}, {"--replicates", &options->replicates_}, {"--threads", &options->threads_}};
        const std::map<std::string, bool*> switches{{"--diagnostics", &options->diagnostics_}, {"--brownian-bridge", &options->bridge_}};
        if (ReadMapped(strings, name, text, ReadText) || ReadMapped(counts, name, text, ParseNumber<size_t>) ||
            ReadMapped(switches, name, text, ReadBool))
            return true;
        if (name == "--training")
            return ReadTraining(text, &options->training_);
        return name == "--vol" && ReadVolatility(text, &options->volatility_);
    }

    void ValidateCounts(const SampleOptions_& options) {
        constexpr size_t MAX_INT = static_cast<size_t>(std::numeric_limits<int>::max());
        REQUIRE(options.pricing_ > 0 && options.pricing_ <= std::numeric_limits<uint32_t>::max(), "invalid --pricing count");
        REQUIRE(options.validation_ <= MAX_INT, "invalid --validation count");
        REQUIRE(options.replicates_ >= 2 && options.replicates_ <= MAX_INT, "invalid --replicates count");
        REQUIRE(options.trainingSeed_ <= MAX_INT, "invalid --training-seed");
        REQUIRE(options.pricingSeed_ <= MAX_INT, "invalid --pricing-seed");
        REQUIRE(options.threads_ > 0 && options.threads_ <= 256, "invalid --threads count");
    }

    void ValidateModes(const SampleOptions_& options) {
        REQUIRE(options.engine_ == "tree" || options.engine_ == "compiled", "invalid --engine");
        REQUIRE(options.mode_ == "hard" || options.mode_ == "frozen" || options.mode_ == "retrained", "invalid --mode");
        REQUIRE(options.precision_ == "Default" || options.precision_ == "Fast" || options.precision_ == "Precise", "invalid --normal-precision");
    }

    SampleOptions_ ReadOptions(int argc, char** argv) {
        REQUIRE((argc - 2) % 2 == 0, "expected --option value pairs");
        SampleOptions_ options;
        std::set<std::string> seen;
        for (int index = 2; index < argc; index += 2) {
            const std::string name(argv[index]);
            REQUIRE(seen.insert(name).second, "duplicate option " + name);
            REQUIRE(ReadOption(name, argv[index + 1], &options), "invalid option/value for " + name);
        }
        ValidateCounts(options);
        ValidateModes(options);
        return options;
    }

    struct StudyCase_ {
        const char* name_;
        const char* frequency_;
        const char* payoff_;
        size_t features_;
        double spot_ = 100.0;
        double volatility_ = 0.2;
        double dividend_ = 0.02;
    };

    StudyCase_ FindCase(const std::string& name) {
        static const StudyCase_ CASES[]{
            {"positive_control", "single", "positive", 1},
            {"european_put", "single", "put", 1},
            {"bermudan_atm", "two", "put", 1},
            {"bermudan_itm", "two", "put", 1, 70.0},
            {"bermudan_otm", "two", "put", 1, 130.0},
            {"bermudan_low_vol", "two", "put", 1, 95.0, 0.002},
            {"weekly_put", "1W", "put", 1},
            {"daily_put", "1CD", "put", 1},
            {"weekly_call", "1W", "call", 1, 100.0, 0.2, 0.08},
            {"asian_two", "1W", "asian", 2},
            {"asian_three", "1W", "asian", 3},
            {"local_vol_weekly", "1W", "put", 1},
            {"mixed_original", "1CD", "mixed", 1},
        };
        const auto found = std::find_if(std::begin(CASES), std::end(CASES), [&](const auto& item) { return name == item.name_; });
        REQUIRE(found != std::end(CASES), "unknown --case " + name);
        return *found;
    }

    Vector_<Date_> StudyDates(const StudyCase_& study) {
        const Date_ maturity(2025, 1, 1);
        const std::string frequency(study.frequency_);
        if (frequency == "single")
            return {maturity};
        if (frequency == "two")
            return {Date_(2024, 7, 1), maturity};
        Vector_<Date_> dates;
        const auto increment = Date::ParseIncrement(study.frequency_);
        for (Date_ date(2024, 1, 8); date < maturity; date = increment->FwdFrom(date))
            dates.push_back(date);
        dates.push_back(maturity);
        return dates;
    }

    String_ Intrinsic(const StudyCase_& study) {
        const std::string payoff(study.payoff_);
        if (payoff == "positive")
            return "K + SPOT()";
        if (payoff == "call")
            return "MAX(SPOT() - K, 0)";
        return payoff == "asian" ? "MAX(K - avg, 0)" : "MAX(K - SPOT(), 0)";
    }

    ScriptProductSettings_ StudyContract(const StudyCase_& study) {
        ScriptProductSettings_ contract;
        if (study.features_ > 1) {
            contract.regressionFeatures_ = {"VAR[s]", "VAR[avg]"};
            if (study.features_ == 3)
                contract.regressionFeatures_.push_back("VAR[peak]");
        }
        if (std::string(study.name_) == "local_vol_weekly")
            contract.defaultIndex_ = "EQ[DAL486_STUDY]";
        return contract;
    }

    String_ StudyStatePrefix(const StudyCase_& study, size_t index) {
        if (study.features_ == 1)
            return {};
        return String_(index == 0 ? "runningTotal = 0 n = 0 peak = 0 " : "") +
               "s = SPOT() runningTotal = runningTotal + s n = n + 1 avg = runningTotal / n peak = MAX(peak, s) ";
    }

    String_ StudyEvent(const StudyCase_& study, size_t index, size_t count) {
        const bool exercise = count == 1 || index + 1 < count;
        return StudyStatePrefix(study, index) + (exercise ? "EXERCISE " : "pay PAYS ") + Intrinsic(study);
    }

    ScriptProductData_ StudyProduct(const StudyCase_& study, ProductFactory_ mixedProduct) {
        if (std::string(study.payoff_) == "mixed")
            return mixedProduct(study.frequency_, false);
        const auto dates = StudyDates(study);
        Vector_<Cell_> eventDates{Cell_("K")};
        Vector_<String_> events{"100"};
        for (size_t index = 0; index < dates.size(); ++index) {
            eventDates.push_back(Cell_(dates[index]));
            events.push_back(StudyEvent(study, index, dates.size()));
        }
        return {"LSMC training study", eventDates, events, StudyContract(study)};
    }

    Handle_<ModelData_> StudyModel(const StudyCase_& study, const SampleOptions_& options) {
        const double volatility = options.volatility_.value_or(study.volatility_);
        if (std::string(study.name_) == "local_vol_weekly")
            return MakeFlatRateLocalVolHybridModelData("study", "EQ[DAL486_STUDY]", study.spot_, 0.05, study.dividend_, {50.0, 150.0}, {0.0, 2.0},
                                                       Matrix_<>(2, 2, volatility));
        return Handle_<ModelData_>(new BSModelData_("study", study.spot_, volatility, 0.05, study.dividend_));
    }

    MonteCarloSettings_ StudySettings(const SampleOptions_& options) {
        MonteCarloSettings_ settings;
        settings.compiled_ = options.engine_ == "compiled";
        settings.enableAad_ = options.mode_ != "hard";
        settings.lsmcPolicyRiskMode_ = options.mode_ == "retrained" ? "RetrainedBump" : "Frozen";
        if (options.training_)
            settings.lsmcTrainingPaths_ = static_cast<int>(*options.training_);
        if (options.validation_)
            settings.lsmcValidationPaths_ = static_cast<int>(options.validation_);
        settings.lsmcRqmcReplicates_ = static_cast<int>(options.replicates_);
        settings.lsmcTrainingSeed_ = static_cast<int>(options.trainingSeed_);
        settings.lsmcPricingSeed_ = static_cast<int>(options.pricingSeed_);
        settings.normalPrecision_ = options.precision_.c_str();
        settings.useBb_ = options.bridge_;
        return settings;
    }

    template <class V_> void WriteNumbers(Writer_* writer, const V_& values) {
        writer->StartArray();
        for (const auto& value : values)
            writer->Double(static_cast<double>(value));
        writer->EndArray();
    }

    void WriteText(Writer_* writer, const char* key, const char* value) {
        writer->Key(key);
        writer->String(value);
    }

    void WriteDouble(Writer_* writer, const char* key, double value) {
        writer->Key(key);
        writer->Double(value);
    }

    size_t ExerciseCount(const PreparedScript_& prepared) {
        size_t count = 0;
        for (const auto& event : prepared.Product().Events())
            for (const auto& statement : event)
                count += static_cast<size_t>(dynamic_cast<const NodeExercise_*>(statement.get()) != nullptr);
        return count;
    }

    size_t ValidationOffset(const SampleOptions_& options) {
#if defined(DAL_LSMC_STUDY_PRICING_OFFSET)
        static_cast<void>(options);
        return static_cast<size_t>(DAL_LSMC_STUDY_PRICING_OFFSET);
#else
        return options.training_.value_or(options.pricing_);
#endif
    }

    void WriteSettings(Writer_* writer, const SampleOptions_& options, const StudyCase_& study, const PreparedScript_& prepared) {
        WriteText(writer, "record", "lsmc_training_sample");
#if defined(DAL_LSMC_STUDY_PRICING_OFFSET)
        WriteText(writer, "addressing", "controlled");
#else
        WriteText(writer, "addressing", "stock");
#endif
        WriteText(writer, "case", study.name_);
        WriteText(writer, "engine", options.engine_.c_str());
        WriteText(writer, "mode", options.mode_.c_str());
        WriteText(writer, "normal_precision", options.precision_.c_str());
        WriteText(writer, "timing_scope", "preparation and prepared native valuation reported separately");
        const size_t offset = ValidationOffset(options);
        const std::pair<const char*, size_t> counts[]{{"training_paths", options.training_.value_or(options.pricing_)},
                                                      {"validation_paths", options.validation_},
                                                      {"pricing_paths", options.pricing_},
                                                      {"validation_offset", offset},
                                                      {"pricing_offset", offset + options.validation_},
                                                      {"replicates", options.replicates_},
                                                      {"training_seed", options.trainingSeed_},
                                                      {"pricing_seed", options.pricingSeed_},
                                                      {"threads", ThreadPool_::GetInstance()->NumThreads()},
                                                      {"events", prepared.Product().EventDates().size()},
                                                      {"exercise_dates", ExerciseCount(prepared)},
                                                      {"features", prepared.Plan().RegressionFeatureCount()}};
        for (const auto& item : counts) {
            writer->Key(item.first);
            writer->Uint64(item.second);
        }
        writer->Key("brownian_bridge");
        writer->Bool(options.bridge_);
        writer->Key("diagnostics");
        writer->Bool(options.diagnostics_);
        writer->Key("training_key");
        writer->Uint64(static_cast<uint64_t>(options.trainingSeed_) << 32);
        writer->Key("pricing_keys");
        writer->StartArray();
        for (size_t replicate = 0; replicate < options.replicates_; ++replicate)
            writer->Uint64((uint64_t{1} << 63) | (static_cast<uint64_t>(options.pricingSeed_) << 32) | replicate);
        writer->EndArray();
        WriteDouble(writer, "spot", study.spot_);
        WriteDouble(writer, "volatility", options.volatility_.value_or(study.volatility_));
        WriteDouble(writer, "rate", 0.05);
        WriteDouble(writer, "dividend", study.dividend_);
        WriteDouble(writer, "strike", 100.0);
        WriteDouble(writer, "maturity_time", prepared.TimeLine().back());
    }

    void WritePolicy(Writer_* writer, const LsmcDiagnostics_& diagnostics) {
        writer->Key("policy");
        writer->StartArray();
        for (const auto& event : diagnostics.events_) {
            writer->StartObject();
            writer->Key("event_id");
            writer->Uint64(event.eventId_);
            writer->Key("selected_degree");
            writer->Int(event.basisDegree_);
            writer->Key("rank");
            writer->Uint64(event.effectiveRank_);
            writer->Key("eligible_paths");
            writer->Uint64(event.numCondTruePaths_);
            WriteText(writer, "solver", event.solver_.c_str());
            WriteText(writer, "fallback", event.fallbackReason_.c_str());
            writer->Key("degenerate");
            writer->Bool(event.degenerate_);
            WriteDouble(writer, "exercise_rate", event.exerciseRate_);
            writer->Key("coefficients");
            WriteNumbers(writer, event.coefficients_);
            writer->Key("means");
            WriteNumbers(writer, event.normalizationMeans_);
            writer->Key("sigmas");
            WriteNumbers(writer, event.normalizationSigmas_);
            writer->Key("powers");
            writer->StartArray();
            for (const auto& powers : event.basisPowers_)
                WriteNumbers(writer, powers);
            writer->EndArray();
            writer->Key("validation_mse");
            if (event.validationMse_)
                writer->Double(*event.validationMse_);
            else
                writer->Null();
            writer->EndObject();
        }
        writer->EndArray();
    }

    void WriteRisks(Writer_* writer, const SimResults_& value, bool enabled) {
        writer->Key("risk_names");
        writer->StartArray();
        if (enabled)
            for (const auto& name : value.names_)
                writer->String(name.c_str());
        writer->EndArray();
        writer->Key("risks");
        writer->StartArray();
        if (enabled)
            for (double risk : value.risks_)
                writer->Double(risk);
        writer->EndArray();
    }

    LsmcDiagnostics_ HardDiagnostics(const ScriptProductData_& product, const Handle_<ModelData_>& data, const SampleOptions_& options) {
        auto settings = StudySettings(options);
        settings.enableAad_ = false;
        settings.lsmcPolicyRiskMode_ = "Frozen";
        auto model = CreateModel<double>(data);
        const auto prepared = PrepareScript(product, model.get(), {}, settings);
        LsmcDiagnostics_ diagnostics;
        static_cast<void>(MCLsmcSimulation(prepared, model.get(), options.pricing_, &diagnostics));
        return diagnostics;
    }

    using Clock_ = std::chrono::steady_clock;
    double Milliseconds(const Clock_::time_point& begin) { return std::chrono::duration<double, std::milli>(Clock_::now() - begin).count(); }

    void RunSample(const SampleOptions_& options, ProductFactory_ mixedProduct) {
        const auto study = FindCase(options.case_);
        ThreadPool_::GetInstance()->Start(options.threads_, true);
        const auto prepareStart = Clock_::now();
        const auto product = StudyProduct(study, mixedProduct);
        const auto data = StudyModel(study, options);
        auto model = CreateModel<double>(data);
        const auto prepared = PrepareScript(product, model.get(), {}, StudySettings(options));
        const double preparationMs = Milliseconds(prepareStart);
        const auto valuationStart = Clock_::now();
        const auto result = options.mode_ == "hard" ? MCLsmcSimulation(prepared, model.get(), options.pricing_)
                                                    : MCLsmcAadSimulation(prepared, data, options.pricing_);
        const double valuationMs = Milliseconds(valuationStart);
        REQUIRE(std::isfinite(result.aggregated_) &&
                    std::all_of(result.risks_.begin(), result.risks_.end(), [](double risk) { return std::isfinite(risk); }),
                "non-finite native study result");
        std::optional<LsmcDiagnostics_> diagnostics;
        const auto diagnosticStart = Clock_::now();
        if (options.diagnostics_)
            diagnostics.emplace(HardDiagnostics(product, data, options));
        const double diagnosticMs = options.diagnostics_ ? Milliseconds(diagnosticStart) : 0.0;
        rapidjson::StringBuffer buffer;
        Writer_ writer(buffer);
        writer.StartObject();
        WriteSettings(&writer, options, study, prepared);
        WriteDouble(&writer, "pv", result.aggregated_ / static_cast<double>(options.pricing_));
        WriteRisks(&writer, result, options.mode_ != "hard");
        WriteDouble(&writer, "preparation_ms", preparationMs);
        WriteDouble(&writer, "valuation_ms", valuationMs);
        WriteDouble(&writer, "diagnostic_ms", diagnosticMs);
        if (diagnostics) {
            WriteDouble(&writer, "hard_pv", diagnostics->payoffSum_ / static_cast<double>(diagnostics->nPaths_));
            writer.Key("hard_replicate_means");
            WriteNumbers(&writer, diagnostics->replicateMeans_);
            WritePolicy(&writer, *diagnostics);
        }
        writer.EndObject();
        std::cout << buffer.GetString() << '\n';
    }
} // namespace

int RunLsmcTrainingSample(int argc, char** argv, ProductFactory_ mixedProduct) {
    try {
        RunSample(ReadOptions(argc, argv), mixedProduct);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "lsmc-training-sample: " << error.what() << '\n';
        return 2;
    }
}
