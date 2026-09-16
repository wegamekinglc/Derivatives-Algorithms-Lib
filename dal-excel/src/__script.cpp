//
// Created by wegam on 2022/11/19.
//

#include "__platform.hpp"
#include "__script_test_api.hpp"
#include <dal-public/src/script.hpp>
#include <dal/script/event.hpp>

/*IF--------------------------------------------------------------------------
public Product_New
    Create a product from description
&inputs
name is string
    A name for the object being created
dates is cell[]
    The event dates
events is string[]
    The event strings
&outputs
product is handle ScriptProductData
    The product
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Product_Debug
    output the debug information of a product
&inputs
product is handle ScriptProductData
    The product
&outputs
out is string[]
    The product debug info
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Product_NewWithSettings
    Create a script product using contract settings
&inputs
name is string
    Object name
dates is cell[]
    Event dates or definition labels; same conversion as PRODUCT.NEW
events is string[]
    Event script text; use FIX(EQ[AAPL]) without index quotes
settings is handle StorableScriptProductSettings
    Required SCRIPTPRODUCTSETTINGS.NEW handle
&outputs
product is handle ScriptProductData
    Script product
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public Product_Describe
    Describe contract syntax without date capture, history reads or model setup
&inputs
product is handle ScriptProductData
    Script product handle
&outputs
json is string[]
    JSON chunks in one column; concatenate without separators for dal.script-product/2
-IF-------------------------------------------------------------------------*/

namespace Dal {
    using Dal::Script::ScriptProduct_;
    namespace {
        Vector_<Cell_> ProductDates(const Vector_<Cell_>& dates) {
            // excel may parse dates as a double
            Vector_<Cell_> parsed;
            for (const auto& v : dates) {
                if (Cell::IsDouble(v))
                    parsed.emplace_back(Date::FromExcel(Cell::ToInt(v)));
                else
                    parsed.emplace_back(v);
            }
            return parsed;
        }

        void Product_Debug(const Handle_<ScriptProductData_>& product, Vector_<String_>* out) {
            String_ desc = DebugScriptProduct(product);
            *out = String::Split(desc, '\n', true);
        }
    } // namespace

    void Product_New(const String_& name, const Vector_<Cell_>& dates, const Vector_<String_>& events, Handle_<ScriptProductData_>* product) {
        NewScriptProduct(name, ProductDates(dates), events).swap(*product);
    }

    void Product_NewWithSettings(const String_& name,
                                 const Vector_<Cell_>& dates,
                                 const Vector_<String_>& events,
                                 const Handle_<StorableScriptProductSettings_>& settings,
                                 Handle_<ScriptProductData_>* product) {
        REQUIRE(settings, "InvalidSetting: Product_NewWithSettings; settings; expected non-null product settings handle");
        const auto contract = settings->val_;
        NewScriptProduct(name, ProductDates(dates), events, contract).swap(*product);
    }

    void Product_Describe(const Handle_<ScriptProductData_>& product, Vector_<String_>* json) {
        *json = Excel::ScriptDiagnosticChunks(DescribeScriptProduct(product), "Product_Describe");
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_Product_Debug_public.inc>
#include <dal-excel/auto/MG_Product_Describe_public.inc>
#include <dal-excel/auto/MG_Product_NewWithSettings_public.inc>
#include <dal-excel/auto/MG_Product_New_public.inc>
#endif
} // namespace Dal
