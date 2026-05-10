#!/usr/bin/env python3
"""
VEML7700 校准验证脚本 (Vishay AN 84323 方案)
=============================================
按照官方 AN 84323 流程:
  1. 分辨率修正: raw_counts × resolution → uncorrected lux
  2. AN 多项式校正 (GAIN_1/4, GAIN_1/8): Lux_corrected = 6.0135E-13·x⁴ - ... + 1.0023·x
  3. 盖板透光补偿: Lux_final = Lux_corrected × transmission_factor

本脚本对比 AN 方案与参考照度计读数，计算最优透光补偿系数和误差。
可替代旧的逐级幂律校准。

输入: CSV 对照数据文件，格式为 "VEML7700未校正读数, 标准照度计读数"
输出: 各级别透光系数、误差统计
"""

import sys
import math

# Vishay AN 84323 官方多项式系数
AN_COEFFS = (6.0135e-13, -9.3924e-09, 8.1488e-05, 1.0023)


def an_correction(x):
    """AN 官方非线性校正"""
    a, b, c, d = AN_COEFFS
    return a * x**4 + b * x**3 + c * x**2 + d * x


def load_csv(filepath):
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


def main():
    filepath = sys.argv[1] if len(sys.argv) > 1 else "lux对照值-新.csv"
    print(f"加载校准数据: {filepath}\n")

    pairs = load_csv(filepath)
    print(f"共读取 {len(pairs)} 组对照数据\n")

    # 将 AN 校正后的值算出来
    raw_vals = [p[0] for p in pairs]
    ref_vals = [p[1] for p in pairs]

    an_corrected = [an_correction(r) for r in raw_vals]

    # 计算最优透光补偿系数: factor = ref / an_corrected (取中位数，抗异常值)
    ratios = [ref_vals[i] / an_corrected[i] for i in range(len(pairs)) if an_corrected[i] > 0]
    ratios.sort()
    n = len(ratios)
    factor_median = ratios[n // 2]
    factor_mean = sum(ratios) / n

    # 应用透光系数后的结果
    final_median = [c * factor_median for c in an_corrected]
    final_mean = [c * factor_mean for c in an_corrected]

    # 误差统计
    def err_stats(pred, ref):
        errs = [abs(pred[i] - ref[i]) / ref[i] * 100 for i in range(len(pred))]
        return sum(errs) / len(errs), max(errs)

    err_before_mean, err_before_max = err_stats(raw_vals, ref_vals)
    err_an_mean, err_an_max = err_stats(an_corrected, ref_vals)
    err_median_mean, err_median_max = err_stats(final_median, ref_vals)
    err_mean_mean, err_mean_max = err_stats(final_mean, ref_vals)

    print(f"{'方案':<30s}  {'均值误差':>10s}  {'最大误差':>10s}")
    print("-" * 52)
    print(f"{'未校正 (raw×resolution)':<30s}  {err_before_mean:9.1f}%  {err_before_max:9.1f}%")
    print(f"{'AN多项式校正后':<30s}  {err_an_mean:9.1f}%  {err_an_max:9.1f}%")
    print(f"{'AN + 透光补偿(中位数)':<30s}  {err_median_mean:9.1f}%  {err_median_max:9.1f}%")
    print(f"{'AN + 透光补偿(均值)':<30s}  {err_mean_mean:9.1f}%  {err_mean_max:9.1f}%")
    print()

    print(f"透光补偿系数 (中位数): {factor_median:.4f}")
    print(f"透光补偿系数 (均值):   {factor_mean:.4f}")
    print()
    print(f"C 代码:  hw_veml7700_set_transmission({factor_median:.4f}f);")
    print()

    # 明细
    print(f"{'raw':>10s}  {'ref':>10s}  {'AN_corr':>10s}  {'final':>10s}  {'err':>8s}")
    print("-" * 56)
    for i in range(len(pairs)):
        print(f"{raw_vals[i]:10.1f}  {ref_vals[i]:10.1f}  "
              f"{an_corrected[i]:10.1f}  {final_median[i]:10.1f}  "
              f"{abs(final_median[i] - ref_vals[i]) / ref_vals[i] * 100:7.1f}%")


if __name__ == "__main__":
    main()
