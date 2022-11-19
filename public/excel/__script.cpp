//
// Created by wegam on 2022/11/19.
//

#include <public/excel/__platform.hpp>
#include <public/src/script.hpp>
#include <dal/script/event.hpp>

/*IF--------------------------------------------------------------------------
public Product_New
    Create a product from description
&inputs
name is string
    A name for the object being created
dates is date[]
    The event dates
events is string[]
    The event strings
&outputs
product is handle ScriptProduct
    The product
-IF-------------------------------------------------------------------------*/


/*IF--------------------------------------------------------------------------
public Product_Debug
    output the debug information of a product
&inputs
product is handle ScriptProduct
    The product
&outputs
out is string[]
    The product debug info
-IF-------------------------------------------------------------------------*/



namespace Dal {
    using Dal::Script::ScriptProduct_;
    namespace {
        void Product_New(const String_& name,
                         const Vector_<Date_>& dates,
                         const Vector_<String_>& events,
                         Handle_<ScriptProduct_>* product) {
            NewScriptProduct(name, dates, events).swap(*product);
        }

        void Product_Debug(const Handle_<ScriptProduct_>& product, Vector_<String_>* out) {
            String_ desc = DebugScriptProduct(product);
            *out = String::Split(desc, '\n', true);
        }
    }
#ifdef _WIN32
#include <public/auto/MG_Product_New_public.inc>
#include <public/auto/MG_Product_Debug_public.inc>
#endif
}