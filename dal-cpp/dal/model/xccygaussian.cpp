#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/model/xccygaussianmath.hpp>

namespace Dal::XccyGaussianDetail {
    void Check(bool valid, const String_& code, const String_& field, const String_& expected, const String_& actual) {
        CalibrationDiagnostic_ context;
        if (code == "CalibrationNumericalFailure")
            context.stage_ = CalibrationStage_::Value_::Evaluate;
        CalibrationInternal::Check(valid, code, field, expected, actual, context);
    }

    void CheckAtDate(bool valid, const String_& field, const Date_& date, double actual) {
        if (valid)
            return;
        CalibrationDiagnostic_ context;
        context.stage_ = CalibrationStage_::Value_::Evaluate;
        context.eventDate_ = date;
        CalibrationInternal::Check(valid, "CalibrationNumericalFailure", "/model/transition/" + field, "finite model value in its supported range",
                                   CalibrationInternal::NumberText(actual), context);
    }

    void RethrowAtDate(const CalibrationError_& error, const Date_& from, const Date_& to) {
        auto context = error.Failure().diagnostic_;
        context.eventDate_ = to;
        const String_ interval = Date::ToString(from) + " -> " + Date::ToString(to) + ": " + error.what();
        CalibrationInternal::Fail(context.code_, context.field_.value_or("/model/transition"), context.expected_.value_or("valid transition"),
                                  interval, context);
    }
} // namespace Dal::XccyGaussianDetail
