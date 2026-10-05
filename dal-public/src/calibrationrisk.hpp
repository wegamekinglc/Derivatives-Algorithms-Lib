//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/model/dupirerisk.hpp>

namespace Dal {
    class CalibrationPullback_ {
    public:
        using Source_ = std::variant<DupireCalibrationSnapshot_, RateQuoteRiskProvenance_>;

    private:
        std::shared_ptr<const Source_> source_;

        explicit CalibrationPullback_(Source_ source);
        friend CalibrationPullback_ NewCalibrationPullback(const DupireCalibrationSnapshot_&);
        friend CalibrationPullback_ NewCalibrationPullback(const RateQuoteRiskProvenance_&);

    public:
        [[nodiscard]] const Source_& Source() const { return *source_; }
        [[nodiscard]] const String_& Domain() const;
        [[nodiscard]] int ParameterRows() const;
        [[nodiscard]] int ParameterCols() const;
        [[nodiscard]] int QuoteRows() const;
        [[nodiscard]] int QuoteCols() const;
        [[nodiscard]] const String_& Method() const;
        [[nodiscard]] const String_& Unit() const;
        [[nodiscard]] const String_& Boundary() const;
        [[nodiscard]] bool Matches(const CalibrationPullback_& other) const;
    };

    CalibrationPullback_ NewCalibrationPullback(const DupireCalibrationSnapshot_& calibration);
    CalibrationPullback_ NewCalibrationPullback(const RateQuoteRiskProvenance_& calibration);

    struct CalibrationParameterSeedTag_ {};
    struct CalibrationDirectQuoteSeedTag_ {};

    template <class T_> class CalibrationAdjoints_ {
        static_assert(std::is_same_v<T_, CalibrationParameterSeedTag_> || std::is_same_v<T_, CalibrationDirectQuoteSeedTag_>);
        CalibrationPullback_ calibration_;
        Matrix_<> adjoints_;

    public:
        CalibrationAdjoints_(const CalibrationPullback_& calibration, const Matrix_<>& adjoints);
        [[nodiscard]] const CalibrationPullback_& Calibration() const { return calibration_; }
        [[nodiscard]] const Matrix_<>& Adjoints() const { return adjoints_; }
    };

    using CalibrationParameterAdjoints_ = CalibrationAdjoints_<CalibrationParameterSeedTag_>;
    using CalibrationDirectQuoteAdjoints_ = CalibrationAdjoints_<CalibrationDirectQuoteSeedTag_>;

    CalibrationParameterAdjoints_ NewCalibrationParameterAdjoints(const CalibrationPullback_& calibration, const Matrix_<>& adjoints);
    CalibrationDirectQuoteAdjoints_ NewCalibrationDirectQuoteAdjoints(const CalibrationPullback_& calibration, const Matrix_<>& adjoints);

    class CalibrationQuoteRisk_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;

        CalibrationQuoteRisk_(const CalibrationPullback_&, Matrix_<>&&, Matrix_<>&&, Matrix_<>&&);
        friend CalibrationQuoteRisk_
        PullbackCalibration(const CalibrationPullback_&, const CalibrationParameterAdjoints_&, const std::optional<CalibrationDirectQuoteAdjoints_>&);

    public:
        [[nodiscard]] const CalibrationPullback_& Calibration() const;
        [[nodiscard]] const Matrix_<>& CalibrationAdjoints() const;
        [[nodiscard]] const Matrix_<>& DirectAdjoints() const;
        [[nodiscard]] const Matrix_<>& TotalAdjoints() const;
        [[nodiscard]] const String_& Method() const;
        [[nodiscard]] const String_& Unit() const;
        [[nodiscard]] const String_& Boundary() const;
    };

    CalibrationQuoteRisk_ PullbackCalibration(const CalibrationPullback_& calibration,
                                              const CalibrationParameterAdjoints_& parameters,
                                              const std::optional<CalibrationDirectQuoteAdjoints_>& direct = {});
} // namespace Dal
