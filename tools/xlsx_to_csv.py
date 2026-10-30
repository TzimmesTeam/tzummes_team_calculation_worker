#!/usr/bin/env python3
"""Export the Excel prototypes into the CSV inputs / reference results used by irrbb_calc and its tests.

usage: xlsx_to_csv.py <тестовые_сделки_МАИ3.xlsx> <Расчеты_Analytics_1_сделка.xlsx> <data_dir>
"""
import csv, datetime as dt, os, sys
from openpyxl import load_workbook

wb1_path, wb2_path, out = sys.argv[1:4]
os.makedirs(os.path.join(out, "ref"), exist_ok=True)

def cell(v):
    if isinstance(v, dt.datetime): return v.date().isoformat()
    if v is None: return ""
    return v

def write(path, header, rows):
    with open(os.path.join(out, path), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(header); w.writerows(rows)

wb1v = load_workbook(wb1_path, data_only=True); wb1f = load_workbook(wb1_path)
wb2v = load_workbook(wb2_path, data_only=True)

# 1) deals: workbook 2 has the complete explicit principal schedules ("date,amount ...")
ws = wb2v["Тестовые сделки"]
cols = [c for c in range(1, ws.max_column + 1) if ws.cell(1, c).value and ws.cell(1, c).value != "Столбец1"]
write("deals_55.csv", [ws.cell(1, c).value for c in cols],
      [[cell(ws.cell(r, c).value) for c in cols] for r in range(3, ws.max_row + 1) if ws.cell(r, 3).value is not None])

# 2) curves: 19 buckets, annual zero rates in % (base row 11, stress row 12), DF = (1+r)^-t
d = wb1v["ДФ_Источник"]
write("curves.csv", ["idx", "label", "tenor_years", "base_rate_pct", "stress_rate_pct"],
      [[i + 1, d.cell(1, 2 + i).value, d.cell(2, 2 + i).value, d.cell(11, 2 + i).value, d.cell(12, 2 + i).value] for i in range(19)])

# 3) key rate: change points of the CBR daily history
k = wb2v["КС ЦБ"]; prev = None; rows = []
for r in range(1, k.max_row + 1):
    a, b = k.cell(r, 1).value, k.cell(r, 2).value
    if isinstance(a, dt.datetime) and isinstance(b, (int, float)) and b != prev:
        rows.append([a.date().isoformat(), b]); prev = b
write("keyrate.csv", ["effective_from", "key_rate"], rows)

# 4) reference cash flows from workbook 1 (date, principal, interest) ------------------------------------
def ref_wb1(sheet, first, last, kind_of_row, outname):
    s = wb1v[sheet]; sf = wb1f[sheet]; agg = {}
    for r in range(first, last + 1):
        t, p = s.cell(r, 20).value, s.cell(r, 16).value
        if not isinstance(t, dt.datetime) or p is None: continue
        kind = kind_of_row(s, sf, r)
        a = agg.setdefault(t.date().isoformat(), [0.0, 0.0]); a[0 if kind == "P" else 1] += p
    write("ref/" + outname, ["date", "principal", "interest"], [[k_, v[0], v[1]] for k_, v in sorted(agg.items())])

by_label = lambda s, sf, r: "I" if s.cell(r, 18).value == "interest_payment" else "P"
by_formula = lambda s, sf, r: "P" if "83/25/83" in str(sf.cell(r, 16).value) else "I"   # sheet 27 has swapped labels
ref_wb1("сделка 3", 2, 190, by_label, "wb1_deal3.csv")
ref_wb1("сделка 27", 2, 51, by_formula, "wb1_deal27.csv")
ref_wb1("сделка 25", 2, 6, by_label, "wb1_deal25.csv")
ref_wb1("сделка 15", 2, 3, by_label, "wb1_deal15.csv")

# 5) reference from workbook 2 (deal 1): flows + PVs per row and the totals row -----------------------------
s = wb2v["1 сделка"]
write("ref/wb2_deal1.csv", ["date", "principal", "interest", "bucket", "pv_base", "pv_stress"],
      [[cell(s.cell(r, 2).value), s.cell(r, 4).value, s.cell(r, 6).value, s.cell(r, 9).value, s.cell(r, 10).value, s.cell(r, 12).value]
       for r in range(19, 353)])
write("ref/wb2_deal1_totals.csv", ["npv_base", "npv_stress", "dnpv"], [[s["K353"].value, s["M353"].value, s["N353"].value]])
print("exported to", out)
