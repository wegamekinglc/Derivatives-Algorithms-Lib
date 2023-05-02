#ifndef DAL_SCRIPT_I
#define DAL_SCRIPT_I

%{
#include <public/src/script.hpp>
%}

%include date.i
%include strings.i
%include cell.i

%template(ScriptProductData_) Handle_<ScriptProductData_>;

%inline %{
    Handle_<ScriptProductData_> Product_New(const std::vector<Cell_>& dates, const std::vector<std::string>& events) {
        Vector_<Cell_> new_dates(dates.size());
        for(auto i = 0; i < dates.size(); ++i)
            new_dates[i] = dates[i];
        Vector_<String_> new_events;
        for(auto& e : events)
            new_events.push_back(String_(e));
        return NewScriptProduct("ScriptProductData_", new_dates, new_events);
    }

    std::string Product_Debug(const Handle_<ScriptProductData_>& product) {
        return DebugScriptProduct(product).c_str();
    }
%}

#endif