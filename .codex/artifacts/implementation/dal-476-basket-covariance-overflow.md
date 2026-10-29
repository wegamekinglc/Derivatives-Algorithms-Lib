# DAL-476：篮子协方差范围修复实现记录

控制依据为同名 spec、api-notes、critique（A1–A6、R1–R4）及 `validation/dal-476-bug-scan.md`。基线 `1bb6cbd5e5aea332471ad2f05cdfeb64681efd5e`；复用协调器 checkout，原地重命名为 `fix/dal-476-basket-covariance-overflow`。原有未跟踪控制文档和扫描证据均保留，不纳入本阶段提交。本阶段仅本地提交，不推送、建 PR 或调度其他角色；Issue 保持 `in_progress`。

## 设计与范围

唯一生产行为变更位于 `dal-cpp/dal/script/xccymcbasketstatistics.cpp` 的 `OutputCovariance`：先沿用 `Influences` 计算及其原始有限性检查；将局部 influence 缓冲各元素除以 `sqrt(n*(n-1))`，再累积完整有符号 Gram 矩阵，最后沿用输出有限性检查。批次数先转为 double，再乘分母。全部计算使用 double，不依赖较宽的 long double。

精确算术下，缩放后的对角是非负平方和，任何部分和不大于最终对角；交叉项绝对值和按 Cauchy–Schwarz 不超过两对角几何均值。因此在本次要求的有限完整对角范围内，乘积和部分和无须经过未归一化的大值。此依据不承诺任意下溢、严重抵消或贴近最大浮点数的精确边界判定。

保留系数统计、Jacobian、轴、成员顺序、RNG、AAD、上游检查和 `JointStatistics` 接线。输出统计新增一次 `O(n*p)` 原地局部缩放；投影 `O(n*p*d)`、Gram `O(n*p*p)` 与 `O(n*p+p*p)` 存储量级不变。`xccymcbasket_internal.hpp` 仅新增 `TestAccess_::OutputCovariance` 静态转发，调用与生产相同的私有函数，无新公开接口、导出、绑定、故障状态或输入契约。

静态核对 `PreparedXccyMcBasket_::Impl_::Run/Own`：成员完成后清除失败成员和游标，`JointStatistics` 成功后才设 `basketComplete_=true`；统计异常经原 `CalibrationError_` catch 和 `Own` 包装，保留 report/candidate/partial owner。上述代码未修改。A5 算术拒绝与既有失败控制联合验证，不宣称动态覆盖所有成员完成后的联合统计失败生命周期。

## 正式回归

所有新增用例归入现有 `dal-cpp/tests/script/test_xccymcbasket_statistics.cpp`，链接真实生产库，不包含生产 `.cpp`。

- A1：四批 `±1e154`，J=[1]；期望按有限 `a*a` 归一化为 1/3，另确认有限系数协方差的范围前提。
- A3/R1：普通及大尺度，J=[1,-0.5,0] 与 [2,-1,0]；逐元素闭式 oracle、相对误差、符号、对称性、精确零项。R1 的原始平方及负交叉乘积超范围，完整结果仍有限。
- A2：真实公开 core `Evaluate()`；单成员 Price、spread=1、8批×3路径，本金从110缩放至 `110*2^507`。oracle 采用 `(C_small*scale)*scale`，避免构造溢出的未归一化和；检查 Estimate 完成、有限成员统计、无导数占位及消费计数。
- R3/A4：从完整原始批次行独立重建系数均值协方差，再逐元素计算 `J*C_mean*J^T`。多成员 Price/BasisSpread、3个请求AAD轴、4批和最小2批；也应用于重复经济行、全确定性、零动力和混合零块。保留全部原 SE/系数断言及容差。既有 long double oracle 仅用于普通尺度，所有大尺度 oracle 均使用范围安全 double。
- A5/R2：J=[3] 真输出溢出仍拒绝；有限输入形成非有限原始 influence、Inf、NaN 均保留独立 influence 诊断。验证 code、field 和 expected。
- A6：大尺度正式测试无平台 skip，并记录 double/long double 的 `max_exponent`。平台实际执行结果另列，不将 Linux 结果视作 Windows 验收。

## 构建环境与命令

Linux，GCC 15.2.0，CMake 4.2.3，C++17，Release，原生 AAD，`DAL_ENABLE_NATIVE_ARCH=OFF`，静态库，构建并行度8。所有 submodule 均按仓库固定 SHA 初始化，未更新指针。

环境准备：最初 Ninja 配置因本机未安装 Ninja 失败；早期 Make 配置因依赖尚未初始化失败。`git submodule update --init --recursive` 首次 Eigen 克隆收到 GitLab 服务负载错误，自动重试后成功。这些配置/依赖失败不计 RED。

```bash
cmake --preset=Release-linux -S . -B build/dal-476-implementer-make -G 'Unix Makefiles'
cmake --build build/dal-476-implementer-make --target dal_cpp_tests -j8
```

配置与完整 `dal_cpp_tests` 编译/链接成功（exit 0）。首次构建有29条原有测试的警告，无本次变更文件的警告；未修改范围外文件。

## RED

此阶段生产文件仅增加转发接缝，`OutputCovariance` 算法未变。

```bash
set -o pipefail
./build/dal-476-implementer-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='XccyMcBasketStatisticsTest.*' \
  --gtest_output=xml:build/dal-476-implementer-make/red.xml \
  | tee build/dal-476-implementer-make/red.log
```

实际 exit 1：12项中9项通过，3项失败：`TestFiniteLargeCovarianceBeforeNormalization`、`TestSignedAndZeroCovarianceBeforeProductsOverflow`、`TestPublicLargeNotionalCovarianceMatchesHomogeneousScaling`。三者均为运行期 `CalibrationNumericalFailure: /execution/covariance; expected finite output covariance`，不是编译或链接失败。R1 用例先检查普通尺度，再在大尺度 J=[2,-1,0] 首次失败，能直接约束乘法前缩放。独立完整矩阵 oracle、真溢出及非有限 influence 控制已在旧算法上通过。

## 性能 case 选择

调用链为 `OutputCovariance <- JointStatistics <- PreparedXccyMcBasket_::Evaluate`，facade、Python、Excel 委托该 core。扫描 `dal-cpp/benchmarks` 未发现篮子 MC 或该统计路径的调用；九项 paired gate 均不在此调用闭包，`xccy_perf` 的 basket 是原生 rate-pricing 篮子，`script_mc_perf` 也不调用该 prepared basket。因此不选择无关 benchmark，不新增任意计时阈值或改变既有采样/噪声规则。

选择实际调用此路径的 facade 示例 `xccy_script_monte_carlo_basket` 作信息性 smoke：原样保留64批×4路径、None/AAD和不同成员映射，重建后运行并保留原程序的 informational timing。另由正式统计回归覆盖单/多成员、None/3轴AAD、2/4/8批及大尺度，现有 all-native 测试覆盖更宽输出。时间仅为观察，不声称 paired 性能通过；无额外路径或录制由 diff 和消费计数验证。

## 交接边界

本阶段运行必要定向验证；完整本地矩阵、额外工具链和 Windows MSVC、Python/Excel运行时、最终 CI、独立 tester/reviewer/doc-writer 仍由协调器后续串行推进。不得将本记录解释为完整矩阵或跨平台门禁通过。
