#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <dal/platform/platform.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/version.hpp>
#include <boost/version.hpp>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace Dal;

namespace {
    struct IsoDate_ {
        int year_;
        int month_;
        int day_;
    };

    struct Fixture_ {
        IsoDate_ anchor_;
        std::vector<IsoDate_> dates_;
        std::vector<IsoDate_> queries_;
        std::vector<double> dfs_;
        std::vector<double> reference_;
        double absoluteTolerance_;
        double relativeTolerance_;
    };

    const rapidjson::Value& Get(const rapidjson::Value& value, const char* key) {
        REQUIRE(value.IsObject() && value.HasMember(key), std::string("Missing JSON field: ") + key);
        return value[key];
    }

    std::string Text(const rapidjson::Value& value) {
        REQUIRE(value.IsString(), "Expected JSON string");
        return value.GetString();
    }

    double Number(const rapidjson::Value& value) {
        REQUIRE(value.IsNumber(), "Expected JSON number");
        return value.GetDouble();
    }

    IsoDate_ ParseDate(const rapidjson::Value& value) {
        const std::string text = Text(value);
        REQUIRE(text.size() >= 10 && text[4] == '-' && text[7] == '-', "Expected ISO date");
        const auto part = [&](size_t offset, size_t count) {
            int result = 0;
            const char* first = text.data() + offset;
            const auto parsed = std::from_chars(first, first + count, result);
            REQUIRE(parsed.ec == std::errc() && parsed.ptr == first + count, "Invalid ISO date component");
            return result;
        };
        return {part(0, 4), part(5, 2), part(8, 2)};
    }

    std::vector<IsoDate_> ParseDates(const rapidjson::Value& value) {
        REQUIRE(value.IsArray(), "Expected JSON date array");
        std::vector<IsoDate_> result;
        result.reserve(value.Size());
        for (const auto& date : value.GetArray())
            result.push_back(ParseDate(date));
        return result;
    }

    std::vector<double> ParseNumbers(const rapidjson::Value& value) {
        REQUIRE(value.IsArray(), "Expected JSON number array");
        std::vector<double> result;
        result.reserve(value.Size());
        for (const auto& number : value.GetArray())
            result.push_back(Number(number));
        return result;
    }

    Fixture_ ReadFixture(const std::string& path) {
        std::ifstream stream(path);
        REQUIRE(stream.good(), "Cannot open native fixture: " + path);
        const std::string json((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        rapidjson::Document document;
        document.Parse(json.c_str());
        REQUIRE(!document.HasParseError(), rapidjson::GetParseError_En(document.GetParseError()));
        REQUIRE(Text(Get(document, "schema")) == "dal.native-query-fixture/1", "Unexpected native fixture schema");
        const auto& workload = Get(document, "workload");
        REQUIRE(Text(Get(workload, "name")) == "discount_queries" && Text(Get(workload, "operation")) == "discount" &&
                    Get(workload, "size").GetInt() == 4096,
                "Unexpected native workload");
        Fixture_ fixture{ParseDate(Get(document, "anchor")),
                         ParseDates(Get(document, "dates")),
                         ParseDates(Get(document, "queries")),
                         ParseNumbers(Get(document, "dfs")),
                         ParseNumbers(Get(document, "reference")),
                         Number(Get(document, "absolute_tolerance")),
                         Number(Get(document, "relative_tolerance"))};
        REQUIRE(fixture.dates_.size() == 22 && fixture.dfs_.size() == 22, "Native curve must have 22 nodes");
        REQUIRE(fixture.queries_.size() == 4096 && fixture.reference_.size() == 4096, "Native query width changed");
        REQUIRE(fixture.absoluteTolerance_ == 2.0e-12 && fixture.relativeTolerance_ == 1.0e-10,
                "Native query tolerance changed");
        return fixture;
    }

    Date_ DalDate(const IsoDate_& date) { return Date_(date.year_, date.month_, date.day_); }

    QuantLib::Date QuantLibDate(const IsoDate_& date) {
        return QuantLib::Date(date.day_, static_cast<QuantLib::Month>(date.month_), date.year_);
    }

    void Check(const std::vector<double>& values, const Fixture_& fixture) {
        REQUIRE(values.size() == fixture.reference_.size(), "Native result width mismatch");
        for (size_t i = 0; i < values.size(); ++i) {
            REQUIRE(std::isfinite(values[i]), "Nonfinite native query result");
            const double tolerance = std::max(
                fixture.absoluteTolerance_, fixture.relativeTolerance_ * std::max(std::abs(values[i]), std::abs(fixture.reference_[i])));
            REQUIRE(std::abs(values[i] - fixture.reference_[i]) <= tolerance,
                    "Native query correctness failed at index " + std::to_string(i));
        }
    }

    std::function<std::vector<double>()> Prepare(const std::string& backend, const Fixture_& fixture) {
        if (backend == "dal") {
            Dal::RegisterAll_::Init();
            Vector_<Date_> nodes;
            Vector_<> logDfs;
            for (size_t i = 0; i < fixture.dates_.size(); ++i) {
                nodes.push_back(DalDate(fixture.dates_[i]));
                logDfs.push_back(std::log(fixture.dfs_[i]));
            }
            Vector_<Date_> queries;
            for (const auto& query : fixture.queries_)
                queries.push_back(DalDate(query));
            const Date_ anchor = DalDate(fixture.anchor_);
            const DayBasis_ dayCount("ACT_365F");
            auto curve = std::make_shared<Tape::DiscountLogDF_<double>>(
                "native_queries", "USD", nodes, logDfs, dayCount, LogDfScheme_::Value_::LOG_LINEAR);
            return [curve, anchor, queries]() {
                std::vector<double> values;
                values.reserve(queries.size());
                for (const auto& query : queries)
                    values.push_back((*curve)(anchor, query));
                return values;
            };
        }
        REQUIRE(backend == "quantlib", "Unknown native backend: " + backend);
        std::vector<QuantLib::Date> nodes;
        std::vector<QuantLib::Date> queries;
        nodes.reserve(fixture.dates_.size());
        queries.reserve(fixture.queries_.size());
        for (const auto& date : fixture.dates_)
            nodes.push_back(QuantLibDate(date));
        for (const auto& query : fixture.queries_)
            queries.push_back(QuantLibDate(query));
        auto curve = std::make_shared<QuantLib::DiscountCurve>(nodes, fixture.dfs_, QuantLib::Actual365Fixed());
        return [curve, queries]() {
            std::vector<double> values;
            values.reserve(queries.size());
            for (const auto& query : queries)
                values.push_back(curve->discount(query));
            return values;
        };
    }

    void WriteReport(const std::string& path, const std::string& backend, const std::vector<double>& values, long long elapsed) {
        std::ofstream output(path);
        REQUIRE(output.good(), "Cannot create native report: " + path);
        const char* method = backend == "dal" ? "DAL C++ DiscountLogDF" : "QuantLib C++ DiscountCurve";
        output << std::setprecision(17);
        output << "{\"schema\":\"dal.native-query-worker/1\",\"backend\":\"" << backend << "\",\"method\":\"" << method
               << "\",\"workload\":{\"name\":\"discount_queries\",\"operation\":\"discount\",\"size\":4096}"
               << ",\"quantlib_version\":\"" << QL_VERSION << "\",\"boost_version\":\"" << BOOST_LIB_VERSION
               << "\",\"samples_ns\":[" << elapsed << "],\"values\":[";
        for (size_t i = 0; i < values.size(); ++i)
            output << (i == 0 ? "" : ",") << values[i];
        output << "]}\n";
        REQUIRE(output.good(), "Failed to write native report");
    }
} // namespace

int main(int argc, char** argv) {
    try {
        REQUIRE(argc == 7, "Usage: native_query_comparison --backend dal|quantlib --input fixture.json --output report.json");
        REQUIRE(std::string(argv[1]) == "--backend" && std::string(argv[3]) == "--input" && std::string(argv[5]) == "--output",
                "Invalid native worker arguments");
        const std::string backend = argv[2];
        const Fixture_ fixture = ReadFixture(argv[4]);
        std::function<std::vector<double>()> run = Prepare(backend, fixture);
        for (int i = 0; i < 3; ++i)
            Check(run(), fixture);
        const auto start = std::chrono::steady_clock::now();
        const std::vector<double> values = run();
        const auto stop = std::chrono::steady_clock::now();
        Check(values, fixture);
        const long long elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        REQUIRE(elapsed > 0, "Nonpositive native timing");
        WriteReport(argv[6], backend, values, elapsed);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
