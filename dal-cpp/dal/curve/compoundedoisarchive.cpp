#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/compoundedois_internal.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        const String_ ARCHIVE_ERROR = "InvalidCalibrationArchive";
        String_ TextField(const Json_& v, const char* key, const String_& p) { return ReadString(Field(v, key), p + "/" + key); }
        double NumberField(const Json_& v, const char* key, const String_& p) { return ReadNumber(Field(v, key), p + "/" + key); }
        Date_ ReadDate(const Json_& v, const String_& p) { return ParseDate(ReadString(v, p), ARCHIVE_ERROR, p); }
        Date_ DateField(const Json_& v, const char* key, const String_& p) { return ReadDate(Field(v, key), p + "/" + key); }
        template <class E_> E_ EnumField(const Json_& v, const char* key, const String_& p) {
            return Enum<E_>(TextField(v, key, p), ARCHIVE_ERROR, p + "/" + key);
        }
        DateTime_ ReadTime(const Json_& v, const String_& p) {
            Fields(v, p, {"date", "fraction"});
            const auto date = DateField(v, "date", p);
            const double fraction = NumberField(v, "fraction", p);
            Check(fraction >= 0. && fraction < 1., ARCHIVE_ERROR, p + "/fraction", "time fraction in [0,1)");
            return DateTime_(date, fraction);
        }
        std::optional<String_> CouponId(const Json_& row) {
            const auto found = row.object_.find("coupon_id");
            if (found == row.object_.end() || found->second.kind_ != JsonKind_::STRING)
                return {};
            return String_(found->second.string_);
        }
        template <class T_, class F_> Vector_<T_> ReadRows(const Json_& v, const String_& p, const F_& parseRow) {
            CheckArray(v, p);
            Vector_<T_> rows;
            size_t rowIndex = 0;
            for (const auto& row : v.array_) {
                try {
                    rows.push_back(parseRow(row, p + "/" + OrdinalText(rowIndex)));
                } catch (const CalibrationError_& error) {
                    auto d = error.Failure().diagnostic_;
                    if (!d.sourceRow_)
                        d.sourceRow_ = static_cast<int>(rowIndex + 1);
                    if (!d.observationId_)
                        d.observationId_ = CouponId(row);
                    Fail(d.code_, d.field_.value_or(p), d.expected_.value_or("valid table row"), d.actual_.value_or(""), d);
                }
                ++rowIndex;
            }
            return rows;
        }
        HolidayCenterData_ ReadCalendar(const Json_& v, const String_& p) {
            Fields(v, p, {"id", "holidays", "work_weekends"});
            return HolidayCenterData_(TextField(v, "id", p), ReadRows<Date_>(Field(v, "holidays"), p + "/holidays", ReadDate),
                                      ReadRows<Date_>(Field(v, "work_weekends"), p + "/work_weekends", ReadDate));
        }
        CompoundedOisFixedRow_ ReadFixed(const Json_& v, const String_& p) {
            Fields(v, p, {"start", "end", "payment", "accrual"});
            return {DateField(v, "start", p), DateField(v, "end", p), DateField(v, "payment", p), NumberField(v, "accrual", p)};
        }
        CompoundedOisCouponRow_ ReadCoupon(const Json_& v, const String_& p) {
            Fields(v, p, {"coupon_id", "start", "end", "payment"});
            return {TextField(v, "coupon_id", p), DateField(v, "start", p), DateField(v, "end", p), DateField(v, "payment", p)};
        }
        CompoundedOisObservationRow_ ReadObservation(const Json_& v, const String_& p) {
            Fields(v, p, {"start", "end", "fixing_time", "accrual", "coupon_id", "rate_start", "rate_end"});
            return {DateField(v, "start", p),     DateField(v, "end", p),       ReadTime(Field(v, "fixing_time"), p + "/fixing_time"),
                    NumberField(v, "accrual", p), TextField(v, "coupon_id", p), DateField(v, "rate_start", p),
                    DateField(v, "rate_end", p)};
        }
        CompoundedOisScriptSpec_ ReadSpec(const Json_& v) {
            const String_ p = "/spec";
            Fields(v, p,
                   {"instrument_id", "rule", "notional", "coupon", "direction", "currency", "collateral_currency", "collateral_type",
                    "discount_route", "projection_route", "fixing_index", "observation_basis", "fixed_basis", "min_abs_annuity", "calendar",
                    "settlement", "fixed_leg", "coupons", "observations"});
            Check(Bytes(TextField(v, "settlement", p)) == "IncludeUnsettledAtValuation", "UnsupportedCompoundedOisRule", p + "/settlement",
                  "IncludeUnsettledAtValuation");
            CompoundedOisScriptSpec_ s;
            s.instrumentId_ = TextField(v, "instrument_id", p);
            s.ruleId_ = TextField(v, "rule", p);
            s.notional_ = NumberField(v, "notional", p);
            s.coupon_ = NumberField(v, "coupon", p);
            s.minAbsAnnuity_ = NumberField(v, "min_abs_annuity", p);
            s.direction_ = TextField(v, "direction", p);
            s.currency_ = EnumField<Ccy_>(v, "currency", p);
            s.collateralCurrency_ = EnumField<Ccy_>(v, "collateral_currency", p);
            s.collateralType_ = EnumField<CollateralType_>(v, "collateral_type", p);
            s.discountRoute_ = TextField(v, "discount_route", p);
            s.projectionRoute_ = TextField(v, "projection_route", p);
            s.fixingIndex_ = TextField(v, "fixing_index", p);
            s.observationBasis_ = EnumField<DayBasis_>(v, "observation_basis", p);
            s.fixedBasis_ = EnumField<DayBasis_>(v, "fixed_basis", p);
            s.calendar_ = ReadCalendar(Field(v, "calendar"), p + "/calendar");
            s.fixedLeg_ = ReadRows<CompoundedOisFixedRow_>(Field(v, "fixed_leg"), p + "/fixed_leg", ReadFixed);
            s.coupons_ = ReadRows<CompoundedOisCouponRow_>(Field(v, "coupons"), p + "/coupons", ReadCoupon);
            s.observations_ = ReadRows<CompoundedOisObservationRow_>(Field(v, "observations"), p + "/observations", ReadObservation);
            return s;
        }
        struct FixingValue_ {
            DateTime_ time_;
            double value_;
        };
        struct FixingSeries_ {
            String_ index_;
            MarketFixingSnapshot_::history_t history_;
        };
        FixingValue_ ReadFixingValue(const Json_& v, const String_& p) {
            Fields(v, p, {"time", "value"});
            return {ReadTime(Field(v, "time"), p + "/time"), NumberField(v, "value", p)};
        }
        FixingSeries_ ReadFixingSeries(const Json_& v, const String_& p) {
            Fields(v, p, {"index", "history"});
            const auto index = TextField(v, "index", p);
            Check(!index.empty(), ARCHIVE_ERROR, p + "/index", "nonempty fixing index");
            const auto rows = ReadRows<FixingValue_>(Field(v, "history"), p + "/history", ReadFixingValue);
            MarketFixingSnapshot_::history_t history;
            for (size_t i = 0; i < rows.size(); ++i) {
                CalibrationDiagnostic_ d;
                d.sourceRow_ = static_cast<int>(i + 1);
                Check(history.emplace(rows[i].time_, rows[i].value_).second, ARCHIVE_ERROR, p + "/history/" + OrdinalText(i),
                      "unique fixing time, without duplicate or conflict", "", d);
            }
            return {index, history};
        }
        Script::ScriptValuationSettings_ ReadValuation(const Json_& v) {
            const String_ p = "/valuation";
            Fields(v, p, {"evaluation_date", "today_fixing_policy", "fixings"});
            Script::ScriptValuationSettings_ result;
            result.evaluationDate_ = DateField(v, "evaluation_date", p);
            result.todayFixingPolicy_ = EnumField<TodayFixingPolicy_>(v, "today_fixing_policy", p);
            const auto rows = ReadRows<FixingSeries_>(Field(v, "fixings"), p + "/fixings", ReadFixingSeries);
            MarketFixingSnapshot_::values_t values;
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto& r = rows[i];
                CalibrationDiagnostic_ d;
                d.sourceRow_ = static_cast<int>(i + 1);
                Check(values.emplace(r.index_, r.history_).second, ARCHIVE_ERROR, p + "/fixings/" + OrdinalText(i) + "/index", "unique fixing index",
                      "", d);
            }
            try {
                result.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(values));
            } catch (const Exception_& error) {
                Fail(ARCHIVE_ERROR, p + "/fixings", "valid immutable fixing snapshot", String_(error.what()));
            }
            return result;
        }
        std::optional<String_> ArchiveId(const Json_& v) {
            const auto spec = v.object_.find("spec");
            if (spec == v.object_.end())
                return {};
            const auto id = spec->second.object_.find("instrument_id");
            if (id == spec->second.object_.end() || id->second.kind_ != JsonKind_::STRING)
                return {};
            return String_(id->second.string_);
        }
    } // namespace
    Handle_<PreparedCompoundedOis_> LoadPreparedCompoundedOisJson(const String_& json) {
        std::optional<String_> id;
        try {
            const auto value = Parse(json);
            id = ArchiveId(value);
            Fields(value, "", {"schema", "spec", "valuation"});
            Check(Bytes(TextField(value, "schema", "")) == "dal.compounded-ois-preparation/1", "UnsupportedCalibrationSchema", "/schema",
                  "dal.compounded-ois-preparation/1");
            return PrepareCompoundedOis(ReadSpec(Field(value, "spec")), ReadValuation(Field(value, "valuation")));
        } catch (const CalibrationError_& error) {
            auto d = error.Failure().diagnostic_;
            auto path = d.field_.value_or("");
            if (d.stage_ == CalibrationStage_::Value_::Prepare && path.substr(0, 10) != "/valuation")
                path = "/spec" + path;
            d.stage_ = CalibrationStage_::Value_::Load;
            d.instrumentId_ = id;
            if (Bytes(d.code_) != "UnsupportedCalibrationSchema" && Bytes(d.code_) != "UnsupportedCompoundedOisRule")
                d.code_ = ARCHIVE_ERROR;
            Fail(d.code_, path, d.expected_.value_or("valid compounded OIS archive"), d.actual_.value_or(""), d);
        }
    }
} // namespace Dal
