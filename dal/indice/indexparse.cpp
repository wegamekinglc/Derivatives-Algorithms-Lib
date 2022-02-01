//
// Created by wegam on 2022/1/20.
//

#include <dal/indice/indexcomposite.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>
#include <map>

namespace Dal {
    namespace {
        // stubbed out, sorry
        Index_* ParseSuperShot(const String_&) { return nullptr; }

        std::map<String_, Index::parser_t>& TheIndexParsers() { RETURN_STATIC(std::map<String_, Index::parser_t>); }

        Index_* ParseSingle(const String_& name) {
            auto stop = name.find_first_of(":[");
            if (stop == String_::npos)
                return ParseSuperShot(name);
            const String_ ac = name.substr(0, stop);
            auto pp = TheIndexParsers().find(ac);
            REQUIRE(pp != TheIndexParsers().end(), "no parser for '" + name + "'");
            return (*pp->second)(name);
        }

        // stubbed out, sorry
        Index::Composite_* ParseComposite(const String_&) { return nullptr; }
    } // namespace

    Index_* Index::Parse(const String_& name) {
        if (Index::Composite_* test = ParseComposite(name))
            return test;
        return ParseSingle(name);
    }

    Handle_<Index_> Index::Clone(const Index_& src) { return Handle_<Index_>(Parse(src.Name())); }
} // namespace Dal