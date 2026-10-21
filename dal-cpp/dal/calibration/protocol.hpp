#pragma once

#include <dal/calibration/types.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    class CalibrationInstrument_;
    class CalibrationReport_;

    class CalibrationMarketQuote_ final {
        String_ id_, fingerprint_;
        double value_;
        CalibrationValueUnit_ unit_;
        CalibrationQuoteConvention_ convention_;
        CalibrationMarketQuote_(String_ id, double value, CalibrationValueUnit_ unit, CalibrationQuoteConvention_ convention);
        friend Handle_<CalibrationMarketQuote_>
        NewCalibrationMarketQuote(const String_&, double, const CalibrationValueUnit_&, const std::optional<CalibrationQuoteConvention_>&);

    public:
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] double Value() const { return value_; }
        [[nodiscard]] const CalibrationValueUnit_& Unit() const { return unit_; }
        [[nodiscard]] const CalibrationQuoteConvention_& Convention() const { return convention_; }
        [[nodiscard]] const String_& Fingerprint() const { return fingerprint_; }
    };
    class CalibrationObservation_ final {
        String_ id_, measureId_;
        Handle_<CalibrationInstrument_> instrument_;
        Handle_<CalibrationMarketQuote_> quote_;
        double errorScale_;
        CalibrationObservation_(
            String_ id, Handle_<CalibrationInstrument_> instrument, String_ measureId, Handle_<CalibrationMarketQuote_> quote, double errorScale);
        friend Handle_<CalibrationObservation_> NewCalibrationObservation(
            const String_&, const Handle_<CalibrationInstrument_>&, const String_&, const Handle_<CalibrationMarketQuote_>&, double);

    public:
        [[nodiscard]] const String_& Id() const { return id_; }
        [[nodiscard]] const String_& MeasureId() const { return measureId_; }
        [[nodiscard]] const Handle_<CalibrationInstrument_>& Instrument() const { return instrument_; }
        [[nodiscard]] const Handle_<CalibrationMarketQuote_>& Quote() const { return quote_; }
        [[nodiscard]] double ErrorScale() const { return errorScale_; }
    };
    struct CalibrationObservationAxes_ {
        Vector_<Handle_<CalibrationObservation_>> observations_;
        Vector_<Handle_<CalibrationMarketQuote_>> quotes_;
        Vector_<int> quoteIndex_;
        Matrix_<> quoteMap_;
    };
    class CalibrationReport_ final : public Storable_ {
        CalibrationStage_ stage_;
        CalibrationObservationAxes_ axes_;
        Vector_<CalibrationDiagnostic_> errors_;

    public:
        explicit CalibrationReport_(CalibrationStage_ stage, CalibrationObservationAxes_ axes = {}, Vector_<CalibrationDiagnostic_> errors = {});
        [[nodiscard]] const char* Schema() const { return "dal.calibration-report/1"; }
        [[nodiscard]] CalibrationStage_ Stage() const { return stage_; }
        [[nodiscard]] const CalibrationObservationAxes_& Axes() const { return axes_; }
        [[nodiscard]] const Vector_<CalibrationDiagnostic_>& Errors() const { return errors_; }
        [[nodiscard]] bool QuoteRiskAvailable() const { return false; }
        void Write(Archive::Store_& dst) const override;
    };
    struct CalibrationFailure_ {
        CalibrationDiagnostic_ diagnostic_;
        Handle_<CalibrationReport_> report_;
        Handle_<Storable_> candidate_;
    };
    class BASE_EXPORT CalibrationError_ : public Exception_ {
        std::shared_ptr<const CalibrationFailure_> failure_;

    public:
        CalibrationError_(const std::string& file, long line, const std::string& function, std::shared_ptr<const CalibrationFailure_> failure);
        [[nodiscard]] const CalibrationFailure_& Failure() const noexcept { return *failure_; }
        [[nodiscard]] const char* what() const noexcept override { return failure_->diagnostic_.message_.c_str(); }
    };
    Handle_<CalibrationMarketQuote_> NewCalibrationMarketQuote(const String_& id,
                                                               double value,
                                                               const CalibrationValueUnit_& unit,
                                                               const std::optional<CalibrationQuoteConvention_>& convention = std::nullopt);
    Handle_<CalibrationObservation_> NewCalibrationObservation(const String_& id,
                                                               const Handle_<CalibrationInstrument_>& instrument,
                                                               const String_& measureId,
                                                               const Handle_<CalibrationMarketQuote_>& quote,
                                                               double errorScale);
    CalibrationObservationAxes_ BuildCalibrationObservationAxes(const Vector_<Handle_<CalibrationObservation_>>& observations);
} // namespace Dal
