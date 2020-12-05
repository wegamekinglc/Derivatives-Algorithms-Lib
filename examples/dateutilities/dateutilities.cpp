//
// Created by wegam on 2020/12/5.
//

#include <iostream>
#include <dal/time/date.hpp>

using namespace Dal;
using namespace std;

int main() {

    Date_ d(2018, 8, 8);
    cout << Date::ToString(d) << endl;

    return 0;
}