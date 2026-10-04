from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import Counter
from datetime import date, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EVALUATION_DATE = date(2026, 10, 4)

FAMILIES = [
    ("european_call", "欧式看涨", "欧式与收益结构"),
    ("european_put", "欧式看跌", "欧式与收益结构"),
    ("cash_digital_call", "现金数字看涨", "欧式与收益结构"),
    ("cash_digital_put", "现金数字看跌", "欧式与收益结构"),
    ("asset_digital_call", "资产数字看涨", "欧式与收益结构"),
    ("asset_digital_put", "资产数字看跌", "欧式与收益结构"),
    ("forward_long", "远期多头", "欧式与收益结构"),
    ("forward_short", "远期空头", "欧式与收益结构"),
    ("bull_call_spread", "牛市看涨价差", "欧式与收益结构"),
    ("bear_put_spread", "熊市看跌价差", "欧式与收益结构"),
    ("straddle", "跨式多头", "欧式与收益结构"),
    ("strangle", "宽跨式多头", "欧式与收益结构"),
    ("butterfly", "看涨蝶式", "欧式与收益结构"),
    ("call_ratio_spread", "看涨比例价差", "欧式与收益结构"),
    ("capped_call", "封顶看涨", "欧式与收益结构"),
    ("interval_cash", "区间数字支付", "欧式与收益结构"),
    ("tiered_cash", "分档数字支付", "欧式与收益结构"),
    ("power_call", "平方价格看涨", "欧式与收益结构"),
    ("log_contract", "对数收益合约", "欧式与收益结构"),
    ("geometric_return_call", "复利收益看涨", "欧式与收益结构"),
    ("arithmetic_asian_call", "算术平均亚式看涨", "路径依赖与多点观察"),
    ("arithmetic_asian_put", "算术平均亚式看跌", "路径依赖与多点观察"),
    ("geometric_asian_call", "几何平均亚式看涨", "路径依赖与多点观察"),
    ("weighted_asian", "加权平均亚式看涨", "路径依赖与多点观察"),
    ("floating_asian_call", "浮动行权价亚式看涨", "路径依赖与多点观察"),
    ("lookback_fixed_call", "固定行权价回望看涨", "路径依赖与多点观察"),
    ("lookback_fixed_put", "固定行权价回望看跌", "路径依赖与多点观察"),
    ("lookback_float_call", "浮动行权价回望看涨", "路径依赖与多点观察"),
    ("range_option", "观察价格极差合约", "路径依赖与多点观察"),
    ("cliquet", "逐期收益累积", "路径依赖与多点观察"),
    ("capped_cliquet", "逐期封顶保底收益", "路径依赖与多点观察"),
    ("realized_variance", "离散已实现方差", "路径依赖与多点观察"),
    ("realized_volatility", "离散已实现波动率", "路径依赖与多点观察"),
    ("up_out_call", "向上敲出看涨", "路径依赖与多点观察"),
    ("down_out_put", "向下敲出看跌", "路径依赖与多点观察"),
    ("up_in_call", "向上敲入看涨", "路径依赖与多点观察"),
    ("down_in_put", "向下敲入看跌", "路径依赖与多点观察"),
    ("double_out_call", "双障碍敲出看涨", "路径依赖与多点观察"),
    ("double_in_put", "双障碍敲入看跌", "路径依赖与多点观察"),
    ("autocall", "自动赎回票据", "路径依赖与多点观察"),
    ("memory_coupon", "记忆票息票据", "路径依赖与多点观察"),
    ("corridor_coupon", "股票区间票息", "路径依赖与多点观察"),
    ("equity_linked_note", "股票挂钩本金票据", "路径依赖与多点观察"),
    ("forward_start_call", "远期起始看涨", "路径依赖与多点观察"),
    ("delayed_settlement_call", "延迟结算看涨", "支付与历史"),
    ("historical_fix_call", "历史观察与未来支付", "支付与历史"),
    ("weighted_option_strip", "向量加权期权组合", "向量与日程"),
    ("nested_loop_portfolio", "嵌套循环期权组合", "向量与日程"),
    ("sparse_vector_basket", "稀疏向量价格组合", "向量与日程"),
    ("conditional_vector", "条件向量平均价格", "向量与日程"),
    ("schedule_asian", "日程展开亚式看涨", "向量与日程"),
    ("schedule_fixed_coupon", "日程展开固定票息", "向量与日程"),
    ("bermudan_put", "百慕大看跌", "提前行权"),
    ("bermudan_call", "百慕大看涨", "提前行权"),
    ("bermudan_asian_put", "百慕大亚式看跌", "提前行权"),
    ("conditional_bermudan_put", "附条件百慕大看跌", "提前行权"),
    ("american_put_daily", "美式看跌每日网格近似", "提前行权"),
    ("american_call_daily", "美式看涨每日网格近似", "提前行权"),
    ("rate_fixed_bond", "固定票息债券", "利率"),
    ("floating_note", "浮动利率票据", "利率"),
    ("caplet", "单期利率上限", "利率"),
    ("floorlet", "单期利率下限", "利率"),
    ("cap", "多期利率上限", "利率"),
    ("rate_corridor", "利率区间票息", "利率"),
    ("payer_swap", "支付固定利率互换", "利率"),
    ("european_payer_swaption", "欧式现金结算互换期权", "利率"),
    ("bermudan_bond_call", "百慕大零息债券看涨", "利率"),
    ("rate_vector_average", "平均利率看涨", "利率"),
    ("multi_asset_basket", "双股票篮子看涨", "多资产与混合"),
    ("hybrid_equity_rate", "股票利率混合支付", "多资产与混合"),
]


def number(value):
    return format(round(value, 10), ".10f").rstrip("0").rstrip(".")


def chinese_date(value):
    value = date.fromisoformat(value) if isinstance(value, str) else value
    return f"{value.year}年{value.month}月{value.day}日"


def markdown_table(headers, rows):
    rows = [[str(cell).replace("\n", "<br>") for cell in row] for row in rows]
    widths = [max(len(headers[i]), *(len(row[i]) for row in rows)) for i in range(len(headers))]
    def line(row):
        return "| " + " | ".join(cell.ljust(width) for cell, width in zip(row, widths)) + " |"
    return "\n".join([line(headers), "|" + "|".join("-" * (width + 2) for width in widths) + "|", *(line(row) for row in rows)])


def parse_answer(answer):
    lines = answer.splitlines()
    if len(lines) < 3 or [cell.strip() for cell in lines[0].split("|")[1:-1]] != ["日期 / 定义", "DAL script / 数值"]:
        raise ValueError("Answer must be a DAL event Markdown table")
    result = []
    for line in lines[2:]:
        cells = [cell.strip() for cell in line.split("|")[1:-1]]
        if len(cells) != 2 or any(not cell.startswith("`") or not cell.endswith("`") for cell in cells):
            raise ValueError("Malformed event table row")
        result.append({"date_or_definition": cells[0][1:-1].replace("<br>", "\n"), "script": cells[1][1:-1].replace("<br>", "\n")})
    return result


def digest(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def build_record(family_index, variant):
    family, label, category = FAMILIES[family_index]
    s0 = round(80 + 1.75 * variant, 2)
    strike = round(s0 * (0.82 + 0.0045 * variant), 4)
    quantity = 1 + 0.25 * (variant % 8)
    # Different families use disjoint date residues, so contract uniqueness is independent of wording.
    days = 120 + 7 * variant + family_index % 7
    maturity = EVALUATION_DATE + timedelta(days=days)
    if family_index == 0 and variant == 0:
        s0, strike, maturity = 200.0, 210.0, date(2027, 1, 30)
        days = (maturity - EVALUATION_DATE).days
    count = 3 + variant % 6
    observations = [EVALUATION_DATE + timedelta(days=days * (i + 1) // count) for i in range(count)]
    coupon = round(1 + 0.05 * variant, 4)
    spread = round(s0 * (0.10 + 0.001 * (variant % 9)), 4)
    lower, upper = round(s0 * 0.70, 4), round(s0 * 1.30, 4)
    rate_strike = round(0.012 + 0.00035 * variant, 6)
    notional = 10000 + 1250 * variant
    n = number
    obs = "FIX(EQ[ABC])"
    rows = []
    features = {"FIX", "PAYS"}
    settings = {}
    model = {"type": "black_scholes", "spot": s0, "vol": 0.15 + 0.01 * (variant % 12), "rate": 0.025, "div": 0.01}
    terms = {"quantity": quantity, "initial_price": s0, "strike": strike, "maturity": maturity.isoformat()}
    description = ""
    notes = []
    if (family_index < 58 or family == "hybrid_equity_rate") and variant % 5 == 4:
        obs = "SPOT()"
        settings["default_index"] = "EQ[ABC]"
        features.add("default_index_binding")

    def definition(name, value):
        rows.append({"date_or_definition": name, "script": n(value) if isinstance(value, (int, float)) else value})
        features.add("numeric_constants" if isinstance(value, (int, float)) else "constant_vectors" if value.startswith("[") else "text_macros")

    def event(dt, script):
        rows.append({"date_or_definition": dt.isoformat() if isinstance(dt, date) else dt, "script": script})

    def cash(expression):
        event(maturity, f"payoff PAYS {n(quantity)} * ({expression})")

    def observe(text):
        terms["observation_dates"] = [d.isoformat() for d in observations]
        features.add("multiple_observations")
        return text + "观察日期依次为" + "、".join(chinese_date(d) for d in observations) + "，最后一个观察日结算。"

    def vector_events(append, payment):
        for i, dt in enumerate(observations):
            event(dt, append + ("\n" + payment if i == count - 1 else ""))
        features.update(["mutable_vectors", "APPEND"])

    if family_index < 20:
        definition("STRIKE", strike)
        definition("OBS", obs)
        call, put = "MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)"
        features.add("MAX")
        if family == "european_call":
            definition("CALLPAY", "MAX(OBS - STRIKE, 0)")
            cash("CALLPAY")
            description = f"欧式看涨期权，行权价为{n(strike)}，到期支付股价超过行权价的部分。"
        elif family == "european_put":
            cash(put)
            description = f"欧式看跌期权，行权价为{n(strike)}，到期支付行权价超过股价的部分。"
        elif family.startswith("cash_digital") or family.startswith("asset_digital"):
            up = family.endswith("call")
            amount = n(coupon) if family.startswith("cash") else "OBS"
            features.update(["IF", "ELSE", ">" if up else "<"])
            terms["digital_cash"] = coupon
            smoothing = ":0.2" if variant % 2 else ""
            if smoothing:
                features.add("comparison_smoothing")
            event(maturity, f"IF OBS {'>' if up else '<'} STRIKE{smoothing} THEN\n    payoff PAYS {n(quantity)} * {amount}\nELSE\n    payoff PAYS 0\nEND")
            description = f"{'现金' if family.startswith('cash') else '资产'}数字{'看涨' if up else '看跌'}期权，股价严格{'高于' if up else '低于'}{n(strike)}时，每份支付{'现金' + n(coupon) if family.startswith('cash') else '当日股价对应的现金'}，其他情况支付零。"
        elif family in ("forward_long", "forward_short"):
            cash("+(OBS - STRIKE)" if family.endswith("long") else "-(OBS - STRIKE)")
            features.add("unary_operators")
            description = f"交割价为{n(strike)}的远期{'多头' if family.endswith('long') else '空头'}，每份现金结算金额为{'股价减交割价' if family.endswith('long') else '交割价减股价'}，负值表示持有人付款。"
        elif family in ("bull_call_spread", "bear_put_spread", "strangle", "butterfly", "call_ratio_spread"):
            terms["width"] = spread
            definition("WIDTH", spread)
            expressions = {
                "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
                "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
                "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
                "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
                "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
            }
            descriptions = {
                "bull_call_spread": f"牛市看涨价差，买入行权价{n(strike)}的看涨期权，卖出行权价{n(strike + spread)}的看涨期权。",
                "bear_put_spread": f"熊市看跌价差，买入行权价{n(strike + spread)}的看跌期权，卖出行权价{n(strike)}的看跌期权。",
                "strangle": f"宽跨式多头，买入行权价{n(strike)}的看跌期权和行权价{n(strike + spread)}的看涨期权。",
                "butterfly": f"看涨蝶式组合：买入一份行权价{n(strike)}的看涨、卖出两份行权价{n(strike + spread)}的看涨、买入一份行权价{n(strike + 2 * spread)}的看涨。",
                "call_ratio_spread": f"看涨比例价差：买入一份行权价{n(strike)}的看涨、卖出两份行权价{n(strike + spread)}的看涨，净支付可为负。",
            }
            cash(expressions[family])
            description = descriptions[family]
        elif family == "straddle":
            cash(f"{call} + {put}")
            description = f"跨式多头，同时买入行权价均为{n(strike)}的一份看涨和一份看跌期权。"
        elif family == "capped_call":
            terms["cap"] = spread
            cash(f"MIN({call}, {n(spread)})")
            features.add("MIN")
            description = f"封顶看涨期权，行权价为{n(strike)}，每份到期支付股价超过行权价的正差额，但最高支付{n(spread)}。"
        elif family == "interval_cash":
            terms.update(lower=lower, upper=upper, digital_cash=coupon)
            event(maturity, f"IF OBS >= {n(lower)} AND OBS <= {n(upper)} THEN payoff PAYS {n(quantity * coupon)} ELSE payoff PAYS 0 END")
            features.update(["IF", "AND", ">=", "<="])
            description = f"区间数字产品：到期股价位于闭区间[{n(lower)}, {n(upper)}]内时，每份支付{n(coupon)}；其他情况支付零。"
        elif family == "tiered_cash":
            terms.update(width=spread, digital_cash=coupon)
            event(maturity, f"IF OBS = STRIKE THEN payoff PAYS {n(quantity * coupon)} ELSE\n IF OBS > STRIKE AND OBS < STRIKE + {n(spread)} THEN payoff PAYS {n(quantity * coupon * 2)} ELSE\n  IF OBS >= STRIKE + {n(spread)} THEN payoff PAYS {n(quantity * coupon * 3)} ELSE payoff PAYS 0 END\n END\nEND")
            features.update(["IF", "ELSE", "AND", "=", ">", "<", ">=", "nested_conditions"])
            description = f"分档数字产品：到期股价低于{n(strike)}时每份支付零；恰好等于{n(strike)}时支付{n(coupon)}；严格介于{n(strike)}和{n(strike + spread)}之间时支付{n(2 * coupon)}；大于或等于{n(strike + spread)}时支付{n(3 * coupon)}。"
        elif family == "power_call":
            cash("MAX(OBS ^ 2 - STRIKE ^ 2, 0)")
            features.add("power")
            description = f"平方价格看涨，到期每份支付股价的平方减去{n(strike)}的平方的正部分。"
        elif family == "log_contract":
            definition("INITIAL", s0)
            cash("LOG(OBS / INITIAL)")
            features.add("LOG")
            description = "对数收益合约，每份支付到期股价与当前股价之比的自然对数，负值表示持有人付款。"
        elif family == "geometric_return_call":
            terms["return_strike"] = round(0.01 + 0.001 * variant, 4)
            definition("INITIAL", s0)
            cash(f"MAX(EXP(LOG(OBS / INITIAL)) - 1 - {n(terms['return_strike'])}, 0)")
            features.update(["LOG", "EXP"])
            description = f"收益率看涨，每份支付到期股价相对当前股价的简单收益率超过{n(terms['return_strike'])}的部分。"
    elif 20 <= family_index < 33:
        definition("STRIKE", strike)
        definition("INITIAL", s0)
        description = observe("")
        terms["count"] = count
        payment = ""
        append = f"APPEND(prices, {obs})"
        features.update(["MAX", "AVERAGE"])
        if family == "arithmetic_asian_call":
            payment = "MAX(AVERAGE(prices) - STRIKE, 0)"
            description += f"算术平均亚式看涨，行权价为{n(strike)}，每份支付观察价格平均值超过行权价的部分。"
        elif family == "arithmetic_asian_put":
            payment = "MAX(STRIKE - AVERAGE(prices), 0)"
            description += f"算术平均亚式看跌，行权价为{n(strike)}，每份支付行权价超过观察价格平均值的部分。"
        elif family == "geometric_asian_call":
            append = f"APPEND(prices, LOG({obs}))"
            payment = "MAX(EXP(AVERAGE(prices)) - STRIKE, 0)"
            features.update(["LOG", "EXP"])
            description += f"几何平均亚式看涨，行权价为{n(strike)}，每份支付观察价格几何平均值超过行权价的部分。"
        elif family == "weighted_asian":
            weights = [i + 1 for i in range(count)]
            definition("WEIGHTS", "[" + ", ".join(map(str, weights)) + "]")
            terms["weights"] = weights
            payment = "MAX(weighted / SUM(WEIGHTS) - STRIKE, 0)"
            features.update(["FOR", "SUM", "vector_indexing"])
            description += f"加权亚式看涨，观察价格权重依次为{weights}并按权重总和归一化，行权价为{n(strike)}。"
        elif family == "floating_asian_call":
            payment = f"MAX({obs} - AVERAGE(prices), 0)"
            description += "浮动行权价亚式看涨，每份支付最后观察价格超过全部观察价格算术平均值的部分。"
        elif family == "lookback_fixed_call":
            payment = "MAX(MAX(prices) - STRIKE, 0)"
            description += f"固定行权价回望看涨，行权价为{n(strike)}，每份支付最高观察股价超过行权价的部分。"
        elif family == "lookback_fixed_put":
            payment = "MAX(STRIKE - MIN(prices), 0)"
            features.add("MIN")
            description += f"固定行权价回望看跌，行权价为{n(strike)}，每份支付行权价超过最低观察股价的部分。"
        elif family == "lookback_float_call":
            payment = f"MAX({obs} - MIN(prices), 0)"
            features.add("MIN")
            description += "浮动行权价回望看涨，每份支付最后观察股价与最低观察股价的差额。"
        elif family == "range_option":
            payment = "MAX(prices) - MIN(prices)"
            features.add("MIN")
            description += "价格极差合约，每份支付最高观察价格减去最低观察价格。"
        else:
            append = f"APPEND(prices, {obs} / previous - 1)\nprevious = {obs}"
            features.update(["scalar_state", "SUM"])
            if family == "cliquet":
                payment = "SUM(prices)"
                description += "逐期收益合约：首期以当前股价为基准，后续以上次观察价为基准，每份支付各期简单收益率之和，允许负值。"
            elif family == "capped_cliquet":
                terms.update(local_floor=-0.03, local_cap=round(0.04 + 0.001 * variant, 4))
                append = f"APPEND(prices, MIN(MAX({obs} / previous - 1, -0.03), {n(terms['local_cap'])}))\nprevious = {obs}"
                payment = "MAX(SUM(prices), 0)"
                features.add("MIN")
                description += f"逐期封顶保底产品：首期基准为当前股价，每期简单收益率限制在[-0.03, {n(terms['local_cap'])}]，每份支付截断收益率总和的正部分。"
            else:
                append = f"APPEND(prices, LOG({obs} / previous) ^ 2)\nprevious = {obs}"
                terms["annualization"] = round(365 / days, 10)
                expr = f"SUM(prices) * {n(terms['annualization'])}"
                payment = expr if family == "realized_variance" else f"SQRT({expr})"
                features.update(["LOG", "power"])
                if family == "realized_volatility":
                    features.add("SQRT")
                description += f"首期基准为当前股价，计算相邻观察价格比值的自然对数平方之和，再乘以365/{days}；每份支付该{'年化方差' if family == 'realized_variance' else '年化方差的平方根'}。"
            event(EVALUATION_DATE, "previous = INITIAL")
        for i, dt in enumerate(observations):
            final = ""
            if i == count - 1:
                if family == "weighted_asian":
                    final = f"\nweighted = 0\nFOR(i, 0, {count}) weighted = weighted + WEIGHTS[i] * prices[i] END"
                final += f"\npayoff PAYS {n(quantity)} * ({payment})"
            event(dt, append + final)
        features.update(["mutable_vectors", "APPEND"])
    elif 33 <= family_index < 39:
        definition("STRIKE", strike)
        description = observe("")
        terms.update(lower=lower, upper=upper)
        event(EVALUATION_DATE, "hit = 0")
        features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        is_up = family.startswith("up")
        is_double = family.startswith("double")
        is_out = "out" in family
        condition = f"{obs} >= {n(upper)}" if is_up else f"{obs} <= {n(lower)}"
        if is_double:
            condition = f"{obs} <= {n(lower)} OR {obs} >= {n(upper)}"
            features.add("OR")
        vanilla = f"MAX({obs} - STRIKE, 0)" if family.endswith("call") else f"MAX(STRIKE - {obs}, 0)"
        for i, dt in enumerate(observations):
            event(dt, f"IF {condition} THEN hit = 1 END" + (f"\npayoff PAYS {n(quantity)} * {'(1 - hit)' if is_out else 'hit'} * {vanilla}" if i == count - 1 else ""))
        boundary = f"小于或等于{n(lower)}或大于或等于{n(upper)}" if is_double else f"大于或等于{n(upper)}" if is_up else f"小于或等于{n(lower)}"
        description += f"离散观察{'敲出' if is_out else '敲入'}{'看涨' if family.endswith('call') else '看跌'}，行权价为{n(strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。{'触发后支付零，否则支付普通期权收益' if is_out else '触发后才支付普通期权收益，否则支付零'}，无返还款。"
    elif 39 <= family_index < 44:
        description = observe("")
        definition("INITIAL", s0)
        features.update(["IF", "scalar_state"])
        if family == "autocall":
            terms.update(principal=notional, coupon=coupon, trigger=strike)
            event(EVALUATION_DATE, "alive = 1")
            for i, dt in enumerate(observations):
                script = f"IF alive != 0 AND {obs} >= {n(strike)} THEN\n payoff PAYS {notional} + {n(coupon)}\n alive = 0\nEND"
                features.add("!=")
                if i == count - 1:
                    script += f"\nIF alive = 1 THEN payoff PAYS {notional} END"
                event(dt, script)
            features.update(["AND", "multiple_payments"])
            description += f"自动赎回票据，本金为{notional}，每次观察股价大于或等于{n(strike)}且尚未赎回时，立即支付本金加现金票息{n(coupon)}并终止。此前无票息；到期仍未赎回时仅返还本金。"
        elif family == "memory_coupon":
            terms.update(principal=notional, coupon=coupon, trigger=strike)
            event(EVALUATION_DATE, "memory = 0")
            for i, dt in enumerate(observations):
                event(dt, f"memory = memory + {n(coupon)}\nIF {obs} >= {n(strike)} THEN payoff PAYS memory memory = 0 END" + (f"\npayoff PAYS {notional}" if i == count - 1 else ""))
            features.add("multiple_payments")
            description += f"记忆票息票据，本金为{notional}。每次观察先累积现金票息{n(coupon)}，股价大于或等于{n(strike)}时支付全部累计票息并清零；到期另返还本金，尚未支付的累计票息作废。"
        elif family == "corridor_coupon":
            terms.update(lower=lower, upper=upper, coupon=coupon)
            for dt in observations:
                event(dt, f"IF {obs} >= {n(lower)} AND {obs} <= {n(upper)} THEN payoff PAYS {n(quantity * coupon)} ELSE payoff PAYS 0 END")
            features.update(["AND", "multiple_payments"])
            description += f"股票区间票息合约，每次观察价落在闭区间[{n(lower)}, {n(upper)}]内时每份即时支付{n(coupon)}，其他情况不支付，无本金返还。"
        elif family == "equity_linked_note":
            terms.update(principal=notional, coupon=coupon, trigger=strike)
            # Only terminal observation determines this note; remove unused observation dates from its terms.
            terms.pop("observation_dates")
            features.discard("multiple_observations")
            description = f"股票挂钩票据，本金为{notional}，到期另支付现金票息{n(coupon)}；若到期股价大于或等于{n(strike)}，全额返还本金，否则返还本金乘以到期股价/{n(strike)}。"
            event(maturity, f"IF {obs} >= {n(strike)} THEN payoff PAYS {notional} + {n(coupon)} ELSE payoff PAYS {notional} * {obs} / {n(strike)} + {n(coupon)} END")
        elif family == "forward_start_call":
            reset = observations[0]
            terms.update(reset_date=reset.isoformat(), moneyness=round(0.9 + 0.002 * variant, 4))
            terms.pop("observation_dates")
            event(reset, f"resetstrike = {n(terms['moneyness'])} * {obs}")
            event(maturity, f"payoff PAYS {n(quantity)} * MAX({obs} - resetstrike, 0)")
            features.add("MAX")
            description = f"远期起始看涨，在{chinese_date(reset)}将行权价设为当日股价的{n(terms['moneyness'])}倍，到期每份支付股价超过该行权价的部分。"
    elif family_index in (44, 45):
        definition("STRIKE", strike)
        features.add("MAX")
        if family == "delayed_settlement_call":
            payment_date = maturity + timedelta(days=2 + variant % 9)
            terms["payment_date"] = payment_date.isoformat()
            event(maturity, f"payoff PAYS {n(quantity)} * MAX({obs} - STRIKE, 0) ON {payment_date}")
            features.add("delayed_payment_ON")
            description = f"行权价为{n(strike)}的欧式看涨，到期确定每份股价超过行权价的支付额，于{chinese_date(payment_date)}现金结算，期间不重新观察股价。"
        else:
            historical_date = EVALUATION_DATE - timedelta(days=5 + variant % 23)
            historical_price = round(s0 * (0.92 + 0.001 * variant), 4)
            multiplier = round(0.9 + 0.0015 * variant, 4)
            terms.update(historical_date=historical_date.isoformat(), historical_price=historical_price, reset_multiplier=multiplier)
            model["fixings"] = {"EQ[ABC]": {historical_date.isoformat(): historical_price}}
            event(maturity, f"historicalstrike = {n(multiplier)} * FIX(EQ[ABC], {historical_date})\npayoff PAYS {n(quantity)} * MAX({obs} - historicalstrike, 0)")
            features.update(["explicit_fixing_date", "historical_snapshot", "scalar_state"])
            description = f"历史基准看涨，{chinese_date(historical_date)}的已知股票定盘价为{n(historical_price)}；行权价为该历史价格的{n(multiplier)}倍，到期每份支付股价超过这一行权价的部分。"
    elif 46 <= family_index < 52:
        features.update(["FOR", "vector_indexing"])
        if family in ("weighted_option_strip", "nested_loop_portfolio"):
            strikes = [strike, round(strike + spread, 4), round(strike + 2 * spread, 4)]
            weights = [1, 2, 3]
            definition("STRIKES", "[" + ", ".join(map(n, strikes)) + "]")
            definition("WEIGHTS", "[1, 2, 3]")
            definition("NOPTIONS", 3)
            features.add("constant_loop_bounds")
            terms.update(strikes=strikes, weights=weights)
            if family == "weighted_option_strip":
                event(maturity, f"FOR(i, 0, NOPTIONS) payoff PAYS {n(quantity)} * WEIGHTS[i] * MAX({obs} - STRIKES[i], 0) END")
                description = f"三档看涨组合，行权价依次为{strikes}，对应买入数量权重为{weights}，到期支付三档期权收益的加权和。"
            else:
                definition("SHIFTS", "[0, " + n(spread / 2) + "]")
                terms["shifts"] = [0, spread / 2]
                event(maturity, f"FOR(i, 0, NOPTIONS)\n FOR(j, 0, 2) payoff PAYS {n(quantity)} * WEIGHTS[i] * MAX({obs} - STRIKES[i] - SHIFTS[j], 0) END\nEND")
                features.add("nested_FOR")
                description = f"六档看涨组合，三个基础行权价为{strikes}，权重为{weights}；每个基础行权价分别加上0和{n(spread / 2)}形成两档，并按该基础行权价的权重买入，支付六档收益之和。"
            features.add("MAX")
        elif family == "sparse_vector_basket":
            description = observe("") + f"从全部观察日期中仅取第1次、第2次和最后一次价格，按[1, 2, 3]加权后除以6，形成行权价为{n(strike)}的看涨支付。"
            terms["selected_observations"] = [0, 1, count - 1]
            definition("FIRST", 0)
            event(observations[0], f"values[FIRST] = {obs}")
            event(observations[1], f"values[2] = {obs}")
            event(maturity, f"values[4] = {obs}\npayoff PAYS {n(quantity)} * MAX((values[0] + 2 * values[2] + 3 * values[4]) / 6 - {n(strike)}, 0)")
            features.update(["mutable_vectors", "vector_assignment", "zero_padding", "MAX"])
            features.discard("FOR")
        elif family == "conditional_vector":
            description = observe("") + f"条件平均价格看涨：每次观察价严格高于{n(strike)}时将该价格计入样本，否则计入零；使用全部样本的算术平均值，支付其超过{n(lower)}的部分。"
            terms["average_strike"] = lower
            vector_events(f"IF {obs} > {n(strike)} THEN APPEND(values, {obs}) ELSE APPEND(values, 0) END", f"payoff PAYS {n(quantity)} * MAX(AVERAGE(values) - {n(lower)}, 0)")
            features.update(["IF", "ELSE", "AVERAGE", "MAX", ">"])
            features.discard("FOR")
            features.discard("vector_indexing")
        else:
            start = EVALUATION_DATE + timedelta(days=20 + variant)
            schedule_count = 3 + variant % 9
            maturity = start + timedelta(days=7 * schedule_count)
            observations = [start + timedelta(days=7 * (i + 1)) for i in range(schedule_count)]
            schedule = f"START: {start}\nEND: {maturity}\nFREQ: 7CD\nBizRule: Unadjusted\nFIXING: END"
            terms.update(maturity=maturity.isoformat(), schedule_start=start.isoformat(), observation_dates=[d.isoformat() for d in observations])
            features.difference_update(["FOR", "vector_indexing"])
            features.update(["schedule_expansion", "PeriodBegin", "PeriodEnd", "unadjusted_calendar"])
            if family == "schedule_asian":
                definition("STRIKE", strike)
                event(schedule, "APPEND(values, FIX(EQ[ABC], PeriodEnd))")
                event(maturity, f"payoff PAYS {n(quantity)} * MAX(AVERAGE(values) - STRIKE, 0)")
                features.update(["mutable_vectors", "APPEND", "AVERAGE", "MAX", "explicit_fixing_date"])
                description = f"行权价为{n(strike)}的算术平均亚式看涨；从{chinese_date(start)}开始，每7个日历日生成一期，共{schedule_count}期，每期期末观察股价，不作营业日调整，最后一期期末结算。起始日价格不计入平均。"
            else:
                terms.update(notional=notional, fixed_rate=rate_strike, day_basis="ACT_365F")
                definition("NOTIONAL", notional)
                definition("COUPON", rate_strike)
                if variant % 2:
                    schedule = schedule.replace("FIXING: END", "FIXING: BEGIN")
                    event(schedule, "payoff PAYS NOTIONAL * COUPON * DCF(ACT_365F, PeriodBegin, PeriodEnd) ON PeriodEnd")
                    features.update(["schedule_fixing_BEGIN", "delayed_payment_ON"])
                else:
                    event(schedule, "payoff PAYS NOTIONAL * COUPON * DCF(ACT_365F, PeriodBegin, PeriodEnd)")
                features.update(["DCF", "multiple_payments"])
                description = f"固定票息合约，名义本金为{notional}，年票息率为{n(rate_strike)}；从{chinese_date(start)}起每7个日历日一期，共{schedule_count}期，按ACT/365F计算每期利息并在期末支付，无本金返还，不作营业日调整。"
    elif 52 <= family_index < 58:
        definition("STRIKE", strike)
        features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        features.discard("PAYS")
        daily = family.startswith("american")
        if daily:
            daily_count = 60 + variant % 40
            maturity = EVALUATION_DATE + timedelta(days=daily_count)
            observations = [EVALUATION_DATE + timedelta(days=i) for i in range(1, daily_count + 1)]
            terms["maturity"] = maturity.isoformat()
            terms["exercise_grid"] = "daily_calendar"
            notes.append("按每日网格近似美式行权；不是连续时间美式价格。")
            features.add("american_daily_approximation")
        terms["exercise_dates"] = [dt.isoformat() for dt in observations]
        if family in ("bermudan_call", "american_call_daily"):
            model["div"] = 0.06
        settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX(STRIKE - {obs}, 0)" if "put" in family else f"MAX({obs} - STRIKE, 0)"
        if family == "bermudan_asian_put":
            settings["regression_features"] = ["EQ[ABC]", "VAR[meanprice]"]
            features.update(["mutable_vectors", "APPEND", "AVERAGE", "regression_features", "scalar_state"])
            for dt in observations:
                event(dt, f"APPEND(values, {obs})\nmeanprice = AVERAGE(values)\nEXERCISE {n(quantity)} * MAX(STRIKE - meanprice, 0)")
            description = f"百慕大亚式看跌，行权价为{n(strike)}，在每个允许行权日先将当日股价计入历史观察序列；若行权，每份支付行权价超过截至当日观察价格算术平均值的部分，行权后终止。"
        else:
            condition = ""
            if family == "conditional_bermudan_put":
                terms["exercise_limit"] = round(strike * 0.96, 4)
                condition = f" IF {obs} < {n(terms['exercise_limit'])}"
                features.add("exercise_IF")
            for dt in observations:
                event(dt, f"EXERCISE {n(quantity)} * {expression}{condition}")
            description = f"{'美式' if daily else '百慕大'}{'看跌' if 'put' in family else '看涨'}，行权价为{n(strike)}，行权时支付普通期权内在价值并终止。"
            if condition:
                description += f"每个行权日（包括到期日）仅在股价严格低于{n(terms['exercise_limit'])}时允许行权；未能行权即到期作废。"
        if daily:
            description += f"用每日网格近似：从{chinese_date(observations[0])}至{chinese_date(maturity)}每个日历日允许行权，包括非营业日；估值日不在行权网格内。"
        else:
            description += "允许行权日期仅为" + "、".join(chinese_date(d) for d in observations) + "，到期未行权则作废。"
    elif 58 <= family_index < 68:
        model = {"type": "gsr", "currency": "USD", "curve_rate": 0.025, "g": 0.012, "h": 1.0, "curve_horizon": "2046-10-04"}
        terms.update(notional=notional, fixed_rate=rate_strike, day_basis="ACT_360")
        definition("NOTIONAL", notional)
        definition("RATESTRIKE", rate_strike)
        features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        if family == "rate_fixed_bond":
            start = EVALUATION_DATE + timedelta(days=30 + variant)
            observations = [start + timedelta(days=90 * (i + 1)) for i in range(count)]
            maturity = observations[-1]
            terms.update(start_date=start.isoformat(), maturity=maturity.isoformat(), payment_dates=[d.isoformat() for d in observations], day_basis="ACT_365F")
            previous = start
            for i, dt in enumerate(observations):
                event(dt, f"payoff PAYS NOTIONAL * RATESTRIKE * DCF(ACT_365F, {previous}, {dt})" + ("\npayoff PAYS NOTIONAL" if i == count - 1 else ""))
                previous = dt
            features.discard("FIX")
            features.update(["DCF", "multiple_payments"])
            description = f"美元固定票息债券，本金{notional}、年票息率{n(rate_strike)}，从{chinese_date(start)}起每90个日历日一期，共{count}期，采用ACT/365F，期末付息并在最后一期返还本金。"
        elif family in ("floating_note", "caplet", "floorlet", "cap", "payer_swap"):
            start = EVALUATION_DATE + timedelta(days=30 + variant)
            periods = 1 if family in ("caplet", "floorlet") else count
            fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
            payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
            terms.update(fixing_dates=[d.isoformat() for d in fixing_dates], payment_dates=[d.isoformat() for d in payment_dates], maturity=payment_dates[-1].isoformat(), index="IR[USD,LIBOR_3M_LCH]")
            for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
                expression = libor
                if family in ("caplet", "cap"):
                    expression = f"MAX({libor} - RATESTRIKE, 0)"
                elif family == "floorlet":
                    expression = f"MAX(RATESTRIKE - {libor}, 0)"
                elif family == "payer_swap":
                    expression = f"{libor} - RATESTRIKE"
                event(fixing, f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}")
                if family == "floating_note" and i == periods - 1:
                    event(payment, "payoff PAYS NOTIONAL")
            features.update(["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"])
            if family in ("cap", "caplet", "floorlet"):
                features.add("MAX")
            names = {"floating_note": "浮动利率票据", "caplet": "单期利率上限", "floorlet": "单期利率下限", "cap": "多期利率上限", "payer_swap": "支付固定利率互换"}
            descriptions = {
                "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
                "caplet": f"支付定盘利率超过{n(rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
                "floorlet": f"支付{n(rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
                "cap": f"每期支付定盘利率超过{n(rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
                "payer_swap": f"每期收浮动利率、付固定利率{n(rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
            }
            description = f"美元{names[family]}，名义本金{notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions[family]}。"
        elif family == "rate_corridor":
            low, high = round(0.005 + 0.0001 * variant, 6), round(0.04 + 0.0001 * variant, 6)
            terms.update(rate_lower=low, rate_upper=high, coupon=coupon, observation_dates=[d.isoformat() for d in observations], index="IR[USD,LIBOR_3M_LCH]")
            for dt in observations:
                event(dt, f"IF {libor} >= {n(low)} AND {libor} <= {n(high)} THEN payoff PAYS {n(coupon)} ELSE payoff PAYS 0 END")
            features.update(["IR_LIBOR", "AND", "IF", "multiple_payments"])
            description = "美元利率区间现金票息，观察日期为" + "、".join(chinese_date(d) for d in observations) + f"；每次USD LIBOR 3M LCH定盘利率落在闭区间[{n(low)}, {n(high)}]时立即支付{n(coupon)}，否则支付零，无本金返还。"
        elif family == "european_payer_swaption":
            first, second = maturity + timedelta(days=182), maturity + timedelta(days=365)
            swap_index = f"IR[USD,SWAP,1Y,{maturity}]"
            terms.update(swap_index=swap_index, annuity_dates=[first.isoformat(), second.isoformat()], annuity_weights=[0.5, 0.5])
            event(maturity, f"payoff PAYS NOTIONAL * MAX(FIX({swap_index}) - RATESTRIKE, 0) * (0.5 * FIX(IR[USD,DF,{first}]) + 0.5 * FIX(IR[USD,DF,{second}]))")
            features.update(["IR_SWAP", "IR_DF", "MAX"])
            description = f"美元欧式现金结算支付方互换期权，名义本金{notional}，行权利率{n(rate_strike)}；到期观察起息日为到期日的一年期美元标准互换利率，支付其超过行权利率的部分乘以现金结算年金。年金明确约定为0.5乘以到期日至{chinese_date(first)}的零息债价格，加0.5乘以到期日至{chinese_date(second)}的零息债价格；现金在到期日支付。"
            notes.append("现金结算年金日期和权重为显式合同约定，不代表实物交割标准互换。")
        elif family == "bermudan_bond_call":
            bond_maturity = maturity + timedelta(days=365 + variant)
            bond_strike = round(0.88 + 0.0008 * variant, 6)
            bond_index = f"IR[USD,DF,{bond_maturity}]"
            terms.update(bond_maturity=bond_maturity.isoformat(), bond_strike=bond_strike, exercise_dates=[d.isoformat() for d in observations])
            settings["regression_features"] = [bond_index]
            for dt in observations:
                event(dt, f"EXERCISE NOTIONAL * MAX(FIX({bond_index}) - {n(bond_strike)}, 0)")
            features.discard("PAYS")
            features.update(["IR_DF", "EXERCISE", "LSMC", "regression_features", "MAX", "multiple_exercise_dates"])
            description = f"美元百慕大零息债券看涨，名义本金{notional}，债券到期日为{chinese_date(bond_maturity)}，每单位债券价格行权价为{n(bond_strike)}。仅在" + "、".join(chinese_date(d) for d in observations) + "允许行权，行权时现金支付债券价格超过行权价的部分乘以名义本金并终止；最后行权日未行权即作废。"
        elif family == "rate_vector_average":
            terms.update(observation_dates=[d.isoformat() for d in observations], index="IR[USD,LIBOR_3M_LCH]")
            vector_events(f"APPEND(rates, {libor})", "payoff PAYS NOTIONAL * MAX(AVERAGE(rates) - RATESTRIKE, 0)")
            features.update(["IR_LIBOR", "AVERAGE", "MAX"])
            description = f"美元平均利率看涨，名义本金{notional}，行权利率{n(rate_strike)}；在" + "、".join(chinese_date(d) for d in observations) + "观察USD LIBOR 3M LCH利率，最后观察日支付观察利率算术平均值超过行权利率的部分乘以名义本金，不再乘计息年数。"
    else:
        features.add("MAX")
        if family == "multi_asset_basket":
            other_spot = round(s0 * 1.2, 4)
            weight = round(0.25 + 0.005 * variant, 4)
            terms.update(other_initial_price=other_spot, basket_weight=weight)
            model = {"type": "correlated_bs", "indices": ["EQ[ABC]", "EQ[XYZ]"], "spots": [s0, other_spot], "vols": [0.2, 0.25], "divs": [0.01, 0.02], "rate": 0.025, "correlation": 0.35}
            definition("STRIKE", strike)
            event(maturity, f"payoff PAYS {n(quantity)} * MAX({n(weight)} * FIX(EQ[ABC]) + {n(1 - weight)} * FIX(EQ[XYZ]) - STRIKE, 0)")
            features.update(["multi_asset", "correlated_BS", "multiple_named_indices"])
            description = f"双股票篮子欧式看涨，ABC和XYZ当前股价分别为{n(s0)}和{n(other_spot)}；篮子价格为{n(weight)}乘以ABC股价加{n(1 - weight)}乘以XYZ股价，行权价为{n(strike)}，每份支付到期篮子价格超过行权价的部分。"
        else:
            bond_maturity = maturity + timedelta(days=365)
            terms["bond_maturity"] = bond_maturity.isoformat()
            model = {"type": "hybrid_equity_gsr", "spot": s0, "vol": 0.2, "div": 0.01, "curve_rate": 0.025, "g": 0.012, "h": 1.0, "curve_horizon": "2046-10-04", "correlation": -0.20}
            definition("STRIKE", strike)
            event(maturity, f"payoff PAYS {n(quantity)} * MAX({obs} - STRIKE, 0) * FIX(IR[USD,DF,{bond_maturity}])")
            features.update(["hybrid_equity_GSR", "IR_DF", "multiple_named_indices"])
            description = f"股票利率混合支付产品，ABC当前股价为{n(s0)}，股票行权价为{n(strike)}；每份到期支付股票看涨内在价值，乘以当时美元零息债券价格，该债券在{chinese_date(bond_maturity)}到期。支付当日还需按美元随机利率折现。"

    if not description or not rows:
        raise ValueError(f"Unimplemented family {family}")
    script_text = "\n".join(row["script"] for row in rows)
    for keyword in ("FIX", "SPOT", "PAYS", "MAX", "MIN", "IF", "ELSE", "AND", "OR", "FOR", "APPEND", "AVERAGE", "SUM", "LOG", "EXP", "SQRT", "DCF", "EXERCISE", "PeriodBegin", "PeriodEnd"):
        if re.search(r"\b" + keyword + r"\b", script_text):
            features.add(keyword)
        else:
            features.discard(keyword)
    if settings.get("regression_features"):
        features.add("regression_features")
    for comparator in ("!=", ">=", "<=", ">", "<", "="):
        if comparator not in script_text:
            features.discard(comparator)
    if model["type"] in ("gsr",):
        prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，采用美元结算。"
        multiplier = ""
    elif family == "schedule_fixed_coupon":
        prefix = f"估值日为{chinese_date(EVALUATION_DATE)}。"
        multiplier = ""
    elif family in ("autocall", "memory_coupon", "equity_linked_note"):
        prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，标的是ABC股票，当前价格为{n(s0)}。"
        multiplier = ""
    else:
        prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，标的是ABC股票，当前价格为{n(s0)}。"
        multiplier = f"持有{n(quantity)}份，每份按上述支付规则计算。"
    if family == "multi_asset_basket":
        prefix = f"估值日为{chinese_date(EVALUATION_DATE)}。"
    question = prefix + description + f"最终到期日为{chinese_date(terms['maturity'])}。" + multiplier + "所有日期均使用约定的日历日期，支付为现金结算。请给出对应的DAL事件表。"
    answer = markdown_table(["日期 / 定义", "DAL script / 数值"], [[f"`{r['date_or_definition']}`", f"`{r['script']}`"] for r in rows])
    if parse_answer(answer) != rows:
        raise ValueError("Markdown round trip failed")
    identity = {"family": family, "terms": terms}
    return {"id": f"DAL-QA-{family_index * 100 + variant + 1:05d}", "question": question, "answer": answer,
            "category": category, "product_type": label, "family": family, "features": sorted(features),
            "events": rows, "contract_terms": terms, "product_settings": settings,
            "pricing_context": {"evaluation_date": EVALUATION_DATE.isoformat(), "model": model,
                                "recommended_simulation": {"method": "sobol", "compiled": True, "n_paths": 16384,
                                                           "lsmc_training_paths": 8192, "lsmc_basis_degree": 3}},
            "limitations": notes, "contract_fingerprint": digest(identity)}


def write_index(records):
    counts = Counter(record["product_type"] for record in records)
    group_counts = Counter(record["category"] for record in records)
    feature_counts = Counter(feature for record in records for feature in record["features"])
    lines = ["# DAL 自然语言产品与脚本问答索引", "", "数据：[qa.jsonl](qa.jsonl)。UTF-8 JSONL，每个物理行是一条完整问答，问题为自然语言，答案为 Markdown 事件表。", "",
             f"共 {len(records)} 条，{len(counts)} 种产品类型。每种类型 100 个不同条款组合。当前价格在定价上下文中提供；事件表读取各观察日价格。", "",
             "表中的记录链接以 JSONL 物理行号定位；若查看器不支持 `#L`，使用同列行号或记录 ID 查找。", "",
             "美式类型采用明确的每日行权网格近似。利率使用 GSR，双股票使用相关 BS，股票利率混合使用 Hybrid；不能将所有脚本都交给单股票 BS 模型。", "",
             "验证方法及复现命令见 [README.md](README.md)，逐条结果见 [validation.jsonl](validation.jsonl)，统计见 [validation_summary.json](validation_summary.json)。", "",
             "## 分类统计", "", markdown_table(["分类", "数量"], [[k, str(v)] for k, v in group_counts.items()]), "",
             "## 产品类型统计", "", markdown_table(["产品类型", "数量"], [[k, str(v)] for k, v in counts.items()]), "",
             "## 功能覆盖", "", markdown_table(["功能", "涉及问答数"], [[k, str(v)] for k, v in sorted(feature_counts.items())]), "",
             "## 逐条分类总表", ""]
    table_rows = [[f"[{r['id']}](qa.jsonl#L{i})", str(i), r["category"], r["product_type"], "、".join(r["features"])] for i, r in enumerate(records, 1)]
    lines.append(markdown_table(["记录 / JSONL 链接", "JSONL 行", "分类", "产品类型", "主要功能点"], table_rows))
    (ROOT / "index.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate():
    records = [build_record(family_index, variant) for family_index in range(len(FAMILIES)) for variant in range(100)]
    if len(records) != 7000:
        raise ValueError("Expected exactly 7000 records")
    for key in ("id", "question", "answer", "contract_fingerprint"):
        if len({r[key] for r in records}) != len(records):
            raise ValueError(f"Duplicate {key}")
    with (ROOT / "qa.jsonl").open("w", encoding="utf-8") as stream:
        for record in records:
            stream.write(json.dumps(record, ensure_ascii=False, separators=(",", ":"), allow_nan=False) + "\n")
    write_index(records)
    print(json.dumps({"records": len(records), "families": len(FAMILIES), "categories": dict(Counter(r['category'] for r in records)), "output": str(ROOT / 'qa.jsonl')}, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.parse_args()
    generate()
