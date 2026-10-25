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
#include <dal/curve/calibration.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/yield/bootstraptraits.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/currencies/america.hpp>
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
        std::string caseName_;
        std::string operation_;
        std::string kind_;
        int size_ = 0;
        IsoDate_ anchor_;
        std::vector<IsoDate_> dates_;
        std::vector<IsoDate_> queries_;
        std::vector<double> dfs_;
        std::vector<double> quotes_;
        std::vector<double> forwardQuotes_;
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
        const std::string schema = Text(Get(document, "schema"));
        const auto& workload = Get(document, "workload");
        Fixture_ fixture;
        fixture.caseName_ = Text(Get(workload, "name"));
        fixture.operation_ = Text(Get(workload, "operation"));
        fixture.kind_ = workload.HasMember("kind") ? Text(workload["kind"]) : "";
        fixture.size_ = Get(workload, "size").GetInt();
        fixture.anchor_ = ParseDate(Get(document, "anchor"));
        fixture.dates_ = ParseDates(Get(document, "dates"));
        fixture.queries_ = ParseDates(Get(document, "queries"));
        fixture.reference_ = ParseNumbers(Get(document, "reference"));
        fixture.absoluteTolerance_ = Number(Get(document, "absolute_tolerance"));
        fixture.relativeTolerance_ = Number(Get(document, "relative_tolerance"));
        REQUIRE(fixture.relativeTolerance_ == 1.0e-10, "Native relative tolerance changed");
        if (schema == "dal.native-query-fixture/1") {
            fixture.dfs_ = ParseNumbers(Get(document, "dfs"));
            REQUIRE(fixture.caseName_ == "discount_queries" && fixture.operation_ == "discount" && fixture.size_ == 4096,
                    "Unexpected native discount workload");
            REQUIRE(fixture.dates_.size() == 22 && fixture.dfs_.size() == 22, "Native curve must have 22 nodes");
            REQUIRE(fixture.queries_.size() == 4096 && fixture.reference_.size() == 4096, "Native query width changed");
            REQUIRE(fixture.absoluteTolerance_ == 2.0e-12, "Native query tolerance changed");
        } else if (schema == "dal.native-calibration-fixture/1") {
            fixture.quotes_ = ParseNumbers(Get(document, "quotes"));
            const bool single = fixture.caseName_ == "calibration_single_5" || fixture.caseName_ == "calibration_single_15";
            const bool staged = fixture.caseName_ == "calibration_multi_staged_5" || fixture.caseName_ == "calibration_multi_staged_15";
            REQUIRE((single || staged) && fixture.operation_ == "calibration", "Unexpected native calibration workload");
            REQUIRE(fixture.size_ == 5 || fixture.size_ == 15, "Unexpected native calibration size");
            REQUIRE(fixture.kind_ == (staged ? "multi_staged" : "single"), "Unexpected native calibration kind");
            REQUIRE(fixture.dates_.size() == static_cast<size_t>(fixture.size_ + 1) && fixture.quotes_.size() == fixture.size_,
                    "Native calibration input width changed");
            REQUIRE(fixture.queries_.size() == static_cast<size_t>(2 * fixture.size_) &&
                        fixture.reference_.size() == static_cast<size_t>((staged ? 4 : 2) * fixture.size_),
                    "Native calibration output width changed");
            if (staged) {
                fixture.forwardQuotes_ = ParseNumbers(Get(document, "forward_quotes"));
                REQUIRE(fixture.forwardQuotes_.size() == fixture.size_, "Native forward quote width changed");
            }
            REQUIRE(fixture.absoluteTolerance_ == 2.0e-8, "Native calibration tolerance changed");
        } else {
            REQUIRE(false, "Unexpected native fixture schema");
        }
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

    std::function<std::vector<double>()> PrepareDiscount(const std::string& backend, const Fixture_& fixture) {
        REQUIRE(fixture.caseName_ == "discount_queries", "Invalid discount-query fixture");
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

    std::function<std::vector<double>()> PrepareDalCalibration(const Fixture_& fixture) {
        Dal::RegisterAll_::Init();
        const Date_ today = DalDate(fixture.anchor_);
        Vector_<Date_> nodes;
        Vector_<Date_> queries;
        for (const auto& date : fixture.dates_)
            nodes.push_back(DalDate(date));
        for (const auto& query : fixture.queries_)
            queries.push_back(DalDate(query));
        const auto quotes = fixture.quotes_;
        const auto forwardQuotes = fixture.forwardQuotes_;
        const bool staged = fixture.kind_ == "multi_staged";
        return [today, nodes, queries, quotes, forwardQuotes, staged]() {
            const auto solve = [&](const std::string& key, const std::vector<double>& keyQuotes,
                                   const Handle_<DiscountCurve_>& discount) -> CurveCalibrationResult_ {
                RateLegConvention_ leg;
                leg.paymentLag_ = 0;
                leg.paymentFrequency_ = PeriodLength_("12M");
                leg.dayBasis_ = DayBasis_("ACT_365F");
                leg.accrualHolidays_ = Holidays::None();
                leg.paymentHolidays_ = Holidays::None();
                leg.businessDayConvention_ = BizDayConvention_("Unadjusted");
                leg.paymentConvention_ = BizDayConvention_("Unadjusted");
                RateIndexConvention_ index;
                index.forecastTenor_ = PeriodLength_("12M");
                index.dayBasis_ = DayBasis_("ACT_365F");
                index.fixingLag_ = 0;
                index.spotLag_ = 0;
                index.fixingHolidays_ = Holidays::None();
                index.accrualHolidays_ = Holidays::None();
                index.businessDayConvention_ = BizDayConvention_("Unadjusted");
                index.useProjectionCurve_ = key == "usd_forward";
                CurveCalibrationSpec_ spec;
                spec.today_ = today;
                spec.ccy_ = "USD";
                spec.curveName_ = String_(key.c_str());
                spec.targetCollateral_ = CollateralType_(CollateralType_::Value_::OIS);
                spec.targetTenor_ = PeriodLength_("12M");
                spec.calibrateDiscountCurve_ = key == "usd_discount";
                spec.liborBasis_ = DayBasis_("ACT_365F");
                spec.parameterization_ = CurveParameterization_(CurveParameterization_::Value_::LOG_DISCOUNT);
                spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
                spec.solveMode_ = CurveSolveMode_::Value_::EXACT;
                spec.tolerance_ = 1.0e-12;
                spec.fitTolerance_ = 1.0e-10;
                spec.initialGuess_ = 0.025;
                spec.logDfScheme_ = LogDfScheme_(LogDfScheme_::Value_::LOG_LINEAR);
                spec.knotDates_ = nodes;
                if (key == "usd_forward")
                    spec.discountCurves_.emplace(CollateralType_(CollateralType_::Value_::OIS), discount);
                spec.instruments_.reserve(keyQuotes.size());
                for (size_t i = 0; i < keyQuotes.size(); ++i)
                    spec.instruments_.emplace_back(new Swap_(today, today, nodes[i + 1], keyQuotes[i], leg, index, leg));
                CurveCalibrationOptions_ options;
                options.jacobianMode_ = CurveJacobianMode_(CurveJacobianMode_::Value_::ANALYTIC);
                options.computeEffJacobianInverse_ = false;
                options.computeForwardJacobian_ = false;
                return CalibrateYieldCurve(spec, options);
            };
            auto discountResult = solve("usd_discount", quotes, {});
            std::vector<double> values;
            values.reserve((staged ? 2 : 1) * queries.size());
            for (const auto& query : queries)
                values.push_back((*discountResult.curve_)(today, query));
            if (staged) {
                Handle_<DiscountCurve_> discount(std::move(discountResult.curve_));
                auto forwardResult = solve("usd_forward", forwardQuotes, discount);
                for (const auto& query : queries)
                    values.push_back((*forwardResult.curve_)(today, query));
            }
            return values;
        };
    }

    std::function<std::vector<double>()> PrepareQuantLibCalibration(const Fixture_& fixture) {
        const QuantLib::Date today = QuantLibDate(fixture.anchor_);
        QuantLib::Settings::instance().evaluationDate() = today;
        std::vector<QuantLib::Date> queries;
        queries.reserve(fixture.queries_.size());
        for (const auto& query : fixture.queries_)
            queries.push_back(QuantLibDate(query));
        const auto quotes = fixture.quotes_;
        const auto forwardQuotes = fixture.forwardQuotes_;
        const bool staged = fixture.kind_ == "multi_staged";
        return [today, queries, quotes, forwardQuotes, staged]() {
            using Curve = QuantLib::PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>;
            const auto solve = [&](const std::vector<double>& keyQuotes,
                                   QuantLib::Handle<QuantLib::YieldTermStructure> discountingCurve) {
                auto index = QuantLib::ext::make_shared<QuantLib::IborIndex>(
                    "calibration-usd", QuantLib::Period(12, QuantLib::Months), 0, QuantLib::USDCurrency(),
                    QuantLib::NullCalendar(), QuantLib::Unadjusted, false, QuantLib::Actual365Fixed(),
                    QuantLib::Handle<QuantLib::YieldTermStructure>());
                std::vector<QuantLib::ext::shared_ptr<QuantLib::Discount::helper>> helpers;
                helpers.reserve(keyQuotes.size());
                for (size_t i = 0; i < keyQuotes.size(); ++i) {
                    const int year = static_cast<int>(i) + 1;
                    helpers.push_back(QuantLib::ext::make_shared<QuantLib::SwapRateHelper>(
                        keyQuotes[i], QuantLib::Period(year, QuantLib::Years), QuantLib::NullCalendar(), QuantLib::Annual,
                        QuantLib::Unadjusted, QuantLib::Actual365Fixed(), index, QuantLib::Handle<QuantLib::Quote>(),
                        QuantLib::Period(0, QuantLib::Days), discountingCurve, 0));
                }
                auto curve = QuantLib::ext::make_shared<Curve>(today, helpers, QuantLib::Actual365Fixed());
                curve->discount(helpers.back()->latestDate());
                return curve;
            };
            auto discountCurve = solve(quotes, QuantLib::Handle<QuantLib::YieldTermStructure>());
            std::vector<double> values;
            values.reserve((staged ? 2 : 1) * queries.size());
            for (const auto& query : queries)
                values.push_back(discountCurve->discount(query));
            if (staged) {
                auto forwardCurve = solve(forwardQuotes, QuantLib::Handle<QuantLib::YieldTermStructure>(discountCurve));
                for (const auto& query : queries)
                    values.push_back(forwardCurve->discount(query));
            }
            return values;
        };
    }

    std::function<std::vector<double>()> PrepareCalibration(const std::string& backend, const Fixture_& fixture) {
        REQUIRE(fixture.operation_ == "calibration", "Invalid calibration fixture");
        if (backend == "dal")
            return PrepareDalCalibration(fixture);
        REQUIRE(backend == "quantlib", "Unknown native backend: " + backend);
        return PrepareQuantLibCalibration(fixture);
    }

    std::function<std::vector<double>()> Prepare(const std::string& backend, const Fixture_& fixture) {
        if (fixture.caseName_ == "discount_queries")
            return PrepareDiscount(backend, fixture);
        if (fixture.operation_ == "calibration")
            return PrepareCalibration(backend, fixture);
        REQUIRE(false, "Unknown native case: " + fixture.caseName_);
    }

    void WriteReport(const std::string& path,
                     const std::string& backend,
                     const Fixture_& fixture,
                     const std::vector<double>& values,
                     long long elapsed) {
        std::ofstream output(path);
        REQUIRE(output.good(), "Cannot create native report: " + path);
        const bool calibration = fixture.operation_ == "calibration";
        const char* schema = calibration ? "dal.native-calibration-worker/1" : "dal.native-query-worker/1";
        std::string method;
        if (!calibration)
            method = backend == "dal" ? "DAL C++ DiscountLogDF" : "QuantLib C++ DiscountCurve";
        else if (backend == "dal" && fixture.kind_ == "multi_staged")
            method = "DAL C++ analytic staged two-curve solve (tol 1e-12, fit 1e-10, diagnostics off)";
        else if (backend == "dal")
            method = "DAL C++ analytic single-curve solve (tol 1e-12, fit 1e-10, diagnostics off)";
        else
            method = "QuantLib C++ PiecewiseYieldCurve<Discount, LogLinear> (accuracy 1e-12)";
        output << std::setprecision(17);
        output << "{\"schema\":\"" << schema << "\",\"backend\":\"" << backend << "\",\"method\":\"" << method
               << "\",\"workload\":{\"name\":\"" << fixture.caseName_ << "\",\"operation\":\"" << fixture.operation_
               << "\",\"size\":" << fixture.size_;
        if (!fixture.kind_.empty())
            output << ",\"kind\":\"" << fixture.kind_ << "\"";
        output << "},\"quantlib_version\":\"" << QL_VERSION << "\",\"boost_version\":\"" << BOOST_LIB_VERSION
               << "\",\"samples_ns\":[" << elapsed << "],\"values\":[";
        for (size_t i = 0; i < values.size(); ++i)
            output << (i == 0 ? "" : ",") << values[i];
        output << "]}\n";
        REQUIRE(output.good(), "Failed to write native report");
    }
} // namespace

int main(int argc, char** argv) {
    try {
        REQUIRE(argc == 7 || argc == 9,
                "Usage: native_query_comparison --backend dal|quantlib --input fixture.json --output report.json [--case name]");
        REQUIRE(std::string(argv[1]) == "--backend" && std::string(argv[3]) == "--input" && std::string(argv[5]) == "--output",
                "Invalid native worker arguments");
        const std::string backend = argv[2];
        const std::string caseName = argc == 9 && std::string(argv[7]) == "--case" ? argv[8] : "discount_queries";
        REQUIRE(argc == 7 || std::string(argv[7]) == "--case", "Invalid native case arguments");
        const Fixture_ fixture = ReadFixture(argv[4]);
        REQUIRE(caseName == fixture.caseName_, "Native case selection does not match fixture");
        std::function<std::vector<double>()> run = Prepare(backend, fixture);
        for (int i = 0; i < 3; ++i)
            Check(run(), fixture);
        const auto start = std::chrono::steady_clock::now();
        const std::vector<double> values = run();
        const auto stop = std::chrono::steady_clock::now();
        Check(values, fixture);
        const long long elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        REQUIRE(elapsed > 0, "Nonpositive native timing");
        WriteReport(argv[6], backend, fixture, values, elapsed);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
