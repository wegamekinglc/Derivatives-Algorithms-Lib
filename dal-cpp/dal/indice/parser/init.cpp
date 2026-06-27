//
// Created by wegam on 2023/1/24.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/indice/parser/init.hpp>
#include <dal/string/strings.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/indice/parser/fx.hpp>

namespace Dal {

    bool IndexParsers_::init_ = false;

    void IndexParsers_::Init() {
        if (!init_) {
            Index::RegisterParser("EQ", Index::EquityParser);
            Index::RegisterParser("FX", Index::FxParser);
            init_ = true;
        }
    }
} // namespace Dal
