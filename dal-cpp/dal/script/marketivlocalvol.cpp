#include <dal/platform/platform.hpp>

#include <dal/concurrency/threadpool.hpp>
#include <dal/model/marketivlocalvolinternal.hpp>
#include <dal/model/marketivlocalvolmodel.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/marketivlocalvol.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal::Script {
    namespace {
        void ValidateIdentityTextSize(const ScriptProductData_& product, const PreparedScript_& prepared) {
            using namespace MarketIvLocalVolDetail;
            size_t bytes = CheckedAdd(4096, product.Settings().defaultIndex_.size());
            for (const auto& text : product.EventTexts())
                bytes = CheckedAdd(bytes, text.size());
            for (const auto& date : product.Dates())
                bytes = CheckedAdd(bytes, Cell::ToString(date).size());
            const auto& frozen = prepared.Settings().fixings_;
            if (frozen)
                for (const auto& [name, history] : frozen->Values())
                    bytes = CheckedAdd(bytes, CheckedMultiply(history.size(), CheckedAdd(name.size(), 128)));
            const size_t envelope = CheckedMultiply(1024, CheckedMultiply(bytes, bytes));
            CheckLimit(envelope, std::string().max_size(), "/execution/request_identity", "Prepare");
        }

        String_ RequestIdentity(const ScriptProductData_& product, const PreparedScript_& prepared, const MarketIvLocalVolExecution_& execution) {
            ValidateIdentityTextSize(product, prepared);
            using J_ = CanonicalJson::Json_;
            J_ root = J_::Object();
            root.object_["schema"] = J_::String("dal.market-iv-local-vol-request/2");
            root.object_["model_identity"] = J_::String(execution.modelIdentity_);
            root.object_["product"] = J_::String(DescribeScriptProductData(product));
            root.object_["prepared"] = J_::String(ExplainPreparedScript(prepared));
            root.object_["workers"] = J_::String(String_(std::to_string(execution.workers_)));
            root.object_["layout"] = J_::String("SHARED_HYBRID_PRIVATE_PATH_CAPACITY/1");
            root.object_["policy_risk_mode"] = J_::String(prepared.Simulation().lsmcPolicyRiskMode_);
            root.object_["policy_bump"] = J_::Number(prepared.Simulation().lsmcPolicyBumpRelative_);
            root.object_["passive_constants"] = J_::Boolean(prepared.Simulation().usePassiveConstants_);
            J_ fixings = J_::Array();
            const auto& frozen = prepared.Settings().fixings_;
            if (frozen)
                for (const auto& [name, history] : frozen->Values())
                    for (const auto& [time, value] : history) {
                        J_ row = J_::Array();
                        row.array_ = {J_::String(name), J_::String(Date::ToString(time.Date())), J_::Number(time.Frac()), J_::Number(value)};
                        fixings.array_.push_back(std::move(row));
                    }
            root.object_["fixings"] = std::move(fixings);
            return CanonicalJson::Fingerprint(root);
        }
    } // namespace

    MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(const ScriptProductData_& product,
                                                                const MarketIvLocalVolModelData_& data,
                                                                const ScriptValuationSettings_& valuation,
                                                                const MonteCarloSettings_& simulation) {
        AAD::MarketIvLocalVolModel_ model(data);
        const auto prepared = PrepareScript(product, &model, valuation, simulation);
        if (prepared.AllExpired()) {
            const Vector_<> timeline{0.};
            const Vector_<AAD::SampleDef_> definitions(1);
            model.Allocate(timeline, definitions);
            model.Init(timeline, definitions);
        }
        model.PrepareWorkers(ThreadPool_::GetInstance()->NumThreads());
        auto execution = model.Execution();
        execution.requestIdentity_ = RequestIdentity(product, prepared, execution);
        return execution;
    }
} // namespace Dal::Script
