//
// Created by wegam on 2020/12/16.
//

#pragma once

namespace Dal {
    double NCDF(double z, bool precise = true);
    double InverseNCDF(double x, bool precise = true, bool polish = true);
}