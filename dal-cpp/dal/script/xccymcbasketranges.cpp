#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/xccygaussianmoments.hpp>
#include <dal/script/xccymcbasket_internal.hpp>

namespace Dal::XccyMcBasketInternal {
    namespace {
        using namespace CalibrationInternal;
        Json_ Range(const XccyMcRange_& r) {
            return Obj({{"first_path", Str(OrdinalText(r.firstPath_))},
                        {"path_end", Str(OrdinalText(r.pathEnd_))},
                        {"first_coordinate", Str(OrdinalText(r.firstCoordinate_))},
                        {"coordinate_end", Str(OrdinalText(r.coordinateEnd_))}});
        }
        Json_ Geometry(const XccyMcBasketMemberAllocation_& m) {
            return Obj({{"id", Str(m.memberId_)},
                        {"instrument", Str(m.instrumentIdentity_)},
                        {"source", Str(m.sourceIdentity_)},
                        {"definition", Str(m.definitionIdentity_)},
                        {"binding", Str(m.bindingIdentity_)},
                        {"measure", Str(m.measureId_)},
                        {"preparation", Str(m.preparationIdentity_)},
                        {"context", Str(m.contextIdentity_)},
                        {"compiler", Str(m.compilerIdentity_)},
                        {"low", Str(m.lowBindingIdentity_)},
                        {"high", Str(m.highBindingIdentity_)},
                        {"native_geometry", Str(m.nativeMapIdentity_)},
                        {"mapping", Str(m.mapping_.identity_)},
                        {"dimension", Str(OrdinalText(m.mapping_.dimension_))},
                        {"grid", Array(m.grid_, [](const auto& d) { return Str(Date::ToString(d)); })},
                        {"first", Str(OrdinalText(m.firstCoordinate_))},
                        {"end", Str(OrdinalText(m.coordinateEnd_))},
                        {"phases", Array(m.phaseRanges_, Range)}});
        }
        String_ AllocationIdentity(const XccyMcBasketAllocation_& a) {
            return Fingerprint(Obj({{"layout", Str(a.version_)},
                                    {"stream", Str(a.streamVersion_)},
                                    {"normal", Str(a.normalPrecision_)},
                                    {"seek", Str(a.seekConvention_)},
                                    {"seed", Num(a.seed_)},
                                    {"capacity", Str(OrdinalText(a.reservedPaths_))},
                                    {"members", Array(a.members_, Geometry)}}));
        }
        String_ ExecutionIdentity(const XccyMcBasketAllocation_& a) {
            return Fingerprint(Obj({{"allocation", Str(a.identity_)},
                                    {"phase", Str(a.phase_.String())},
                                    {"batches", Num(a.batches_)},
                                    {"paths_per_batch", Num(a.pathsPerBatch_)},
                                    {"ranges", Array(a.members_, [](const auto& m) { return Array(m.ranges_, Range); })}}));
        }
        XccyMcRange_ Shift(XccyMcRange_ r, size_t offset) {
            r.firstCoordinate_ = Add(offset, r.firstCoordinate_);
            r.coordinateEnd_ = Add(offset, r.coordinateEnd_);
            return r;
        }
        void AllocateMember(XccyMcBasketMemberAllocation_* m, const XccyMcSettings_& settings, size_t offset) {
            const size_t d = m->mapping_.dimension_;
            Check(d % 5 == 0 && m->mapping_.normalCoordinates_.size() == d, "CalibrationContextMismatch", "/sampling/mapping",
                  "complete five-slot member mapping");
            m->firstCoordinate_ = offset;
            m->coordinateEnd_ = Add(offset, Multiply(2359296, d));
            m->deterministic_ = d == 0;
            m->phaseRanges_.clear();
            m->ranges_.clear();
            for (const auto phase : XccyMonteCarloPhaseListAll()) {
                auto s = settings;
                s.phase_ = phase;
                s.batches_ = s.pathsPerBatch_ = 2;
                const auto r = PrepareXccyMcReservation(s, m->mapping_);
                m->phaseRanges_.push_back(
                    Shift({r.reservedFirst_, r.reservedEnd_, Multiply(r.reservedFirst_, d), Multiply(r.reservedEnd_, d)}, offset));
            }
            for (const auto& range : PrepareXccyMcReservation(settings, m->mapping_).ranges_)
                m->ranges_.push_back(Shift(range, offset));
        }
        bool SameRanges(const Vector_<XccyMcRange_>& a, const Vector_<XccyMcRange_>& b) { return Jcs(Array(a, Range)) == Jcs(Array(b, Range)); }
        bool HeaderMatches(const XccyMcBasketAllocation_& a) {
            return Bytes(a.version_) == "dal.xccy-gaussian-basket-coordinate-ranges/1" && a.reservedPaths_ == 2359296 &&
                   Bytes(a.normalPrecision_) == "Precise" && Bytes(a.seekConvention_) == "absolute-scalar/1";
        }
        void MatrixCapacity(size_t rows, size_t cols) {
            Check(rows <= INT_MAX && cols <= INT_MAX, "InvalidCalibrationPlan", "/sampling", "matrix axes within int capacity");
            Storage(Multiply(rows, cols), sizeof(double));
        }
    } // namespace
    void Capacity(size_t members, size_t labels, const XccyMcSettings_& s) {
        const size_t width = Multiply(2, Add(1, labels)), axes = Multiply(members, width);
        MatrixCapacity(axes, axes);
        MatrixCapacity(static_cast<size_t>(s.batches_), axes);
        Storage(members, sizeof(XccyMcBasketMemberAllocation_));
        Storage(Multiply(members, static_cast<size_t>(s.batches_)), sizeof(XccyMcBasketCompletedBatch_));
        if (s.retainPaths_) {
            const size_t paths = Multiply(static_cast<size_t>(s.batches_), static_cast<size_t>(s.pathsPerBatch_));
            Storage(Multiply(paths, members), sizeof(XccyMcBasketRetainedPath_));
            Storage(Multiply(Multiply(paths, members), width), sizeof(double));
        }
    }
    void ModelCapacity(const XccyGaussianModelData_& model) {
        const auto& market = *model.market_;
        auto curves = market.projections_;
        curves.Append(Vector_<Handle_<XccyGaussianCurveData_>>{market.domestic_, market.foreign_, market.basis_});
        size_t rows = Add(1, Multiply(3, model.dynamics_->bucketEndDates_.size())), cols = rows;
        for (const auto& curve : curves) {
            rows = Add(rows, curve->nodeDates_.size() - 1);
            cols = Add(cols, curve->nativeLabels_.size());
        }
        MatrixCapacity(rows, cols);
        Storage(rows, sizeof(String_));
        Storage(cols, sizeof(String_));
    }
    String_ NativeMapIdentity(const XccyGaussianModelData_& model) {
        Vector_<String_> labels;
        const auto map = XccyGaussianMomentNativeMap(model, &labels);
        Json_ rows = Json_::Array();
        for (int i = 0; i < map.Rows(); ++i) {
            Json_ row = Json_::Array();
            for (int j = 0; j < map.Cols(); ++j)
                row.array_.push_back(Num(map(i, j)));
            rows.array_.push_back(std::move(row));
        }
        return Fingerprint(Obj({{"labels", Array(labels, [](const auto& x) { return Str(x); })}, {"map", rows}}));
    }
    XccyMcBasketMemberAllocation_ MemberAllocation(const String_& id, const XccyMcInternal::Member_& member) {
        XccyMcBasketMemberAllocation_ m;
        const auto& a = member.admitted_;
        m.instrument_ = a.Instrument();
        m.frozenContext_ = member.frozenContext_;
        m.measureUnit_ = a.Measure().unit_.Token();
        m.memberId_ = id;
        m.instrumentId_ = a.Instrument()->Id();
        m.instrumentIdentity_ = a.Instrument()->InstrumentIdentity();
        m.sourceIdentity_ = a.Instrument()->Definition()->SourceIdentity();
        m.definitionIdentity_ = a.Instrument()->Definition()->DefinitionIdentity();
        m.bindingIdentity_ = a.Instrument()->BindingIdentity();
        m.measureId_ = a.Measure().id_;
        m.preparationIdentity_ = a.PlanIdentity();
        m.contextIdentity_ = a.ContextIdentity();
        m.compilerIdentity_ = a.CompilerIdentity();
        m.lowBindingIdentity_ = member.zero_.Instrument()->BindingIdentity();
        m.highBindingIdentity_ = member.unit_.Instrument()->BindingIdentity();
        m.nativeMapIdentity_ = NativeMapIdentity(*member.model_);
        m.mapping_ = member.mapping_;
        m.grid_ = Script::XccyScriptPathProvider_<>(member.zero_).Model().Grid();
        m.observations_ = member.zero_.Observations();
        m.requests_ = member.zero_.Requests();
        return m;
    }
    XccyMcBasketAllocation_ Allocate(Vector_<XccyMcBasketMemberAllocation_> members, const XccyMcSettings_& s) {
        XccyMcBasketAllocation_ a;
        a.phase_ = s.phase_;
        a.seed_ = s.seed_;
        a.batches_ = s.batches_;
        a.pathsPerBatch_ = s.pathsPerBatch_;
        a.streamVersion_ = s.streamVersion_;
        a.members_ = std::move(members);
        size_t dimensions = 0;
        for (auto& m : a.members_) {
            dimensions = Add(dimensions, m.mapping_.dimension_);
            AllocateMember(&m, s, a.coordinateEnd_);
            a.coordinateEnd_ = m.coordinateEnd_;
            a.expectedConsumption_ = Add(a.expectedConsumption_, Multiply(Multiply(s.batches_, s.pathsPerBatch_), m.mapping_.dimension_));
        }
        Storage(Multiply(static_cast<size_t>(s.batches_), dimensions), sizeof(double));
        a.identity_ = AllocationIdentity(a);
        a.executionIdentity_ = ExecutionIdentity(a);
        return a;
    }
    void RequireAllocation(const XccyMcBasketAllocation_& a) {
        Check(HeaderMatches(a), "CalibrationContextMismatch", "/sampling/ranges", "known immutable basket layout header");
        XccyMcSettings_ s;
        s.phase_ = a.phase_;
        s.seed_ = a.seed_;
        s.batches_ = a.batches_;
        s.pathsPerBatch_ = a.pathsPerBatch_;
        s.streamVersion_ = a.streamVersion_;
        for (const auto& m : a.members_)
            Check(Bytes(m.mapping_.identity_) == Bytes(XccyMcInternal::MappingIdentity(m.mapping_)), "CalibrationContextMismatch",
                  "/sampling/mapping", "complete unchanged member mapping");
        const auto expected = Allocate(a.members_, s);
        Check(Bytes(a.identity_) == Bytes(AllocationIdentity(a)), "CalibrationContextMismatch", "/sampling/mapping", "unchanged allocation content");
        Check(Bytes(a.identity_) == Bytes(expected.identity_), "CalibrationContextMismatch", "/sampling/mapping", "owned allocation identity");
        for (size_t i = 0; i < a.members_.size(); ++i)
            Check(SameRanges(a.members_[i].ranges_, expected.members_[i].ranges_) &&
                      SameRanges(a.members_[i].phaseRanges_, expected.members_[i].phaseRanges_),
                  "CalibrationContextMismatch", "/sampling/ranges", "exact derived planned ranges");
        Check(Bytes(a.executionIdentity_) == Bytes(expected.executionIdentity_) && a.expectedConsumption_ == expected.expectedConsumption_ &&
                  a.coordinateEnd_ == expected.coordinateEnd_,
              "CalibrationContextMismatch", "/sampling/ranges", "unchanged execution ledger");
    }
} // namespace Dal::XccyMcBasketInternal

namespace Dal {
    namespace {
        bool NonOverlap(const XccyMcRange_& r, const XccyMcRange_& t) {
            return r.firstCoordinate_ == r.coordinateEnd_ || t.firstCoordinate_ == t.coordinateEnd_ || r.coordinateEnd_ <= t.firstCoordinate_ ||
                   t.coordinateEnd_ <= r.firstCoordinate_;
        }
    } // namespace
    void RequireXccyMcBasketIndependent(const XccyMcBasketResult_& first, const XccyMcBasketResult_& second, bool crn) {
        using namespace CalibrationInternal;
        const auto& a = first.Snapshot().allocation_;
        const auto& b = second.Snapshot().allocation_;
        XccyMcBasketInternal::RequireAllocation(a);
        XccyMcBasketInternal::RequireAllocation(b);
        Check(Bytes(a.identity_) == Bytes(b.identity_), "CalibrationContextMismatch", "/sampling/mapping", "one identical basket allocation");
        if (crn) {
            Check(Bytes(a.executionIdentity_) == Bytes(b.executionIdentity_), "CalibrationContextMismatch", "/sampling/ranges",
                  "identical explicitly declared CRN execution");
            return;
        }
        Check(a.expectedConsumption_ > 0 && b.expectedConsumption_ > 0, "InvalidCalibrationPlan", "/sampling/ranges",
              "stochastic members for independence");
        for (const auto& x : first.Snapshot().members_)
            for (const auto& y : second.Snapshot().members_)
                for (const auto& r : x.actualRanges_)
                    for (const auto& t : y.actualRanges_)
                        Check(NonOverlap(r, t), "InvalidCalibrationPlan", "/sampling/ranges", "nonoverlapping independent member scalar spans",
                              x.memberId_ + " / " + y.memberId_);
    }
} // namespace Dal
