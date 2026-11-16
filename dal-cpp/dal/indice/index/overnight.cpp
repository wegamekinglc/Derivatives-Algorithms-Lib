#include <dal/platform/platform.hpp>

#include <cctype>

#include <dal/indice/index/ir.hpp>
#include <dal/indice/parser/ir.hpp>
#include <dal/storage/canonicaljson_internal.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::Index {
    namespace {
        bool Plain(unsigned char c) {
            static const std::string ALLOWED = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-";
            return ALLOWED.find(static_cast<char>(c)) != std::string::npos;
        }
        String_ Encode(const String_& value) {
            static constexpr char HEX[] = "0123456789ABCDEF";
            String_ result;
            for (const unsigned char c : value) {
                if (Plain(c))
                    result.push_back(static_cast<char>(c));
                else {
                    result.push_back('%');
                    result.push_back(HEX[c >> 4]);
                    result.push_back(HEX[c & 15]);
                }
            }
            return result;
        }
        unsigned char Hex(char c) {
            const std::string hex = "0123456789ABCDEF";
            const auto pos = hex.find(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            REQUIRE(pos != std::string::npos, "InvalidIndex: invalid ON identifier escape");
            return static_cast<unsigned char>(pos);
        }
        String_ Decode(const String_& value) {
            String_ result;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '%') {
                    REQUIRE(value.size() - i >= 3, "InvalidIndex: incomplete ON identifier escape");
                    result.push_back(static_cast<char>(16 * Hex(value[i + 1]) + Hex(value[i + 2])));
                    i += 2;
                } else {
                    REQUIRE(Plain(static_cast<unsigned char>(value[i])), "InvalidIndex: ON identifiers require percent escaping");
                    result.push_back(value[i]);
                }
            }
            return result;
        }
        void Identifier(const String_& value) {
            REQUIRE(!value.empty() && value.find('\0') == String_::npos && CanonicalJson::ValidUtf8(std::string(value.c_str(), value.size())),
                    "InvalidIndex: ON requires nonempty UTF-8 identifiers without NUL");
        }
        Date_ ExactDate(const String_& value) {
            const auto date = Date::FromString(value);
            REQUIRE(date.IsValid() && Date::ToString(date) == value, "InvalidIndex: ON requires ISO date-only fields");
            return date;
        }
        void Validate(const OvernightObservationSpec_& s) {
            REQUIRE(s.currency_.String() != nullptr, "InvalidIndex: ON requires a supported currency");
            Identifier(s.fixingKey_);
            Identifier(s.route_);
            REQUIRE(s.accrualStart_.IsValid() && s.accrualEnd_.IsValid() && s.rateStart_.IsValid() && s.rateEnd_.IsValid(),
                    "InvalidIndex: valid ON dates required");
            REQUIRE(s.accrualStart_ < s.accrualEnd_ && s.rateStart_ < s.rateEnd_, "InvalidIndex: ordered ON intervals required");
            REQUIRE(s.basis_ == DayBasis_("ACT_360") || s.basis_ == DayBasis_("ACT_365F"), "InvalidIndex: ON requires ACT/360 or ACT/365F");
        }
        String_ Name(const OvernightObservationSpec_& s, const String_& token) {
            return "IR[" + String_(s.currency_.String()) + "," + token + "," + Encode(s.fixingKey_) + "," + Encode(s.route_) + "," +
                   Date::ToString(s.accrualStart_) + "," + Date::ToString(s.accrualEnd_) + "," + Date::ToString(s.rateStart_) + "," +
                   Date::ToString(s.rateEnd_) + "," + s.basis_.String() + "]";
        }
    } // namespace
    OvernightObservation_::OvernightObservation_(OvernightObservationSpec_ spec) : spec_(std::move(spec)) { Validate(spec_); }
    String_ OvernightObservation_::Name() const { return Index::Name(spec_, "ON2"); }
    Overnight_::Overnight_(const Ccy_& ccy,
                           String_ fixingKey,
                           String_ route,
                           const Date_& start,
                           const Date_& end,
                           const Date_& rateStart,
                           const Date_& rateEnd,
                           const DayBasis_& basis)
        : ccy_(ccy), fixingKey_(std::move(fixingKey)), route_(std::move(route)), start_(start), end_(end), rateStart_(rateStart), rateEnd_(rateEnd),
          basis_(basis) {
        Validate({ccy_, fixingKey_, route_, start_, end_, rateStart_, rateEnd_, basis_});
        REQUIRE(rateStart_ <= start_ && start_ < end_ && end_ <= rateEnd_, "InvalidIndex: ordered ON intersection within full rate interval");
    }

    String_ Overnight_::Name() const { return Index::Name({ccy_, fixingKey_, route_, start_, end_, rateStart_, rateEnd_, basis_}, "ON"); }
    std::unique_ptr<Index_> OvernightParser(const Vector_<String_>& fields) {
        REQUIRE(fields.size() == 9, "InvalidIndex: ON requires currency, fixing key, route, four dates and basis");
        const OvernightObservationSpec_ s{Ccy_(fields[0]),      Decode(fields[2]),    Decode(fields[3]),    ExactDate(fields[4]),
                                          ExactDate(fields[5]), ExactDate(fields[6]), ExactDate(fields[7]), DayBasis_(fields[8])};
        if (fields[1] == "ON2")
            return std::make_unique<OvernightObservation_>(s);
        return std::make_unique<Overnight_>(s.currency_, s.fixingKey_, s.route_, s.accrualStart_, s.accrualEnd_, s.rateStart_, s.rateEnd_, s.basis_);
    }
} // namespace Dal::Index
