import subprocess
from pathlib import Path

import pandas as pd

HERE = Path(__file__).parent
EXCEL = next(HERE.glob("*.xlsx"))
CALC = HERE / "calc.exe" 

df = pd.read_excel(EXCEL, sheet_name=0)
df = df[df["deal_id"].notna()] 


def d(v):
    if pd.isna(v):
        return ""
    return v.strftime("%d.%m.%Y") if hasattr(v, "strftime") else str(v)


def s(v): 
    return "" if pd.isna(v) else str(v).strip()


def pct(v):         
    return "" if pd.isna(v) else repr(float(v) * 100)


def num(v):                 
    return "" if pd.isna(v) else str(int(v))


lines = []
for r in df.itertuples(index=False):
    lines.append(";".join([
        str(int(r.deal_id)), d(r.report_dttm), d(r.maturity_dttm), d(r.deal_dttm),
        s(r.discriminator), s(r.rate_type_cd), pct(r.interest_rate),
        s(r.regular_payment_rule_period_cd), s(r.interest_payment_rule_period_cd),
        s(r.day_convention_cd),
        d(r.interest_payment_rule_start_dttm), d(r.regular_payment_rule_start_dttm),
        repr(float(r.current_principal_amt)),
        d(r.repricing_rule_start_dttm), s(r.repricing_rule_period_cd), num(r.repricing_rule_day_num),
        pct(r.cap_limit_interest_rate), pct(r.floor_limit_interest_rate),
    ]))

res = subprocess.run([str(CALC)], input="\n".join(lines) + "\n",
                     capture_output=True, text=True, encoding="utf-8")

print(res.stdout)