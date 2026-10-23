#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/script/xccymcbasket_internal.hpp>
#include <set>

namespace Dal {
    namespace {
        using namespace CalibrationInternal;
        using namespace XccyMcBasketInternal;
        String_ Field(size_t i, const String_& field) { return "/members/" + OrdinalText(i) + "/" + field; }
        void Sampling(const XccyMcSettings_& settings) {
            CalibrationMcMapping_ preliminary;
            preliminary.seed_ = settings.seed_;
            preliminary.identity_ = "preflight";
            static_cast<void>(PrepareXccyMcReservation(settings, preliminary));
        }
        void Inputs(const Vector_<XccyMcBasketMember_>& members,
                    const CalibrationPreparationContext_& context,
                    const Handle_<XccyGaussianModelData_>& model,
                    const XccyMcSettings_& settings) {
            Check(static_cast<bool>(model), "InvalidCalibrationPlan", "/model", "explicit immutable XCCY model");
            Check(context.xccy_ && context.xccy_->fixings_, "InvalidCalibrationPlan", "/context/fixings", "explicit nonnull fixing snapshot");
            Check(!members.empty(), "InvalidCalibrationPlan", "/members", "nonempty ordered basket");
            Sampling(settings);
            Capacity(members.size(), settings.parameterLabels_.size(), settings);
            std::set<std::string> ids;
            for (size_t i = 0; i < members.size(); ++i) {
                Check(!members[i].memberId_.empty() && ids.insert(Bytes(members[i].memberId_)).second, "InvalidCalibrationPlan",
                      Field(i, "member_id"), "nonempty byte-exact unique member ID", members[i].memberId_);
                Check(static_cast<bool>(members[i].instrument_), "InvalidCalibrationPlan", Field(i, "instrument"), "nonnull instrument");
            }
            ModelCapacity(*model);
            XccyMcInternal::RequireDerivatives(*model, settings);
        }
        Vector_<String_> Units(const XccyGaussianModelData_& model, const XccyMcSettings_& settings) {
            Vector_<String_> units;
            const auto foreignSigma =
                "sigma:" + Bytes(model.market_->pair_.foreign_.String()) + "/" + Bytes(model.market_->pair_.domestic_.String()) + ":";
            for (const auto& label : settings.parameterLabels_) {
                const auto s = Bytes(label);
                if (s.rfind("fxspot:", 0) == 0)
                    units.push_back(String_(model.market_->pair_.domestic_.String()) + "/" + model.market_->pair_.foreign_.String());
                else if (s.rfind(foreignSigma, 0) == 0)
                    units.push_back("year^-1/2");
                else if (s.rfind("sigma:", 0) == 0)
                    units.push_back("year^-3/2");
                else
                    units.push_back(s.rfind("forward:", 0) == 0 ? "year^-1" : "Dimensionless");
            }
            return units;
        }
        CalibrationReportSnapshot_ ReportData(const XccyMcBasketPartialSnapshot_& progress) {
            CalibrationReportSnapshot_ data;
            data.summary_.domain_ = "XCCYGaussianMonteCarloBasket";
            data.summary_.status_ = progress.basketComplete_ ? "Estimate" : "Failed";
            data.summary_.contextIdentity_ = progress.header_.planIdentity_;
            data.execution_.modelIdentity_ = progress.header_.attemptedModelIdentity_.value_or("");
            for (size_t i = 0; i < progress.members_.size(); ++i) {
                const auto& member = progress.members_[i];
                if (!member.completedEstimate_)
                    continue;
                const auto& allocation = progress.allocation_.members_[i];
                CalibrationReportObservation_ observation;
                observation.instrument_ = allocation.instrument_;
                observation.representation_ = "Script";
                observation.measureId_ = allocation.measureId_;
                observation.unit_ = allocation.measureUnit_;
                observation.backend_ = "ScriptMC";
                observation.derivativeMethod_ = progress.header_.derivativeMethod_;
                observation.planIdentity_ = member.completedEstimate_->estimate_.planIdentity_;
                observation.modelValue_ = member.completedEstimate_->estimate_.mean_;
                observation.standardError_ = member.completedEstimate_->estimate_.standardError_;
                data.observations_.push_back(std::move(observation));
                CalibrationReportPlan_ plan;
                plan.measureId_ = allocation.measureId_;
                plan.ruleId_ = progress.header_.ruleId_;
                plan.backend_ = "ScriptMC";
                plan.derivativeMethod_ = progress.header_.derivativeMethod_;
                plan.sourceIdentity_ = allocation.sourceIdentity_;
                plan.definitionIdentity_ = allocation.definitionIdentity_;
                plan.bindingIdentity_ = allocation.bindingIdentity_;
                plan.compilerIdentity_ = allocation.compilerIdentity_;
                plan.contextIdentity_ = member.completedEstimate_->estimate_.contextIdentity_;
                plan.planIdentity_ = member.completedEstimate_->estimate_.planIdentity_;
                data.plans_.push_back(std::move(plan));
            }
            return data;
        }
        [[noreturn]] void Own(const CalibrationError_& error,
                              CalibrationStage_ stage,
                              std::optional<XccyMcBasketPartialSnapshot_> progress = std::nullopt,
                              const Handle_<XccyGaussianModelData_>& validated = {},
                              const XccyMcBasketMember_* member = nullptr,
                              std::optional<size_t> index = std::nullopt) {
            auto d = error.Failure().diagnostic_;
            d.stage_ = stage;
            if (member) {
                d.instrumentId_ = member->instrument_ ? member->instrument_->Id() : "";
                d.measureId_ = member->measureId_;
                d.observationId_ = member->memberId_;
                d.sourceRow_ = static_cast<int>(index.value_or(0));
                d.message_ += "; member=" + member->memberId_ + "; index=" + OrdinalText(index.value_or(0));
            }
            if (progress && progress->failedMember_)
                progress->members_[*progress->failedMember_].failure_ = d;
            const auto data = progress ? ReportData(*progress) : CalibrationReportSnapshot_{};
            const Handle_<CalibrationReport_> report(new CalibrationReport_(stage, {}, {d}, data));
            const auto partial = progress ? Producer_::Partial(std::move(*progress), report) : Handle_<XccyMcBasketPartial_>{};
            throw XccyMcBasketError_(__FILE__, __LINE__, __func__,
                                     std::make_shared<const CalibrationFailure_>(CalibrationFailure_{d, report, Handle_<Storable_>(validated)}),
                                     partial);
        }
        CalibrationMcBatchValue_ EmptyBatch(size_t n) {
            CalibrationMcBatchValue_ batch;
            batch.coefficients_ = CalibrationAffineCoefficients_{0., 0., Vector_<>(n, 0.), Vector_<>(n, 0.)};
            return batch;
        }
        void Call(const Hook_& hook, const String_& stage, size_t member, size_t batch = 0, size_t path = 0) {
            if (hook)
                hook(stage, member, batch, path);
        }
        CalibrationMcResult_ EstimateHeader(const XccyMcInternal::Member_& member, const XccyMcBasketExecutionHeader_& header) {
            CalibrationMcResult_ estimate;
            estimate.instrumentIdentity_ = member.admitted_.Instrument()->InstrumentIdentity();
            estimate.contextIdentity_ =
                Fingerprint(Obj({{"source_context", Str(member.admitted_.ContextIdentity())}, {"model", Str(*header.attemptedModelIdentity_)}}));
            estimate.mappingIdentity_ = member.mapping_.identity_;
            estimate.planIdentity_ = Fingerprint(Obj({{"basket", Str(header.planIdentity_)}, {"evaluation", Str(estimate.contextIdentity_)}}));
            estimate.measureId_ = member.admitted_.Measure().id_;
            estimate.parameterLabels_ = header.settings_.parameterLabels_;
            estimate.validation_ = header.settings_.phase_ == XccyMonteCarloPhase_::Value_::Validation;
            estimate.heldOut_ = header.settings_.phase_ == XccyMonteCarloPhase_::Value_::HeldOut;
            return estimate;
        }
        void Consumed(size_t path, size_t cursor, XccyMcBasketPartialSnapshot_* progress) {
            auto& member = progress->members_[*progress->failedMember_];
            auto& range = member.actualRanges_.back();
            const size_t count = cursor - range.coordinateEnd_;
            range.coordinateEnd_ = cursor;
            range.pathEnd_ = Add(path, 1);
            member.consumedCoordinates_ = Add(member.consumedCoordinates_, count);
            progress->consumedCoordinates_ = Add(progress->consumedCoordinates_, count);
            progress->observedCursor_ = cursor;
        }
    } // namespace

    XccyMcBasketPartial_::XccyMcBasketPartial_(XccyMcBasketPartialSnapshot_ snapshot, Handle_<CalibrationReport_> report)
        : snapshot_(std::move(snapshot)), report_(std::move(report)) {}
    XccyMcBasketError_::XccyMcBasketError_(const std::string& file,
                                           long line,
                                           const std::string& function,
                                           std::shared_ptr<const CalibrationFailure_> failure,
                                           Handle_<XccyMcBasketPartial_> partial)
        : CalibrationError_(file, line, function, std::move(failure)), partial_(std::move(partial)) {}
    XccyMcBasketResult_::XccyMcBasketResult_(XccyMcBasketSnapshot_ snapshot, Handle_<CalibrationReport_> report)
        : snapshot_(std::make_shared<const XccyMcBasketSnapshot_>(std::move(snapshot))), report_(std::move(report)) {}
    Handle_<XccyMcBasketPartial_> XccyMcBasketInternal::Producer_::Partial(XccyMcBasketPartialSnapshot_ snapshot,
                                                                           Handle_<CalibrationReport_> report) {
        return Handle_<XccyMcBasketPartial_>(new XccyMcBasketPartial_(std::move(snapshot), std::move(report)));
    }
    XccyMcBasketResult_ XccyMcBasketInternal::Producer_::Result(XccyMcBasketSnapshot_ snapshot, Handle_<CalibrationReport_> report) {
        return XccyMcBasketResult_(std::move(snapshot), std::move(report));
    }
    struct PreparedXccyMcBasket_::Impl_ {
        Vector_<XccyMcInternal::Member_> members_;
        XccyMcBasketAllocation_ allocation_;
        XccyMcBasketExecutionHeader_ header_;
        Handle_<XccyGaussianModelData_> model_;

        Impl_(Vector_<XccyMcInternal::Member_> members,
              XccyMcBasketAllocation_ allocation,
              const Handle_<XccyGaussianModelData_>& model,
              const XccyMcSettings_& settings)
            : members_(std::move(members)), allocation_(std::move(allocation)), model_(model) {
            header_.preparedModelIdentity_ = model->Identity();
            header_.settings_ = settings;
            header_.derivativeMethod_ = settings.aad_ ? "AAD" : "None";
            header_.parameterUnits_ = Units(*model, settings);
            header_.planIdentity_ = Fingerprint(Obj({{"allocation", Str(allocation_.identity_)},
                                                     {"execution", Str(allocation_.executionIdentity_)},
                                                     {"model", Str(model->Identity())},
                                                     {"aad", Bool(settings.aad_)},
                                                     {"compiled", Bool(settings.compiled_)},
                                                     {"retain", Bool(settings.retainPaths_)},
                                                     {"labels", Array(settings.parameterLabels_, [](const auto& x) { return Str(x); })}}));
        }
        XccyMcBasketPartialSnapshot_ Progress() const {
            XccyMcBasketPartialSnapshot_ progress;
            progress.header_ = header_;
            progress.allocation_ = allocation_;
            for (const auto& m : allocation_.members_) {
                XccyMcBasketMemberProgress_ member;
                member.memberId_ = m.memberId_;
                progress.members_.push_back(std::move(member));
            }
            return progress;
        }
        void Preflight(const Handle_<XccyGaussianModelData_>& data, const Hook_& hook, XccyMcBasketPartialSnapshot_* p) const {
            p->header_.attemptedModelIdentity_ = data->Identity();
            RequireAllocation(allocation_);
            model_->RequireSameGeometry(*data);
            ModelCapacity(*data);
            p->header_.nativeMapIdentity_ = NativeMapIdentity(*data);
            for (size_t i = 0; i < members_.size(); ++i) {
                p->failedMember_ = i;
                Call(hook, "Preflight", i);
                members_[i].Preflight(data);
            }
            p->failedMember_.reset();
            p->header_.candidateGeometryValidated_ = true;
        }
        void RunBatch(size_t i,
                      size_t b,
                      XccyMcInternal::CoefficientExecution_* execution,
                      const Hook_& hook,
                      XccyMcResult_* work,
                      XccyMcBasketPartialSnapshot_* p) const {
            const auto& range = allocation_.members_[i].ranges_[b];
            auto& member = p->members_[i];
            p->failedBatch_ = b;
            p->failedPath_.reset();
            ScalarNormals_ adapter(allocation_.seed_, members_[i].mapping_.dimension_, range);
            member.actualRanges_.push_back({range.firstPath_, range.firstPath_, range.firstCoordinate_, range.firstCoordinate_});
            auto batch = EmptyBatch(header_.settings_.parameterLabels_.size());
            Vector_<> first;
            for (size_t path = range.firstPath_; path < range.pathEnd_; ++path) {
                p->failedPath_ = path;
                const auto normals = adapter.Next();
                Consumed(path, adapter.Cursor(), p);
                if (path == range.firstPath_)
                    first = normals;
                Call(hook, "AfterDraw", i, b, path);
                const auto c = execution->Next(normals, &work->estimate_);
                XccyMcInternal::Accumulate(c, range.pathEnd_ - range.firstPath_, &batch);
                ++member.completedPaths_;
                if (header_.settings_.retainPaths_)
                    member.retainedPaths_.push_back({path, adapter.Cursor() - normals.size(), c});
            }
            adapter.Finish();
            const auto row = Row(*batch.coefficients_);
            Check(row.size() == Multiply(2, Add(1, header_.settings_.parameterLabels_.size())), "CalibrationContextMismatch", "/derivatives",
                  "complete ordered native coefficient columns");
            XccyMcInternal::FinishBatch(members_[i].coupon_, &batch);
            member.batches_.push_back({b, range.pathEnd_ - range.firstPath_, range, std::move(first), row});
            ++member.completedBatches_;
            work->estimate_.batches_.push_back(std::move(batch));
            work->estimate_.ranges_.push_back({range.firstPath_, range.pathEnd_ - range.firstPath_});
            work->estimate_.pathsPerView_ += range.pathEnd_ - range.firstPath_;
        }
        void RunMember(size_t i, const Handle_<XccyGaussianModelData_>& data, const Hook_& hook, XccyMcBasketPartialSnapshot_* p) const {
            p->failedMember_ = i;
            auto& progress = p->members_[i];
            progress.state_ = "InProgress";
            XccyMcResult_ work;
            work.estimate_ = EstimateHeader(members_[i], p->header_);
            auto execution = members_[i].Execution(data);
            for (size_t b = 0; b < allocation_.members_[i].ranges_.size(); ++b)
                RunBatch(i, b, execution.get(), hook, &work, p);
            execution.reset();
            p->failedBatch_.reset();
            p->failedPath_.reset();
            progress.state_ = "CoefficientsComplete";
            Call(hook, "Summary", i);
            work.estimate_.engineRuns_ = 2 * work.estimate_.batches_.size();
            XccyMcInternal::Summarize(&work, members_[i].admitted_, members_[i].coupon_);
            progress.completedEstimate_ =
                XccyMcBasketMemberEstimate_{std::move(work.estimate_), std::move(work.coefficients_), std::move(work.coefficientCovariance_)};
            progress.state_ = "Complete";
            ++p->completedMembers_;
        }
        XccyMcBasketResult_ Run(const Handle_<XccyGaussianModelData_>& candidate, const Hook_& hook, bool reverse) const {
            auto p = Progress();
            Handle_<XccyGaussianModelData_> validated;
            try {
                const auto data = candidate ? candidate : model_;
                Preflight(data, hook, &p);
                validated = data;
                for (size_t k = 0; k < members_.size(); ++k)
                    RunMember(reverse ? members_.size() - 1 - k : k, data, hook, &p);
                p.failedMember_.reset();
                p.observedCursor_.reset();
                Check(p.consumedCoordinates_ == allocation_.expectedConsumption_, "CalibrationContextMismatch", "/sampling/ranges",
                      "exact basket scalar consumption");
                XccyMcBasketSnapshot_ result;
                result.header_ = p.header_;
                result.allocation_ = p.allocation_;
                result.members_ = p.members_;
                result.consumedCoordinates_ = p.consumedCoordinates_;
                result.model_ = data;
                JointStatistics(members_, &result);
                p.basketComplete_ = true;
                const Handle_<CalibrationReport_> report(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, {}, {}, ReportData(p)));
                return Producer_::Result(std::move(result), report);
            } catch (const CalibrationError_& error) {
                const auto index = p.failedMember_;
                std::optional<XccyMcBasketMember_> member;
                if (index)
                    member = XccyMcBasketMember_{allocation_.members_[*index].memberId_, members_[*index].admitted_.Instrument(),
                                                 members_[*index].admitted_.Measure().id_};
                Own(error, CalibrationStage_::Value_::Evaluate, std::move(p), validated, member ? &*member : nullptr, index);
            }
        }
    };
    PreparedXccyMcBasket_::PreparedXccyMcBasket_(const Vector_<XccyMcBasketMember_>& input,
                                                 const CalibrationPreparationContext_& context,
                                                 const Handle_<XccyGaussianModelData_>& model,
                                                 const XccyMcSettings_& settings) {
        std::optional<size_t> active;
        try {
            Inputs(input, context, model, settings);
            Vector_<XccyMcInternal::Member_> members;
            Vector_<XccyMcBasketMemberAllocation_> allocations;
            for (size_t i = 0; i < input.size(); ++i) {
                active = i;
                members.push_back(XccyMcInternal::Member_(input[i].instrument_, input[i].measureId_, context, model, settings));
                members.back().Preflight(model);
                allocations.push_back(MemberAllocation(input[i].memberId_, members.back()));
            }
            active.reset();
            auto allocation = Allocate(std::move(allocations), settings);
            RequireAllocation(allocation);
            impl_ = std::make_shared<const Impl_>(std::move(members), std::move(allocation), model, settings);
        } catch (const CalibrationError_& error) {
            Own(error, CalibrationStage_::Value_::Prepare, std::nullopt, {}, active ? &input[*active] : nullptr, active);
        }
    }
    const XccyMcBasketAllocation_& PreparedXccyMcBasket_::Allocation() const { return impl_->allocation_; }
    const String_& PreparedXccyMcBasket_::PlanIdentity() const { return impl_->header_.planIdentity_; }
    XccyMcBasketResult_ PreparedXccyMcBasket_::Evaluate(const Handle_<XccyGaussianModelData_>& candidate) const {
        return impl_->Run(candidate, {}, false);
    }
    XccyMcBasketResult_ XccyMcBasketInternal::TestAccess_::Evaluate(const PreparedXccyMcBasket_& prepared,
                                                                    const Handle_<XccyGaussianModelData_>& candidate,
                                                                    const Hook_& hook,
                                                                    bool reverse) {
        return prepared.impl_->Run(candidate, hook, reverse);
    }
} // namespace Dal
