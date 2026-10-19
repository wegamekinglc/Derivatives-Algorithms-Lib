#include <dal/platform/platform.hpp>

#include <dal/model/surface/marketivsurfaceinternal.hpp>

namespace Dal {
    void ValidateMarketIvDupireRequest(const MarketIvDupireRequest_& request) {
        using namespace MarketIvDetail;
        ValidateMarketIvEvaluationSettings(request.settings_.evaluation_);
        auto axis = [](const Vector_<>& values, const char* name) {
            REQUIRE(!values.empty() && values.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                    String_("InvalidMarketIvRequest: ") + name + "; nonempty representable axis required");
            for (size_t i = 0; i < values.size(); ++i)
                REQUIRE(std::isfinite(values[i]) && values[i] > 0. && (i == 0 || values[i] > values[i - 1]),
                        String_("InvalidMarketIvRequest: ") + name +
                            "; expected positive strictly increasing axis; row=" + String_(std::to_string(i)));
        };
        axis(request.spots_, "spots");
        axis(request.times_, "times");
        const size_t g = CheckedMultiply(request.spots_.size(), request.times_.size());
        REQUIRE(g <= request.settings_.maxGridPoints_, "MarketIvBudgetExceeded: max_grid_points");
    }
    MarketIvDupireResult_ CalibrateMarketIvDupire(const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request) {
        using namespace MarketIvDetail;
        ValidateMarketIvDupireRequest(request);
        const size_t g = CheckedMultiply(request.spots_.size(), request.times_.size()), n = CheckedAdd(request.spots_.size(), request.times_.size());
        MarketIvExecution_ execution;
        execution.plannedNumericPayloadBytes_ = CheckedAdd(snapshot.Execution().plannedNumericPayloadBytes_,
                                                           CheckedAdd(8192, CheckedAdd(CheckedMultiply(512, g), CheckedMultiply(16, n))));
        execution.plannedWorkspaceBytes_ = CheckedAdd(16384, CheckedAdd(CheckedMultiply(2048, snapshot.Times().size() - 1), CheckedMultiply(64, n)));
        CheckBudget(execution.plannedNumericPayloadBytes_, execution.plannedWorkspaceBytes_, request.settings_.numericPayloadBudgetBytes_,
                    request.settings_.workspaceBudgetBytes_);
        auto data = std::make_shared<DupireData_>(snapshot, request);
        data->execution_ = execution;
        data->nodes_.reserve(g);
        auto surface = std::make_unique<LocalVolSurfaceData_>("market-iv-dupire", Vector_<>{request.spots_.front()},
                                                              Vector_<>{request.times_.front()}, Matrix_<>(1, 1, 0.));
        surface->spots_ = request.spots_;
        surface->times_ = request.times_;
        surface->vols_ = Matrix_<>(static_cast<int>(request.spots_.size()), static_cast<int>(request.times_.size()), 0.);
        for (size_t i = 0; i < request.spots_.size(); ++i)
            for (size_t j = 0; j < request.times_.size(); ++j) {
                auto point = EvaluateMarketIvSurface(snapshot, {request.spots_[i], request.times_[j]}, request.settings_.evaluation_);
                ++data->execution_.outputEvaluations_;
                if (point.DupireStatus() == "SUCCESS")
                    surface->vols_(static_cast<int>(i), static_cast<int>(j)) = *point.LocalVol();
                else if (data->failure_.empty())
                    data->failure_ = "Dupire node [" + String_(std::to_string(i)) + "," + String_(std::to_string(j)) + "]: " + point.DupireStatus();
                data->nodes_.push_back(std::move(point));
            }
        data->execution_.numericPayloadBytes_ = snapshot.Execution().numericPayloadBytes_ + sizeof(*data) + Bytes(data->nodes_) +
                                                Bytes(data->request_.spots_) + Bytes(data->request_.times_) + sizeof(*surface) +
                                                Bytes(surface->spots_) + Bytes(surface->times_) + sizeof(double) * surface->vols_.Capacity();
        data->execution_.peakWorkspaceBytes_ = EvaluationWorkspaceBytes() + sizeof(MarketIvDupireResult_);
        CheckBudget(data->execution_.numericPayloadBytes_, data->execution_.peakWorkspaceBytes_, execution.plannedNumericPayloadBytes_,
                    execution.plannedWorkspaceBytes_);
        if (data->failure_.empty())
            data->surface_ = Handle_<LocalVolSurfaceData_>(surface.release());
        return MarketIvDupireResult_(data);
    }
    bool MarketIvDupireResult_::IsSuccess() const { return data_->failure_.empty(); }
    const String_& MarketIvDupireResult_::Failure() const { return data_->failure_; }
    const MarketIvCalibrationSnapshot_& MarketIvDupireResult_::Snapshot() const { return data_->snapshot_; }
    const MarketIvDupireRequest_& MarketIvDupireResult_::Request() const { return data_->request_; }
    const Vector_<MarketIvPointResult_>& MarketIvDupireResult_::Nodes() const { return data_->nodes_; }
    const LocalVolSurfaceData_& MarketIvDupireResult_::SurfaceData() const {
        REQUIRE(IsSuccess(), "MarketIvDupireUnavailable: " + data_->failure_);
        return *data_->surface_;
    }
    const MarketIvExecution_& MarketIvDupireResult_::Execution() const { return data_->execution_; }
} // namespace Dal
