#include <cstdint>
#include <dal/calibration/validation_internal.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/xccymc.hpp>
#include <dal/storage/json.hpp>
#include <limits>

namespace Dal {
#include <dal/auto/MG_XccyMonteCarloPhase_enum.inc>
    namespace {
        using namespace CalibrationInternal;
        std::pair<size_t, size_t> Reservation(const XccyMonteCarloPhase_& phase) {
            if (phase == XccyMonteCarloPhase_::Value_::Fit)
                return {0, 262144};
            if (phase == XccyMonteCarloPhase_::Value_::Validation)
                return {262144, 1310720};
            Check(phase == XccyMonteCarloPhase_::Value_::HeldOut, "InvalidCalibrationPlan", "/phase", "Fit, Validation or HeldOut",
                  phase == XccyMonteCarloPhase_::Value_::_NOT_SET ? String_("_NOT_SET") : String_(std::to_string(static_cast<int>(phase.Switch()))));
            return {1310720, 2359296};
        }
        size_t Multiply(size_t a, size_t b) {
            Check(b == 0 || a <= std::numeric_limits<size_t>::max() / b, "InvalidCalibrationPlan", "/sampling", "counts within size_t");
            Check(b == 0 || a <= std::numeric_limits<uint64_t>::max() / b, "InvalidCalibrationPlan", "/sampling", "counts within RNG uint64_t");
            return a * b;
        }
        String_ Identity(const XccyMcReservation_& r) {
            return Fingerprint(Obj({{"layout", Str(r.version_)},
                                    {"stream", Str("dal.mrg32-normal-paths/1")},
                                    {"generator", Str("MRG32")},
                                    {"normal", Str("Precise")},
                                    {"bridge", Bool(false)},
                                    {"phase", Str(r.phase_.String())},
                                    {"mapping", Str(r.mappingIdentity_)},
                                    {"seed", Num(r.seed_)},
                                    {"dimension", Str(OrdinalText(r.dimension_))},
                                    {"reservation_start", Str(OrdinalText(r.reservedFirst_))},
                                    {"reservation_end", Str(OrdinalText(r.reservedEnd_))},
                                    {"ranges", Array(r.ranges_, [](const auto& x) {
                                         return Obj({{"first", Str(OrdinalText(x.firstPath_))},
                                                     {"end", Str(OrdinalText(x.pathEnd_))},
                                                     {"coordinate_first", Str(OrdinalText(x.firstCoordinate_))},
                                                     {"coordinate_end", Str(OrdinalText(x.coordinateEnd_))}});
                                     })}}));
        }
        void RequireLedger(const XccyMcReservation_& r) {
            const auto reserved = Reservation(r.phase_);
            bool valid = Bytes(r.version_) == "dal.xccy-gaussian-phase-ranges/1" && r.reservedFirst_ == reserved.first &&
                         r.reservedEnd_ == reserved.second && r.seed_ >= 0 && !r.mappingIdentity_.empty() && r.ranges_.size() >= 2;
            size_t previous = reserved.first, count = 0;
            for (const auto& x : r.ranges_) {
                valid &= x.firstPath_ == previous && x.pathEnd_ > x.firstPath_ && x.pathEnd_ <= reserved.second;
                valid &= x.firstCoordinate_ == Multiply(x.firstPath_, r.dimension_) && x.coordinateEnd_ == Multiply(x.pathEnd_, r.dimension_);
                const size_t paths = x.pathEnd_ - x.firstPath_;
                valid &= paths >= 2 && (count == 0 || count == paths);
                count = paths;
                previous = x.pathEnd_;
            }
            Check(valid && Bytes(r.identity_) == Bytes(Identity(r)), "CalibrationContextMismatch", "/sampling/ranges",
                  "unchanged checked phase ledger");
        }
    } // namespace

    XccyMonteCarloPhase_ ParseXccyMonteCarloPhase(const String_& token) {
        for (const auto phase : XccyMonteCarloPhaseListAll())
            if (CalibrationInternal::Bytes(token) == phase.String())
                return phase;
        CalibrationInternal::Fail("InvalidCalibrationPlan", "/phase", "Fit, Validation or HeldOut", token);
    }
    XccyMcReservation_ PrepareXccyMcReservation(const XccyMcSettings_& s, const CalibrationMcMapping_& mapping) {
        using namespace CalibrationInternal;
        const auto reserved = Reservation(s.phase_);
        Check(s.batches_ >= 2 && s.pathsPerBatch_ >= 2 && s.seed_ >= 0, "InvalidCalibrationPlan", "/sampling",
              "at least two batches of two paths and nonnegative seed",
              String_(std::to_string(s.batches_)) + " batches, " + String_(std::to_string(s.pathsPerBatch_)) + " paths, seed " +
                  String_(std::to_string(s.seed_)));
        Check(Bytes(s.streamVersion_) == "dal.mrg32-normal-paths/1", "InvalidCalibrationPlan", "/sampling", "dal.mrg32-normal-paths/1",
              s.streamVersion_);
        Check(mapping.seed_ == s.seed_ && Bytes(mapping.streamVersion_) == Bytes(s.streamVersion_) && !mapping.identity_.empty() &&
                  mapping.dimension_ % 5 == 0 && Bytes(mapping.generator_) == "mrg32" && Bytes(mapping.normalPrecision_) == "Precise" &&
                  !mapping.brownianBridge_,
              "CalibrationContextMismatch", "/sampling/mapping", "same seed/version, five innovations, MRG32 Precise and no bridge");
        const size_t count = Multiply(static_cast<size_t>(s.batches_), static_cast<size_t>(s.pathsPerBatch_));
        Check(count <= reserved.second - reserved.first, "CalibrationBudgetExceeded", "/sampling", "prefix within frozen phase capacity",
              OrdinalText(count));
        Multiply(reserved.second, mapping.dimension_);
        XccyMcReservation_ result;
        result.phase_ = s.phase_;
        result.seed_ = s.seed_;
        result.dimension_ = mapping.dimension_;
        result.mappingIdentity_ = mapping.identity_;
        result.reservedFirst_ = reserved.first;
        result.reservedEnd_ = reserved.second;
        for (int batch = 0; batch < s.batches_; ++batch) {
            const size_t first = result.reservedFirst_ + static_cast<size_t>(batch) * s.pathsPerBatch_;
            const size_t end = first + s.pathsPerBatch_;
            result.ranges_.push_back({first, end, first * mapping.dimension_, end * mapping.dimension_});
        }
        result.identity_ = Identity(result);
        return result;
    }

    void RequireXccyMcIndependent(const XccyMcReservation_& a, const XccyMcReservation_& b, bool crn) {
        using namespace CalibrationInternal;
        RequireLedger(a);
        RequireLedger(b);
        Check(Bytes(a.mappingIdentity_) == Bytes(b.mappingIdentity_) && a.dimension_ == b.dimension_, "CalibrationContextMismatch",
              "/sampling/mapping", "one common mapping; mixed mappings require a separate coordinate allocator");
        if (crn) {
            Check(Bytes(a.identity_) == Bytes(b.identity_), "CalibrationContextMismatch", "/sampling/ranges", "identical declared CRN reservation");
            return;
        }
        Check(a.dimension_ > 0 && b.dimension_ > 0, "InvalidCalibrationPlan", "/sampling/ranges", "nonempty coordinates for independent draws");
        if (a.seed_ != b.seed_)
            return;
        for (const auto& x : a.ranges_)
            for (const auto& y : b.ranges_)
                Check(x.coordinateEnd_ <= y.firstCoordinate_ || y.coordinateEnd_ <= x.firstCoordinate_, "InvalidCalibrationPlan", "/sampling/ranges",
                      "nonoverlapping independent scalar coordinates");
    }
} // namespace Dal
