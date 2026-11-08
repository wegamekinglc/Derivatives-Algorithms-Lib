#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/resource.h>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "bbs.hpp"

namespace {
    using Writer_ = rapidjson::Writer<rapidjson::OStreamWrapper>;
    double Number(const rapidjson::Value& value, const char* key) {
        if (!value.HasMember(key) || !value[key].IsNumber())
            throw std::runtime_error(std::string("missing numerical input: ") + key);
        return value[key].GetDouble();
    }
    PdeRiskReference::Input_ Input(const rapidjson::Value& value) {
        PdeRiskReference::Input_ r;
        r.spot_ = Number(value, "spot");
        r.strike_ = Number(value, "strike");
        r.volatility_ = Number(value, "vol");
        r.days_ = Number(value, "days");
        r.call_ = value["call"].GetBool();
        r.american_ = value["american"].GetBool();
        r.carry_.clear();
        for (const auto& row : value["carry"].GetArray())
            r.carry_.push_back({row[0].GetDouble(), row[1].GetDouble(), row[2].GetDouble()});
        r.exercise_ = r.american_ ? std::vector<double>{} : std::vector<double>{r.days_};
        return r;
    }
    uint64_t Nodes(int n) { return n >= 1 && n <= 16385 ? uint64_t(n + 1) * (n + 2) / 2 : 0; }
    uint64_t BsCalls(int n) { return n >= 1 && n <= 16385 ? uint64_t(n) : 0; }
    bool Result(Writer_& w, const rapidjson::Value& job) {
        PdeRiskBbs::Result_ result;
        bool success = true;
        w.StartObject();
        try {
            const auto& request = job["request"];
            PdeRiskBbs::Evaluate(Input(request), request["tree_steps"].GetInt(), Number(request, "deadline_epoch"), &result);
            w.Key("price");
            w.Double(result.price_);
            w.Key("root_continuation");
            w.Double(result.rootContinuation_);
            w.Key("root_intrinsic");
            w.Double(result.rootIntrinsic_);
            w.Key("root_exercised");
            w.Bool(result.rootExercised_);
        } catch (const std::exception& error) {
            success = false;
            w.Key("error");
            w.String(error.what());
        }
        w.Key("visited_nodes");
        w.Uint64(result.nodes_);
        w.Key("bs_evaluations");
        w.Uint64(result.bsCalls_);
        w.Key("obstacle_hits");
        w.Uint64(result.obstacleHits_);
        w.Key("terminal_obstacle_hits");
        w.Uint64(result.terminalHits_);
        w.EndObject();
        return success;
    }
    void Job(std::ostream& output, const rapidjson::Value& job) {
        const auto start = std::chrono::steady_clock::now();
        rapidjson::OStreamWrapper stream(output);
        Writer_ w(stream);
        w.StartObject();
        w.Key("id");
        w.String(job["id"].GetString());
        w.Key("method");
        w.String("BBS-last-step-v1");
        w.Key("result");
        const bool success = Result(w, job);
        w.Key("elapsed_seconds");
        w.Double(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        struct rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0)
            throw std::runtime_error("RSS unavailable");
        w.Key("max_rss_kib");
        w.Uint64(usage.ru_maxrss);
        w.Key("finished_epoch");
        w.Double(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count());
        w.EndObject();
        output << '\n';
        output.flush();
        if (!success && !job["expected_failure"].GetBool())
            throw std::runtime_error("unexpected BBS failure; worker stopped");
    }
    void ValidateJob(const rapidjson::Value& job) {
        if (std::string(job["method"].GetString()) != "BBS-last-step-v1")
            throw std::runtime_error("method identity mismatch");
        const int n = job["request"]["tree_steps"].GetInt();
        if (job["charged_nodes"].GetUint64() != Nodes(n) || job["reserved_bs"].GetUint64() != BsCalls(n))
            throw std::runtime_error("pilot reservation mismatch");
    }
    rapidjson::Document Parse(const std::string& line) {
        rapidjson::Document job;
        job.Parse(line.c_str());
        if (job.HasParseError() || !job.IsObject())
            throw std::runtime_error("invalid pilot JSON");
        ValidateJob(job);
        return job;
    }
    int Batch(const std::filesystem::path& path) {
        const auto destination = path.string() + ".results.jsonl";
        if (std::filesystem::exists(destination))
            throw std::runtime_error("preserve existing pilot outputs");
        std::ifstream input(path);
        std::ofstream output(destination);
        if (!input || !output)
            throw std::runtime_error("pilot input/output unavailable");
        uint64_t nodes = 0, bs = 0;
        std::string line;
        while (std::getline(input, line)) {
            const auto job = Parse(line);
            nodes += job["charged_nodes"].GetUint64();
            bs += job["reserved_bs"].GetUint64();
            if (nodes > 6000000000ULL || bs > 2000000ULL)
                throw std::runtime_error("pilot reservation exhausted");
            Job(output, job);
            if (output.tellp() > 16777216)
                throw std::runtime_error("pilot output cap exhausted");
        }
        return 0;
    }
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 || std::string(argv[1]) != "--batchfile")
            throw std::runtime_error("usage: dal502_bbs --batchfile jobs.jsonl");
        return Batch(argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
