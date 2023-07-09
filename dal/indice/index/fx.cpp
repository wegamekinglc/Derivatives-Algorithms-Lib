//
// Created by wegam on 2023/1/23.
//

#include <dal/platform/strict.hpp>
#include <dal/indice/index/fx.hpp>

namespace Dal::Index {
    double Index::Fx_::Fixing(_ENV, const DateTime_& time) const {
        const double test = PastFixing(_env, XName(false), time, true);
        return test > -Dal::INF ? test : 1.0 / PastFixing(_env, XName(true), time);
    }

    String_ Index::Fx_::XName(bool invert) const {
        static const String_ SEP("/");
        return String_("FX[") + (invert ? dom_ : fgn_).String() + SEP + (invert ? fgn_ : dom_).String() + String_("]");
    }
}
