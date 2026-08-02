//
// Created by dal-implementer on 2026/8/1.
//

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <dal/platform/platform.hpp>
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>

namespace Dal {
    namespace {
        namespace BcgExactMagnitudePrivate_ {
            constexpr int LIMB_BITS_ = 32;

            template <int LimbCount_, int MaxBitIndex_> struct Magnitude_ {
                static constexpr int LIMB_COUNT_ = LimbCount_;
                static constexpr int MAX_BIT_INDEX_ = MaxBitIndex_;

                std::array<std::uint32_t, LimbCount_> limbs_{};
                int first_ = LimbCount_;
                int last_ = -1;
            };

            template <class Magnitude_> inline void Reset_(Magnitude_* magnitude) noexcept {
                if (magnitude->first_ == Magnitude_::LIMB_COUNT_ && magnitude->last_ == -1)
                    return;
                if (magnitude->first_ >= 0 && magnitude->first_ <= magnitude->last_ && magnitude->last_ < Magnitude_::LIMB_COUNT_) {
                    for (int i = magnitude->first_; i <= magnitude->last_; ++i)
                        magnitude->limbs_[i] = 0;
                } else {
                    for (std::uint32_t& limb : magnitude->limbs_)
                        limb = 0;
                }
                magnitude->first_ = Magnitude_::LIMB_COUNT_;
                magnitude->last_ = -1;
            }

            template <class Magnitude_> inline void Touch_(int limb, Magnitude_* magnitude) noexcept {
                magnitude->first_ = std::min(magnitude->first_, limb);
                magnitude->last_ = std::max(magnitude->last_, limb);
            }

            template <class Magnitude_> inline bool WordFits_(int limb, std::uint32_t value) noexcept {
                if (value == 0)
                    return true;
                if (limb < 0 || limb >= Magnitude_::LIMB_COUNT_)
                    return false;
                const int availableBits = Magnitude_::MAX_BIT_INDEX_ - LIMB_BITS_ * limb + 1;
                return availableBits >= LIMB_BITS_ || (availableBits > 0 && (value >> availableBits) == 0);
            }

            template <class Magnitude_> inline bool AddAt_(int limb, std::uint32_t value, Magnitude_* magnitude) noexcept {
                std::uint64_t carry = value;
                while (carry != 0) {
                    if (limb < 0 || limb >= Magnitude_::LIMB_COUNT_)
                        return false;
                    const std::uint64_t sum = static_cast<std::uint64_t>(magnitude->limbs_[limb]) + carry;
                    const std::uint32_t word = static_cast<std::uint32_t>(sum);
                    if (!WordFits_<Magnitude_>(limb, word))
                        return false;
                    magnitude->limbs_[limb] = word;
                    Touch_(limb, magnitude);
                    carry = sum >> LIMB_BITS_;
                    ++limb;
                }
                return true;
            }

            template <class Magnitude_>
            inline bool AddShiftedLimb_(std::uint32_t value, int firstBit, Magnitude_* magnitude) noexcept {
                if (value == 0)
                    return true;
                if (firstBit < 0)
                    return false;
                const int limb = firstBit / LIMB_BITS_;
                const int shift = firstBit % LIMB_BITS_;
                const std::uint64_t shifted = static_cast<std::uint64_t>(value) << shift;
                return AddAt_(limb, static_cast<std::uint32_t>(shifted), magnitude) &&
                       AddAt_(limb + 1, static_cast<std::uint32_t>(shifted >> LIMB_BITS_), magnitude);
            }

            template <class Magnitude_>
            inline bool AddShiftedWord_(std::uint64_t value, int firstBit, Magnitude_* magnitude) noexcept {
                return AddShiftedLimb_(static_cast<std::uint32_t>(value), firstBit, magnitude) &&
                       AddShiftedLimb_(static_cast<std::uint32_t>(value >> LIMB_BITS_), firstBit + LIMB_BITS_, magnitude);
            }

            inline std::array<std::uint32_t, 4> Multiply53_(std::uint64_t lhs, std::uint64_t rhs) noexcept {
                const std::array<std::uint32_t, 2> left = {static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(lhs >> LIMB_BITS_)};
                const std::array<std::uint32_t, 2> right = {static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(rhs >> LIMB_BITS_)};
                std::array<std::uint32_t, 4> result{};
                for (int i = 0; i < 2; ++i) {
                    std::uint64_t carry = 0;
                    for (int j = 0; j < 2; ++j) {
                        const std::uint64_t sum = static_cast<std::uint64_t>(result[i + j]) + static_cast<std::uint64_t>(left[i]) * right[j] + carry;
                        result[i + j] = static_cast<std::uint32_t>(sum);
                        carry = sum >> LIMB_BITS_;
                    }
                    int output = i + 2;
                    while (carry != 0 && output < static_cast<int>(result.size())) {
                        const std::uint64_t sum = static_cast<std::uint64_t>(result[output]) + carry;
                        result[output] = static_cast<std::uint32_t>(sum);
                        carry = sum >> LIMB_BITS_;
                        ++output;
                    }
                }
                return result;
            }

            template <class Magnitude_>
            inline bool AddProduct_(std::uint64_t lhs, std::uint64_t rhs, int firstBit, Magnitude_* magnitude) noexcept {
                if (lhs == 0 || rhs == 0)
                    return true;
                const std::array<std::uint32_t, 4> product = Multiply53_(lhs, rhs);
                for (int word = 0; word < static_cast<int>(product.size()); ++word)
                    if (!AddShiftedLimb_(product[word], firstBit + LIMB_BITS_ * word, magnitude))
                        return false;
                return true;
            }

            template <class Magnitude_> inline void Trim_(Magnitude_* magnitude) noexcept {
                while (magnitude->first_ <= magnitude->last_ && magnitude->limbs_[magnitude->first_] == 0)
                    ++magnitude->first_;
                while (magnitude->last_ >= magnitude->first_ && magnitude->limbs_[magnitude->last_] == 0)
                    --magnitude->last_;
                if (magnitude->last_ < magnitude->first_) {
                    magnitude->first_ = Magnitude_::LIMB_COUNT_;
                    magnitude->last_ = -1;
                }
            }

            template <class Magnitude_> inline int Compare_(const Magnitude_& lhs, const Magnitude_& rhs) noexcept {
                if (lhs.last_ != rhs.last_)
                    return lhs.last_ < rhs.last_ ? -1 : 1;
                for (int i = lhs.last_; i >= std::min(lhs.first_, rhs.first_); --i)
                    if (lhs.limbs_[i] != rhs.limbs_[i])
                        return lhs.limbs_[i] < rhs.limbs_[i] ? -1 : 1;
                return 0;
            }

            template <class Magnitude_> inline bool Add_(const Magnitude_& rhs, Magnitude_* lhs) noexcept {
                for (int i = rhs.first_; i <= rhs.last_; ++i)
                    if (!AddAt_(i, rhs.limbs_[i], lhs))
                        return false;
                return true;
            }

            template <class Magnitude_> inline bool Subtract_(const Magnitude_& rhs, Magnitude_* lhs) noexcept {
                const int first = std::min(lhs->first_, rhs.first_);
                const int last = lhs->last_;
                lhs->first_ = first;
                std::uint64_t borrow = 0;
                for (int i = first; i <= last; ++i) {
                    Touch_(i, lhs);
                    const std::uint64_t subtrahend = static_cast<std::uint64_t>(rhs.limbs_[i]) + borrow;
                    const std::uint64_t minuend = lhs->limbs_[i];
                    if (minuend < subtrahend) {
                        lhs->limbs_[i] = static_cast<std::uint32_t>((1ULL << LIMB_BITS_) + minuend - subtrahend);
                        borrow = 1;
                    } else {
                        lhs->limbs_[i] = static_cast<std::uint32_t>(minuend - subtrahend);
                        borrow = 0;
                    }
                }
                Trim_(lhs);
                return borrow == 0;
            }

            template <class Magnitude_> inline bool Bit_(const Magnitude_& value, int bit) noexcept {
                if (bit < 0 || bit > Magnitude_::MAX_BIT_INDEX_)
                    return false;
                return (value.limbs_[bit / LIMB_BITS_] & (1U << (bit % LIMB_BITS_))) != 0;
            }

            template <class Magnitude_> inline bool HasBitBelow_(const Magnitude_& value, int exclusiveBit) noexcept {
                if (exclusiveBit <= 0 || value.last_ < value.first_)
                    return false;
                const int lastBit = std::min(exclusiveBit - 1, Magnitude_::MAX_BIT_INDEX_);
                const int lastLimb = lastBit / LIMB_BITS_;
                for (int i = value.first_; i < lastLimb; ++i)
                    if (value.limbs_[i] != 0)
                        return true;
                if (lastLimb < value.first_)
                    return false;
                const int bits = lastBit % LIMB_BITS_ + 1;
                const std::uint32_t mask =
                    bits == LIMB_BITS_ ? std::numeric_limits<std::uint32_t>::max() : (1U << bits) - 1U;
                return (value.limbs_[lastLimb] & mask) != 0;
            }

            template <class Magnitude_> inline int HighestBit_(const Magnitude_& value) noexcept {
                int highestLimbBit = 0;
                for (std::uint32_t top = value.limbs_[value.last_]; top > 1; top >>= 1U)
                    ++highestLimbBit;
                return LIMB_BITS_ * value.last_ + highestLimbBit;
            }
        } // namespace BcgExactMagnitudePrivate_

        namespace BcgScaledAlphaPrivate_ {
            struct AccumulatorBounds_ {
                int minExponent_;
                int maxExponent_;
                int limbBits_;
                int limbCount_;
                int storedSignificandBits_;
                int topRoundingCarry_;
            };

            struct CandidateBounds_ {
                int storedMinExponent_;
                int storedMaxExponent_;
                int alphaMinExponent_;
                int alphaMaxExponent_;
                int maxAlignmentDistance_;
                int logicalMagnitudeBits_;
                int candidateLimbCount_;
                int maxLogicalBitIndex_;
                int physicalMagnitudeBits_;
            };

            using BoundsFingerprint_ = std::array<int, 15>;

            constexpr CandidateBounds_ DeriveCandidateBounds_(const AccumulatorBounds_& input) {
                const int accumulatorBits = input.limbBits_ * input.limbCount_;
                const int storedMinExponent = input.minExponent_ + 1 - input.storedSignificandBits_;
                const int storedMaxExponent = input.maxExponent_ + input.topRoundingCarry_ - input.storedSignificandBits_;
                const int alphaMinExponent = storedMinExponent - storedMaxExponent;
                const int alphaMaxExponent = storedMaxExponent - storedMinExponent;
                const int productMinExponent = alphaMinExponent - 1074;
                const int productMaxExponent = alphaMaxExponent + 971;
                const int maxAlignmentDistance = std::max(productMaxExponent + 1074, 971 - productMinExponent);
                const int logicalMagnitudeBits = 2 * input.storedSignificandBits_ + maxAlignmentDistance + 1;
                const int candidateLimbCount = (logicalMagnitudeBits + input.limbBits_ - 1) / input.limbBits_;
                return {storedMinExponent,  storedMaxExponent,        alphaMinExponent,
                        alphaMaxExponent,   maxAlignmentDistance,     logicalMagnitudeBits,
                        candidateLimbCount, logicalMagnitudeBits - 1, candidateLimbCount * input.limbBits_};
            }

            constexpr BoundsFingerprint_ MakeBoundsFingerprint_(const AccumulatorBounds_& input, const CandidateBounds_& derived) {
                return {input.minExponent_,
                        input.maxExponent_,
                        input.limbBits_,
                        input.limbCount_,
                        input.storedSignificandBits_,
                        input.topRoundingCarry_,
                        derived.storedMinExponent_,
                        derived.storedMaxExponent_,
                        derived.alphaMinExponent_,
                        derived.alphaMaxExponent_,
                        derived.maxAlignmentDistance_,
                        derived.logicalMagnitudeBits_,
                        derived.candidateLimbCount_,
                        derived.maxLogicalBitIndex_,
                        derived.physicalMagnitudeBits_};
            }

            constexpr bool SameBoundsFingerprint_(const BoundsFingerprint_& lhs, const BoundsFingerprint_& rhs) {
                for (int i = 0; i < static_cast<int>(lhs.size()); ++i)
                    if (lhs[i] != rhs[i])
                        return false;
                return true;
            }

            constexpr bool SameAccumulatorBounds_(const AccumulatorBounds_& lhs, const AccumulatorBounds_& rhs) {
                return lhs.minExponent_ == rhs.minExponent_ && lhs.maxExponent_ == rhs.maxExponent_ && lhs.limbBits_ == rhs.limbBits_ &&
                       lhs.limbCount_ == rhs.limbCount_ && lhs.storedSignificandBits_ == rhs.storedSignificandBits_ &&
                       lhs.topRoundingCarry_ == rhs.topRoundingCarry_;
            }

            constexpr AccumulatorBounds_ WithTopRoundingCarryDelta_(const AccumulatorBounds_& input, int delta) {
                return {input.minExponent_, input.maxExponent_,           input.limbBits_,
                        input.limbCount_,   input.storedSignificandBits_, input.topRoundingCarry_ + delta};
            }

            constexpr AccumulatorBounds_ REVIEWED_ACCUMULATOR_BOUNDS_{-8608, 8352, 32, 530, 53, 1};
            constexpr BoundsFingerprint_ REVIEWED_BOUNDS_FINGERPRINT_{-8608,  8352,  32,    530,   53,  1,     -8660, 8300,
                                                                      -16960, 16960, 19005, 19112, 598, 19111, 19136};
            constexpr CandidateBounds_ REVIEWED_CANDIDATE_BOUNDS_ = DeriveCandidateBounds_(REVIEWED_ACCUMULATOR_BOUNDS_);
            constexpr bool REVIEWED_BOUNDS_FINGERPRINT_MATCHES_ = SameBoundsFingerprint_(
                MakeBoundsFingerprint_(REVIEWED_ACCUMULATOR_BOUNDS_, REVIEWED_CANDIDATE_BOUNDS_), REVIEWED_BOUNDS_FINGERPRINT_);

            static_assert(sizeof(double) == sizeof(std::uint64_t), "DAL35_BINARY64_SIZE_MISMATCH");
            static_assert(std::numeric_limits<double>::is_iec559, "DAL35_BINARY64_IEC559_REQUIRED");
            static_assert(std::numeric_limits<double>::digits == 53, "DAL35_BINARY64_SIGNIFICAND_MISMATCH");
            static_assert(std::numeric_limits<double>::min_exponent == -1021, "DAL35_BINARY64_MIN_EXPONENT_MISMATCH");
            static_assert(std::numeric_limits<double>::max_exponent == 1024, "DAL35_BINARY64_MAX_EXPONENT_MISMATCH");

            struct StoredScaledBits_ {
                std::uint64_t mantissaBits_;
                int exponent_;
                bool normalized_;
            };

            struct Dyadic_ {
                std::uint64_t significand_;
                int binaryExponent_;
                bool negative_;
            };

            struct ExactAlpha_ {
                std::uint64_t numerator_;
                std::uint64_t denominator_;
                int binaryExponent_;
                bool negative_;
            };

            // Internal byte tags avoid adding implementation details to the public Machinist enum surface.
            struct AlphaPath_ {
                static constexpr std::uint8_t DENOMINATOR_ZERO = 0;
                static constexpr std::uint8_t LEGACY_ZERO = 1;
                static constexpr std::uint8_t ORDINARY_NORMAL = 2;
                static constexpr std::uint8_t SCALED_EXACT = 3;
            };
            using AlphaPathValue_ = std::uint8_t;

            struct AlphaPlan_ {
                ExactAlpha_ exact_;
                AlphaPathValue_ path_;
            };

            constexpr std::uint64_t BINARY64_FRACTION_MASK_ = (1ULL << 52U) - 1ULL;
            constexpr std::uint64_t BINARY64_EXPONENT_MASK_ = 0x7ff0000000000000ULL;
            constexpr std::uint64_t BINARY64_SIGN_MASK_ = 0x8000000000000000ULL;

            inline int HighestBit_(std::uint64_t value) {
                int result = -1;
                while (value != 0) {
                    value >>= 1U;
                    ++result;
                }
                return result;
            }

            inline bool IsPowerOfTwo_(std::uint64_t value) { return value != 0 && (value & (value - 1U)) == 0; }

            inline Dyadic_ DecodeFinite_(std::uint64_t bits) {
                const std::uint64_t exponentField = (bits & BINARY64_EXPONENT_MASK_) >> 52U;
                const std::uint64_t fraction = bits & BINARY64_FRACTION_MASK_;
                const bool negative = (bits & BINARY64_SIGN_MASK_) != 0;
                if (exponentField == 0)
                    return {fraction, -1074, negative};
                return {(1ULL << 52U) | fraction, static_cast<int>(exponentField) - 1023 - 52, negative};
            }

            inline bool IsFiniteBits_(std::uint64_t bits) { return (bits & BINARY64_EXPONENT_MASK_) != BINARY64_EXPONENT_MASK_; }

            inline bool IsRawSubnormal_(std::uint64_t bits) { return (bits & BINARY64_EXPONENT_MASK_) == 0 && (bits & BINARY64_FRACTION_MASK_) != 0; }

            inline bool IsLegacyInteriorExponentDifference_(int difference) { return difference >= -1020 && difference <= 1022; }

            inline bool CanUseLegacyRatioFast_(const StoredScaledBits_& numerator, const StoredScaledBits_& denominator) {
                const int numeratorExponent = static_cast<int>((numerator.mantissaBits_ & BINARY64_EXPONENT_MASK_) >> 52U);
                const int denominatorExponent = static_cast<int>((denominator.mantissaBits_ & BINARY64_EXPONENT_MASK_) >> 52U);
                if (numeratorExponent == 0 || numeratorExponent == 0x7ff || denominatorExponent == 0 || denominatorExponent == 0x7ff)
                    return false;
                const int rawDifference = numeratorExponent - denominatorExponent;
                if (!IsLegacyInteriorExponentDifference_(rawDifference))
                    return false;
                const int storedDifference =
                    rawDifference + (numerator.normalized_ ? numerator.exponent_ : 0) - (denominator.normalized_ ? denominator.exponent_ : 0);
                return IsLegacyInteriorExponentDifference_(storedDifference);
            }

            inline ExactAlpha_ ReduceRatio_(Dyadic_ numerator, Dyadic_ denominator) {
                const std::uint64_t divisor = std::gcd(numerator.significand_, denominator.significand_);
                return {numerator.significand_ / divisor, denominator.significand_ / divisor, numerator.binaryExponent_ - denominator.binaryExponent_,
                        numerator.negative_ != denominator.negative_};
            }

            inline ExactAlpha_ DecodeStoredRatio_(const StoredScaledBits_& numerator, const StoredScaledBits_& denominator) {
                Dyadic_ decodedNumerator = DecodeFinite_(numerator.mantissaBits_);
                Dyadic_ decodedDenominator = DecodeFinite_(denominator.mantissaBits_);
                if (numerator.normalized_)
                    decodedNumerator.binaryExponent_ += numerator.exponent_;
                if (denominator.normalized_)
                    decodedDenominator.binaryExponent_ += denominator.exponent_;
                return ReduceRatio_(decodedNumerator, decodedDenominator);
            }

            inline int CompareScaledU64_(std::uint64_t lhs, int lhsExponent, std::uint64_t rhs, int rhsExponent) {
                const int lhsHighest = HighestBit_(lhs) + lhsExponent;
                const int rhsHighest = HighestBit_(rhs) + rhsExponent;
                if (lhsHighest != rhsHighest)
                    return lhsHighest < rhsHighest ? -1 : 1;
                if (lhsExponent == rhsExponent)
                    return lhs == rhs ? 0 : (lhs < rhs ? -1 : 1);
                if (lhsExponent > rhsExponent) {
                    const int shift = lhsExponent - rhsExponent;
                    const std::uint64_t shifted = lhs << shift;
                    return shifted == rhs ? 0 : (shifted < rhs ? -1 : 1);
                }
                const int shift = rhsExponent - lhsExponent;
                const std::uint64_t shifted = rhs << shift;
                return lhs == shifted ? 0 : (lhs < shifted ? -1 : 1);
            }

            inline int FloorLog2Ratio_(const ExactAlpha_& value) {
                const int numeratorHighest = HighestBit_(value.numerator_);
                const int denominatorHighest = HighestBit_(value.denominator_);
                const std::uint64_t normalizedNumerator = value.numerator_ << (52 - numeratorHighest);
                const std::uint64_t normalizedDenominator = value.denominator_ << (52 - denominatorHighest);
                return numeratorHighest - denominatorHighest + value.binaryExponent_ + (normalizedNumerator < normalizedDenominator ? -1 : 0);
            }

            inline bool IsExactDyadicNormalFinite_(const ExactAlpha_& value, int highestExponent) {
                if (!IsPowerOfTwo_(value.denominator_) || highestExponent < -1022 || highestExponent > 1023)
                    return false;
                if (highestExponent < 1023)
                    return true;
                const int denominatorExponent = HighestBit_(value.denominator_);
                return CompareScaledU64_(value.numerator_, value.binaryExponent_ - denominatorExponent, (1ULL << 53U) - 1ULL, 971) <= 0;
            }

            inline bool IsLegacyStageSafe_(const ExactAlpha_& value) {
                const int highestExponent = FloorLog2Ratio_(value);
                if (highestExponent >= -1021 && highestExponent <= 1022)
                    return true;
                return IsExactDyadicNormalFinite_(value, highestExponent);
            }

            inline bool ProveLegacyRatio_(const StoredScaledBits_& numerator, const StoredScaledBits_& denominator, const ExactAlpha_& exact) {
                if (IsRawSubnormal_(numerator.mantissaBits_) || IsRawSubnormal_(denominator.mantissaBits_))
                    return false;
                if (!numerator.normalized_ && !denominator.normalized_)
                    return IsLegacyStageSafe_(exact);
                const ExactAlpha_ raw = ReduceRatio_(DecodeFinite_(numerator.mantissaBits_), DecodeFinite_(denominator.mantissaBits_));
                if (!IsLegacyStageSafe_(raw))
                    return false;
                return IsLegacyStageSafe_(exact);
            }

            inline AlphaPlan_ ClassifyAlpha_(const StoredScaledBits_& numerator, const StoredScaledBits_& denominator) {
                const Dyadic_ decodedNumerator = DecodeFinite_(numerator.mantissaBits_);
                const Dyadic_ decodedDenominator = DecodeFinite_(denominator.mantissaBits_);
                if (decodedDenominator.significand_ == 0)
                    return {{0, 0, 0, false}, AlphaPath_::DENOMINATOR_ZERO};
                if (decodedNumerator.significand_ == 0)
                    return {{0, 1, 0, decodedNumerator.negative_ != decodedDenominator.negative_}, AlphaPath_::LEGACY_ZERO};
                const ExactAlpha_ exact = DecodeStoredRatio_(numerator, denominator);
                return {exact, ProveLegacyRatio_(numerator, denominator, exact) ? AlphaPath_::ORDINARY_NORMAL : AlphaPath_::SCALED_EXACT};
            }

            template <class LegacyConversion_>
            inline AlphaPlan_ ClassifyAlphaAndInvokeLegacy_(const StoredScaledBits_& numerator,
                                                            const StoredScaledBits_& denominator,
                                                            LegacyConversion_&& legacyConversion) {
                const AlphaPlan_ plan = ClassifyAlpha_(numerator, denominator);
                if (plan.path_ == AlphaPath_::LEGACY_ZERO || plan.path_ == AlphaPath_::ORDINARY_NORMAL)
                    legacyConversion();
                return plan;
            }

#if defined(DAL_BCG_WORKSPACE_BOUNDARY_TEST_SEAM)
            template <class LegacyConversion_, class ScaledEvaluation_>
            inline AlphaPlan_ ObserveAlphaDispatchForTest_(const StoredScaledBits_& numerator,
                                                          const StoredScaledBits_& denominator,
                                                          LegacyConversion_&& legacyConversion,
                                                          ScaledEvaluation_&& scaledEvaluation) {
                const AlphaPlan_ plan = ClassifyAlpha_(numerator, denominator);
                if (plan.path_ == AlphaPath_::LEGACY_ZERO || plan.path_ == AlphaPath_::ORDINARY_NORMAL)
                    legacyConversion();
                if (plan.path_ == AlphaPath_::SCALED_EXACT)
                    scaledEvaluation(plan.exact_);
                return plan;
            }
#endif

            constexpr int EXACT_CANDIDATE_LIMB_COUNT_ = REVIEWED_CANDIDATE_BOUNDS_.candidateLimbCount_;

            using ExactMagnitude_ = BcgExactMagnitudePrivate_::Magnitude_<EXACT_CANDIDATE_LIMB_COUNT_,
                                                                          REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_>;

            struct ExactWorkspace_ {
                ExactMagnitude_ positive_;
                ExactMagnitude_ negative_;
            };

            inline void ResetMagnitude_(ExactMagnitude_* magnitude) noexcept {
                BcgExactMagnitudePrivate_::Reset_(magnitude);
            }

            class WorkspaceCallReset_ {
                ExactWorkspace_* workspace_;

            public:
                explicit WorkspaceCallReset_(ExactWorkspace_* workspace) noexcept : workspace_(workspace) {}
                ~WorkspaceCallReset_() noexcept {
                    ResetMagnitude_(&workspace_->positive_);
                    ResetMagnitude_(&workspace_->negative_);
                }
            };

            inline void CheckCanonicalMagnitude_(const ExactMagnitude_& magnitude, const char* name) {
                if (magnitude.first_ == EXACT_CANDIDATE_LIMB_COUNT_ && magnitude.last_ == -1)
                    return;
                THROW(std::string("scaled candidate workspace ") + name + " is not canonical: first=" + std::to_string(magnitude.first_) +
                      ", last=" + std::to_string(magnitude.last_) + ", expected first=" + std::to_string(EXACT_CANDIDATE_LIMB_COUNT_) + ", last=-1");
            }

            inline void CheckCanonicalWorkspace_(const ExactWorkspace_& workspace) {
                CheckCanonicalMagnitude_(workspace.positive_, "positive");
                CheckCanonicalMagnitude_(workspace.negative_, "negative");
            }

            inline void CheckLogicalBitRange_(int firstBit, int lastBit) {
                if (firstBit < 0 || lastBit < firstBit || lastBit > REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_)
                    THROW(std::string("scaled candidate exact bit range violates reviewed [0,") +
                          std::to_string(REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_) + "] bound");
            }

            inline void AddProduct_(std::uint64_t lhs, std::uint64_t rhs, int shift, ExactMagnitude_* magnitude) {
                if (!BcgExactMagnitudePrivate_::AddProduct_(lhs, rhs, shift, magnitude))
                    CheckLogicalBitRange_(REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_ + 1,
                                          REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_ + 1);
            }

            inline int Compare_(const ExactMagnitude_& lhs, const ExactMagnitude_& rhs) {
                return BcgExactMagnitudePrivate_::Compare_(lhs, rhs);
            }

            inline void Subtract_(const ExactMagnitude_& subtrahend, ExactMagnitude_* minuend) {
                const int first = std::min(minuend->first_, subtrahend.first_);
                const int last = minuend->last_;
                CheckLogicalBitRange_(32 * first, std::min(REVIEWED_CANDIDATE_BOUNDS_.maxLogicalBitIndex_, 32 * last + 31));
                if (!BcgExactMagnitudePrivate_::Subtract_(subtrahend, minuend))
                    THROW("scaled candidate exact subtraction underflow");
            }

            inline bool Bit_(const ExactMagnitude_& value, int bit) {
                return BcgExactMagnitudePrivate_::Bit_(value, bit);
            }

            inline bool HasBitBelow_(const ExactMagnitude_& value, int exclusiveBit) {
                return BcgExactMagnitudePrivate_::HasBitBelow_(value, exclusiveBit);
            }

            inline int HighestBit_(const ExactMagnitude_& value) {
                return BcgExactMagnitudePrivate_::HighestBit_(value);
            }

            inline int CompareNormalized_(const ExactMagnitude_& numerator, std::uint64_t denominator) {
                const int numeratorHighest = HighestBit_(numerator);
                const int denominatorHighest = HighestBit_(denominator);
                std::uint64_t numeratorTop = 0;
                for (int bit = numeratorHighest; bit > numeratorHighest - 53; --bit)
                    numeratorTop = (numeratorTop << 1U) | static_cast<std::uint64_t>(Bit_(numerator, bit));
                const std::uint64_t denominatorTop = denominator << (52 - denominatorHighest);
                if (numeratorTop != denominatorTop)
                    return numeratorTop < denominatorTop ? -1 : 1;
                return HasBitBelow_(numerator, numeratorHighest - 52) ? 1 : 0;
            }

            inline int FloorLog2Ratio_(const ExactMagnitude_& numerator, std::uint64_t denominator) {
                return HighestBit_(numerator) - HighestBit_(denominator) + (CompareNormalized_(numerator, denominator) < 0 ? -1 : 0);
            }

            struct Division_ {
                std::uint64_t quotient_;
                std::uint64_t remainder_;
                bool started_;
            };

            inline void FeedDivisionBit_(bool bit, std::uint64_t denominator, Division_* division) {
                division->remainder_ = (division->remainder_ << 1U) | static_cast<std::uint64_t>(bit);
                const bool quotientBit = division->remainder_ >= denominator;
                if (quotientBit)
                    division->remainder_ -= denominator;
                if (!division->started_ && !quotientBit)
                    return;
                division->started_ = true;
                if (division->quotient_ > (((1ULL << 53U) - 1ULL) >> 1U))
                    THROW("scaled candidate quotient exceeds binary64 significand bound");
                division->quotient_ = (division->quotient_ << 1U) | static_cast<std::uint64_t>(quotientBit);
            }

            inline Division_ DivideBits_(const ExactMagnitude_& numerator, int lowestBit, std::uint64_t denominator) {
                Division_ result{0, 0, false};
                for (int bit = HighestBit_(numerator); bit >= lowestBit; --bit)
                    FeedDivisionBit_(Bit_(numerator, bit), denominator, &result);
                return result;
            }

            inline int RoundingRelation_(const ExactMagnitude_& numerator, int shift, std::uint64_t denominator, Division_* division) {
                if (shift >= 0) {
                    *division = DivideBits_(numerator, 0, denominator);
                    for (int i = 0; i < shift; ++i)
                        FeedDivisionBit_(false, denominator, division);
                    const std::uint64_t twiceRemainder = division->remainder_ << 1U;
                    return twiceRemainder == denominator ? 0 : (twiceRemainder < denominator ? -1 : 1);
                }

                const int discardedBits = -shift;
                if (discardedBits > HighestBit_(numerator))
                    *division = {0, 0, false};
                else
                    *division = DivideBits_(numerator, discardedBits, denominator);
                const std::int64_t comparison = static_cast<std::int64_t>(division->remainder_ << 1U) - static_cast<std::int64_t>(denominator);
                if (comparison > 0)
                    return 1;
                if (comparison == 0)
                    return HasBitBelow_(numerator, discardedBits) ? 1 : 0;
                if (comparison <= -2)
                    return -1;
                if (!Bit_(numerator, discardedBits - 1))
                    return -1;
                return HasBitBelow_(numerator, discardedBits - 1) ? 1 : 0;
            }

            struct RoundedClass_ {
                static constexpr std::uint8_t FINITE = 0;
                static constexpr std::uint8_t NON_FINITE = 1;
            };
            using RoundedClassValue_ = std::uint8_t;

            struct RoundedBinary64_ {
                std::uint64_t bits_;
                RoundedClassValue_ classification_;
            };

            inline RoundedBinary64_ Round_(const ExactMagnitude_& numerator, std::uint64_t denominator, int binaryExponent, bool negative) {
                int highestExponent = FloorLog2Ratio_(numerator, denominator) + binaryExponent;
                if (highestExponent >= 1024)
                    return {0, RoundedClass_::NON_FINITE};
                const int unitExponent = highestExponent <= -1023 ? -1074 : highestExponent - 52;
                Division_ division{};
                const int relation = RoundingRelation_(numerator, binaryExponent - unitExponent, denominator, &division);
                std::uint64_t significand = division.quotient_;
                if (relation > 0 || (relation == 0 && (significand & 1U) != 0))
                    ++significand;
                const std::uint64_t sign = negative ? BINARY64_SIGN_MASK_ : 0;
                if (significand == 0)
                    return {sign, RoundedClass_::FINITE};
                if (highestExponent <= -1023) {
                    if (significand < (1ULL << 52U))
                        return {sign | significand, RoundedClass_::FINITE};
                    return {sign | (1ULL << 52U), RoundedClass_::FINITE};
                }
                if (significand == (1ULL << 53U)) {
                    significand >>= 1U;
                    ++highestExponent;
                }
                if (highestExponent >= 1024)
                    return {0, RoundedClass_::NON_FINITE};
                const std::uint64_t exponent = static_cast<std::uint64_t>(highestExponent + 1023) << 52U;
                return {sign | exponent | (significand - (1ULL << 52U)), RoundedClass_::FINITE};
            }

            inline RoundedBinary64_
            EvaluateElement_(const ExactAlpha_& alpha, std::uint64_t valueBits, std::uint64_t baseBits, ExactWorkspace_* workspace) {
                WorkspaceCallReset_ cleanup(workspace);
                CheckCanonicalWorkspace_(*workspace);
                if (alpha.numerator_ == 0 || alpha.denominator_ == 0 || alpha.binaryExponent_ < REVIEWED_CANDIDATE_BOUNDS_.alphaMinExponent_ ||
                    alpha.binaryExponent_ > REVIEWED_CANDIDATE_BOUNDS_.alphaMaxExponent_)
                    THROW("scaled candidate exact alpha violates reviewed bounds");
                if (!IsFiniteBits_(valueBits) || !IsFiniteBits_(baseBits))
                    THROW("scaled candidate evaluator requires finite binary64 inputs");

                const Dyadic_ value = DecodeFinite_(valueBits);
                const Dyadic_ base = DecodeFinite_(baseBits);
                const bool productNegative = alpha.negative_ != value.negative_;
                if (value.significand_ == 0 && base.significand_ == 0) {
                    const bool negative = productNegative == base.negative_ ? productNegative : false;
                    return {negative ? BINARY64_SIGN_MASK_ : 0, RoundedClass_::FINITE};
                }

                const int productExponent = alpha.binaryExponent_ + value.binaryExponent_;
                int commonExponent = productExponent;
                if (value.significand_ == 0)
                    commonExponent = base.binaryExponent_;
                else if (base.significand_ != 0)
                    commonExponent = std::min(productExponent, base.binaryExponent_);

                if (value.significand_ != 0)
                    AddProduct_(alpha.numerator_, value.significand_, productExponent - commonExponent,
                                productNegative ? &workspace->negative_ : &workspace->positive_);
                if (base.significand_ != 0)
                    AddProduct_(alpha.denominator_, base.significand_, base.binaryExponent_ - commonExponent,
                                base.negative_ ? &workspace->negative_ : &workspace->positive_);

                const int comparison = Compare_(workspace->positive_, workspace->negative_);
                if (comparison == 0)
                    return {0, RoundedClass_::FINITE};
                ExactMagnitude_* magnitude = comparison > 0 ? &workspace->positive_ : &workspace->negative_;
                const ExactMagnitude_& subtrahend = comparison > 0 ? workspace->negative_ : workspace->positive_;
                if (subtrahend.last_ >= subtrahend.first_)
                    Subtract_(subtrahend, magnitude);
                return Round_(*magnitude, alpha.denominator_, commonExponent, comparison < 0);
            }

            struct CandidateSubject_ {
                static constexpr std::uint8_t NONE = 0;
                static constexpr std::uint8_t X = 1;
                static constexpr std::uint8_t RESIDUAL = 2;
                static constexpr std::uint8_t SHADOW_RESIDUAL = 3;
            };
            using CandidateSubjectValue_ = std::uint8_t;

            struct CandidateEvidence_ {
                CandidateSubjectValue_ subject_;
                int firstNonFiniteIndex_;
            };

            struct CandidateGroup_ {
                const Vector_<>* direction_;
                const Vector_<>* xBase_;
                const Vector_<>* residualBase_;
                const Vector_<>* shadowBase_;
                Vector_<>* xOutput_;
                Vector_<>* residualInOut_;
                Vector_<>* shadowInOut_;
            };

            inline std::uint64_t CandidateBits_(double value) {
                std::uint64_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                return bits;
            }

            inline CandidateEvidence_ EvaluateCandidateSubject_(const ExactAlpha_& alpha,
                                                                const Vector_<>& values,
                                                                const Vector_<>& base,
                                                                CandidateSubjectValue_ subject,
                                                                ExactWorkspace_* workspace,
                                                                Vector_<>* output) {
                for (int i = 0; i < static_cast<int>(values.size()); ++i) {
                    const RoundedBinary64_ rounded = EvaluateElement_(alpha, CandidateBits_(values[i]), CandidateBits_(base[i]), workspace);
                    if (rounded.classification_ == RoundedClass_::NON_FINITE)
                        return {subject, i};
                    std::memcpy(&(*output)[i], &rounded.bits_, sizeof(rounded.bits_));
                }
                return {CandidateSubject_::NONE, -1};
            }

            inline CandidateEvidence_ EvaluateCandidateGroup_(const ExactAlpha_& alpha, const CandidateGroup_& group, ExactWorkspace_* workspace) {
                CandidateEvidence_ evidence =
                    EvaluateCandidateSubject_(alpha, *group.direction_, *group.xBase_, CandidateSubject_::X, workspace, group.xOutput_);
                if (evidence.subject_ != CandidateSubject_::NONE)
                    return evidence;

                ExactAlpha_ negativeAlpha = alpha;
                negativeAlpha.negative_ = !negativeAlpha.negative_;
                evidence = EvaluateCandidateSubject_(negativeAlpha, *group.residualInOut_, *group.residualBase_, CandidateSubject_::RESIDUAL,
                                                     workspace, group.residualInOut_);
                if (evidence.subject_ != CandidateSubject_::NONE)
                    return evidence;
                if (group.shadowInOut_ == nullptr)
                    return evidence;
                return EvaluateCandidateSubject_(negativeAlpha, *group.shadowInOut_, *group.shadowBase_, CandidateSubject_::SHADOW_RESIDUAL,
                                                 workspace, group.shadowInOut_);
            }

            static_assert(std::is_standard_layout<ExactMagnitude_>::value, "DAL35_EXACT_MAGNITUDE_MUST_BE_STANDARD_LAYOUT");
            static_assert(std::is_trivially_copyable<ExactMagnitude_>::value, "DAL35_EXACT_MAGNITUDE_MUST_BE_TRIVIALLY_COPYABLE");
            static_assert(std::is_standard_layout<ExactWorkspace_>::value, "DAL35_EXACT_WORKSPACE_MUST_BE_STANDARD_LAYOUT");
            static_assert(std::is_trivially_copyable<ExactWorkspace_>::value, "DAL35_EXACT_WORKSPACE_MUST_BE_TRIVIALLY_COPYABLE");
        } // namespace BcgScaledAlphaPrivate_
    } // namespace
} // namespace Dal
