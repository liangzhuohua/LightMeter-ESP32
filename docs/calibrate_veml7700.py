#!/usr/bin/env python3
"""
VEML7700 逐级幂律校准脚本 (纯标准库，无第三方依赖)
====================================================
对每个自适应量程级别，拟合幂律模型:  E_cal = a × E_raw^b

算法:
  对数空间线性回归:  ln(y) = ln(a) + b × ln(x)
  最小二乘线性回归得到 (ln(a), b)，再还原为 (a, b)

  为什么不用原始空间迭代精炼 (Gauss-Newton)?
  GN 最小化的是 Σ(E_cal - E_ref)² (绝对误差平方和)，大数值会主导优化方向，
  牺牲暗光端的拟合精度。测光表关心的是相对误差 (百分比)，而对数空间的最小
  二乘天然最小化的是相对误差，更符合摄影测光的需求。

输入: CSV 对照数据文件，格式为 "VEML7700读数, 标准照度计读数"
输出: 各级别 (a, b) 系数、R²、校准前后相对误差对比、C 代码片段
"""

import sys
import math

# ── 量程划分阈值 ────────────────────────────────────
# 根据 VEML7700 原始 lux 读数自动分配到对应级别
LEVEL_THRESHOLDS = [
    (500,    "LEVEL_0 (暗光,   GAIN_2)"),
    (4000,   "LEVEL_1 (室内,   GAIN_1)"),
    (999999, "LEVEL_2 (户外, GAIN_1/8)"),
]


def load_csv(filepath):
    """从 CSV 加载对照数据，跳过注释行和空行"""
    pairs = []
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.replace(",", " ").split()
            if len(parts) >= 2:
                try:
                    raw = float(parts[0])
                    ref = float(parts[1])
                    pairs.append((raw, ref))
                except ValueError:
                    continue
    return pairs


def split_by_level(pairs):
    """按阈值将数据分到各级别"""
    levels = {}
    for _, name in LEVEL_THRESHOLDS:
        levels[name] = []
    for raw, ref in pairs:
        for thresh, name in LEVEL_THRESHOLDS:
            if raw <= thresh:
                levels[name].append((raw, ref))
                break
    return levels


def fit_power_law(raw, ref):
    """
    幂律拟合: y = a × x^b

    对数空间线性回归:
      ln y = ln a + b × ln x
      令 Y = ln y,  X = ln x,  A = ln a
      则  Y = A + b × X  (标准线性回归)

    在对数空间做最小二乘，等价于最小化相对误差，而非绝对误差。
    这对于测光应用是正确的——±5% at 100lux 和 ±5% at 10000lux 同等重要。
    """
    n = len(raw)

    ln_x = [math.log(x) for x in raw]
    ln_y = [math.log(y) for y in ref]

    mean_x = sum(ln_x) / n
    mean_y = sum(ln_y) / n

    ss_xy = sum((ln_x[i] - mean_x) * (ln_y[i] - mean_y) for i in range(n))
    ss_xx = sum((x - mean_x) ** 2 for x in ln_x)

    if ss_xx < 1e-12:
        return None  # 数据点x值完全相同，无法拟合

    b = ss_xy / ss_xx
    ln_a = mean_y - b * mean_x
    a = math.exp(ln_a)

    return a, b


def fit_level(name, data):
    """对单个级别的数据进行幂律拟合并输出报告"""
    if len(data) < 3:
        print(f"  {name}: 数据点不足 ({len(data)} 组), 跳过")
        return None

    raw = [d[0] for d in data]
    ref = [d[1] for d in data]
    n = len(raw)

    result = fit_power_law(raw, ref)
    if result is None:
        print(f"  {name}: 拟合失败")
        return None

    a, b = result
    fitted = [a * math.pow(x, b) for x in raw]

    # R² (决定系数) — 在对数空间计算，衡量 ln(cal) vs ln(ref) 的拟合优度
    mean_ref = sum(ref) / n
    ss_res = sum((ref[i] - fitted[i]) ** 2 for i in range(n))
    ss_tot = sum((ref[i] - mean_ref) ** 2 for i in range(n))
    r_squared = 1 - ss_res / ss_tot if ss_tot > 0 else 1.0

    # 相对误差
    err_before = [abs(raw[i] - ref[i]) / ref[i] * 100 for i in range(n)]
    err_after  = [abs(fitted[i] - ref[i]) / ref[i] * 100 for i in range(n)]
    mean_bef = sum(err_before) / n
    mean_aft = sum(err_after) / n
    max_bef  = max(err_before)
    max_aft  = max(err_after)

    print(f"\n  {name} ({n} 组数据)")
    print(f"    a = {a:.6f},  b = {b:.6f},  R² = {r_squared:.6f}")
    print(f"    校准前误差:  mean = {mean_bef:5.1f}%,  max = {max_bef:5.1f}%")
    print(f"    校准后误差:  mean = {mean_aft:5.1f}%,  max = {max_aft:5.1f}%")

    # 打印对照明细
    print(f"    {'raw':>8s}  {'ref':>8s}  {'cal':>8s}  {'bef_err':>8s}  {'aft_err':>8s}")
    for i in range(n):
        print(f"    {raw[i]:8.1f}  {ref[i]:8.1f}  {fitted[i]:8.1f}  "
              f"{err_before[i]:7.1f}%  {err_after[i]:7.1f}%")

    return {
        "name": name, "a": a, "b": b, "r_squared": r_squared,
        "err_mean_before": mean_bef, "err_mean_after": mean_aft,
        "n": n,
    }


def main():
    filepath = sys.argv[1] if len(sys.argv) > 1 else "lux对照值-新.csv"
    print(f"加载校准数据: {filepath}\n")

    pairs = load_csv(filepath)
    print(f"共读取 {len(pairs)} 组对照数据\n")

    levels = split_by_level(pairs)

    # 数据分布概览
    print("数据分布:")
    for name, data in levels.items():
        if data:
            print(f"  {name}: {len(data)} 组")
    print()

    print("=" * 64)
    print("                    逐级幂律拟合结果")
    print("=" * 64)

    results = {}
    for thresh, name in LEVEL_THRESHOLDS:
        data = levels.get(name)
        if data:
            r = fit_level(name, data)
            if r:
                results[name] = r

    # ── C 代码输出 ──
    print("\n" + "=" * 64)
    print("          C 语言校准数组 (直接粘贴到 firmware)")
    print("=" * 64)
    print("static veml7700_calib_t calibration[VEML7700_LEVEL_COUNT] = {")
    for thresh, name in LEVEL_THRESHOLDS:
        if name in results:
            r = results[name]
            level_idx = name.split("_")[1].split()[0].rstrip(",")
            print(f"    [VEML7700_LEVEL_{level_idx}] = {{{r['a']:.4f}f, {r['b']:.4f}f}},  "
                  f"/* {name} */")
    print("};")

    # ── 总体评估 ──
    print("\n" + "=" * 64)
    print("                        总体评估")
    print("=" * 64)
    print(f"  {'级别':<20s}  {'样本':>4s}  {'校准前均值':>9s}  {'校准后均值':>9s}  {'改善':>6s}")
    for name, r in results.items():
        print(f"  {name:<20s}  {r['n']:4d}  {r['err_mean_before']:8.1f}%  "
              f"{r['err_mean_after']:8.1f}%  {r['err_mean_before'] - r['err_mean_after']:5.1f}%")


if __name__ == "__main__":
    main()
