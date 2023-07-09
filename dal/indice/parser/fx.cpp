//
// Created by wegam on 2023/1/24.
//

#include <dal/platform/strict.hpp>
#include <dal/indice/parser/fx.hpp>
#include <dal/indice/index/fx.hpp>

namespace Dal::Index {
    Index_* FxParser(const String_& name) {
        auto fx_start = name.find_first_of("[");
        auto fx_stop = name.find_first_of("]");
        auto fx_sep = name.find_first_of("/");
        REQUIRE(fx_start != String_::npos && fx_stop != String_::npos && fx_sep != String_::npos, "fx index pattern is not good");

        String_ fgn = name.substr(fx_start + 1, fx_sep - fx_start - 1);
        String_ dom = name.substr(fx_sep + 1, fx_stop - fx_sep - 1);
        return new Fx_(Ccy_(dom), Ccy_(fgn));
    }
} // namespace Dal::Index