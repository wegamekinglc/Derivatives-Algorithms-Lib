#pragma once

#include <dal/model/marketivlocalvol.hpp>

namespace Dal::MarketIvLocalVolDetail {
    [[noreturn]] void Fail(const String_& code, const String_& stage, const String_& field, const String_& expected, const String_& actual);
    size_t CheckedAdd(size_t a, size_t b);
    size_t CheckedMultiply(size_t a, size_t b);
    void CheckLimit(size_t value, size_t limit, const String_& field, const String_& stage = "Build");
    double CheckedExp(double value, const String_& field, const String_& stage = "Build");

    struct CarryPoint_ {
        double forward_, discount_, logForwardOverSpot_, logDiscount_, rate_, yield_;
    };
    struct IntegratedCarry_ {
        double rate_, yield_;
    };

    class Carry_ {
        double logSpot_, rate_, yield_;
        bool flat_;
        Vector_<> times_, logForward_, logDiscount_;

    public:
        explicit Carry_(const MarketIvMarket_& market);
        [[nodiscard]] CarryPoint_ At(double time) const;
        [[nodiscard]] IntegratedCarry_ Integrated(double from, double to) const;
        [[nodiscard]] const Vector_<>& Times() const { return times_; }
        [[nodiscard]] const Vector_<>& LogDiscounts() const { return logDiscount_; }
        [[nodiscard]] size_t NumericBytes() const;
    };

    struct SourceData_;
    class Source_;
    std::shared_ptr<const Source_> BuildSource(const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings);
    void ValidateIdentityInput(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request);
    String_ SourceIdentity(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request, const LocalVolSurfaceData_& surface);
    String_ ModelIdentity(const String_& sourceIdentity, const MarketIvLocalVolModelSettings_& settings);

    class Source_ {
        std::shared_ptr<const SourceData_> data_;
        explicit Source_(std::shared_ptr<const SourceData_> data) : data_(std::move(data)) {}
        friend std::shared_ptr<const Source_> BuildSource(const MarketIvDupireResult_&, const MarketIvLocalVolModelSettings_&);

    public:
        [[nodiscard]] String_ SourceIdentity() const;
        [[nodiscard]] String_ ModelIdentity() const;
        [[nodiscard]] MarketIvLocalVolModelSettings_ Configuration() const;
        [[nodiscard]] Vector_<MarketIvLocalVolCarryTerm_> CarryTerms() const;
        [[nodiscard]] Vector_<MarketIvLocalVolWarning_> Warnings() const;
        [[nodiscard]] MarketIvLocalVolConstructionExecution_ ConstructionExecution() const;
        [[nodiscard]] LocalVolSurfaceData_ SurfaceData() const;
        [[nodiscard]] const Carry_& Carry() const;
        [[nodiscard]] const MarketIvCalibrationSnapshot_& Snapshot() const;
        [[nodiscard]] const LocalVolSurfaceData_& Surface() const;
    };
} // namespace Dal::MarketIvLocalVolDetail
