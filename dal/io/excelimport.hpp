//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT

// Office type libraries - paths provided by CMake via OFFICE_MSO_PATH, OFFICE_VBE_PATH, OFFICE_EXCEL_PATH
#import OFFICE_MSO_PATH rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RGBXL")
#import OFFICE_VBE_PATH
#import OFFICE_EXCEL_PATH rename("DialogBox", "DialogBoxXL") rename("RGB", "RGBXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces

#endif