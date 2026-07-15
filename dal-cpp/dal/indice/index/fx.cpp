//
// Created by wegam on 2023/1/23.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/indice/fixingsnapshot.hpp>
#include <dal/indice/index/fx.hpp>

namespace Dal {
    String_ FxIndexName(const Ccy_& domestic, const Ccy_& foreign) {
        return String_("FX[") + foreign.String() + String_("/") + domestic.String() + String_("]");
    }

    String_ FxIndexName(const CurrencyPair_& pair) { return FxIndexName(pair.domestic_, pair.foreign_); }

    String_ ReverseFxIndexName(const CurrencyPair_& pair) { return FxIndexName(pair.foreign_, pair.domestic_); }

    namespace Index {
        double Fx_::Fixing(_ENV, const DateTime_& time) const {
            const double test = PastFixing(_env, FxIndexName(dom_, fgn_), time, true);
            return test > -Dal::INF ? test : 1.0 / PastFixing(_env, FxIndexName(fgn_, dom_), time);
        }
    } // namespace Index
} // namespace Dal
