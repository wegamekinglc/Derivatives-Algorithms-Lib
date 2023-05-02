#ifndef DAL_CELL_I
#define DAL_CELL_I

%include strings.i
%include "std_vector.i"
    %{
#include <dal/math/cell.hpp>
    %}

class Cell_ {
public:
    Cell_(bool b);
    Cell_(double d);
    Cell_(const Date_& dt);
    Cell_(const String_& s);
    Cell_(const char* s);
};

%template(CellVector) std::vector<Cell_>;

#endif