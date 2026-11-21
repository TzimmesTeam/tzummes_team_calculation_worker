import datetime as dt
import json
import shutil
import subprocess
import sys
from pathlib import Path

import pandas as pd

HERE = Path(__file__).parent
EPOCH = dt.date(1970, 1, 1)
RESULT_DIR = HERE / "result"
CALC = HERE / ("worker.exe" if sys.platform.startswith("win") else "worker")

KEY_RATES = [
    ("2020-01-01", 6.25), ("2020-03-01", 6.0), ("2020-05-01", 5.5), ("2020-07-01", 4.5),
    ("2020-08-01", 4.25), ("2021-04-01", 4.5), ("2021-05-01", 5.0), ("2021-07-01", 5.5),
    ("2021-08-01", 6.5), ("2021-10-01", 6.75), ("2021-11-01", 7.5), ("2022-01-01", 8.5),
    ("2022-03-01", 20.0), ("2022-05-01", 14.0), ("2022-06-01", 11.0), ("2022-07-01", 9.5),
    ("2022-08-01", 8.0), ("2022-10-01", 7.5), ("2023-08-01", 8.5), ("2023-09-01", 12.0),
    ("2023-10-01", 13.0), ("2023-11-01", 15.0), ("2024-01-01", 16.0), ("2024-08-01", 18.0),
    ("2024-10-01", 19.0), ("2024-11-01", 21.0), ("2025-07-01", 20.0), ("2025-08-01", 18.0),
    ("2025-10-01", 17.0), ("2025-11-01", 16.5), ("2026-01-01", 16.0), ("2026-03-01", 15.5),
    ("2026-04-01", 15.0), ("2026-05-01", 14.5), ("2026-07-01", 14.25), ("2026-08-01", 14.0),
]

DISC = {"ANN": 1, "RGM": 2, "SIMPLE": 3}
RATE_TYPE = {"FIXED": 1, "FLOAT": 2}
DAY_CONV = {"ACT/ACT": 1, "ACT/ACT ISDA": 2, "ACT/365": 3}

def find_deals_file():
    for f in sorted(HERE.glob("*.xlsx")):
        if not f.name.startswith("Расчеты") and not f.name.startswith("~$"):
            return f
    raise FileNotFoundError()


def is_empty(v):
    return v is None or (not isinstance(v, str) and pd.isna(v))


def to_date(v):
    if is_empty(v):
        return None
    if isinstance(v, (pd.Timestamp, dt.datetime)):
        return v.date()
    if isinstance(v, dt.date):
        return v
    s = str(v).strip()
    if not s:
        return None
    for fmt in ("%d.%m.%Y", "%Y-%m-%d", "%Y-%m-%d %H:%M:%S"):
        try:
            return dt.datetime.strptime(s[:19], fmt).date()
        except ValueError:
            pass
    raise ValueError()


def secs(v):
    d = to_date(v)
    return "" if d is None else str((d - EPOCH).days * 86400)


def dates_list(v):
    if is_empty(v):
        return []
    parts = str(v).split() if isinstance(v, str) else [v]
    return sorted({int(secs(p)) for p in parts if str(p).strip()})


def period_code(v):
    if is_empty(v):
        return ""
    s = str(v).strip().upper()
    if not s:
        return ""
    if s == "ENDDATE":
        return "0"
    sign = -1 if s.startswith("-") else 1
    s = s.lstrip("+-")
    n, unit = int(s[:-1]), s[-1]
    factor = {"M": 1, "Y": 12, "Q": 3}
    if unit in factor:
        code = factor[unit] * n
    elif unit == "D":
        code = 1000000 + n
    elif unit == "W":
        code = 1000000 + 7 * n
    else:
        raise ValueError()
    return str(sign * code)


def num(v):
    return "" if is_empty(v) else str(int(v))


def flt(v):
    return "" if is_empty(v) else repr(float(v))


def txt(v):
    return "" if is_empty(v) else str(v).strip()


def code(mapping, v):
    s = txt(v)
    return "" if not s else str(mapping[s])


def read_supplement(folder):
    files = sorted(folder.glob("Расчеты*.xlsx"))
    if not files:
        return {}
    try:
        df = pd.read_excel(files[0], sheet_name="Тестовые сделки", header=0)
    except (ValueError, KeyError):
        return {}
    out = {}
    for _, r in df.iterrows():
        if is_empty(r.get("deal_id")) or is_empty(r.get("principal_payment")):
            continue
        pairs = []
        for tok in str(r["principal_payment"]).split():
            d, _, a = tok.partition(",")
            if a:
                try:
                    pairs.append((int(secs(d)), float(a)))
                except ValueError:
                    pass
        if pairs:
            try:
                out[int(r["deal_id"])] = sorted(pairs)
            except (TypeError, ValueError):
                pass
    return out


def write_rates_json(path):
    series = [{"from": d, "value": pct / 100} for d, pct in KEY_RATES]
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"CBKEY1M": series}, f, ensure_ascii=False, indent=1)


def build_line(r, supplement):
    g = lambda name: r.get(name)
    deal_id = int(g("deal_id"))

    prin_dates = dates_list(g("principal_payment"))
    prin_amounts = []
    if prin_dates and deal_id in supplement:
        sup = supplement[deal_id]
        prin_dates = [d for d, _ in sup]
        prin_amounts = [repr(a) for _, a in sup]

    fields = [
        str(deal_id),
        secs(g("report_dttm")), secs(g("maturity_dttm")), secs(g("deal_dttm")),
        str(DISC[txt(g("discriminator"))]),
        code(RATE_TYPE, g("rate_type_cd")),
        flt(g("interest_rate")),
        flt(g("base_indicator_rate")), txt(g("base_indicator")),
        secs(g("interest_payment_rule_start_dttm")),
        period_code(g("interest_payment_rule_period_cd")),
        num(g("interest_payment_rule_day_num")),
        code(DAY_CONV, g("day_convention_cd")),
        secs(g("regular_payment_rule_start_dttm")),
        period_code(g("regular_payment_rule_period_cd")),
        num(g("regular_payment_rule_day_num")),
        secs(g("repricing_rule_start_dttm")),
        period_code(g("repricing_rule_period_cd")),
        num(g("repricing_rule_day_num")),
        flt(g("accrued_interest")),
        repr(float(g("current_principal_amt"))),
        ",".join(str(x) for x in dates_list(g("interest_payment"))),
        ",".join(str(x) for x in prin_dates),
        ",".join(prin_amounts),
        ",".join(str(x) for x in dates_list(g("repricing_dttm_schedule"))),
        "", "", "", "",
        flt(g("cap_limit_interest_rate")), flt(g("floor_limit_interest_rate")),
        period_code(g("indicator_lag_cd")),
        code(RATE_TYPE, g("sub_rate_type_cd")),
        flt(g("sub_interest_rate")), txt(g("sub_float_indicator")),
        flt(g("sub_coeff_index")),
        secs(g("sub_payment_rule_start_dttm")),
        period_code(g("sub_payment_rule_period_cd")),
        num(g("sub_payment_rule_day_num")),
        secs(g("sub_repricing_rule_start_dttm")),
        period_code(g("sub_repricing_rule_period_cd")),
        num(g("sub_repricing_rule_day_num")),
        flt(g("sub_cap_limit_interest_rate")), flt(g("sub_floor_limit_interest_rate")),
        period_code(g("sub_indicator_lag_cd")),
        txt(g("behavior_model_cd")),
    ]
    return ";".join(fields)


def main():
    if not CALC.exists():
        raise FileNotFoundError()

    excel = Path(sys.argv[1]) if len(sys.argv) > 1 else find_deals_file()
    df = pd.read_excel(excel, sheet_name=0)
    rows = [r for r in df.to_dict("records") if not is_empty(r.get("deal_id"))]
    supplement = read_supplement(HERE)

    write_rates_json(HERE / "rates.json")

    lines = []
    for r in rows:
        try:
            lines.append(build_line(r, supplement))
        except (ValueError, KeyError, TypeError):
            lines.append("")

    if RESULT_DIR.exists():
        shutil.rmtree(RESULT_DIR)

    res = subprocess.run([str(CALC), str(RESULT_DIR)],
                         input="\n".join(lines) + "\n",
                         capture_output=True, text=True, encoding="utf-8")
    print(res.stdout, end="")
    if res.returncode != 0:
        print(res.stderr, file=sys.stderr)


if __name__ == "__main__":
    main()