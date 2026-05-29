# DAL 项目拆分重构原始需求

## 背景

需要将当前 DAL 项目进行结构性重构，把现有代码拆分为多个职责清晰、可独立构建和测试的项目。

## 原始需求

1. 将整个项目拆分成 4 个项目。
2. `dal-cpp`：DAL 的核心项目，提供最丰富的 API 接口，并且是其他项目的基础。
3. `dal-public`：DAL 的 C++ 形式对外接口项目，维护所有 DAL 项目希望对外提供的接口内容。
4. `dal-python`：DAL 的 Python 接口，接口内容来自 `dal-public` 提供的部分，并通过 SWIG 构造 Python binding。
5. `dal-excel`：DAL 的 Excel 接口，接口内容来自 `dal-public` 提供的部分。
6. 所有项目都应该提供测试用例。
7. C++ 测试框架使用 GoogleTest（gtest）。
8. `dal-python` 的 Python 测试框架使用 pytest。
9. 每个项目单独占用一个项目文件夹。
10. C++ 项目使用 CMake 作为编译管理。

## 目标依赖关系

```text
dal-cpp
  ↑
dal-public
  ↑        ↑
dal-python dal-excel
```

## 核心原则

- `dal-cpp` 负责核心能力和完整 C++ API。
- `dal-public` 负责稳定、受控的对外 C++ API。
- `dal-python` 和 `dal-excel` 只通过 `dal-public` 暴露能力。
- 所有项目都应可以独立构建、测试和维护。
