#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/xccymcbasket_internal.hpp>

namespace Dal::XccyMcBasketInternal {
    using namespace CalibrationInternal;

    size_t Add(size_t a, size_t b, size_t limit) {
        Check(a <= limit && b <= limit - a, "InvalidCalibrationPlan", "/sampling", "checked endpoint addition",
              OrdinalText(a) + "+" + OrdinalText(b) + "; limit=" + OrdinalText(limit));
        return a + b;
    }
    size_t Multiply(size_t a, size_t b, size_t limit) {
        Check(b == 0 || a <= limit / b, "InvalidCalibrationPlan", "/sampling", "checked count multiplication",
              OrdinalText(a) + "*" + OrdinalText(b) + "; limit=" + OrdinalText(limit));
        return a * b;
    }
    void Storage(size_t count, size_t width) {
        static_cast<void>(Multiply(count, width, static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max())));
    }
    ScalarNormals_::ScalarNormals_(int seed, size_t dimension, const XccyMcRange_& range)
        : dimension_(dimension), cursor_(range.firstCoordinate_), end_(range.coordinateEnd_), pathsLeft_(range.pathEnd_ - range.firstPath_) {
        Check(seed >= 0 && range.pathEnd_ >= range.firstPath_, "InvalidCalibrationPlan", "/sampling", "valid seed and ordered path endpoints");
        Check(Add(cursor_, Multiply(pathsLeft_, dimension_)) == end_, "CalibrationContextMismatch", "/sampling/ranges",
              "exact scalar span for all requested path vectors");
        Storage(dimension_, sizeof(double));
        if (dimension_ > 0) {
            rng_ = New(RNGType_("MRG32"), seed, 1, true);
            rng_->SkipNormalTo(cursor_);
        }
    }
    Vector_<> ScalarNormals_::Next() {
        Check(pathsLeft_ > 0, "CalibrationContextMismatch", "/sampling/ranges", "no extra normal vectors");
        Vector_<> normals(dimension_);
        if (rng_)
            rng_->FillNormal(&normals);
        Check(normals.size() == dimension_, "CalibrationContextMismatch", "/sampling/mapping", "exact member normal dimension");
        cursor_ = Add(cursor_, dimension_);
        --pathsLeft_;
        Check(cursor_ <= end_, "CalibrationContextMismatch", "/sampling/ranges", "cursor within checked batch endpoint");
        return normals;
    }
    void ScalarNormals_::Finish() const {
        Check(pathsLeft_ == 0 && cursor_ == end_, "CalibrationContextMismatch", "/sampling/ranges", "no missing normals; exact end cursor");
    }
} // namespace Dal::XccyMcBasketInternal
