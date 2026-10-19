"""Linux control-flow isolation of generated wrappers; not a Windows ABI test."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def block(path, token, semicolon=False):
    text = path.read_text()
    start = text.index(token)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end + int(semicolon)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path.cwd()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    excel = root / 'dal-excel/src'
    fields = {
        'MarketIvMarket_New': ['evaluation_date', 'spot', 'terms'],
        'MarketIvFlatMarket_New': ['evaluation_date', 'spot', 'rate', 'dividend_yield', 'expiries'],
        'MarketIvFitRequest_New': ['quotes'],
        'MarketIvQuery_New': ['strike', 'time'],
        'MarketIvDupireRequest_New': ['spots', 'times'],
        'MarketIvRefitRiskRequest_New': ['quote_ids', 'outputs', 'steps'],
    }
    order = {}
    for name, names in fields.items():
        text = (root / f'dal-excel/auto/MG_{name}_public.inc').read_text()
        first = min(text.index('Excel::' + conversion) for conversion in ('ToDouble(', 'ToCellMatrix(', 'ToHandle<')
                    if 'Excel::' + conversion in text)
        order[name] = all(text.index(f'normalized_{field}.Get()') < first for field in names)

    (output / 'Windows.h').write_text('#pragma once\n#include <cstdint>\nusing DWORD_PTR = std::uintptr_t;\n'
                                    '#define __nullterminated\n#define __stdcall\n#define _stdcall\n#define _cdecl\n#define pascal\n')
    prefix = r'''
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#define _WIN32
#define __declspec(x)
#include <dal-excel/src/_xlcall.hpp>
#define REQUIRE(c, m) do { if (!(c)) throw std::runtime_error(m); } while (false)
#define THROW(m) throw std::runtime_error(m)
#define ENV_SEED_TYPE(x) int _env = 0
namespace Dal {
using OPER_ = xloper12;
using String_ = std::string;
using std::pair;
template<class T> using Vector_ = std::vector<T>;
template<class T> using Handle_ = std::shared_ptr<T>;
using Cell_ = double;
template<class T = double> struct Matrix_ {
    std::vector<T> data; int columns = 0;
    void Resize(int r, int c) { data.resize(static_cast<size_t>(r)*c); columns=c; }
    T& operator()(int r, int c) { return data[r*columns+c]; }
};
struct StorableMarketIvQuery_ {};
struct StorableMarketIvDupireSettings_ {};
struct StorableMarketIvDupireRequest_ {};
std::string lastError;
int factories = 0;
namespace Logging { void Write(const char*) {} }
namespace Excel {
void InitializeSessionIfNeeded() {}
String_ ToString(const OPER_*) { return "probe"; }
double ToDouble(const OPER_*);
template<class T> Handle_<T> ToHandle(int, const OPER_*, bool) { return {}; }
struct Retval_ {
    template<class T> void Load(int, const T&) {}
    OPER_* ToXloper() { static OPER_ result{}; return &result; }
};
OPER_* Error(const char* message, const char*) { lastError=message; static OPER_ result{}; return &result; }
template<class... T> void Register(T...) {}
'''
    functions = [
        block(excel / '__scriptinput.hpp', 'inline String_ ScriptSettingLocation'),
        block(excel / '__scriptinput.hpp', 'class ScriptSettingsInput_', True),
        block(excel / '__scriptinput.hpp', 'inline const OPER_* ScriptScalarInput'),
        block(excel / '__scriptinput.hpp', 'inline bool ScriptInputBlank'),
        block(excel / '__dupireinput.hpp', 'inline void ValidateNativeTextInput'),
        block(excel / '__marketivinput.hpp', 'inline bool ValidMarketIvArray'),
        block(excel / '__marketivinput.hpp', 'inline void ValidateMarketIvNumber'),
        block(excel / '__marketivinput.hpp', 'inline void ValidateMarketIvRange'),
    ]
    converters = '\n} // namespace Excel\n' + block(excel / '_excel.cpp', 'double Excel::ToDouble(const OPER_* src)')
    converters += r'''
pair<int,int> NonblankSize(const OPER_*) { throw std::runtime_error("unexpected nongreedy path"); }
'''
    converters += 'template<class E_>\n' + block(excel / '_excel.cpp', 'Matrix_<E_> ToMatrix(')
    converters += r'''
Cell_ CellMatrixEntry(const OPER_* src) { REQUIRE(src->xltype == xltypeNum, "cell not normalized"); return src->val.num; }
namespace Excel {
Matrix_<Cell_> ToCellMatrix(const OPER_* src, bool optional, bool greedy) {
    return ToMatrix<Cell_>(src, optional, greedy, CellMatrixEntry);
}
}
void MarketIvQuery_New(const String_&, double k, double t, Handle_<StorableMarketIvQuery_>*) {
    REQUIRE(k == 100. && t == 1., "wrong scalar values"); ++factories;
}
void MarketIvDupireRequest_New(const String_&, const Matrix_<Cell_>& k, const Matrix_<Cell_>& t,
                             const Handle_<StorableMarketIvDupireSettings_>&, Handle_<StorableMarketIvDupireRequest_>*) {
    REQUIRE(k.data == std::vector<double>{100.} && t.data == std::vector<double>{1.}, "wrong range values"); ++factories;
}
#include <dal-excel/auto/MG_MarketIvQuery_New_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireRequest_New_public.inc>
} // namespace Dal
int main(int argc, char** argv) {
    using namespace Dal;
    REQUIRE(argc == 2, "mode required");
    std::string mode=argv[1];
    OPER_ name{}, bad{}, k{}, t{}, settings{};
    name.xltype=settings.xltype=xltypeNil;
    k.xltype=t.xltype=xltypeNum; k.val.num=100.; t.val.num=1.;
    bad.xltype=xltypeMulti; bad.val.array.rows=bad.val.array.columns=1; bad.val.array.lparray=nullptr;
    if (mode == "bool") { bad.xltype=xltypeBool; bad.val.xbool=1; }
    bool range=mode == "range-null" || mode == "range-int";
    if (mode == "scalar-int" || mode == "range-int") { bad.xltype=xltypeInt; bad.val.w=100; }
    if (range) xl_MarketIvDupireRequest_New(&name,&bad,&t,&settings);
    else xl_MarketIvQuery_New(&name,&bad,&t);
    std::cout << "factories=" << factories << "; error=" << lastError << '\n';
    bool positive=mode == "scalar-int" || mode == "range-int";
    if (positive) return !(lastError.empty() && factories == 1);
    auto field=range ? "spots" : "strike";
    auto reason=mode == "bool" ? "excluding bool" : "array dimensions/storage";
    return !(factories == 0 && lastError.find(field) != std::string::npos && lastError.find(reason) != std::string::npos);
}
'''
    source = output / 'wrapper-probe.cpp'
    source.write_text(prefix + '\n'.join(functions) + converters)
    binary = output / 'wrapper-probe'
    command = ['c++', '-std=c++17', '-O0', '-g', '-I', str(root), '-I', str(output), str(source), '-o', str(binary)]
    subprocess.run(command, check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    results = {}
    for mode in ('bool', 'scalar-null', 'range-null', 'scalar-int', 'range-int'):
        run = subprocess.run([str(binary), mode], capture_output=True, text=True)
        results[mode] = {'exit': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr}
    report = {'scope': __doc__, 'constructor_order': order, 'isolated_wrappers': results, 'compile_command': command,
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return int(not (all(order.values()) and all(run['exit'] == 0 for run in results.values())))


if __name__ == '__main__':
    raise SystemExit(main())
