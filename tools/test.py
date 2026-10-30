import subprocess
import pandas as pd

df = pd.read_excel("тестовые_сделки_МАИ3.xlsx", sheet_name=0)

def d(v):
    if pd.isna(v): return ""
    return v.strftime("%d.%m.%Y") if hasattr(v, "strftime") else str(v)

def s(v):
    return "" if pd.isna(v) else str(v).strip()

lines = []
for r in df.itertuples(index=False):
    lines.append(";".join([
        str(int(r.deal_id)), d(r.report_dttm), d(r.maturity_dttm), d(r.deal_dttm),
        s(r.discriminator), s(r.rate_type_cd), repr(float(r.interest_rate) * 100),
        s(r.regular_payment_rule_period_cd), s(r.interest_payment_rule_period_cd),
        s(r.day_convention_cd),
        d(r.interest_payment_rule_start_dttm), d(r.regular_payment_rule_start_dttm),
        repr(float(r.current_principal_amt)),
    ]))

res = subprocess.run(["./calc"], input="\n".join(lines) + "\n",
                     capture_output=True, text=True, encoding="utf-8")
print(res.stdout)