#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Six-month lockout snowball";
    terms.firstCall_ = 6;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
