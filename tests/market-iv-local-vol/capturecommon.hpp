#pragma once

#include <dal/model/surface/marketivsurface.hpp>
#include <dal/platform/platform.hpp>
#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

namespace DalTest {
    using Writer_ = rapidjson::Writer<rapidjson::OStreamWrapper>;
    using Json_ = rapidjson::Value;

    inline double Number(const Json_& value) { return value.IsString() ? std::stod(value.GetString()) : value.GetDouble(); }

    inline void Text(Writer_* writer, const char* key, const Dal::String_& value) {
        writer->Key(key);
        writer->String(value.c_str());
    }

    inline void Number(Writer_* writer, const char* key, double value) {
        writer->Key(key);
        writer->Double(value);
    }

    inline void Optional(Writer_* writer, const char* key, const std::optional<double>& value) {
        writer->Key(key);
        if (value)
            writer->Double(*value);
        else
            writer->Null();
    }

    inline void Vector(Writer_* writer, const char* key, const Dal::Vector_<>& values) {
        writer->Key(key);
        writer->StartArray();
        for (const double value : values)
            writer->Double(value);
        writer->EndArray();
    }

    inline Dal::Vector_<> Axis(const Json_& values) {
        Dal::Vector_<> result;
        for (const auto& value : values.GetArray())
            result.push_back(Number(value));
        return result;
    }

    inline Dal::MarketIvMarket_ Market(bool flat) {
        const Dal::Date_ today(2026, 11, 16);
        const Dal::Vector_<Dal::Date_> expiries{today.AddDays(91), today.AddDays(182), today.AddDays(273), today.AddDays(365), today.AddDays(730)};
        if (flat)
            return Dal::NewFlatMarketIvMarket(today, 100., .03, .01, expiries);
        return {
            today,
            100.,
            {{expiries[0], 101., .995}, {expiries[1], 99., 1.005}, {expiries[2], 102., .998}, {expiries[3], 100.5, 1.01}, {expiries[4], 104., .97}}};
    }

    inline Dal::MarketIvFitRequest_ Request(const Json_& fixture) {
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
} // namespace DalTest
