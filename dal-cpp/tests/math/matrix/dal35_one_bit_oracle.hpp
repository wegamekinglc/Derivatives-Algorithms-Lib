//
// Created by dal-implementer on 2026/7/29.
//

#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace Dal35OneBitOracle_ {
    using OracleNat_ = std::vector<std::uint8_t>;

    struct OracleAlpha_ {
        OracleNat_ numerator_;
        OracleNat_ denominator_;
        int binaryExponent_;
        bool negative_;
    };

    struct OracleInput_ {
        OracleAlpha_ alpha_;
        std::uint64_t valueBits_;
        std::uint64_t baseBits_;
    };

    enum class OracleClass_ : std::uint8_t { FINITE, NON_FINITE };

    struct OracleResult_ {
        std::uint64_t bits_;
        OracleClass_ classification_;
    };

    struct RawFinite_ {
        OracleNat_ significand_;
        int binaryExponent_;
        bool negative_;
    };

    struct Division_ {
        OracleNat_ quotient_;
        OracleNat_ remainder_;
    };

    inline void Trim_(OracleNat_* value) {
        while (!value->empty() && value->back() == 0)
            value->pop_back();
    }

    inline OracleNat_ FromU64_(std::uint64_t value) {
        OracleNat_ result;
        while (value != 0) {
            result.push_back(static_cast<std::uint8_t>(value & 1U));
            value >>= 1U;
        }
        return result;
    }

    inline std::uint64_t ToU64_(const OracleNat_& value) {
        std::uint64_t result = 0;
        for (int bit = static_cast<int>(value.size()) - 1; bit >= 0; --bit)
            result = (result << 1U) | value[bit];
        return result;
    }

    inline int Compare_(const OracleNat_& lhs, const OracleNat_& rhs) {
        if (lhs.size() != rhs.size())
            return lhs.size() < rhs.size() ? -1 : 1;
        for (int bit = static_cast<int>(lhs.size()) - 1; bit >= 0; --bit)
            if (lhs[bit] != rhs[bit])
                return lhs[bit] < rhs[bit] ? -1 : 1;
        return 0;
    }

    inline OracleNat_ Shift_(const OracleNat_& value, int bits) {
        if (value.empty())
            return {};
        OracleNat_ result(bits, 0);
        result.insert(result.end(), value.begin(), value.end());
        return result;
    }

    inline OracleNat_ Add_(const OracleNat_& lhs, const OracleNat_& rhs) {
        OracleNat_ result(std::max(lhs.size(), rhs.size()) + 1, 0);
        std::uint8_t carry = 0;
        for (int bit = 0; bit < static_cast<int>(result.size()); ++bit) {
            const std::uint8_t left = bit < static_cast<int>(lhs.size()) ? lhs[bit] : 0;
            const std::uint8_t right = bit < static_cast<int>(rhs.size()) ? rhs[bit] : 0;
            const std::uint8_t sum = left + right + carry;
            result[bit] = sum & 1U;
            carry = sum >> 1U;
        }
        Trim_(&result);
        return result;
    }

    inline OracleNat_ Subtract_(const OracleNat_& lhs, const OracleNat_& rhs) {
        OracleNat_ result(lhs.size(), 0);
        int borrow = 0;
        for (int bit = 0; bit < static_cast<int>(lhs.size()); ++bit) {
            int difference = static_cast<int>(lhs[bit]) - borrow;
            if (bit < static_cast<int>(rhs.size()))
                difference -= rhs[bit];
            if (difference < 0) {
                difference += 2;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result[bit] = static_cast<std::uint8_t>(difference);
        }
        Trim_(&result);
        return result;
    }

    inline OracleNat_ Multiply_(const OracleNat_& lhs, const OracleNat_& rhs) {
        OracleNat_ result;
        for (int bit = 0; bit < static_cast<int>(rhs.size()); ++bit)
            if (rhs[bit] != 0)
                result = Add_(result, Shift_(lhs, bit));
        return result;
    }

    inline Division_ Divide_(const OracleNat_& numerator, const OracleNat_& denominator) {
        Division_ result{{}, numerator};
        if (Compare_(result.remainder_, denominator) < 0)
            return result;
        const int maximumShift = static_cast<int>(result.remainder_.size() - denominator.size());
        result.quotient_.assign(maximumShift + 1, 0);
        for (int shift = maximumShift; shift >= 0; --shift) {
            const OracleNat_ shiftedDenominator = Shift_(denominator, shift);
            if (Compare_(result.remainder_, shiftedDenominator) >= 0) {
                result.remainder_ = Subtract_(result.remainder_, shiftedDenominator);
                result.quotient_[shift] = 1;
            }
        }
        Trim_(&result.quotient_);
        return result;
    }

    inline RawFinite_ Decode_(std::uint64_t bits) {
        const std::uint64_t exponent = (bits >> 52U) & 0x7ffU;
        const std::uint64_t fraction = bits & ((1ULL << 52U) - 1ULL);
        if (exponent == 0)
            return {FromU64_(fraction), -1074, (bits >> 63U) != 0};
        return {FromU64_((1ULL << 52U) | fraction), static_cast<int>(exponent) - 1023 - 52, (bits >> 63U) != 0};
    }

    inline int FloorLog2Ratio_(const OracleNat_& numerator, const OracleNat_& denominator) {
        const int initial = static_cast<int>(numerator.size()) - static_cast<int>(denominator.size());
        if (initial >= 0)
            return Compare_(numerator, Shift_(denominator, initial)) < 0 ? initial - 1 : initial;
        return Compare_(Shift_(numerator, -initial), denominator) < 0 ? initial - 1 : initial;
    }

    inline OracleResult_ Round_(const OracleNat_& numerator, const OracleNat_& denominator, int binaryExponent, bool negative) {
        int highestExponent = FloorLog2Ratio_(numerator, denominator) + binaryExponent;
        if (highestExponent >= 1024)
            return {0, OracleClass_::NON_FINITE};
        const int unitExponent = highestExponent <= -1023 ? -1074 : highestExponent - 52;
        const int shift = binaryExponent - unitExponent;
        const OracleNat_ scaledNumerator = shift >= 0 ? Shift_(numerator, shift) : numerator;
        const OracleNat_ scaledDenominator = shift >= 0 ? denominator : Shift_(denominator, -shift);
        Division_ division = Divide_(scaledNumerator, scaledDenominator);
        const int halfComparison = Compare_(Shift_(division.remainder_, 1), scaledDenominator);
        std::uint64_t significand = ToU64_(division.quotient_);
        if (halfComparison > 0 || (halfComparison == 0 && (significand & 1U) != 0))
            ++significand;

        const std::uint64_t sign = negative ? (1ULL << 63U) : 0;
        if (significand == 0)
            return {sign, OracleClass_::FINITE};
        if (highestExponent <= -1023) {
            if (significand < (1ULL << 52U))
                return {sign | significand, OracleClass_::FINITE};
            return {sign | (1ULL << 52U), OracleClass_::FINITE};
        }
        if (significand == (1ULL << 53U)) {
            significand >>= 1U;
            ++highestExponent;
        }
        if (highestExponent >= 1024)
            return {0, OracleClass_::NON_FINITE};
        const std::uint64_t exponent = static_cast<std::uint64_t>(highestExponent + 1023) << 52U;
        return {sign | exponent | (significand - (1ULL << 52U)), OracleClass_::FINITE};
    }

    inline OracleResult_ Evaluate_(const OracleInput_& input) {
        const RawFinite_ value = Decode_(input.valueBits_);
        const RawFinite_ base = Decode_(input.baseBits_);
        const bool productNegative = input.alpha_.negative_ != value.negative_;
        if (value.significand_.empty() && base.significand_.empty()) {
            const bool negative = productNegative == base.negative_ ? productNegative : false;
            return {negative ? (1ULL << 63U) : 0, OracleClass_::FINITE};
        }

        const int productExponent = input.alpha_.binaryExponent_ + value.binaryExponent_;
        int commonExponent = productExponent;
        if (value.significand_.empty())
            commonExponent = base.binaryExponent_;
        else if (!base.significand_.empty())
            commonExponent = std::min(productExponent, base.binaryExponent_);

        OracleNat_ positive;
        OracleNat_ negative;
        if (!value.significand_.empty()) {
            OracleNat_ product = Shift_(Multiply_(input.alpha_.numerator_, value.significand_), productExponent - commonExponent);
            (productNegative ? negative : positive) = std::move(product);
        }
        if (!base.significand_.empty()) {
            const OracleNat_ baseTerm = Shift_(Multiply_(input.alpha_.denominator_, base.significand_), base.binaryExponent_ - commonExponent);
            if (base.negative_)
                negative = Add_(negative, baseTerm);
            else
                positive = Add_(positive, baseTerm);
        }

        const int comparison = Compare_(positive, negative);
        if (comparison == 0)
            return {0, OracleClass_::FINITE};
        const OracleNat_ magnitude = comparison > 0 ? Subtract_(positive, negative) : Subtract_(negative, positive);
        return Round_(magnitude, input.alpha_.denominator_, commonExponent, comparison < 0);
    }
} // namespace Dal35OneBitOracle_
