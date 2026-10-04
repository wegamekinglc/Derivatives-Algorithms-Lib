# DAL 自然语言产品与脚本问答集

[qa.jsonl](qa.jsonl) 包含 7000 个问答对，使用 UTF-8 JSONL：每个物理行是一个完整 JSON 对象。`question` 是中文自然语言产品描述，`answer` 是 Markdown 两列事件表。表格中的换行使用 `<br>`；JSON 字符串中的换行使用标准 `\n` 转义。

[index.md](index.md) 是完整的逐条分类总表，包含 7000 条记录的 ID、JSONL 行号、分类、产品类型和主要功能点。每条索引引用原始 JSONL 数据，不另存一份问答正文。

## 内容与字段

共 70 种产品结构，每种结构 100 个不同条款组合。分类数量如下。

| 分类        | 数量   |
|-----------|------|
| 欧式与收益结构   | 2000 |
| 路径依赖与多点观察 | 2400 |
| 支付与历史     | 200  |
| 向量与日程     | 600  |
| 提前行权      | 600  |
| 利率        | 1000 |
| 多资产与混合    | 200  |

覆盖看涨与看跌、现金与资产数字支付、远期与价差、算术和几何平均亚式、离散回望、障碍、自动赎回、记忆票息、逐期收益、离散方差与波动率、多资产篮子、利率票据、上限与下限、互换及现金结算互换期权。

脚本功能包含数值常量、文本宏及依赖宏、不可变数值向量、可变向量、零基索引、稀疏索引赋值、自动补零、APPEND、SUM、AVERAGE、MIN、MAX、FOR、嵌套循环、常量循环边界、条件分支与嵌套条件、比较运算、AND、OR、LOG、EXP、SQRT、乘方、一元运算、DCF、日程展开、PeriodBegin/PeriodEnd、期初与期末事件、PAYS、多次支付、ON 延迟结算、命名 FIX、显式观察日期、历史定盘、默认索引绑定 SPOT、EXERCISE、回归状态和比较平滑参数。功能覆盖计数见索引。

每条 JSON 对象还包含以下复现信息。

| 字段                        | 含义                        |
|---------------------------|---------------------------|
| `id`                      | 稳定记录 ID，与索引及验证记录对应        |
| `category`                | 分类                        |
| `product_type` / `family` | 中文产品类型及生成模板标识             |
| `features`                | 该记录涉及的主要脚本功能              |
| `events`                  | 与答案表格逐行一致的结构化日期和脚本，方便程序读取 |
| `contract_terms`          | 独立收益参考计算所用的合同参数           |
| `product_settings`        | 产品默认索引和提前行权回归特征等设置        |
| `pricing_context`         | 估值日、模型输入、历史定盘和建议 MC 设置    |
| `limitations`             | 该产品需要明确的约定或近似             |
| `contract_fingerprint`    | 模板与合同参数的 SHA-256 指纹       |

答案表格是 DAL 产品事件表。非日期行定义常量、向量或宏；日程行可展开为多个事件。全部定义位于事件前。当前价格、波动率、曲线和相关系数由模型提供，不能把事件日价格替换成当前价格常数。

定价时需要同时读取 `product_settings` 和 `pricing_context`。单股票使用 BS；双股票使用相关 BS；美元利率使用 GSR；股票与利率混合产品使用 Hybrid。GSR 配置使用估值日与曲线截止日两个节点，平坦连续复利输入利率生成两节点的对数折现因子，且 g/H 在估值日分别取记录给出的常量。未来利率观察没有另设投影曲线，按 DAL 规则使用 OIS 曲线。

## 验证结果

全量验证通过 7000/7000 条，失败为零。每条答案表格都被重新读取，转换成 DAL 产品，并由实际 MC 引擎定价。

- 随机模型定价：每条用相同的 4096 条 Sobol 路径分别运行解释模式和编译模式，共 14000 次；要求 PV 有限且两种模式一致。
- 独立收益验证：每条在三个零波动率场景下运行 MC，与 [reference.py](reference.py) 按合同参数计算的独立参考值比较，共 21000 次。场景覆盖向下和向上变化的现价、正利率及负利率；历史定盘和合同初始参考价保持固定。
- AAD 验证：每种产品取第 1、50、100 条，分别运行解释和编译模式，共 420 次；检查 PV 和全部导数有限，并比较两种模式的 PV。
- 去重：ID、问题正文、答案表格、合同参数指纹均唯一。另使用 DAL 自身解析后的 AST 去除宏、常量名称、变量名称、源位置等表示差异，并把默认绑定 SPOT 与同名 FIX 归一化，7000 个规范化合同指纹均唯一。

上述去重是文本、合同参数和规范化语法层面的检查；一般代数意义上的支付函数等价不由 AST 指纹判定。此数据使用不同日期、行权价、障碍、观察次数、现金支付和名义本金等真实合同差异生成，未以替换变量名或改写措辞来扩充条数。

逐条结果在 [validation.jsonl](validation.jsonl)，包含两种模式的 PV、三个参考场景的结果、展开后事件数、AAD 检查和规范化合同指纹。[validation_summary.json](validation_summary.json) 保存总计、实际引擎源码提交和原始 JSONL 的 SHA-256，可用于确认数据与验证报告一致。

这里的随机 PV 用于检验脚本能够执行和两种模式一致，不能作为具有收敛误差保证的市场报价。正式估值可提高路径数并检查收敛。美式类型明确采用 60–99 个每日行权日期的有限网格近似；连续时间美式行权需要网格收敛检验。障碍和回望产品仅观察问题中列出的日期。

## 复现

所有命令在独立 worktree 根目录执行。生成器固定参数顺序和数值格式，可以重建相同的原始 JSONL；无外部市场数据依赖。

```bash
.venv/bin/python datasets/dal_script_qa_7000/generate.py
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=2 .venv/bin/python datasets/dal_script_qa_7000/validate.py \
    --paths 4096 --workers 4 --source-revision 3fe44ecd0969a159bd9172be3f8813ef30f00a04 > build/qa_validation.log 2>&1
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=2 .venv/bin/python -m pytest datasets/dal_script_qa_7000/test_dataset.py -q > build/qa_dataset_tests.log 2>&1
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=2 .venv/bin/python -m pytest \
    dal-python/tests/test_script.py dal-python/tests/test_fix_valuation.py \
    dal-python/tests/test_exercise_valuation.py dal-python/tests/test_models.py \
    -q > build/qa_existing_tests.log 2>&1
.venv/bin/python datasets/dal_script_qa_7000/verify_artifacts.py
```

单条定价使用 [price_one.py](price_one.py)，读取数据自身的建议设置，默认 16384 条定价路径和 8192 条 LSMC 训练路径。

```bash
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=2 .venv/bin/python datasets/dal_script_qa_7000/price_one.py --id DAL-QA-05201
PYTHONPATH=build/Release-linux/dal-python DAL_NUM_THREADS=2 .venv/bin/python datasets/dal_script_qa_7000/price_one.py --id DAL-QA-06601 --aad
```

Python 接口按仓库的 CMake 方式编译，环境为 CPython 3.13、pybind11 3.1.0、GCC 15.2、C++17、Release、原生 AAD。已在独立 worktree 中执行以下配置与构建。

`--source-revision` 必须填写编译所加载原生扩展时使用的引擎源码提交。上面的值是本数据报告的实际引擎版本；使用其他版本重建扩展后，请填写对应的完整 Git 提交 ID。验证器不启动 Git 子进程，也不把当前数据文件提交自动视为扩展的源码版本。

```bash
cmake --preset=Release-linux -S . -B build/Release-linux \
    -DDAL_BUILD_PYTHON=ON -DDAL_CPP_BUILD_BENCHMARKS=OFF \
    -DDAL_CPP_BUILD_EXAMPLES=OFF \
    -DPython3_EXECUTABLE="$PWD/.venv/bin/python" \
    -Dpybind11_DIR="$PWD/.venv/lib/python3.13/site-packages/pybind11/share/cmake/pybind11"
cmake --build build/Release-linux --target _dal -j 12
```

另外执行了仓库现有脚本、FIX 定价、提前行权和模型测试，101 个测试通过。构建与日志均在 worktree 的 `build/` 中。

[verify_artifacts.py](verify_artifacts.py) 检查数据、逐条验证结果、索引分类和链接及上述测试日志，并重新计算三个确定性场景的参考支付及记录中的误差，拒绝非有限值、不一致的结果或不完整的验证统计，生成 [manifest.json](manifest.json) 中的文件大小和 SHA-256。修改数据或重新验证后，应按顺序运行测试和此检查，以更新清单。17 个数据测试涵盖全量重生成一致性、重复问题、答案损坏、错误表格分隔行、源码版本格式、参考结果篡改、合同归一化和计息日历。
