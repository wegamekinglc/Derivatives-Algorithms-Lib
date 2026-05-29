# DAL 项目拆分重构方案

## 一、最终目标结构

建议最终形成 4 个独立项目目录：

```text
Derivatives-Algorithms-Lib/
  CMakeLists.txt                  # 可选：顶层 superbuild / workspace build

  dal-cpp/
    CMakeLists.txt
    cmake/
    include/
      dal/
    src/
    auto/
    config/
    externals/
    tests/
    examples/

  dal-public/
    CMakeLists.txt
    cmake/
    include/
      dal_public/
    src/
    auto/
    config/
    tests/

  dal-python/
    CMakeLists.txt
    cmake/
    swig/
    python/
    tests/

  dal-excel/
    CMakeLists.txt
    cmake/
    include/
    src/
    tests/
```

目标依赖关系固定为：

```text
dal-cpp
  ↑
dal-public
  ↑        ↑
dal-python dal-excel
```

禁止反向依赖和横向依赖：

```text
dal-cpp -> dal-public
dal-cpp -> dal-python
dal-cpp -> dal-excel
dal-public -> dal-python
dal-public -> dal-excel
dal-python -> dal-excel
dal-excel -> dal-python
```

测试框架约束：

- `dal-cpp`、`dal-public`、`dal-excel` 的 C++ 测试统一使用 GoogleTest（gtest）。
- `dal-python` 的 Python 测试统一使用 pytest。
- CTest 可以作为 CMake 侧的测试执行入口，但不能替代具体测试框架；C++ 测试应通过 CTest 注册 gtest target，Python 测试可以通过 CTest 调用 pytest。

---

## 二、四个项目职责边界

### 1. `dal-cpp`

`dal-cpp` 是核心 C++ Quant Library，包含最完整的内部能力和算法实现。

应包含当前核心目录：

```text
dal/
  math/
  script/
  model/
  curve/
  indice/
  risk/
  concurrency/
  storage/
  auto/
```

以及：

```text
config/
externals/
examples/
tests/
```

不应包含：

```text
public/
public/python/
public/excel/
public/swig/
```

输出产物：

```text
libdal_cpp.a / libdal_cpp.so
dal-cppConfig.cmake
dal-cppTargets.cmake
include/dal/...
```

测试重点：

- C++ 单元测试使用 gtest。
- 数学库
- AAD
- 曲线构造
- 优化器
- 脚本引擎
- 风控计算
- storage / serialization
- examples 可编译性

### 2. `dal-public`

`dal-public` 是稳定的 C++ 对外接口层。它不应该重复实现核心业务逻辑，而应当作为 `dal-cpp` 之上的公共 API 包装层。

建议迁移当前内容：

```text
public/src/       -> dal-public/src/
public/auto/      -> dal-public/auto/
public codegen input -> dal-public/config/ 或保留顶层 source of truth 并显式声明
public headers    -> dal-public/include/dal_public/
```

建议整理为：

```text
dal-public/
  include/
    dal_public/
      dal.hpp
      date.hpp
      curve.hpp
      model.hpp
      risk.hpp
      script.hpp
  src/
    date.cpp
    curve.cpp
    model.cpp
    risk.cpp
    script.cpp
  auto/
  config/
  tests/
```

不应包含：

- Python-specific 代码
- Excel-specific 代码
- SWIG-specific 代码
- 内部核心实现细节

输出产物：

```text
libdal_public.a / libdal_public.so
dal-publicConfig.cmake
dal-publicTargets.cmake
include/dal_public/...
```

测试重点：

- C++ 单元测试使用 gtest。
- 对外 API 是否能独立 include
- 对外 API 是否能链接
- public 类型是否稳定
- public API 是否正确调用 `dal-cpp`
- serialization round-trip
- error handling 是否适合外部用户

### 3. `dal-python`

`dal-python` 是 Python binding 项目，只通过 `dal-public` 暴露能力。

建议迁移当前内容：

```text
public/swig/      -> dal-python/swig/
public/python/    -> dal-python/python/
```

建议结构：

```text
dal-python/
  CMakeLists.txt
  swig/
    dal_public.i
    typemaps.i
    script.i
    curve.i
  python/
    dal/
      __init__.py
      curve.py
      risk.py
  tests/
    test_import.py
    test_curve.py
    test_script.py
```

依赖规则：

- 只依赖 `dal-public`。
- 不直接 include `dal-cpp` 内部头文件。
- SWIG 输入应只引用 public header。

输出产物：

```text
dal/_dal*.so 或平台等价扩展模块
dal/*.py / Python package
wheel package
```

测试重点：

- Python 测试使用 pytest。
- `import dal`
- 基本类型构造
- curve API
- script API
- exception 映射
- Python ownership / lifetime
- Python package install 后是否可用

### 4. `dal-excel`

`dal-excel` 是 Excel 接口项目，只通过 `dal-public` 暴露能力。

建议迁移当前内容：

```text
public/excel/ -> dal-excel/
```

建议结构：

```text
dal-excel/
  include/
  src/
    addin.cpp
    registration.cpp
    conversion.cpp
    curve_functions.cpp
    risk_functions.cpp
  tests/
    test_conversion.cpp
    test_registration.cpp
```

依赖规则：

- 只依赖 `dal-public`。
- 不直接依赖 Python 或核心内部实现。

输出产物，Windows 下：

```text
dal_excel.xll
dal_excel.dll
```

测试重点：

- C++ 单元测试使用 gtest。
- C++ 层参数转换
- Excel function registration
- error conversion
- string/date/number/vector conversion
- 核心函数 smoke test

Excel UI 自动化可以后置，第一阶段优先保证接口层 C++ 单测和手工验证清单。

---

## 三、迁移分阶段计划

### Phase 0：准备阶段

目标：明确当前代码边界，建立重构保护网。

工作项：

1. 记录当前构建方式：
   - `build_linux.sh`
   - top-level `CMakeLists.txt`
   - `public/CMakeLists.txt`
   - `tests/CMakeLists.txt`
2. 记录当前 target：
   - `dal_library`
   - `dal_public`
   - `dal_excel`
   - `test_suite`
3. 记录当前目录归属：

```text
dal/              -> dal-cpp
public/src/       -> dal-public
public/swig/      -> dal-python
public/python/    -> dal-python
public/excel/     -> dal-excel
tests/            -> mostly dal-cpp，部分 public tests 后续拆出
examples/         -> dal-cpp
config/           -> 迁移期保留顶层 codegen source of truth；后续拆入 dal-cpp / dal-public
externals/        -> dal-cpp initially
```

4. 跑一次当前完整测试：

```bash
bash ./build_linux.sh
bin/test_suite
```

5. 确认当前测试框架和执行入口：
   - 当前 C++ 测试基于 gtest。
   - 如缺少 `enable_testing()` / `add_test()` / `gtest_discover_tests()`，先补齐 CTest 注册。
   - CTest 只作为执行入口，gtest 仍是 C++ 测试框架。
6. 记录当前代码生成链路：
   - `config/dal.ifc`
   - `config/dal.mgl`
   - `build_linux.sh` 中生成 `dal/auto` 和 `public/auto` 的命令
   - 第一轮迁移前必须明确 codegen source of truth，避免 `dal-public` 隐式依赖 `dal-cpp/config`。
7. 生成 public / Python / Excel 依赖清单：
   - `public/src` 直接 include 的 `dal/...` 内部头
   - `public/swig` 直接 include 的 `dal/...` 和 `public/src/...`
   - `public/excel` 直接 include 的 `dal/...` 和 `public/src/...`
8. 记录当前静态/动态构建差异：
   - 特别是当前 `dal_public` 静态构建会把 `dal/*.cpp` 重复编入 public library 的行为。

产出：

- 当前构建基线
- 当前测试基线
- 初始模块归属表
- 测试框架和 CTest 注册基线
- codegen 输入/输出归属基线
- public / Python / Excel 对 core 内部头的依赖清单

### Phase 1：建立新目录，不移动逻辑

目标：先创建 4 个项目目录和 CMake 框架，但不大规模改代码。

新增目录：

```text
dal-cpp/
dal-public/
dal-python/
dal-excel/
```

顶层 `CMakeLists.txt` 第一阶段不要直接替换成新 workspace。当前顶层还承载版本、三方库、AAD backend、Office 探测、旧目录构建和旧测试入口；第一步应保留旧构建默认路径，只增加默认关闭的新 workspace 入口：

```cmake
option(DAL_ENABLE_REFACTORED_WORKSPACE "Enable refactored workspace targets" OFF)

if(DAL_ENABLE_REFACTORED_WORKSPACE)
    option(DAL_BUILD_CPP "Build dal-cpp" ON)
    option(DAL_BUILD_PUBLIC "Build dal-public" ON)
    option(DAL_BUILD_PYTHON "Build dal-python" OFF)
    option(DAL_BUILD_EXCEL "Build dal-excel" OFF)

    if(DAL_BUILD_CPP)
        add_subdirectory(dal-cpp)
    endif()

    if(DAL_BUILD_PUBLIC)
        add_subdirectory(dal-public)
    endif()

    if(DAL_BUILD_PYTHON)
        add_subdirectory(dal-python)
    endif()

    if(DAL_BUILD_EXCEL)
        add_subdirectory(dal-excel)
    endif()
endif()
```

产出：

- 新的 4 项目空壳
- 默认不影响旧构建的可配置入口
- 旧 `bash ./build_linux.sh` 和旧 `bin/test_suite` 仍可使用

### Phase 2：拆出 `dal-cpp`

目标：让核心库变成独立项目。

迁移内容：

```text
dal/        -> dal-cpp/src 或 dal-cpp/dal
config/     -> dal-cpp/config，但要保留 public 代码生成可用的 source of truth
examples/   -> dal-cpp/examples
tests/      -> dal-cpp/tests，先整体迁移
```

建议第一步保持 include 路径不变：

```text
dal-cpp/include/dal/...
```

或者短期保留：

```text
dal-cpp/dal/...
```

不要同时做“大改 include 路径”和“拆 CMake”。

CMake target：

```cmake
add_library(dal_cpp ...)
add_library(DAL::cpp ALIAS dal_cpp)
```

核心代码内部继续：

```cpp
#include <dal/...>
```

安装规则：

```cmake
install(TARGETS dal_cpp EXPORT dal-cppTargets)
install(DIRECTORY include/dal DESTINATION include)
install(EXPORT dal-cppTargets NAMESPACE DAL:: DESTINATION lib/cmake/dal-cpp)
```

测试 target：

```cmake
enable_testing()
if(NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

add_executable(dal_cpp_tests ...)
target_link_libraries(dal_cpp_tests PRIVATE DAL::cpp GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(dal_cpp_tests)
```

验收标准：

```bash
cmake -S dal-cpp -B build/dal-cpp -DDAL_CPP_BUILD_TESTS=ON
cmake --build build/dal-cpp
ctest --test-dir build/dal-cpp
```

### Phase 3：拆出 `dal-public`

目标：把当前 public C++ 接口做成独立项目。

迁移内容：

```text
public/src/       -> dal-public/src/
public/auto/      -> dal-public/auto/
public codegen input -> dal-public/config/ 或保留顶层 source of truth 并显式声明
public headers    -> dal-public/include/dal_public/
```

monorepo 内构建：

```cmake
target_link_libraries(dal_public PUBLIC DAL::cpp)
```

独立构建：

```cmake
find_package(dal-cpp REQUIRED)
target_link_libraries(dal_public PUBLIC DAL::cpp)
```

CMake target：

```cmake
add_library(dal_public ...)
add_library(DAL::public ALIAS dal_public)
```

关键重构点：

`dal-public` 的头文件应该尽量减少直接暴露 `dal-cpp` 内部类型。更理想是 public 层提供稳定 wrapper：

```cpp
#include <dal_public/curve.hpp>
#include <dal_public/script.hpp>
```

测试应模拟外部用户：

```cpp
#include <dal_public/curve.hpp>
```

而不是 include 内部头。

当前 public 静态构建会把 `dal/*.cpp` 编入 `dal_public`。拆分时必须先消除这种重复编译路径，静态和动态构建都统一依赖 `DAL::cpp`：

```cmake
target_link_libraries(dal_public PUBLIC DAL::cpp)
```

public C++ 测试使用 gtest：

```cmake
enable_testing()
if(NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

add_executable(dal_public_tests ...)
target_link_libraries(dal_public_tests PRIVATE DAL::public GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(dal_public_tests)
```

验收标准：

```bash
cmake -S dal-public -B build/dal-public -DCMAKE_PREFIX_PATH=<dal-cpp-install>
cmake --build build/dal-public
ctest --test-dir build/dal-public
```

同时覆盖：

```bash
cmake -S dal-public -B build/dal-public-static -DBUILD_SHARED_LIBS=OFF -DCMAKE_PREFIX_PATH=<dal-cpp-install>
cmake --build build/dal-public-static
ctest --test-dir build/dal-public-static
```

### Phase 4：拆出 `dal-python`

目标：Python binding 只依赖 `dal-public`。

迁移内容：

```text
public/swig/      -> dal-python/swig/
public/python/    -> dal-python/python/
```

建议分两步：

1. 先在 `dal-python` 目录复现当前 SWIG + packaging + pytest 流程，保证 `import dal` 可用。
2. 再把 SWIG 输入逐步切换到 `dal_public` 头文件，移除对 `dal/...` 和 `public/src/...` 的直接引用。

主接口文件建议变成：

```text
dal-python/swig/dal_public.i
```

它只 include public 头：

```swig
%module dal

%{
#include <dal_public/dal.hpp>
#include <dal_public/curve.hpp>
#include <dal_public/script.hpp>
%}

%include "dal_public/dal.hpp"
%include "dal_public/curve.hpp"
%include "dal_public/script.hpp"
```

CMake target：

```cmake
find_package(SWIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(dal-public REQUIRED)

swig_add_library(dal_python
    TYPE MODULE
    LANGUAGE python
    SOURCES swig/dal_public.i
)

target_link_libraries(dal_python PRIVATE DAL::public)
```

测试：

```text
dal-python/tests/
  test_import.py
  test_curve.py
  test_exception.py
```

Python 测试框架固定为 pytest。CMake 可以增加一个测试入口调用 pytest：

```cmake
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME dal_python_pytest
         COMMAND Python3::Interpreter -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests)
```

验收标准：

```bash
cmake -S dal-python -B build/dal-python -DCMAKE_PREFIX_PATH=<dal-public-install>
cmake --build build/dal-python
python -m pytest dal-python/tests
```

### Phase 5：拆出 `dal-excel`

目标：Excel 项目只依赖 `dal-public`，Windows 独立构建。

迁移内容：

```text
public/excel/ -> dal-excel/
```

建议结构：

```text
dal-excel/
  include/
  src/
    addin.cpp
    conversion.cpp
    registration.cpp
    functions_curve.cpp
    functions_risk.cpp
  tests/
```

CMake target：

```cmake
find_package(dal-public REQUIRED)

add_library(dal_excel SHARED ...)
target_link_libraries(dal_excel PRIVATE DAL::public)
```

Windows-only：

```cmake
if(WIN32)
    add_subdirectory(src)
else()
    message(STATUS "dal-excel is only built on Windows")
endif()
```

测试策略：

```text
test_excel_conversion.cpp
test_excel_registration.cpp
test_excel_errors.cpp
```

这些 C++ 测试使用 gtest，并通过 CTest 注册执行。

验收标准，Windows：

```powershell
cmake -S dal-excel -B build/dal-excel -DCMAKE_PREFIX_PATH=<dal-public-install>
cmake --build build/dal-excel --config Release
ctest --test-dir build/dal-excel -C Release
```

---

## 四、CMake 详细设计

每个项目都要支持两种模式。

### Monorepo 模式

```cmake
add_subdirectory(dal-cpp)
add_subdirectory(dal-public)
add_subdirectory(dal-python)
add_subdirectory(dal-excel)
```

### Installed package 模式

```cmake
find_package(dal-cpp REQUIRED)
find_package(dal-public REQUIRED)
```

这样将来拆仓库时成本最低。

### 测试集成

C++ 项目：

```cmake
enable_testing()
if(NOT TARGET GTest::gtest_main)
    find_package(GTest REQUIRED)
endif()

target_link_libraries(<test_target> PRIVATE GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(<test_target>)
```

适用项目：

- `dal-cpp`
- `dal-public`
- `dal-excel`

Python 项目：

```cmake
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME dal_python_pytest
         COMMAND Python3::Interpreter -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests)
```

`dal-python` 的测试框架保持 pytest。CTest 只负责从 CMake 构建目录统一调度测试。

### Target 命名建议

| 项目       | CMake target | Alias         |
|------------|--------------|---------------|
| dal-cpp    | `dal_cpp`    | `DAL::cpp`    |
| dal-public | `dal_public` | `DAL::public` |
| dal-python | `dal_python` | `DAL::python` |
| dal-excel  | `dal_excel`  | `DAL::excel`  |

### CMake option 建议

`dal-cpp`：

```cmake
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
option(DAL_CPP_BUILD_EXAMPLES "Build dal-cpp examples" ON)
option(DAL_CPP_USE_XAD_AAD "Use XAD AAD backend" OFF)
option(DAL_CPP_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
option(DAL_CPP_USE_ADEPT_AAD "Use Adept AAD backend" ON)
```

`dal-public`：

```cmake
option(DAL_PUBLIC_BUILD_TESTS "Build dal-public tests" ON)
```

`dal-python`：

```cmake
option(DAL_PYTHON_BUILD_TESTS "Build dal-python tests" ON)
option(DAL_PYTHON_BUILD_WHEEL "Build Python wheel" OFF)
```

`dal-excel`：

```cmake
option(DAL_EXCEL_BUILD_TESTS "Build dal-excel tests" ON)
```

---

## 五、代码生成策略

当前项目有 Machinist 生成代码：

```text
dal/auto/
public/auto/
config/dal.ifc
```

第一阶段不要重写生成系统，先做最小迁移。

第一轮迁移中必须保留一个明确的 codegen source of truth。当前 `config/dal.ifc` 同时生成 core 和 public 内容，因此不能简单把 `config/` 移入 `dal-cpp` 后让 `dal-public` 隐式读取 `dal-cpp/config`。

推荐第一轮策略：

```text
config/ 保留在顶层，作为迁移期 codegen source of truth
生成输出分别落到 dal-cpp/... 和 dal-public/...
待 dal-cpp / dal-public 独立构建稳定后，再拆成各项目自己的 config
```

推荐方案：

### `dal-cpp`

保留核心生成：

```text
dal-cpp/config/  # 第二轮再迁入；第一轮可继续使用顶层 config/
dal-cpp/auto/
```

生成到：

```text
dal-cpp/include/dal/auto/
```

或者短期保留：

```text
dal-cpp/dal/auto/
```

### `dal-public`

如果 public 有独立生成内容，放在：

```text
dal-public/config/
dal-public/auto/
```

但要明确它的输入来自 public 自己的接口定义，而不是隐式依赖 `dal-cpp/config`。

后续理想状态是每个项目自己的 codegen 输入只生成自己的 auto 文件：

```text
dal-cpp/config/dal.ifc
dal-public/config/dal_public.ifc
```

这可以放到第二轮优化，不建议第一轮做。

---

## 六、测试拆分策略

当前测试先整体迁移到 `dal-cpp`：

```text
tests/ -> dal-cpp/tests/
```

然后逐步分流。

测试框架规则：

- `dal-cpp`、`dal-public`、`dal-excel` 的 C++ 测试使用 gtest。
- `dal-python` 的 Python 测试使用 pytest。
- 所有可由 CMake 管理的测试都应注册到 CTest，方便 CI 统一执行。

测试归属规则：

| 测试内容              | 归属项目     |
|-----------------------|--------------|
| 核心算法              | `dal-cpp`    |
| 内部数据结构          | `dal-cpp`    |
| AAD backend           | `dal-cpp`    |
| curve calibration     | `dal-cpp`    |
| public API include/link | `dal-public` |
| public API behavior   | `dal-public` |
| SWIG import           | `dal-python` |
| Python object lifecycle | `dal-python` |
| Python exception      | `dal-python` |
| Excel type conversion | `dal-excel`  |
| Excel registration    | `dal-excel`  |

---

## 七、CI 设计

建议最后形成 4 条 CI job。

### Linux core

```text
build dal-cpp
run dal-cpp gtest tests through ctest
```

### Linux public

```text
build dal-cpp
install dal-cpp
build dal-public
run dal-public gtest tests through ctest
```

### Linux python

```text
build dal-cpp
build dal-public
build dal-python
run pytest
```

### Windows excel

```text
build dal-cpp
build dal-public
build dal-excel
run dal-excel gtest tests through ctest
```

如果时间有限，优先顺序：

1. Linux `dal-cpp`
2. Linux `dal-public`
3. Linux `dal-python`
4. Windows `dal-excel`

---

## 八、推荐迁移顺序

```text
1. 先让 dal-cpp 独立构建
2. 再让 dal-public 独立构建
3. 再让 dal-python 依赖 dal-public
4. 最后让 dal-excel 依赖 dal-public
5. 最后清理旧 public/ 目录和旧 CMake
```

不要一开始删除旧目录。建议采用并行迁移：

```text
旧构建仍然可用
新构建逐步完善
新构建完全通过后再删除旧构建
```

---

## 九、每阶段验收清单

### Phase 1 验收

- [ ] 顶层能识别 4 个项目目录
- [ ] 默认不影响旧构建
- [ ] `bash ./build_linux.sh` 仍通过
- [ ] `bin/test_suite` 仍可运行
- [ ] 新 CMake option 可配置

### Phase 2 验收：`dal-cpp`

- [ ] `dal-cpp` 可单独 configure
- [ ] `dal-cpp` 可单独 build
- [ ] `dal-cpp` gtest tests 通过
- [ ] `dal-cpp` tests 已注册到 CTest
- [ ] examples 至少能编译
- [ ] install 后可被外部 CMake 项目 `find_package(dal-cpp)`

### Phase 3 验收：`dal-public`

- [ ] `dal-public` 不直接依赖 Python/Excel
- [ ] `dal-public` 只链接 `DAL::cpp`
- [ ] `dal-public` 不再把 `dal/*.cpp` 重复编入 public library
- [ ] public gtest tests 通过
- [ ] public tests 已注册到 CTest
- [ ] public headers 可被外部项目 include
- [ ] install 后可被外部 CMake 项目 `find_package(dal-public)`

### Phase 4 验收：`dal-python`

- [ ] SWIG 只 include public headers
- [ ] Python module 可 build
- [ ] `import dal` 成功
- [ ] pytest tests 通过
- [ ] 可生成 wheel 或至少可本地安装

### Phase 5 验收：`dal-excel`

- [ ] Excel target 只链接 `DAL::public`
- [ ] Windows build 通过
- [ ] Excel gtest tests 通过
- [ ] Excel tests 已注册到 CTest
- [ ] 插件产物生成成功

---

## 十、主要风险和处理方式

### 风险 1：include 路径改动太大

第一轮保持：

```cpp
#include <dal/...>
```

不要立刻改成：

```cpp
#include <dal_cpp/...>
```

否则会造成大量无意义 diff。

### 风险 2：public 层暴露太多内部类型

拆分时先接受少量泄露，但建立规则：

- 新 public API 不再暴露内部实现类型。
- 老 API 后续逐步封装。
- Python/Excel 只能绑定 public header。

### 风险 3：CMake install/export 复杂

先实现 monorepo build，再实现 install/export。

推荐顺序：

```text
add_subdirectory 可用
↓
target_link_libraries 可用
↓
install 可用
↓
find_package 可用
```

### 风险 4：Python/Excel 直接依赖 core

在 CI 中加检查：

```bash
rg '#include\s*[<"]dal/' dal-python
rg '#include\s*[<"]dal/' dal-excel
rg '%include\s*["<]dal/' dal-python/swig
```

原则上 `dal-python` 和 `dal-excel` 应只 include：

```cpp
#include <dal_public/...>
```

### 风险 5：过早替换顶层 CMake

当前顶层 CMake 承载三方库、AAD backend、Office 探测、旧目录构建和测试入口。Phase 1 只能新增默认关闭的新 workspace 入口，不能直接替换旧入口。

### 风险 6：codegen 输入归属不清

当前 `config/dal.ifc` 同时生成 `dal/auto` 和 `public/auto`。第一轮迁移应保留明确 source of truth，并把生成输出显式路由到 `dal-cpp` 和 `dal-public`，不要让 `dal-public` 隐式读取 `dal-cpp/config`。

### 风险 7：CTest 通过但实际没有运行测试

当前计划使用 `ctest` 作为验收入口，因此每个 C++ gtest target 必须通过 `add_test()` 或 `gtest_discover_tests()` 注册。`dal-python` 可以通过 CTest 调用 pytest，但 pytest 仍是 Python 测试框架。

---

## 十一、建议里程碑

### Milestone 1：核心独立化

目标：

```text
dal-cpp 可以独立构建、测试、安装
```

### Milestone 2：公共 API 独立化

目标：

```text
dal-public 可以依赖 dal-cpp 独立构建、测试、安装
```

### Milestone 3：Python binding 独立化

目标：

```text
dal-python 可以依赖 dal-public 构建并通过 pytest
```

### Milestone 4：Excel 独立化

目标：

```text
dal-excel 可以依赖 dal-public 在 Windows 构建
```

### Milestone 5：删除旧结构

目标：

```text
旧 public/、旧 CMake、旧 build script 被清理
新结构成为唯一构建方式
```

---

## 十二、建议第一步

不要马上移动所有文件。第一步建议先做一个“边界确认 PR”：

1. 在当前仓库中创建 4 个目标目录。
2. 加入默认关闭的顶层 refactored workspace CMake 选项。
3. 不删除旧构建。
4. 补齐当前 gtest target 到 CTest 的注册，确保 `ctest` 不是空跑。
5. 记录 codegen source of truth 和 public / Python / Excel 的 core include 清单。
6. 只迁移最少量 CMake scaffolding。
7. 确认新旧构建可以共存。

第二个 PR 再正式迁移 `dal-cpp`。这样风险最低，也方便 review。
