#include <dal/platform/platform.hpp>

#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/surface/marketivsurfaceinternal.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal {
    namespace {
        void ValidateName(const String_& name) {
            if (name.empty() || name.find('\0') != String_::npos || !CanonicalJson::ValidUtf8(std::string(name.data(), name.size())))
                MarketIvLocalVolDetail::Fail("InvalidMarketIvLocalVolRequest", "Input", "/name", "nonempty UTF-8 without NUL", "invalid name");
        }
    } // namespace
    MarketIvLocalVolModelData_::MarketIvLocalVolModelData_(const String_& name, std::shared_ptr<const MarketIvLocalVolDetail::Source_> source)
        : ModelData_("MarketIvLocalVolModelData_", name), source_(std::move(source)) {}

    Handle_<MarketIvLocalVolModelData_>
    BuildMarketIvLocalVolModelData(const String_& name, const MarketIvDupireResult_& grid, const MarketIvLocalVolModelSettings_& settings) {
        ValidateName(name);
        return Handle_<MarketIvLocalVolModelData_>(new MarketIvLocalVolModelData_(name, MarketIvLocalVolDetail::BuildSource(grid, settings)));
    }
    String_ MarketIvLocalVolModelData_::SourceIdentity() const { return source_->SourceIdentity(); }
    String_ MarketIvLocalVolModelData_::ModelIdentity() const { return source_->ModelIdentity(); }
    const MarketIvCalibrationSnapshot_& MarketIvLocalVolModelData_::Snapshot() const { return source_->Snapshot(); }
    const LocalVolSurfaceData_& MarketIvLocalVolModelData_::SurfaceData() const { return source_->Surface(); }
    MarketIvLocalVolModelSettings_ MarketIvLocalVolModelData_::Configuration() const { return source_->Configuration(); }
    Vector_<MarketIvLocalVolCarryTerm_> MarketIvLocalVolModelData_::CarryTerms() const { return source_->CarryTerms(); }
    Vector_<MarketIvLocalVolWarning_> MarketIvLocalVolModelData_::Warnings() const { return source_->Warnings(); }
    MarketIvLocalVolConstructionExecution_ MarketIvLocalVolModelData_::ConstructionExecution() const { return source_->ConstructionExecution(); }

    size_t MarketIvLocalVolModelData_::SharedNumericPayloadBytes(const MarketIvLocalVolModelData_& other) const {
        if (source_ == other.source_)
            return ConstructionExecution().numericPayloadBytes_;
        const auto& snapshot = source_->Snapshot();
        return &MarketIvData(snapshot) == &MarketIvData(other.source_->Snapshot()) ? snapshot.Execution().numericPayloadBytes_ : 0;
    }

    MarketIvLocalVolModelSummary_ MarketIvLocalVolModelData_::Summary() const {
        MarketIvLocalVolModelSummary_ result;
        const auto settings = Configuration();
        const auto& snapshot = source_->Snapshot();
        const auto& market = snapshot.Request().market_;
        const auto& fit = MarketIvData(snapshot);
        result.sourceIdentity_ = SourceIdentity();
        result.modelIdentity_ = ModelIdentity();
        result.evaluationDate_ = market.EvaluationDate();
        result.spot_ = market.Spot();
        result.carryMode_ = market.CarryMode();
        result.index_ = settings.index_;
        result.currency_ = settings.currency_;
        result.factor_ = settings.factor_;
        result.horizon_ = settings.horizon_;
        result.maxStep_ = settings.maxStep_;
        result.gridRows_ = source_->Surface().spots_.size();
        result.gridColumns_ = source_->Surface().times_.size();
        result.gridFirstTime_ = source_->Surface().times_.front();
        result.gridLastTime_ = source_->Surface().times_.back();
        result.fitStatus_ = fit.status_;
        result.underidentified_ = fit.underidentified_;
        result.nearTie_ = fit.nearTie_;
        return result;
    }
    std::unique_ptr<ModelData_> MarketIvLocalVolModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for ModelData");
        if (newName)
            ValidateName(*newName);
        return std::unique_ptr<ModelData_>(new MarketIvLocalVolModelData_(newName ? *newName : Name(), source_));
    }
    void MarketIvLocalVolModelData_::Write(Archive::Store_&) const {
        MarketIvLocalVolDetail::Fail("ArchiveUnsupported", "Archive", "/model", "in-process handle", Type());
    }
    void RequireMarketIvLocalVolPassive(const ModelData_& model, const String_& field) {
        if (dynamic_cast<const MarketIvLocalVolModelData_*>(&model))
            MarketIvLocalVolDetail::Fail("DerivativeUnavailable", "Prepare", field, "passive Price", model.Type());
    }

} // namespace Dal
