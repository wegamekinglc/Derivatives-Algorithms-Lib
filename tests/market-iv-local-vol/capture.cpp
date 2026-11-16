#include <fstream>
#include <iostream>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <dal/model/surface/marketivsurface.hpp>
#include <dal/platform/platform.hpp>

namespace {
    using Writer_ = rapidjson::Writer<rapidjson::OStreamWrapper>;
    using Json_ = rapidjson::Value;

    double Number(const Json_& value) { return value.IsString() ? std::stod(value.GetString()) : value.GetDouble(); }

    void Text(Writer_* writer, const char* key, const Dal::String_& value) {
        writer->Key(key);
        writer->String(value.c_str());
    }

    void Number(Writer_* writer, const char* key, double value) {
        writer->Key(key);
        writer->Double(value);
    }

    void Optional(Writer_* writer, const char* key, const std::optional<double>& value) {
        writer->Key(key);
        if (value)
            writer->Double(*value);
        else
            writer->Null();
    }

    void Vector(Writer_* writer, const char* key, const Dal::Vector_<>& values) {
        writer->Key(key);
        writer->StartArray();
        for (const double value : values)
            writer->Double(value);
        writer->EndArray();
    }

    Dal::Vector_<> Axis(const Json_& values) {
        Dal::Vector_<> result;
        for (const auto& value : values.GetArray())
            result.push_back(Number(value));
        return result;
    }

    Dal::MarketIvMarket_ Market(bool flat) {
        const Dal::Date_ today(2026, 11, 16);
        const Dal::Vector_<Dal::Date_> expiries{today.AddDays(91), today.AddDays(182), today.AddDays(273), today.AddDays(365), today.AddDays(730)};
        if (flat)
            return Dal::NewFlatMarketIvMarket(today, 100., .03, .01, expiries);
        return {
            today,
            100.,
            {{expiries[0], 101., .995}, {expiries[1], 99., 1.005}, {expiries[2], 102., .998}, {expiries[3], 100.5, 1.01}, {expiries[4], 104., .97}}};
    }

    Dal::MarketIvFitRequest_ Request(const Json_& fixture) {
        Dal::MarketIvFitRequest_ result{Market(fixture["flat"].GetBool()), {}, {}};
        if (fixture["strict"].GetBool()) {
            auto& settings = result.settings_;
            settings.initialVolatility_ = .20;
            settings.normalizedPriceTolerance_ = settings.decimalIvTolerance_ = 1e-8;
            settings.projectedGradientTolerance_ = 1e-10;
            settings.parameterStepTolerance_ = settings.feasibilityTolerance_ = 1e-12;
            settings.objectiveTieTolerance_ = 1e-14;
            settings.maxIterationsPerStart_ = 400;
            settings.maxModelRowEvaluations_ = 5000000;
        }
        for (const auto& row : fixture["quotes"].GetArray()) {
            Dal::MarketIvQuote_ quote;
            quote.id_ = row["id"].GetString();
            quote.expiry_ = result.market_.EvaluationDate().AddDays(row["days"].GetInt());
            quote.strike_ = Number(row["strike"]);
            quote.mid_ = Number(row["mid"]);
            quote.optionType_ = Dal::OptionType_(row["option"].GetString());
            quote.quoteType_ = Dal::MarketIvQuoteType_(row["unit"].GetString());
            result.quotes_.push_back(quote);
        }
        return result;
    }

    void Point(Writer_* writer, const Dal::MarketIvPointResult_& point) {
        Text(writer, "value_status", point.ValueStatus());
        Text(writer, "derivative_status", point.DerivativeStatus());
        Text(writer, "dupire_status", point.DupireStatus());
        Optional(writer, "variance", point.TotalVariance());
        Optional(writer, "variance_k", point.VarianceK());
        Optional(writer, "variance_kk", point.VarianceKK());
        Optional(writer, "variance_t", point.VarianceT());
        Optional(writer, "density", point.DensityFactor());
        Optional(writer, "local_variance", point.LocalVariance());
        Optional(writer, "local_vol", point.LocalVol());
        Optional(writer, "call", point.CallPrice());
        Optional(writer, "put", point.PutPrice());
    }

    void Snapshot(Writer_* writer, const Dal::MarketIvCalibrationSnapshot_& snapshot) {
        writer->Key("snapshot");
        writer->StartObject();
        Number(writer, "rho", snapshot.Rho());
        Number(writer, "eta", snapshot.Eta());
        Vector(writer, "times", snapshot.Times());
        Vector(writer, "theta", snapshot.Theta());
        Vector(writer, "slopes", snapshot.Slopes());
        writer->EndObject();
    }

    void Probes(Writer_* writer, const Dal::MarketIvCalibrationSnapshot_& snapshot, const Json_& probes) {
        writer->Key("probes");
        writer->StartArray();
        for (const auto& query : probes.GetArray()) {
            const double time = Number(query["time"]), strike = Number(query["strike"]);
            writer->StartObject();
            Number(writer, "time", time);
            Number(writer, "strike", strike);
            Point(writer, Dal::EvaluateMarketIvSurface(snapshot, {strike, time}));
            writer->EndObject();
        }
        writer->EndArray();
    }

    bool Grid(Writer_* writer, const Dal::MarketIvCalibrationSnapshot_& snapshot, const Json_& grid) {
        writer->StartObject();
        Text(writer, "id", grid["id"].GetString());
        const auto spots = Axis(grid["spots"]), times = Axis(grid["times"]);
        writer->Key("spot_nodes");
        writer->Uint64(spots.size());
        writer->Key("time_nodes");
        writer->Uint64(times.size());
        writer->Key("points");
        writer->Uint64(spots.size() * times.size());
        const auto result = Dal::CalibrateMarketIvDupire(snapshot, {spots, times, {}});
        writer->Key("success");
        writer->Bool(result.IsSuccess());
        Text(writer, "failure", result.Failure());
        size_t invalid = 0;
        writer->Key("invalid_examples");
        writer->StartArray();
        for (size_t ordinal = 0; ordinal < result.Nodes().size(); ++ordinal) {
            const auto& node = result.Nodes()[ordinal];
            if (node.DupireStatus() == "SUCCESS")
                continue;
            if (invalid++ >= 8)
                continue;
            writer->StartObject();
            Number(writer, "strike", spots[ordinal / times.size()]);
            Number(writer, "time", times[ordinal % times.size()]);
            Point(writer, node);
            writer->EndObject();
        }
        writer->EndArray();
        writer->Key("invalid_count");
        writer->Uint64(invalid);
        if (result.IsSuccess()) {
            Vector(writer, "spots", spots);
            Vector(writer, "times", times);
            writer->Key("vols_spot_major");
            writer->StartArray();
            for (const double value : result.SurfaceData().vols_)
                writer->Double(value);
            writer->EndArray();
        }
        writer->EndObject();
        return result.IsSuccess();
    }

    bool Capture(Writer_* writer, const Json_& fixture, const Json_& grids) {
        writer->StartObject();
        Text(writer, "id", fixture["id"].GetString());
        const auto fit = Dal::FitMarketIvSurface(Request(fixture));
        writer->Key("fit_success");
        writer->Bool(fit.IsSuccess());
        Text(writer, "fit_status", fit.Status());
        Text(writer, "fit_failure", fit.Failure());
        bool success = fit.IsSuccess();
        if (success) {
            const auto snapshot = fit.Snapshot();
            Snapshot(writer, snapshot);
            Probes(writer, snapshot, fixture["probes"]);
            writer->Key("grids");
            writer->StartArray();
            for (const auto& grid : grids.GetArray()) {
                const bool valid = Grid(writer, snapshot, grid);
                success = success && valid;
            }
            writer->EndArray();
        }
        writer->EndObject();
        return success;
    }
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: dal_market_iv_capture request.json capture.json\n";
        return 1;
    }
    try {
        std::ifstream input(argv[1]);
        REQUIRE(input.good(), "Cannot open request");
        rapidjson::IStreamWrapper stream(input);
        rapidjson::Document request;
        request.ParseStream(stream);
        REQUIRE(!request.HasParseError(), "Malformed request JSON");
        std::ofstream output(argv[2]);
        REQUIRE(output.good(), "Cannot open capture");
        rapidjson::OStreamWrapper destination(output);
        Writer_ serializer(destination);
        auto* writer = &serializer;
        writer->StartObject();
        Text(writer, "schema", "dal.market-iv-independent-capture/1");
        writer->Key("fixtures");
        writer->StartArray();
        bool success = true;
        for (const auto& fixture : request["fixtures"].GetArray()) {
            const bool valid = Capture(writer, fixture, request["grids"]);
            success = success && valid;
        }
        writer->EndArray();
        writer->EndObject();
        output << '\n';
        return success ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
