//
// Created by wegam on 2023/1/23.
//

#pragma once

#include <dal/platform/config.hpp>

#ifdef USE_EXCEL_REPORT

// ExcelImports.cpp
//
// (C) Datasim Education BV 2005
//
// Import necessary typelib's for Excel

// Office 97 (Office v8)
//#import "d:\program files\microsoft office\office\mso97.dll" no_namespace rename("DocumentProperties", "DocumentPropertiesXL")
//#import "d:\program files\common files\microsoft shared\vba\vbeext1.olb" no_namespace
//#import "d:\program files\microsoft office\office\Excel8.olb" rename("DialogBox", "DialogBoxXL") rename("RGB", "RBGXL") rename("DocumentProperties", "DocumentPropertiesXL") no_dual_interfaces

// Office 2000 - local typelibraries.
//#import "mso9.dll" no_namespace rename("DocumentProperties", "DocumentPropertiesXL")
//#import "vbe6ext.olb" no_namespace rename("Property", "PropertyVB")
//#import "Excel9.olb" rename("DialogBox", "DialogBoxXL") rename("RGB", "RBGXL") rename("DocumentProperties", "DocumentPropertiesXL")  no_dual_interfaces

//!!! #import "vbe6ext.olb" no_namespace rename("Property", "PropertyVB") my be necessary TOO


// Office 2000 (Office v9)
//#import "D:\Program Files\Microsoft Office\Office\mso9.dll" no_namespace rename("DocumentProperties", "DocumentPropertiesXL")
//#import "D:\Program Files\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb" no_namespace no_namespace rename("Property", "PropertyVB")
//#import "D:\Program Files\Microsoft Office\Office\Excel9.olb" rename("DialogBox", "DialogBoxXL") rename("RGB", "RBGXL") rename("DocumentProperties", "DocumentPropertiesXL") no_dual_interfaces

// Office XP (2002) (Office v10)
//#import "C:\Program Files\Common Files\Microsoft Shared\office10\mso.dll" rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RBGXL")
//#import "C:\Program Files\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
//#import "C:\Program Files\Microsoft Office\Office10\EXCEL.EXE" rename("DialogBox", "DialogBoxXL") rename("RGB", "RBGXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces

// Office 2003 (Office v11) (Will give error (actually a warning) that you need W2K or higher to run the result)
//#import "C:\Program Files\Common Files\Microsoft Shared\office11\mso.dll" rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RBGXL")
//#import "C:\Program Files\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
//#import "C:\Program Files\Microsoft Office\Office11\EXCEL.EXE" rename("DialogBox", "DialogBoxXL") rename("RGB", "RBGXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces

// Office 2007 (Office v12) (Will give error (actually a warning) that you need W2K or higher to run the result)
/*#import "C:\Program Files\Common Files\Microsoft Shared\office12\mso.dll" rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RGBXL")
#import "C:\Program Files\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
#import "C:\Program Files\Microsoft Office\Office12\EXCEL.EXE" rename("DialogBox", "DialogBoxXL") rename("RGB", "RGBXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces
*/
//Vs2010

//#import "C:\Program Files (x86)\Common Files\Microsoft Shared\office14\mso.dll" \
//    rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RGBXL")
//#import "C:\Program Files (x86)\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
//#import "C:\Program Files (x86)\Microsoft Office\Office14\EXCEL.EXE" \
//    rename("DialogBox", "DialogBoxXL") rename("RGB", "RGBXL") \
//        rename("DocumentProperties", "DocumentPropertiesXL") \
//            rename("ReplaceText", "ReplaceTextXL") \
//                rename("CopyFile", "CopyFileXL") no_dual_interfaces

/*
#import "C:\Program Files (x86)\Common Files\Microsoft Shared\office14\mso.dll"
#import "C:\Program Files (x86)\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
#import "C:\Program Files (x86)\Microsoft Office\Office14\EXCEL.EXE"
*/

/*
#import "C:\Program Files\Common Files\Microsoft Shared\office14\mso.dll" rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RGBXL")
#import "C:\Program Files\Common Files\Microsoft Shared\VBA\VBA6\vbe6ext.olb"
#import "C:\Program Files\Microsoft Office\Office14\EXCEL.EXE" rename("DialogBox", "DialogBoxXL") rename("RGB", "RGBXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces
*/

// Excel 2016
/*
#import "C:\Program Files\Microsoft Office\root\VFS\ProgramFilesCommonX86\Microsoft Shared\OFFICE16\MSO.DLL"
#import "C:\Program Files\Microsoft Office\root\VFS\ProgramFilesCommonX86\Microsoft Shared\VBA\VBA6\VBE6EXT.OLB"
#import "C:\Program Files\Microsoft Office\root\Office16\EXCEL.EXE"
*/

// Excel 2016
/*
#import "C:\Program Files (x86)\Common Files\microsoft shared\OFFICE15\MSO.DLL"
#import "C:\Program Files (x86)\Common Files\microsoft shared\VBA\VBA6\VBE6EXT.OLB"
#import "C:\Program Files\Microsoft Office\Office15\EXCEL.EXE"
*/

// Excel office 365
#import "C:\Program Files\Microsoft Office\root\VFS\ProgramFilesCommonX86\Microsoft Shared\OFFICE16\MSO.DLL" rename("DocumentProperties", "DocumentPropertiesXL") rename("RGB", "RGBXL")
#import "C:\Program Files\Microsoft Office\root\VFS\ProgramFilesCommonX86\Microsoft Shared\VBA\VBA6\VBE6EXT.OLB"
#import "C:\Program Files\Microsoft Office\root\Office16\EXCEL.EXE" rename("DialogBox", "DialogBoxXL") rename("RGB", "RGBXL") rename("DocumentProperties", "DocumentPropertiesXL") rename("ReplaceText", "ReplaceTextXL") rename("CopyFile", "CopyFileXL") no_dual_interfaces

#endif