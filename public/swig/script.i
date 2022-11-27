#ifndef DAL_SCRIPT_I
#define DAL_SCRIPT_I

%{
#include <public/src/script.hpp>
%}

%include date.i
%include strings.i

%template(ScriptProduct_) Handle_<ScriptProductData_>;

%inline %{
    Handle_<ScriptProduct_> Product_New(const std::vector<Date_>& dates, const std::vector<std::string>& events) {
        Vector_<Date_> new_dates;
        for(auto& d : dates)
            new_dates.push_back(d);
        Vector_<String_> new_events;
        for(auto& e : events)
            new_events.push_back(String_(e));

        return Handle_<ScriptProductData_>(new ScriptProductData_("script_product", new_dates, new_events));
    }

    std::string Product_Debug(const Handle_<ScriptProductData_>& product) {
        return DebugScriptProduct(product).c_str();
    }
%}


#endif