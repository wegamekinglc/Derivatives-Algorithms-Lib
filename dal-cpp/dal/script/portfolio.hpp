//
// Created by Codex on 2026/10/6.
//

#pragma once

#include <dal/model/base.hpp>
#include <dal/script/event.hpp>

/*IF--------------------------------------------------------------------------
storable ScriptPortfolioData
    Sealed script portfolio with explicit model ownership
version 1
manual
&members
name is ?string
tradeIds is string[]
products is +handle ScriptProductData
models is +handle ModelData
modelOwners is integer[]
-IF-------------------------------------------------------------------------*/

namespace Dal::Script {
    struct PortfolioTrade_ {
        String_ id_;
        Handle_<ScriptProductData_> product_;
        Handle_<ModelData_> model_;
    };

    class ScriptPortfolioData_ : public Storable_ {
        Vector_<String_> tradeIds_;
        Vector_<Handle_<ScriptProductData_>> products_;
        Vector_<Handle_<ModelData_>> models_;
        Vector_<int> modelOwners_;

    public:
        ScriptPortfolioData_(const String_& name, const Vector_<PortfolioTrade_>& trades);
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] const Vector_<String_>& TradeIds() const { return tradeIds_; }
        [[nodiscard]] const Vector_<Handle_<ScriptProductData_>>& Products() const { return products_; }
        [[nodiscard]] const Vector_<Handle_<ModelData_>>& Models() const { return models_; }
        [[nodiscard]] const Vector_<int>& ModelOwners() const { return modelOwners_; }
    };
} // namespace Dal::Script
