//
// Created by Codex on 2026/10/6.
//

#include <limits>
#include <set>

#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/portfolio.hpp>
#include <dal/storage/json.hpp>

namespace Dal::Script {
    namespace {
        void ValidateTrades(const Vector_<PortfolioTrade_>& trades) {
            REQUIRE2(!trades.empty() && trades.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                     "InvalidScriptPortfolio: trades must be nonempty and fit integer owner ordinals; field=trades", ScriptError_);
            std::set<String_> ids;
            for (const auto& trade : trades) {
                REQUIRE2(!trade.id_.empty() && ids.insert(trade.id_).second,
                         "InvalidScriptPortfolio: trade ID must be nonempty and unique; field=tradeIds; trade=" + trade.id_, ScriptError_);
                REQUIRE2(trade.product_, "InvalidScriptPortfolio: product must not be null; field=products; trade=" + trade.id_, ScriptError_);
                REQUIRE2(trade.model_, "InvalidScriptPortfolio: model must not be null; field=models; trade=" + trade.id_, ScriptError_);
            }
        }

        Handle_<ModelData_> SnapshotModel(const PortfolioTrade_& trade) {
            try {
                const auto probe = CreateModel<double>(trade.model_);
                REQUIRE2(probe->Parameters().size() == probe->ParameterLabels().size(), "model parameter/label dimensions disagree", ScriptError_);
                for (size_t ordinal = 0; ordinal < probe->NumParams(); ++ordinal)
                    REQUIRE2(probe->ValidParameterValue(ordinal, *probe->Parameters()[ordinal]),
                             "invalid model parameter; ordinal=" + String_(std::to_string(ordinal)), ScriptError_);
                const auto snapshot = JSON::WriteString(*trade.model_);
                JSONReadOptions_ options;
                options.maxInputBytes_ = snapshot.size();
                const auto copy = handle_cast<ModelData_>(JSON::ReadString(snapshot.data(), snapshot.size(), options));
                REQUIRE2(copy && typeid(*copy) == typeid(*trade.model_), "model snapshot type changed", ScriptError_);
                return copy;
            } catch (const std::exception& error) {
                THROW2("InvalidScriptPortfolio: model cannot be sealed; field=models; trade=" + trade.id_ + "; cause=" + String_(error.what()),
                       ScriptError_);
            }
        }

        Vector_<PortfolioTrade_> ReadTrades(const Vector_<String_>& ids,
                                            const Vector_<Handle_<ScriptProductData_>>& products,
                                            const Vector_<Handle_<ModelData_>>& models,
                                            const Vector_<int>& owners) {
            REQUIRE2(!ids.empty() && ids.size() == products.size() && ids.size() == owners.size(),
                     "InvalidScriptPortfolio: archived trade table extents disagree; field=modelOwners", ScriptError_);
            std::set<const ModelData_*> modelIdentities;
            for (const auto& model : models)
                REQUIRE2(model && modelIdentities.insert(model.get()).second,
                         "InvalidScriptPortfolio: archived owner models must be nonnull and distinct; field=models", ScriptError_);
            Vector_<PortfolioTrade_> trades;
            trades.reserve(ids.size());
            size_t nextOwner = 0;
            for (size_t trade = 0; trade < ids.size(); ++trade) {
                const int owner = owners[trade];
                REQUIRE2(owner >= 0 && static_cast<size_t>(owner) < models.size() && static_cast<size_t>(owner) <= nextOwner,
                         "InvalidScriptPortfolio: archived owner ordinal is invalid; field=modelOwners; trade=" + ids[trade], ScriptError_);
                if (static_cast<size_t>(owner) == nextOwner)
                    ++nextOwner;
                trades.push_back({ids[trade], products[trade], models[owner]});
            }
            REQUIRE2(nextOwner == models.size(), "InvalidScriptPortfolio: archived owner model is unused; field=models", ScriptError_);
            return trades;
        }
    } // namespace

    ScriptPortfolioData_::ScriptPortfolioData_(const String_& name, const Vector_<PortfolioTrade_>& trades) : Storable_("ScriptPortfolioData", name) {
        ValidateTrades(trades);
        Vector_<const ModelData_*> originals;
        tradeIds_.reserve(trades.size());
        products_.reserve(trades.size());
        modelOwners_.reserve(trades.size());
        for (const auto& trade : trades) {
            const auto found = std::find(originals.begin(), originals.end(), trade.model_.get());
            const size_t owner = static_cast<size_t>(found - originals.begin());
            if (found == originals.end()) {
                models_.push_back(SnapshotModel(trade));
                originals.push_back(trade.model_.get());
            }
            tradeIds_.push_back(trade.id_);
            products_.emplace_back(
                new ScriptProductData_(trade.product_->Name(), trade.product_->Dates(), trade.product_->EventTexts(), trade.product_->Settings()));
            modelOwners_.push_back(static_cast<int>(owner));
        }
    }

#include <dal/auto/MG_ScriptPortfolioData_v1_Read.inc>
#include <dal/auto/MG_ScriptPortfolioData_v1_Write.inc>

    Storable_* ScriptPortfolioData_v1::Reader_::Build() const {
        return new ScriptPortfolioData_(name_, ReadTrades(tradeIds_, products_, models_, modelOwners_));
    }

    void ScriptPortfolioData_::Write(Archive::Store_& dst) const {
        ScriptPortfolioData_v1::XWrite(dst, name_, tradeIds_, products_, models_, modelOwners_);
    }
} // namespace Dal::Script
