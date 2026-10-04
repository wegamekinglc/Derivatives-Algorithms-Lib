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
    widths = [
        max(len(headers[i]), *(len(row[i]) for row in rows))
        for i in range(len(headers))
    ]

    def line(row):
        return (
            "| "
            + " | ".join((cell.ljust(width) for cell, width in zip(row, widths)))
            + " |"
        )

    return "\n".join(
        [
            line(headers),
            "|" + "|".join("-" * (width + 2) for width in widths) + "|",
            *(line(row) for row in rows),
        ]
    )


def parse_event_row(line):
    cells = [cell.strip() for cell in line.split("|")[1:-1]]
    if len(cells) != 2:
        raise ValueError("Malformed event table row")
    if any(not cell.startswith("`") or not cell.endswith("`") for cell in cells):
        raise ValueError("Malformed event table row")
    return {
        "date_or_definition": cells[0][1:-1].replace("<br>", "\n"),
        "script": cells[1][1:-1].replace("<br>", "\n"),
    }


def parse_answer(answer):
    lines = answer.splitlines()
    if len(lines) < 3 or [cell.strip() for cell in lines[0].split("|")[1:-1]] != [
        "日期 / 定义",
        "DAL script / 数值",
    ]:
        raise ValueError("Answer must be a DAL event Markdown table")
    if not re.fullmatch(r"\|\s*:?-{3,}:?\s*\|\s*:?-{3,}:?\s*\|", lines[1]):
        raise ValueError("Malformed event table separator")
    return [parse_event_row(line) for line in lines[2:]]


def digest(value):
    return hashlib.sha256(
        json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        ).encode()
    ).hexdigest()


class ContractBuilder:
    def __init__(self, family_index, variant):
        self.family_index = family_index
        self.variant = variant
        self.family, self.label, self.category = FAMILIES[family_index]
        self.s0 = round(80 + 1.75 * variant, 2)
        self.strike = round(self.s0 * (0.82 + 0.0045 * variant), 4)
        self.quantity = 1 + 0.25 * (variant % 8)
        self.days = 120 + 7 * variant + family_index % 7
        self.maturity = EVALUATION_DATE + timedelta(days=self.days)
        if family_index == 0 and variant == 0:
            self.s0, self.strike, self.maturity = (200.0, 210.0, date(2027, 1, 30))
            self.days = (self.maturity - EVALUATION_DATE).days
        self.count = 3 + variant % 6
        self.observations = [
            EVALUATION_DATE + timedelta(days=self.days * (i + 1) // self.count)
            for i in range(self.count)
        ]
        self.coupon = round(1 + 0.05 * variant, 4)
        self.spread = round(self.s0 * (0.1 + 0.001 * (variant % 9)), 4)
        self.lower, self.upper = (round(self.s0 * 0.7, 4), round(self.s0 * 1.3, 4))
        self.rate_strike = round(0.012 + 0.00035 * variant, 6)
        self.notional = 10000 + 1250 * variant
        self.obs = "FIX(EQ[ABC])"
        self.rows = []
        self.features = {"FIX", "PAYS"}
        self.settings = {}
        self.model = {
            "type": "black_scholes",
            "spot": self.s0,
            "vol": 0.15 + 0.01 * (variant % 12),
            "rate": 0.025,
            "div": 0.01,
        }
        self.terms = {
            "quantity": self.quantity,
            "initial_price": self.s0,
            "strike": self.strike,
            "maturity": self.maturity.isoformat(),
        }
        self.description = ""
        self.notes = []
        if (
            family_index < 58 or self.family == "hybrid_equity_rate"
        ) and variant % 5 == 4:
            self.obs = "SPOT()"
            self.settings["default_index"] = "EQ[ABC]"
            self.features.add("default_index_binding")

    def definition(self, name, value):
        self.rows.append(
            {
                "date_or_definition": name,
                "script": number(value) if isinstance(value, (int, float)) else value,
            }
        )
        self.features.add(
            "numeric_constants"
            if isinstance(value, (int, float))
            else "constant_vectors"
            if value.startswith("[")
            else "text_macros"
        )

    def event(self, dt, script):
        self.rows.append(
            {
                "date_or_definition": dt.isoformat() if isinstance(dt, date) else dt,
                "script": script,
            }
        )

    def cash(self, expression):
        self.event(
            self.maturity, f"payoff PAYS {number(self.quantity)} * ({expression})"
        )

    def observe(self, text):
        self.terms["observation_dates"] = [d.isoformat() for d in self.observations]
        self.features.add("multiple_observations")
        return (
            text
            + "观察日期依次为"
            + "、".join(chinese_date(d) for d in self.observations)
            + "，最后一个观察日结算。"
        )

    def vector_events(self, append, payment):
        for i, dt in enumerate(self.observations):
            self.event(dt, append + ("\n" + payment if i == self.count - 1 else ""))
        self.features.update(["mutable_vectors", "APPEND"])

    def build_european_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.definition("CALLPAY", "MAX(OBS - STRIKE, 0)")
        self.cash("CALLPAY")
        self.description = f"欧式看涨期权，行权价为{number(self.strike)}，到期支付股价超过行权价的部分。"

    def build_european_put(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        put = "MAX(STRIKE - OBS, 0)"
        self.features.add("MAX")
        self.cash(put)
        self.description = f"欧式看跌期权，行权价为{number(self.strike)}，到期支付行权价超过股价的部分。"

    def build_cash_digital_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        amount = number(self.coupon)
        self.features.update(["IF", "ELSE", ">"])
        self.terms["digital_cash"] = self.coupon
        smoothing = ":0.2" if self.variant % 2 else ""
        if smoothing:
            self.features.add("comparison_smoothing")
        self.event(
            self.maturity,
            f"IF OBS > STRIKE{smoothing} THEN\n    payoff PAYS {number(self.quantity)} * {amount}\nELSE\n    payoff PAYS 0\nEND",
        )
        self.description = f"现金数字看涨期权，股价严格高于{number(self.strike)}时，每份支付{'现金' + number(self.coupon)}，其他情况支付零。"

    def build_cash_digital_put(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        amount = number(self.coupon)
        self.features.update(["IF", "ELSE", "<"])
        self.terms["digital_cash"] = self.coupon
        smoothing = ":0.2" if self.variant % 2 else ""
        if smoothing:
            self.features.add("comparison_smoothing")
        self.event(
            self.maturity,
            f"IF OBS < STRIKE{smoothing} THEN\n    payoff PAYS {number(self.quantity)} * {amount}\nELSE\n    payoff PAYS 0\nEND",
        )
        self.description = f"现金数字看跌期权，股价严格低于{number(self.strike)}时，每份支付{'现金' + number(self.coupon)}，其他情况支付零。"

    def build_asset_digital_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        amount = "OBS"
        self.features.update(["IF", "ELSE", ">"])
        self.terms["digital_cash"] = self.coupon
        smoothing = ":0.2" if self.variant % 2 else ""
        if smoothing:
            self.features.add("comparison_smoothing")
        self.event(
            self.maturity,
            f"IF OBS > STRIKE{smoothing} THEN\n    payoff PAYS {number(self.quantity)} * {amount}\nELSE\n    payoff PAYS 0\nEND",
        )
        self.description = f"资产数字看涨期权，股价严格高于{number(self.strike)}时，每份支付当日股价对应的现金，其他情况支付零。"

    def build_asset_digital_put(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        amount = "OBS"
        self.features.update(["IF", "ELSE", "<"])
        self.terms["digital_cash"] = self.coupon
        smoothing = ":0.2" if self.variant % 2 else ""
        if smoothing:
            self.features.add("comparison_smoothing")
        self.event(
            self.maturity,
            f"IF OBS < STRIKE{smoothing} THEN\n    payoff PAYS {number(self.quantity)} * {amount}\nELSE\n    payoff PAYS 0\nEND",
        )
        self.description = f"资产数字看跌期权，股价严格低于{number(self.strike)}时，每份支付当日股价对应的现金，其他情况支付零。"

    def build_forward_long(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.cash("+(OBS - STRIKE)")
        self.features.add("unary_operators")
        self.description = f"交割价为{number(self.strike)}的远期多头，每份现金结算金额为股价减交割价，负值表示持有人付款。"

    def build_forward_short(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.cash("-(OBS - STRIKE)")
        self.features.add("unary_operators")
        self.description = f"交割价为{number(self.strike)}的远期空头，每份现金结算金额为交割价减股价，负值表示持有人付款。"

    def build_bull_call_spread(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.terms["width"] = self.spread
        self.definition("WIDTH", self.spread)
        expressions = {
            "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
            "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
            "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
            "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
            "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
        }
        descriptions = {
            "bull_call_spread": f"牛市看涨价差，买入行权价{number(self.strike)}的看涨期权，卖出行权价{number(self.strike + self.spread)}的看涨期权。",
            "bear_put_spread": f"熊市看跌价差，买入行权价{number(self.strike + self.spread)}的看跌期权，卖出行权价{number(self.strike)}的看跌期权。",
            "strangle": f"宽跨式多头，买入行权价{number(self.strike)}的看跌期权和行权价{number(self.strike + self.spread)}的看涨期权。",
            "butterfly": f"看涨蝶式组合：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨、买入一份行权价{number(self.strike + 2 * self.spread)}的看涨。",
            "call_ratio_spread": f"看涨比例价差：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨，净支付可为负。",
        }
        self.cash(expressions["bull_call_spread"])
        self.description = descriptions["bull_call_spread"]

    def build_bear_put_spread(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.terms["width"] = self.spread
        self.definition("WIDTH", self.spread)
        expressions = {
            "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
            "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
            "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
            "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
            "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
        }
        descriptions = {
            "bull_call_spread": f"牛市看涨价差，买入行权价{number(self.strike)}的看涨期权，卖出行权价{number(self.strike + self.spread)}的看涨期权。",
            "bear_put_spread": f"熊市看跌价差，买入行权价{number(self.strike + self.spread)}的看跌期权，卖出行权价{number(self.strike)}的看跌期权。",
            "strangle": f"宽跨式多头，买入行权价{number(self.strike)}的看跌期权和行权价{number(self.strike + self.spread)}的看涨期权。",
            "butterfly": f"看涨蝶式组合：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨、买入一份行权价{number(self.strike + 2 * self.spread)}的看涨。",
            "call_ratio_spread": f"看涨比例价差：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨，净支付可为负。",
        }
        self.cash(expressions["bear_put_spread"])
        self.description = descriptions["bear_put_spread"]

    def build_straddle(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.cash(f"{call} + {put}")
        self.description = f"跨式多头，同时买入行权价均为{number(self.strike)}的一份看涨和一份看跌期权。"

    def build_strangle(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.terms["width"] = self.spread
        self.definition("WIDTH", self.spread)
        expressions = {
            "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
            "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
            "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
            "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
            "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
        }
        descriptions = {
            "bull_call_spread": f"牛市看涨价差，买入行权价{number(self.strike)}的看涨期权，卖出行权价{number(self.strike + self.spread)}的看涨期权。",
            "bear_put_spread": f"熊市看跌价差，买入行权价{number(self.strike + self.spread)}的看跌期权，卖出行权价{number(self.strike)}的看跌期权。",
            "strangle": f"宽跨式多头，买入行权价{number(self.strike)}的看跌期权和行权价{number(self.strike + self.spread)}的看涨期权。",
            "butterfly": f"看涨蝶式组合：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨、买入一份行权价{number(self.strike + 2 * self.spread)}的看涨。",
            "call_ratio_spread": f"看涨比例价差：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨，净支付可为负。",
        }
        self.cash(expressions["strangle"])
        self.description = descriptions["strangle"]

    def build_butterfly(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.terms["width"] = self.spread
        self.definition("WIDTH", self.spread)
        expressions = {
            "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
            "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
            "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
            "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
            "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
        }
        descriptions = {
            "bull_call_spread": f"牛市看涨价差，买入行权价{number(self.strike)}的看涨期权，卖出行权价{number(self.strike + self.spread)}的看涨期权。",
            "bear_put_spread": f"熊市看跌价差，买入行权价{number(self.strike + self.spread)}的看跌期权，卖出行权价{number(self.strike)}的看跌期权。",
            "strangle": f"宽跨式多头，买入行权价{number(self.strike)}的看跌期权和行权价{number(self.strike + self.spread)}的看涨期权。",
            "butterfly": f"看涨蝶式组合：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨、买入一份行权价{number(self.strike + 2 * self.spread)}的看涨。",
            "call_ratio_spread": f"看涨比例价差：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨，净支付可为负。",
        }
        self.cash(expressions["butterfly"])
        self.description = descriptions["butterfly"]

    def build_call_ratio_spread(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call, put = ("MAX(OBS - STRIKE, 0)", "MAX(STRIKE - OBS, 0)")
        self.features.add("MAX")
        self.terms["width"] = self.spread
        self.definition("WIDTH", self.spread)
        expressions = {
            "bull_call_spread": f"{call} - MAX(OBS - STRIKE - WIDTH, 0)",
            "bear_put_spread": f"MAX(STRIKE + WIDTH - OBS, 0) - {put}",
            "strangle": f"{put} + MAX(OBS - STRIKE - WIDTH, 0)",
            "butterfly": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0) + MAX(OBS - STRIKE - 2 * WIDTH, 0)",
            "call_ratio_spread": f"{call} - 2 * MAX(OBS - STRIKE - WIDTH, 0)",
        }
        descriptions = {
            "bull_call_spread": f"牛市看涨价差，买入行权价{number(self.strike)}的看涨期权，卖出行权价{number(self.strike + self.spread)}的看涨期权。",
            "bear_put_spread": f"熊市看跌价差，买入行权价{number(self.strike + self.spread)}的看跌期权，卖出行权价{number(self.strike)}的看跌期权。",
            "strangle": f"宽跨式多头，买入行权价{number(self.strike)}的看跌期权和行权价{number(self.strike + self.spread)}的看涨期权。",
            "butterfly": f"看涨蝶式组合：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨、买入一份行权价{number(self.strike + 2 * self.spread)}的看涨。",
            "call_ratio_spread": f"看涨比例价差：买入一份行权价{number(self.strike)}的看涨、卖出两份行权价{number(self.strike + self.spread)}的看涨，净支付可为负。",
        }
        self.cash(expressions["call_ratio_spread"])
        self.description = descriptions["call_ratio_spread"]

    def build_capped_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)
        call = "MAX(OBS - STRIKE, 0)"
        self.features.add("MAX")
        self.terms["cap"] = self.spread
        self.cash(f"MIN({call}, {number(self.spread)})")
        self.features.add("MIN")
        self.description = f"封顶看涨期权，行权价为{number(self.strike)}，每份到期支付股价超过行权价的正差额，但最高支付{number(self.spread)}。"

    def build_interval_cash(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.terms.update(lower=self.lower, upper=self.upper, digital_cash=self.coupon)
        self.event(
            self.maturity,
            f"IF OBS >= {number(self.lower)} AND OBS <= {number(self.upper)} THEN payoff PAYS {number(self.quantity * self.coupon)} ELSE payoff PAYS 0 END",
        )
        self.features.update(["IF", "AND", ">=", "<="])
        self.description = f"区间数字产品：到期股价位于闭区间[{number(self.lower)}, {number(self.upper)}]内时，每份支付{number(self.coupon)}；其他情况支付零。"

    def build_tiered_cash(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.terms.update(width=self.spread, digital_cash=self.coupon)
        self.event(
            self.maturity,
            f"IF OBS = STRIKE THEN payoff PAYS {number(self.quantity * self.coupon)} ELSE\n IF OBS > STRIKE AND OBS < STRIKE + {number(self.spread)} THEN payoff PAYS {number(self.quantity * self.coupon * 2)} ELSE\n  IF OBS >= STRIKE + {number(self.spread)} THEN payoff PAYS {number(self.quantity * self.coupon * 3)} ELSE payoff PAYS 0 END\n END\nEND",
        )
        self.features.update(
            ["IF", "ELSE", "AND", "=", ">", "<", ">=", "nested_conditions"]
        )
        self.description = f"分档数字产品：到期股价低于{number(self.strike)}时每份支付零；恰好等于{number(self.strike)}时支付{number(self.coupon)}；严格介于{number(self.strike)}和{number(self.strike + self.spread)}之间时支付{number(2 * self.coupon)}；大于或等于{number(self.strike + self.spread)}时支付{number(3 * self.coupon)}。"

    def build_power_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.cash("MAX(OBS ^ 2 - STRIKE ^ 2, 0)")
        self.features.add("power")
        self.description = f"平方价格看涨，到期每份支付股价的平方减去{number(self.strike)}的平方的正部分。"

    def build_log_contract(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.definition("INITIAL", self.s0)
        self.cash("LOG(OBS / INITIAL)")
        self.features.add("LOG")
        self.description = "对数收益合约，每份支付到期股价与当前股价之比的自然对数，负值表示持有人付款。"

    def build_geometric_return_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("OBS", self.obs)

        self.features.add("MAX")
        self.terms["return_strike"] = round(0.01 + 0.001 * self.variant, 4)
        self.definition("INITIAL", self.s0)
        self.cash(
            f"MAX(EXP(LOG(OBS / INITIAL)) - 1 - {number(self.terms['return_strike'])}, 0)"
        )
        self.features.update(["LOG", "EXP"])
        self.description = f"收益率看涨，每份支付到期股价相对当前股价的简单收益率超过{number(self.terms['return_strike'])}的部分。"

    def build_arithmetic_asian_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = "MAX(AVERAGE(prices) - STRIKE, 0)"
        self.description += f"算术平均亚式看涨，行权价为{number(self.strike)}，每份支付观察价格平均值超过行权价的部分。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_arithmetic_asian_put(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = "MAX(STRIKE - AVERAGE(prices), 0)"
        self.description += f"算术平均亚式看跌，行权价为{number(self.strike)}，每份支付行权价超过观察价格平均值的部分。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_geometric_asian_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        append = f"APPEND(prices, LOG({self.obs}))"
        payment = "MAX(EXP(AVERAGE(prices)) - STRIKE, 0)"
        self.features.update(["LOG", "EXP"])
        self.description += f"几何平均亚式看涨，行权价为{number(self.strike)}，每份支付观察价格几何平均值超过行权价的部分。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_weighted_asian(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        weights = [i + 1 for i in range(self.count)]
        self.definition("WEIGHTS", "[" + ", ".join(map(str, weights)) + "]")
        self.terms["weights"] = weights
        payment = "MAX(weighted / SUM(WEIGHTS) - STRIKE, 0)"
        self.features.update(["FOR", "SUM", "vector_indexing"])
        self.description += f"加权亚式看涨，观察价格权重依次为{weights}并按权重总和归一化，行权价为{number(self.strike)}。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final = f"\nweighted = 0\nFOR(i, 0, {self.count}) weighted = weighted + WEIGHTS[i] * prices[i] END"
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_floating_asian_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = f"MAX({self.obs} - AVERAGE(prices), 0)"
        self.description += (
            "浮动行权价亚式看涨，每份支付最后观察价格超过全部观察价格算术平均值的部分。"
        )
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_lookback_fixed_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = "MAX(MAX(prices) - STRIKE, 0)"
        self.description += f"固定行权价回望看涨，行权价为{number(self.strike)}，每份支付最高观察股价超过行权价的部分。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_lookback_fixed_put(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = "MAX(STRIKE - MIN(prices), 0)"
        self.features.add("MIN")
        self.description += f"固定行权价回望看跌，行权价为{number(self.strike)}，每份支付行权价超过最低观察股价的部分。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_lookback_float_call(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = f"MAX({self.obs} - MIN(prices), 0)"
        self.features.add("MIN")
        self.description += (
            "浮动行权价回望看涨，每份支付最后观察股价与最低观察股价的差额。"
        )
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_range_option(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        payment = "MAX(prices) - MIN(prices)"
        self.features.add("MIN")
        self.description += "价格极差合约，每份支付最高观察价格减去最低观察价格。"
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_cliquet(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        append = f"APPEND(prices, {self.obs} / previous - 1)\nprevious = {self.obs}"
        self.features.update(["scalar_state", "SUM"])
        payment = "SUM(prices)"
        self.description += "逐期收益合约：首期以当前股价为基准，后续以上次观察价为基准，每份支付各期简单收益率之和，允许负值。"
        self.event(EVALUATION_DATE, "previous = INITIAL")
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_capped_cliquet(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        append = f"APPEND(prices, {self.obs} / previous - 1)\nprevious = {self.obs}"
        self.features.update(["scalar_state", "SUM"])
        self.terms.update(
            local_floor=-0.03, local_cap=round(0.04 + 0.001 * self.variant, 4)
        )
        append = f"APPEND(prices, MIN(MAX({self.obs} / previous - 1, -0.03), {number(self.terms['local_cap'])}))\nprevious = {self.obs}"
        payment = "MAX(SUM(prices), 0)"
        self.features.add("MIN")
        self.description += f"逐期封顶保底产品：首期基准为当前股价，每期简单收益率限制在[-0.03, {number(self.terms['local_cap'])}]，每份支付截断收益率总和的正部分。"
        self.event(EVALUATION_DATE, "previous = INITIAL")
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_realized_variance(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        append = f"APPEND(prices, {self.obs} / previous - 1)\nprevious = {self.obs}"
        self.features.update(["scalar_state", "SUM"])
        append = (
            f"APPEND(prices, LOG({self.obs} / previous) ^ 2)\nprevious = {self.obs}"
        )
        self.terms["annualization"] = round(365 / self.days, 10)
        expr = f"SUM(prices) * {number(self.terms['annualization'])}"
        payment = expr
        self.features.update(["LOG", "power"])
        self.description += f"首期基准为当前股价，计算相邻观察价格比值的自然对数平方之和，再乘以365/{self.days}；每份支付该年化方差。"
        self.event(EVALUATION_DATE, "previous = INITIAL")
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_realized_volatility(self):
        self.definition("STRIKE", self.strike)
        self.definition("INITIAL", self.s0)
        self.description = self.observe("")
        self.terms["count"] = self.count
        payment = ""
        append = f"APPEND(prices, {self.obs})"
        self.features.update(["MAX", "AVERAGE"])
        append = f"APPEND(prices, {self.obs} / previous - 1)\nprevious = {self.obs}"
        self.features.update(["scalar_state", "SUM"])
        append = (
            f"APPEND(prices, LOG({self.obs} / previous) ^ 2)\nprevious = {self.obs}"
        )
        self.terms["annualization"] = round(365 / self.days, 10)
        expr = f"SUM(prices) * {number(self.terms['annualization'])}"
        payment = f"SQRT({expr})"
        self.features.update(["LOG", "power"])
        self.features.add("SQRT")
        self.description += f"首期基准为当前股价，计算相邻观察价格比值的自然对数平方之和，再乘以365/{self.days}；每份支付该年化方差的平方根。"
        self.event(EVALUATION_DATE, "previous = INITIAL")
        for i, dt in enumerate(self.observations):
            final = ""
            if i == self.count - 1:
                final += f"\npayoff PAYS {number(self.quantity)} * ({payment})"
            self.event(dt, append + final)
        self.features.update(["mutable_vectors", "APPEND"])

    def build_up_out_call(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} >= {number(self.upper)}"
        vanilla = f"MAX({self.obs} - STRIKE, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * (1 - hit) * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"大于或等于{number(self.upper)}"
        self.description += f"离散观察敲出看涨，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后支付零，否则支付普通期权收益，无返还款。"

    def build_down_out_put(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} <= {number(self.lower)}"
        vanilla = f"MAX(STRIKE - {self.obs}, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * (1 - hit) * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"小于或等于{number(self.lower)}"
        self.description += f"离散观察敲出看跌，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后支付零，否则支付普通期权收益，无返还款。"

    def build_up_in_call(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} >= {number(self.upper)}"
        vanilla = f"MAX({self.obs} - STRIKE, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * hit * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"大于或等于{number(self.upper)}"
        self.description += f"离散观察敲入看涨，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后才支付普通期权收益，否则支付零，无返还款。"

    def build_down_in_put(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} <= {number(self.lower)}"
        vanilla = f"MAX(STRIKE - {self.obs}, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * hit * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"小于或等于{number(self.lower)}"
        self.description += f"离散观察敲入看跌，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后才支付普通期权收益，否则支付零，无返还款。"

    def build_double_out_call(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} <= {number(self.lower)}"
        condition = (
            f"{self.obs} <= {number(self.lower)} OR {self.obs} >= {number(self.upper)}"
        )
        self.features.add("OR")
        vanilla = f"MAX({self.obs} - STRIKE, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * (1 - hit) * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"小于或等于{number(self.lower)}或大于或等于{number(self.upper)}"
        self.description += f"离散观察敲出看涨，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后支付零，否则支付普通期权收益，无返还款。"

    def build_double_in_put(self):
        self.definition("STRIKE", self.strike)
        self.description = self.observe("")
        self.terms.update(lower=self.lower, upper=self.upper)
        self.event(EVALUATION_DATE, "hit = 0")
        self.features.update(["scalar_state", "IF", "MAX", ">=", "<="])
        condition = f"{self.obs} <= {number(self.lower)}"
        condition = (
            f"{self.obs} <= {number(self.lower)} OR {self.obs} >= {number(self.upper)}"
        )
        self.features.add("OR")
        vanilla = f"MAX(STRIKE - {self.obs}, 0)"
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"IF {condition} THEN hit = 1 END"
                + (
                    f"\npayoff PAYS {number(self.quantity)} * hit * {vanilla}"
                    if i == self.count - 1
                    else ""
                ),
            )
        boundary = f"小于或等于{number(self.lower)}或大于或等于{number(self.upper)}"
        self.description += f"离散观察敲入看跌，行权价为{number(self.strike)}；仅在列出的日期检查障碍，任一观察价{boundary}即触发。触发后才支付普通期权收益，否则支付零，无返还款。"

    def build_autocall(self):
        self.description = self.observe("")
        self.definition("INITIAL", self.s0)
        self.features.update(["IF", "scalar_state"])
        self.terms.update(
            principal=self.notional, coupon=self.coupon, trigger=self.strike
        )
        self.event(EVALUATION_DATE, "alive = 1")
        for i, dt in enumerate(self.observations):
            script = f"IF alive != 0 AND {self.obs} >= {number(self.strike)} THEN\n payoff PAYS {self.notional} + {number(self.coupon)}\n alive = 0\nEND"
            self.features.add("!=")
            if i == self.count - 1:
                script += f"\nIF alive = 1 THEN payoff PAYS {self.notional} END"
            self.event(dt, script)
        self.features.update(["AND", "multiple_payments"])
        self.description += f"自动赎回票据，本金为{self.notional}，每次观察股价大于或等于{number(self.strike)}且尚未赎回时，立即支付本金加现金票息{number(self.coupon)}并终止。此前无票息；到期仍未赎回时仅返还本金。"

    def build_memory_coupon(self):
        self.description = self.observe("")
        self.definition("INITIAL", self.s0)
        self.features.update(["IF", "scalar_state"])
        self.terms.update(
            principal=self.notional, coupon=self.coupon, trigger=self.strike
        )
        self.event(EVALUATION_DATE, "memory = 0")
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"memory = memory + {number(self.coupon)}\nIF {self.obs} >= {number(self.strike)} THEN payoff PAYS memory memory = 0 END"
                + (f"\npayoff PAYS {self.notional}" if i == self.count - 1 else ""),
            )
        self.features.add("multiple_payments")
        self.description += f"记忆票息票据，本金为{self.notional}。每次观察先累积现金票息{number(self.coupon)}，股价大于或等于{number(self.strike)}时支付全部累计票息并清零；到期另返还本金，尚未支付的累计票息作废。"

    def build_corridor_coupon(self):
        self.description = self.observe("")
        self.definition("INITIAL", self.s0)
        self.features.update(["IF", "scalar_state"])
        self.terms.update(lower=self.lower, upper=self.upper, coupon=self.coupon)
        for dt in self.observations:
            self.event(
                dt,
                f"IF {self.obs} >= {number(self.lower)} AND {self.obs} <= {number(self.upper)} THEN payoff PAYS {number(self.quantity * self.coupon)} ELSE payoff PAYS 0 END",
            )
        self.features.update(["AND", "multiple_payments"])
        self.description += f"股票区间票息合约，每次观察价落在闭区间[{number(self.lower)}, {number(self.upper)}]内时每份即时支付{number(self.coupon)}，其他情况不支付，无本金返还。"

    def build_equity_linked_note(self):
        self.description = self.observe("")
        self.definition("INITIAL", self.s0)
        self.features.update(["IF", "scalar_state"])
        self.terms.update(
            principal=self.notional, coupon=self.coupon, trigger=self.strike
        )
        self.terms.pop("observation_dates")
        self.features.discard("multiple_observations")
        self.description = f"股票挂钩票据，本金为{self.notional}，到期另支付现金票息{number(self.coupon)}；若到期股价大于或等于{number(self.strike)}，全额返还本金，否则返还本金乘以到期股价/{number(self.strike)}。"
        self.event(
            self.maturity,
            f"IF {self.obs} >= {number(self.strike)} THEN payoff PAYS {self.notional} + {number(self.coupon)} ELSE payoff PAYS {self.notional} * {self.obs} / {number(self.strike)} + {number(self.coupon)} END",
        )

    def build_forward_start_call(self):
        self.description = self.observe("")
        self.definition("INITIAL", self.s0)
        self.features.update(["IF", "scalar_state"])
        reset = self.observations[0]
        self.terms.update(
            reset_date=reset.isoformat(), moneyness=round(0.9 + 0.002 * self.variant, 4)
        )
        self.terms.pop("observation_dates")
        self.event(
            reset, f"resetstrike = {number(self.terms['moneyness'])} * {self.obs}"
        )
        self.event(
            self.maturity,
            f"payoff PAYS {number(self.quantity)} * MAX({self.obs} - resetstrike, 0)",
        )
        self.features.add("MAX")
        self.description = f"远期起始看涨，在{chinese_date(reset)}将行权价设为当日股价的{number(self.terms['moneyness'])}倍，到期每份支付股价超过该行权价的部分。"

    def build_delayed_settlement_call(self):
        self.definition("STRIKE", self.strike)
        self.features.add("MAX")
        payment_date = self.maturity + timedelta(days=2 + self.variant % 9)
        self.terms["payment_date"] = payment_date.isoformat()
        self.event(
            self.maturity,
            f"payoff PAYS {number(self.quantity)} * MAX({self.obs} - STRIKE, 0) ON {payment_date}",
        )
        self.features.add("delayed_payment_ON")
        self.description = f"行权价为{number(self.strike)}的欧式看涨，到期确定每份股价超过行权价的支付额，于{chinese_date(payment_date)}现金结算，期间不重新观察股价。"

    def build_historical_fix_call(self):
        self.definition("STRIKE", self.strike)
        self.features.add("MAX")
        historical_date = EVALUATION_DATE - timedelta(days=5 + self.variant % 23)
        historical_price = round(self.s0 * (0.92 + 0.001 * self.variant), 4)
        multiplier = round(0.9 + 0.0015 * self.variant, 4)
        self.terms.update(
            historical_date=historical_date.isoformat(),
            historical_price=historical_price,
            reset_multiplier=multiplier,
        )
        self.model["fixings"] = {
            "EQ[ABC]": {historical_date.isoformat(): historical_price}
        }
        self.event(
            self.maturity,
            f"historicalstrike = {number(multiplier)} * FIX(EQ[ABC], {historical_date})\npayoff PAYS {number(self.quantity)} * MAX({self.obs} - historicalstrike, 0)",
        )
        self.features.update(
            ["explicit_fixing_date", "historical_snapshot", "scalar_state"]
        )
        self.description = f"历史基准看涨，{chinese_date(historical_date)}的已知股票定盘价为{number(historical_price)}；行权价为该历史价格的{number(multiplier)}倍，到期每份支付股价超过这一行权价的部分。"

    def build_weighted_option_strip(self):
        self.features.update(["FOR", "vector_indexing"])
        strikes = [
            self.strike,
            round(self.strike + self.spread, 4),
            round(self.strike + 2 * self.spread, 4),
        ]
        weights = [1, 2, 3]
        self.definition("STRIKES", "[" + ", ".join(map(number, strikes)) + "]")
        self.definition("WEIGHTS", "[1, 2, 3]")
        self.definition("NOPTIONS", 3)
        self.features.add("constant_loop_bounds")
        self.terms.update(strikes=strikes, weights=weights)
        self.event(
            self.maturity,
            f"FOR(i, 0, NOPTIONS) payoff PAYS {number(self.quantity)} * WEIGHTS[i] * MAX({self.obs} - STRIKES[i], 0) END",
        )
        self.description = f"三档看涨组合，行权价依次为{strikes}，对应买入数量权重为{weights}，到期支付三档期权收益的加权和。"
        self.features.add("MAX")

    def build_nested_loop_portfolio(self):
        self.features.update(["FOR", "vector_indexing"])
        strikes = [
            self.strike,
            round(self.strike + self.spread, 4),
            round(self.strike + 2 * self.spread, 4),
        ]
        weights = [1, 2, 3]
        self.definition("STRIKES", "[" + ", ".join(map(number, strikes)) + "]")
        self.definition("WEIGHTS", "[1, 2, 3]")
        self.definition("NOPTIONS", 3)
        self.features.add("constant_loop_bounds")
        self.terms.update(strikes=strikes, weights=weights)
        self.definition("SHIFTS", "[0, " + number(self.spread / 2) + "]")
        self.terms["shifts"] = [0, self.spread / 2]
        self.event(
            self.maturity,
            f"FOR(i, 0, NOPTIONS)\n FOR(j, 0, 2) payoff PAYS {number(self.quantity)} * WEIGHTS[i] * MAX({self.obs} - STRIKES[i] - SHIFTS[j], 0) END\nEND",
        )
        self.features.add("nested_FOR")
        self.description = f"六档看涨组合，三个基础行权价为{strikes}，权重为{weights}；每个基础行权价分别加上0和{number(self.spread / 2)}形成两档，并按该基础行权价的权重买入，支付六档收益之和。"
        self.features.add("MAX")

    def build_sparse_vector_basket(self):
        self.features.update(["FOR", "vector_indexing"])
        self.description = (
            self.observe("")
            + f"从全部观察日期中仅取第1次、第2次和最后一次价格，按[1, 2, 3]加权后除以6，形成行权价为{number(self.strike)}的看涨支付。"
        )
        self.terms["selected_observations"] = [0, 1, self.count - 1]
        self.definition("FIRST", 0)
        self.event(self.observations[0], f"values[FIRST] = {self.obs}")
        self.event(self.observations[1], f"values[2] = {self.obs}")
        self.event(
            self.maturity,
            f"values[4] = {self.obs}\npayoff PAYS {number(self.quantity)} * MAX((values[0] + 2 * values[2] + 3 * values[4]) / 6 - {number(self.strike)}, 0)",
        )
        self.features.update(
            ["mutable_vectors", "vector_assignment", "zero_padding", "MAX"]
        )
        self.features.discard("FOR")

    def build_conditional_vector(self):
        self.features.update(["FOR", "vector_indexing"])
        self.description = (
            self.observe("")
            + f"条件平均价格看涨：每次观察价严格高于{number(self.strike)}时将该价格计入样本，否则计入零；使用全部样本的算术平均值，支付其超过{number(self.lower)}的部分。"
        )
        self.terms["average_strike"] = self.lower
        self.vector_events(
            f"IF {self.obs} > {number(self.strike)} THEN APPEND(values, {self.obs}) ELSE APPEND(values, 0) END",
            f"payoff PAYS {number(self.quantity)} * MAX(AVERAGE(values) - {number(self.lower)}, 0)",
        )
        self.features.update(["IF", "ELSE", "AVERAGE", "MAX", ">"])
        self.features.discard("FOR")
        self.features.discard("vector_indexing")

    def build_schedule_asian(self):
        self.features.update(["FOR", "vector_indexing"])
        start = EVALUATION_DATE + timedelta(days=20 + self.variant)
        schedule_count = 3 + self.variant % 9
        self.maturity = start + timedelta(days=7 * schedule_count)
        self.observations = [
            start + timedelta(days=7 * (i + 1)) for i in range(schedule_count)
        ]
        schedule = f"START: {start}\nEND: {self.maturity}\nFREQ: 7CD\nBizRule: Unadjusted\nFIXING: END"
        self.terms.update(
            maturity=self.maturity.isoformat(),
            schedule_start=start.isoformat(),
            observation_dates=[d.isoformat() for d in self.observations],
        )
        self.features.difference_update(["FOR", "vector_indexing"])
        self.features.update(
            ["schedule_expansion", "PeriodBegin", "PeriodEnd", "unadjusted_calendar"]
        )
        self.definition("STRIKE", self.strike)
        self.event(schedule, "APPEND(values, FIX(EQ[ABC], PeriodEnd))")
        self.event(
            self.maturity,
            f"payoff PAYS {number(self.quantity)} * MAX(AVERAGE(values) - STRIKE, 0)",
        )
        self.features.update(
            ["mutable_vectors", "APPEND", "AVERAGE", "MAX", "explicit_fixing_date"]
        )
        self.description = f"行权价为{number(self.strike)}的算术平均亚式看涨；从{chinese_date(start)}开始，每7个日历日生成一期，共{schedule_count}期，每期期末观察股价，不作营业日调整，最后一期期末结算。起始日价格不计入平均。"

    def build_schedule_fixed_coupon(self):
        self.features.update(["FOR", "vector_indexing"])
        start = EVALUATION_DATE + timedelta(days=20 + self.variant)
        schedule_count = 3 + self.variant % 9
        self.maturity = start + timedelta(days=7 * schedule_count)
        self.observations = [
            start + timedelta(days=7 * (i + 1)) for i in range(schedule_count)
        ]
        schedule = f"START: {start}\nEND: {self.maturity}\nFREQ: 7CD\nBizRule: Unadjusted\nFIXING: END"
        self.terms.update(
            maturity=self.maturity.isoformat(),
            schedule_start=start.isoformat(),
            observation_dates=[d.isoformat() for d in self.observations],
        )
        self.features.difference_update(["FOR", "vector_indexing"])
        self.features.update(
            ["schedule_expansion", "PeriodBegin", "PeriodEnd", "unadjusted_calendar"]
        )
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_365F"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("COUPON", self.rate_strike)
        if self.variant % 2:
            schedule = schedule.replace("FIXING: END", "FIXING: BEGIN")
            self.event(
                schedule,
                "payoff PAYS NOTIONAL * COUPON * DCF(ACT_365F, PeriodBegin, PeriodEnd) ON PeriodEnd",
            )
            self.features.update(["schedule_fixing_BEGIN", "delayed_payment_ON"])
        else:
            self.event(
                schedule,
                "payoff PAYS NOTIONAL * COUPON * DCF(ACT_365F, PeriodBegin, PeriodEnd)",
            )
        self.features.update(["DCF", "multiple_payments"])
        self.description = f"固定票息合约，名义本金为{self.notional}，年票息率为{number(self.rate_strike)}；从{chinese_date(start)}起每7个日历日一期，共{schedule_count}期，按ACT/365F计算每期利息并在期末支付，无本金返还，不作营业日调整。"

    def build_bermudan_put(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX(STRIKE - {self.obs}, 0)"
        condition = ""
        for dt in self.observations:
            self.event(
                dt, f"EXERCISE {number(self.quantity)} * {expression}{condition}"
            )
        self.description = f"百慕大看跌，行权价为{number(self.strike)}，行权时支付普通期权内在价值并终止。"
        self.description += (
            "允许行权日期仅为"
            + "、".join(chinese_date(d) for d in self.observations)
            + "，到期未行权则作废。"
        )

    def build_bermudan_call(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.model["div"] = 0.06
        self.settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX({self.obs} - STRIKE, 0)"
        condition = ""
        for dt in self.observations:
            self.event(
                dt, f"EXERCISE {number(self.quantity)} * {expression}{condition}"
            )
        self.description = f"百慕大看涨，行权价为{number(self.strike)}，行权时支付普通期权内在价值并终止。"
        self.description += (
            "允许行权日期仅为"
            + "、".join(chinese_date(d) for d in self.observations)
            + "，到期未行权则作废。"
        )

    def build_bermudan_asian_put(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.settings["regression_features"] = ["EQ[ABC]"]
        self.settings["regression_features"] = ["EQ[ABC]", "VAR[meanprice]"]
        self.features.update(
            [
                "mutable_vectors",
                "APPEND",
                "AVERAGE",
                "regression_features",
                "scalar_state",
            ]
        )
        for dt in self.observations:
            self.event(
                dt,
                f"APPEND(values, {self.obs})\nmeanprice = AVERAGE(values)\nEXERCISE {number(self.quantity)} * MAX(STRIKE - meanprice, 0)",
            )
        self.description = f"百慕大亚式看跌，行权价为{number(self.strike)}，在每个允许行权日先将当日股价计入历史观察序列；若行权，每份支付行权价超过截至当日观察价格算术平均值的部分，行权后终止。"
        self.description += (
            "允许行权日期仅为"
            + "、".join(chinese_date(d) for d in self.observations)
            + "，到期未行权则作废。"
        )

    def build_conditional_bermudan_put(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX(STRIKE - {self.obs}, 0)"
        condition = ""
        self.terms["exercise_limit"] = round(self.strike * 0.96, 4)
        condition = f" IF {self.obs} < {number(self.terms['exercise_limit'])}"
        self.features.add("exercise_IF")
        for dt in self.observations:
            self.event(
                dt, f"EXERCISE {number(self.quantity)} * {expression}{condition}"
            )
        self.description = f"百慕大看跌，行权价为{number(self.strike)}，行权时支付普通期权内在价值并终止。"
        if condition:
            self.description += f"每个行权日（包括到期日）仅在股价严格低于{number(self.terms['exercise_limit'])}时允许行权；未能行权即到期作废。"
        self.description += (
            "允许行权日期仅为"
            + "、".join(chinese_date(d) for d in self.observations)
            + "，到期未行权则作废。"
        )

    def build_american_put_daily(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        daily_count = 60 + self.variant % 40
        self.maturity = EVALUATION_DATE + timedelta(days=daily_count)
        self.observations = [
            EVALUATION_DATE + timedelta(days=i) for i in range(1, daily_count + 1)
        ]
        self.terms["maturity"] = self.maturity.isoformat()
        self.terms["exercise_grid"] = "daily_calendar"
        self.notes.append("按每日网格近似美式行权；不是连续时间美式价格。")
        self.features.add("american_daily_approximation")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX(STRIKE - {self.obs}, 0)"
        condition = ""
        for dt in self.observations:
            self.event(
                dt, f"EXERCISE {number(self.quantity)} * {expression}{condition}"
            )
        self.description = f"美式看跌，行权价为{number(self.strike)}，行权时支付普通期权内在价值并终止。"
        self.description += f"用每日网格近似：从{chinese_date(self.observations[0])}至{chinese_date(self.maturity)}每个日历日允许行权，包括非营业日；估值日不在行权网格内。"

    def build_american_call_daily(self):
        self.definition("STRIKE", self.strike)
        self.features.update(["EXERCISE", "LSMC", "MAX", "multiple_exercise_dates"])
        self.features.discard("PAYS")
        daily_count = 60 + self.variant % 40
        self.maturity = EVALUATION_DATE + timedelta(days=daily_count)
        self.observations = [
            EVALUATION_DATE + timedelta(days=i) for i in range(1, daily_count + 1)
        ]
        self.terms["maturity"] = self.maturity.isoformat()
        self.terms["exercise_grid"] = "daily_calendar"
        self.notes.append("按每日网格近似美式行权；不是连续时间美式价格。")
        self.features.add("american_daily_approximation")
        self.terms["exercise_dates"] = [dt.isoformat() for dt in self.observations]
        self.model["div"] = 0.06
        self.settings["regression_features"] = ["EQ[ABC]"]
        expression = f"MAX({self.obs} - STRIKE, 0)"
        condition = ""
        for dt in self.observations:
            self.event(
                dt, f"EXERCISE {number(self.quantity)} * {expression}{condition}"
            )
        self.description = f"美式看涨，行权价为{number(self.strike)}，行权时支付普通期权内在价值并终止。"
        self.description += f"用每日网格近似：从{chinese_date(self.observations[0])}至{chinese_date(self.maturity)}每个日历日允许行权，包括非营业日；估值日不在行权网格内。"

    def build_rate_fixed_bond(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        self.observations = [
            start + timedelta(days=90 * (i + 1)) for i in range(self.count)
        ]
        self.maturity = self.observations[-1]
        self.terms.update(
            start_date=start.isoformat(),
            maturity=self.maturity.isoformat(),
            payment_dates=[d.isoformat() for d in self.observations],
            day_basis="ACT_365F",
        )
        previous = start
        for i, dt in enumerate(self.observations):
            self.event(
                dt,
                f"payoff PAYS NOTIONAL * RATESTRIKE * DCF(ACT_365F, {previous}, {dt})"
                + ("\npayoff PAYS NOTIONAL" if i == self.count - 1 else ""),
            )
            previous = dt
        self.features.discard("FIX")
        self.features.update(["DCF", "multiple_payments"])
        self.description = f"美元固定票息债券，本金{self.notional}、年票息率{number(self.rate_strike)}，从{chinese_date(start)}起每90个日历日一期，共{self.count}期，采用ACT/365F，期末付息并在最后一期返还本金。"

    def build_floating_note(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        periods = self.count
        fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
        payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
        self.terms.update(
            fixing_dates=[d.isoformat() for d in fixing_dates],
            payment_dates=[d.isoformat() for d in payment_dates],
            maturity=payment_dates[-1].isoformat(),
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
            expression = libor
            self.event(
                fixing,
                f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}",
            )
            if i == periods - 1:
                self.event(payment, "payoff PAYS NOTIONAL")
        self.features.update(
            ["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"]
        )
        names = {
            "floating_note": "浮动利率票据",
            "caplet": "单期利率上限",
            "floorlet": "单期利率下限",
            "cap": "多期利率上限",
            "payer_swap": "支付固定利率互换",
        }
        descriptions = {
            "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
            "caplet": f"支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "floorlet": f"支付{number(self.rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
            "cap": f"每期支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "payer_swap": f"每期收浮动利率、付固定利率{number(self.rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
        }
        self.description = f"美元{names['floating_note']}，名义本金{self.notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions['floating_note']}。"

    def build_caplet(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        periods = 1
        fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
        payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
        self.terms.update(
            fixing_dates=[d.isoformat() for d in fixing_dates],
            payment_dates=[d.isoformat() for d in payment_dates],
            maturity=payment_dates[-1].isoformat(),
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
            expression = libor
            expression = f"MAX({libor} - RATESTRIKE, 0)"
            self.event(
                fixing,
                f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}",
            )
        self.features.update(
            ["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"]
        )
        self.features.add("MAX")
        names = {
            "floating_note": "浮动利率票据",
            "caplet": "单期利率上限",
            "floorlet": "单期利率下限",
            "cap": "多期利率上限",
            "payer_swap": "支付固定利率互换",
        }
        descriptions = {
            "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
            "caplet": f"支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "floorlet": f"支付{number(self.rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
            "cap": f"每期支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "payer_swap": f"每期收浮动利率、付固定利率{number(self.rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
        }
        self.description = f"美元{names['caplet']}，名义本金{self.notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions['caplet']}。"

    def build_floorlet(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        periods = 1
        fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
        payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
        self.terms.update(
            fixing_dates=[d.isoformat() for d in fixing_dates],
            payment_dates=[d.isoformat() for d in payment_dates],
            maturity=payment_dates[-1].isoformat(),
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
            expression = libor
            expression = f"MAX(RATESTRIKE - {libor}, 0)"
            self.event(
                fixing,
                f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}",
            )
        self.features.update(
            ["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"]
        )
        self.features.add("MAX")
        names = {
            "floating_note": "浮动利率票据",
            "caplet": "单期利率上限",
            "floorlet": "单期利率下限",
            "cap": "多期利率上限",
            "payer_swap": "支付固定利率互换",
        }
        descriptions = {
            "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
            "caplet": f"支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "floorlet": f"支付{number(self.rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
            "cap": f"每期支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "payer_swap": f"每期收浮动利率、付固定利率{number(self.rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
        }
        self.description = f"美元{names['floorlet']}，名义本金{self.notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions['floorlet']}。"

    def build_cap(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        periods = self.count
        fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
        payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
        self.terms.update(
            fixing_dates=[d.isoformat() for d in fixing_dates],
            payment_dates=[d.isoformat() for d in payment_dates],
            maturity=payment_dates[-1].isoformat(),
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
            expression = libor
            expression = f"MAX({libor} - RATESTRIKE, 0)"
            self.event(
                fixing,
                f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}",
            )
        self.features.update(
            ["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"]
        )
        self.features.add("MAX")
        names = {
            "floating_note": "浮动利率票据",
            "caplet": "单期利率上限",
            "floorlet": "单期利率下限",
            "cap": "多期利率上限",
            "payer_swap": "支付固定利率互换",
        }
        descriptions = {
            "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
            "caplet": f"支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "floorlet": f"支付{number(self.rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
            "cap": f"每期支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "payer_swap": f"每期收浮动利率、付固定利率{number(self.rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
        }
        self.description = f"美元{names['cap']}，名义本金{self.notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions['cap']}。"

    def build_rate_corridor(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        low, high = (
            round(0.005 + 0.0001 * self.variant, 6),
            round(0.04 + 0.0001 * self.variant, 6),
        )
        self.terms.update(
            rate_lower=low,
            rate_upper=high,
            coupon=self.coupon,
            observation_dates=[d.isoformat() for d in self.observations],
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for dt in self.observations:
            self.event(
                dt,
                f"IF {libor} >= {number(low)} AND {libor} <= {number(high)} THEN payoff PAYS {number(self.coupon)} ELSE payoff PAYS 0 END",
            )
        self.features.update(["IR_LIBOR", "AND", "IF", "multiple_payments"])
        self.description = (
            "美元利率区间现金票息，观察日期为"
            + "、".join(chinese_date(d) for d in self.observations)
            + f"；每次USD LIBOR 3M LCH定盘利率落在闭区间[{number(low)}, {number(high)}]时立即支付{number(self.coupon)}，否则支付零，无本金返还。"
        )

    def build_payer_swap(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        start = EVALUATION_DATE + timedelta(days=30 + self.variant)
        periods = self.count
        fixing_dates = [start + timedelta(days=90 * i) for i in range(periods)]
        payment_dates = [dt + timedelta(days=90) for dt in fixing_dates]
        self.terms.update(
            fixing_dates=[d.isoformat() for d in fixing_dates],
            payment_dates=[d.isoformat() for d in payment_dates],
            maturity=payment_dates[-1].isoformat(),
            index="IR[USD,LIBOR_3M_LCH]",
        )
        for i, (fixing, payment) in enumerate(zip(fixing_dates, payment_dates)):
            expression = libor
            expression = f"{libor} - RATESTRIKE"
            self.event(
                fixing,
                f"payoff PAYS NOTIONAL * DCF(ACT_360, {fixing}, {payment}) * ({expression}) ON {payment}",
            )
        self.features.update(
            ["DCF", "IR_LIBOR", "delayed_payment_ON", "multiple_payments"]
        )
        names = {
            "floating_note": "浮动利率票据",
            "caplet": "单期利率上限",
            "floorlet": "单期利率下限",
            "cap": "多期利率上限",
            "payer_swap": "支付固定利率互换",
        }
        descriptions = {
            "floating_note": "每期支付该期定盘利率乘以名义本金及计息年数，最后一期另返还本金",
            "caplet": f"支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "floorlet": f"支付{number(self.rate_strike)}超过定盘利率的正差额乘以名义本金及计息年数，无本金返还",
            "cap": f"每期支付定盘利率超过{number(self.rate_strike)}的正差额乘以名义本金及计息年数，无本金返还",
            "payer_swap": f"每期收浮动利率、付固定利率{number(self.rate_strike)}，净支付为浮动减固定乘以名义本金及计息年数，无本金交换",
        }
        self.description = f"美元{names['payer_swap']}，名义本金{self.notional}，从{chinese_date(start)}开始，每90个日历日一期，共{periods}期。每期期初观察该日USD LIBOR 3M LCH指数，按ACT/360计算实际90天的票息，在该期期末结算；{descriptions['payer_swap']}。"

    def build_european_payer_swaption(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        first, second = (
            self.maturity + timedelta(days=182),
            self.maturity + timedelta(days=365),
        )
        swap_index = f"IR[USD,SWAP,1Y,{self.maturity}]"
        self.terms.update(
            swap_index=swap_index,
            annuity_dates=[first.isoformat(), second.isoformat()],
            annuity_weights=[0.5, 0.5],
        )
        self.event(
            self.maturity,
            f"payoff PAYS NOTIONAL * MAX(FIX({swap_index}) - RATESTRIKE, 0) * (0.5 * FIX(IR[USD,DF,{first}]) + 0.5 * FIX(IR[USD,DF,{second}]))",
        )
        self.features.update(["IR_SWAP", "IR_DF", "MAX"])
        self.description = f"美元欧式现金结算支付方互换期权，名义本金{self.notional}，行权利率{number(self.rate_strike)}；到期观察起息日为到期日的一年期美元标准互换利率，支付其超过行权利率的部分乘以现金结算年金。年金明确约定为0.5乘以到期日至{chinese_date(first)}的零息债价格，加0.5乘以到期日至{chinese_date(second)}的零息债价格；现金在到期日支付。"
        self.notes.append(
            "现金结算年金日期和权重为显式合同约定，不代表实物交割标准互换。"
        )

    def build_bermudan_bond_call(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        bond_maturity = self.maturity + timedelta(days=365 + self.variant)
        bond_strike = round(0.88 + 0.0008 * self.variant, 6)
        bond_index = f"IR[USD,DF,{bond_maturity}]"
        self.terms.update(
            bond_maturity=bond_maturity.isoformat(),
            bond_strike=bond_strike,
            exercise_dates=[d.isoformat() for d in self.observations],
        )
        self.settings["regression_features"] = [bond_index]
        for dt in self.observations:
            self.event(
                dt,
                f"EXERCISE NOTIONAL * MAX(FIX({bond_index}) - {number(bond_strike)}, 0)",
            )
        self.features.discard("PAYS")
        self.features.update(
            [
                "IR_DF",
                "EXERCISE",
                "LSMC",
                "regression_features",
                "MAX",
                "multiple_exercise_dates",
            ]
        )
        self.description = (
            f"美元百慕大零息债券看涨，名义本金{self.notional}，债券到期日为{chinese_date(bond_maturity)}，每单位债券价格行权价为{number(bond_strike)}。仅在"
            + "、".join(chinese_date(d) for d in self.observations)
            + "允许行权，行权时现金支付债券价格超过行权价的部分乘以名义本金并终止；最后行权日未行权即作废。"
        )

    def build_rate_vector_average(self):
        self.model = {
            "type": "gsr",
            "currency": "USD",
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
        }
        self.terms.update(
            notional=self.notional, fixed_rate=self.rate_strike, day_basis="ACT_360"
        )
        self.definition("NOTIONAL", self.notional)
        self.definition("RATESTRIKE", self.rate_strike)
        self.features.add("stochastic_rates_GSR")
        libor = "FIX(IR[USD,LIBOR_3M_LCH])"
        self.terms.update(
            observation_dates=[d.isoformat() for d in self.observations],
            index="IR[USD,LIBOR_3M_LCH]",
        )
        self.vector_events(
            f"APPEND(rates, {libor})",
            "payoff PAYS NOTIONAL * MAX(AVERAGE(rates) - RATESTRIKE, 0)",
        )
        self.features.update(["IR_LIBOR", "AVERAGE", "MAX"])
        self.description = (
            f"美元平均利率看涨，名义本金{self.notional}，行权利率{number(self.rate_strike)}；在"
            + "、".join(chinese_date(d) for d in self.observations)
            + "观察USD LIBOR 3M LCH利率，最后观察日支付观察利率算术平均值超过行权利率的部分乘以名义本金，不再乘计息年数。"
        )

    def build_multi_asset_basket(self):
        self.features.add("MAX")
        other_spot = round(self.s0 * 1.2, 4)
        weight = round(0.25 + 0.005 * self.variant, 4)
        self.terms.update(other_initial_price=other_spot, basket_weight=weight)
        self.model = {
            "type": "correlated_bs",
            "indices": ["EQ[ABC]", "EQ[XYZ]"],
            "spots": [self.s0, other_spot],
            "vols": [0.2, 0.25],
            "divs": [0.01, 0.02],
            "rate": 0.025,
            "correlation": 0.35,
        }
        self.definition("STRIKE", self.strike)
        self.event(
            self.maturity,
            f"payoff PAYS {number(self.quantity)} * MAX({number(weight)} * FIX(EQ[ABC]) + {number(1 - weight)} * FIX(EQ[XYZ]) - STRIKE, 0)",
        )
        self.features.update(["multi_asset", "correlated_BS", "multiple_named_indices"])
        self.description = f"双股票篮子欧式看涨，ABC和XYZ当前股价分别为{number(self.s0)}和{number(other_spot)}；篮子价格为{number(weight)}乘以ABC股价加{number(1 - weight)}乘以XYZ股价，行权价为{number(self.strike)}，每份支付到期篮子价格超过行权价的部分。"

    def build_hybrid_equity_rate(self):
        self.features.add("MAX")
        bond_maturity = self.maturity + timedelta(days=365)
        self.terms["bond_maturity"] = bond_maturity.isoformat()
        self.model = {
            "type": "hybrid_equity_gsr",
            "spot": self.s0,
            "vol": 0.2,
            "div": 0.01,
            "curve_rate": 0.025,
            "g": 0.012,
            "h": 1.0,
            "curve_horizon": "2046-10-04",
            "correlation": -0.2,
        }
        self.definition("STRIKE", self.strike)
        self.event(
            self.maturity,
            f"payoff PAYS {number(self.quantity)} * MAX({self.obs} - STRIKE, 0) * FIX(IR[USD,DF,{bond_maturity}])",
        )
        self.features.update(["hybrid_equity_GSR", "IR_DF", "multiple_named_indices"])
        self.description = f"股票利率混合支付产品，ABC当前股价为{number(self.s0)}，股票行权价为{number(self.strike)}；每份到期支付股票看涨内在价值，乘以当时美元零息债券价格，该债券在{chinese_date(bond_maturity)}到期。支付当日还需按美元随机利率折现。"

    def update_features(self):
        script_text = "\n".join(row["script"] for row in self.rows)
        for keyword in (
            "FIX",
            "SPOT",
            "PAYS",
            "MAX",
            "MIN",
            "IF",
            "ELSE",
            "AND",
            "OR",
            "FOR",
            "APPEND",
            "AVERAGE",
            "SUM",
            "LOG",
            "EXP",
            "SQRT",
            "DCF",
            "EXERCISE",
            "PeriodBegin",
            "PeriodEnd",
        ):
            if re.search("\\b" + keyword + "\\b", script_text):
                self.features.add(keyword)
            else:
                self.features.discard(keyword)
        if self.settings.get("regression_features"):
            self.features.add("regression_features")
        for comparator in ("!=", ">=", "<=", ">", "<", "="):
            if comparator not in script_text:
                self.features.discard(comparator)

    def question_parts(self):
        if self.model["type"] in ("gsr",):
            prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，采用美元结算。"
            multiplier = ""
        elif self.family == "schedule_fixed_coupon":
            prefix = f"估值日为{chinese_date(EVALUATION_DATE)}。"
            multiplier = ""
        elif self.family in ("autocall", "memory_coupon", "equity_linked_note"):
            prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，标的是ABC股票，当前价格为{number(self.s0)}。"
            multiplier = ""
        else:
            prefix = f"估值日为{chinese_date(EVALUATION_DATE)}，标的是ABC股票，当前价格为{number(self.s0)}。"
            multiplier = f"持有{number(self.quantity)}份，每份按上述支付规则计算。"
        if self.family == "multi_asset_basket":
            prefix = f"估值日为{chinese_date(EVALUATION_DATE)}。"
        return (prefix, multiplier)

    def record(self):
        if not self.description or not self.rows:
            raise ValueError(f"Unimplemented family {self.family}")
        self.update_features()
        prefix, multiplier = self.question_parts()
        question = (
            prefix
            + self.description
            + f"最终到期日为{chinese_date(self.terms['maturity'])}。"
            + multiplier
            + "所有日期均使用约定的日历日期，支付为现金结算。请给出对应的DAL事件表。"
        )
        answer = markdown_table(
            ["日期 / 定义", "DAL script / 数值"],
            [[f"`{r['date_or_definition']}`", f"`{r['script']}`"] for r in self.rows],
        )
        if parse_answer(answer) != self.rows:
            raise ValueError("Markdown round trip failed")
        identity = {"family": self.family, "terms": self.terms}
        return {
            "id": f"DAL-QA-{self.family_index * 100 + self.variant + 1:05d}",
            "question": question,
            "answer": answer,
            "category": self.category,
            "product_type": self.label,
            "family": self.family,
            "features": sorted(self.features),
            "events": self.rows,
            "contract_terms": self.terms,
            "product_settings": self.settings,
            "pricing_context": {
                "evaluation_date": EVALUATION_DATE.isoformat(),
                "model": self.model,
                "recommended_simulation": {
                    "method": "sobol",
                    "compiled": True,
                    "n_paths": 16384,
                    "lsmc_training_paths": 8192,
                    "lsmc_basis_degree": 3,
                },
            },
            "limitations": self.notes,
            "contract_fingerprint": digest(identity),
        }


PRODUCT_BUILDERS = {
    family: getattr(ContractBuilder, "build_" + family) for family, _, _ in FAMILIES
}


def build_record(family_index, variant):
    builder = ContractBuilder(family_index, variant)
    PRODUCT_BUILDERS[builder.family](builder)
    return builder.record()


def write_index(records):
    counts = Counter(record["product_type"] for record in records)
    group_counts = Counter(record["category"] for record in records)
    feature_counts = Counter()
    for record in records:
        feature_counts.update(record["features"])
    lines = [
        "# DAL 自然语言产品与脚本问答索引",
        "",
        "数据：[qa.jsonl](qa.jsonl)。UTF-8 JSONL，每个物理行是一条完整问答，问题为自然语言，答案为 Markdown 事件表。",
        "",
        f"共 {len(records)} 条，{len(counts)} 种产品类型。每种类型 100 个不同条款组合。当前价格在定价上下文中提供；事件表读取各观察日价格。",
        "",
        "表中的记录链接以 JSONL 物理行号定位；若查看器不支持 `#L`，使用同列行号或记录 ID 查找。",
        "",
        "美式类型采用明确的每日行权网格近似。利率使用 GSR，双股票使用相关 BS，股票利率混合使用 Hybrid；不能将所有脚本都交给单股票 BS 模型。",
        "",
        "验证方法及复现命令见 [README.md](README.md)，逐条结果见 [validation.jsonl](validation.jsonl)，统计见 [validation_summary.json](validation_summary.json)。",
        "",
        "## 分类统计",
        "",
        markdown_table(
            ["分类", "数量"], [[k, str(v)] for k, v in group_counts.items()]
        ),
        "",
        "## 产品类型统计",
        "",
        markdown_table(["产品类型", "数量"], [[k, str(v)] for k, v in counts.items()]),
        "",
        "## 功能覆盖",
        "",
        markdown_table(
            ["功能", "涉及问答数"],
            [[k, str(v)] for k, v in sorted(feature_counts.items())],
        ),
        "",
        "## 逐条分类总表",
        "",
    ]
    table_rows = [
        [
            f"[{r['id']}](qa.jsonl#L{i})",
            str(i),
            r["category"],
            r["product_type"],
            "、".join(r["features"]),
        ]
        for i, r in enumerate(records, 1)
    ]
    lines.append(
        markdown_table(
            ["记录 / JSONL 链接", "JSONL 行", "分类", "产品类型", "主要功能点"],
            table_rows,
        )
    )
    (ROOT / "index.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def all_records():
    return [
        build_record(family_index, variant)
        for family_index in range(len(FAMILIES))
        for variant in range(100)
    ]


def generate():
    records = all_records()
    if len(records) != 7000:
        raise ValueError("Expected exactly 7000 records")
    for key in ("id", "question", "answer", "contract_fingerprint"):
        if len({r[key] for r in records}) != len(records):
            raise ValueError(f"Duplicate {key}")
    with (ROOT / "qa.jsonl").open("w", encoding="utf-8") as stream:
        for record in records:
            stream.write(
                json.dumps(
                    record, ensure_ascii=False, separators=(",", ":"), allow_nan=False
                )
                + "\n"
            )
    write_index(records)
    print(
        json.dumps(
            {
                "records": len(records),
                "families": len(FAMILIES),
                "categories": dict(Counter(r["category"] for r in records)),
                "output": str(ROOT / "qa.jsonl"),
            },
            ensure_ascii=False,
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.parse_args()
    generate()
