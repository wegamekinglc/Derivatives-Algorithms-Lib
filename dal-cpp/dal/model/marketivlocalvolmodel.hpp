#pragma once

#include <dal/model/marketivlocalvol.hpp>

namespace Dal::AAD {
    class MarketIvLocalVolModel_ final : public Model_<double> {
        struct State_;
        std::shared_ptr<const MarketIvLocalVolDetail::Source_> source_;
        std::unique_ptr<State_> state_;
        Vector_<String_> assets_, labels_;
        Vector_<double*> parameters_;
        String_ rsg_;
        size_t workers_ = 1;
        size_t pathCount_ = 0;

    public:
        explicit MarketIvLocalVolModel_(const MarketIvLocalVolModelData_& data);
        ~MarketIvLocalVolModel_() override;
        [[nodiscard]] bool SupportsIndex(const Index_& index) const override;
        [[nodiscard]] size_t MaxObservedIndices() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] bool SupportsBrownianBridge() const override { return true; }
        [[nodiscard]] bool NumeraireIsDeterministic() const override { return true; }
        [[nodiscard]] bool SupportsDiscountFactors() const override { return true; }
        [[nodiscard]] std::optional<Date_> EvaluationDate() const override;
        [[nodiscard]] const Vector_<String_>& AssetNames() const override { return assets_; }
        [[nodiscard]] const Vector_<double*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
        void Allocate(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override;
        void Init(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override;
        [[nodiscard]] size_t SimDim() const override;
        void GeneratePath(const Vector_<>& normals, Scenario_<>* path) const override;
        [[nodiscard]] std::unique_ptr<Model_<double>> Clone() const override;
        void ValidateRequest(bool aad, bool exercise, const std::optional<Date_>& date, const String_& rsg, size_t workers = 1);
        void PrepareWorkers(size_t workers);
        void ValidatePaths(size_t paths, const String_& rsg);
        [[nodiscard]] MarketIvLocalVolExecution_ Execution() const;
    };
} // namespace Dal::AAD
