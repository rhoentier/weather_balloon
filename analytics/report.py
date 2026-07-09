"""
Erzeugt einen Verifikations-Bericht für den Flug-Log (flight.csv).
Ausgabe: report.html

Checks:
  1. Zeitstempel-Plausibilität (Monotonie, Lücken, Takt)
  2. Höhenprofil-Plausibilität (Pre → Aufstieg → Abstieg → Landed)
  3. Min/Max aller Messwerte
"""

import sys
from pathlib import Path
import pandas as pd
from read_csv import load

CSV = Path(__file__).parent / "flight.csv"

# ── Analyse-Funktionen ──────────────────────────────────────────────────────

def check_timestamps(df: pd.DataFrame) -> dict:
    deltas = df["t_ms"].diff().dropna()
    median_dt = deltas.median()
    gaps = deltas[deltas > median_dt * 3]
    backwards = deltas[deltas < 0]
    duration_ms = int(df["t_ms"].iloc[-1] - df["t_ms"].iloc[0])

    # Kontinuierliche Zeitachse für Statusleiste
    t_start = int(df["t_ms_cont"].iloc[0])
    t_end   = int(df["t_ms_cont"].iloc[-1])
    span    = t_end - t_start

    # Segmente: abwechselnd Daten-Block und Lücke, aus t_ms_cont aufgebaut.
    # Jede Zeile hat eine "effektive Breite" von median_tick ms.
    # Eine Lücke entsteht dort wo delta > 3× median_tick.
    df_sorted  = df.sort_values("t_ms_cont").reset_index(drop=True)
    median_ms  = int(median_dt)
    bar_segments = []  # {"type": "data"|"gap", "start": %, "end": %, "phase": ..., "gap_s": ...}

    seg_phase  = df_sorted.loc[0, "phase"]
    seg_start  = 0.0
    prev_cont  = int(df_sorted.loc[0, "t_ms_cont"])

    for i in range(1, len(df_sorted)):
        cur_cont = int(df_sorted.loc[i, "t_ms_cont"])
        cur_phase = df_sorted.loc[i, "phase"]
        delta = cur_cont - prev_cont

        if delta > median_ms * 3:
            # Ende des laufenden Daten-Segments
            seg_end = (prev_cont + median_ms - t_start) / span * 100
            bar_segments.append({"type": "data", "phase": seg_phase,
                                  "start": round(seg_start, 3), "end": round(seg_end, 3)})
            # Lücke
            gap_start = seg_end
            gap_end   = (cur_cont - t_start) / span * 100
            bar_segments.append({"type": "gap",
                                  "gap_s": round(delta / 1000, 1),
                                  "phase": seg_phase,
                                  "start": round(gap_start, 3), "end": round(gap_end, 3)})
            seg_start = gap_end
            seg_phase = cur_phase
        elif cur_phase != seg_phase:
            seg_end = (cur_cont - t_start) / span * 100
            bar_segments.append({"type": "data", "phase": seg_phase,
                                  "start": round(seg_start, 3), "end": round(seg_end, 3)})
            seg_start = seg_end
            seg_phase = cur_phase

        prev_cont = cur_cont

    bar_segments.append({"type": "data", "phase": seg_phase,
                          "start": round(seg_start, 3), "end": 100.0})

    # Takt-Statistiken gesamt + pro Phase (über t_ms_cont sortiert)
    def phase_stats(sub):
        d = sub["t_ms_cont"].diff().dropna()
        d = d[d > 0]  # Rücksprünge ignorieren
        if d.empty:
            return None
        return {
            "rows":       len(sub),
            "t_min_s":    round(sub["t_ms_cont"].min() / 1000, 1),
            "t_max_s":    round(sub["t_ms_cont"].max() / 1000, 1),
            "dur_s":      round((sub["t_ms_cont"].max() - sub["t_ms_cont"].min()) / 1000, 1),
            "tick_min_ms": round(float(d.min()), 0),
            "tick_med_ms": round(float(d.median()), 0),
            "tick_max_ms": round(float(d.max()), 0),
            "gaps":        int((d > median_ms * 3).sum()),
        }

    df_s = df.sort_values("t_ms_cont")
    timing_rows = []
    for ph in ["PREFLIGHT", "ASCENT", "DESCENT", "LANDED"]:
        sub = df_s[df_s["phase"] == ph]
        if sub.empty:
            continue
        s = phase_stats(sub)
        if s:
            s["phase"] = ph
            timing_rows.append(s)
    # Gesamt
    s = phase_stats(df_s)
    if s:
        s["phase"] = "GESAMT"
        timing_rows.append(s)

    return {
        "total_rows":         len(df),
        "duration_s":         round(duration_ms / 1000, 1),
        "duration_min":       round(duration_ms / 60_000, 1),
        "median_interval_ms": round(median_dt, 1),
        "backwards_jumps":    len(backwards),
        "gaps_over_3x":       len(gaps),
        "gap_details":        gaps.values.tolist()[:10],
        "bar_segments":       bar_segments,
        "t_start_ms":         t_start,
        "t_end_ms":           t_end,
        "timing_rows":        timing_rows,
    }

def check_height_profile(df: pd.DataFrame) -> dict:
    col = "alt_baro_m"
    has_data = df[col].notna().sum()

    phase_order = ["PREFLIGHT", "ASCENT", "DESCENT", "LANDED"]
    phase_stats = {}
    for ph in phase_order:
        sub = df[df["phase"] == ph][col].dropna()
        if len(sub):
            phase_stats[ph] = {
                "rows": len(df[df["phase"] == ph]),
                "alt_min_m": round(float(sub.min()), 1),
                "alt_max_m": round(float(sub.max()), 1),
                "alt_mean_m": round(float(sub.mean()), 1),
            }

    # Reihenfolge der Phasen im Log prüfen
    seen_phases = list(df["phase"].unique())

    # Plausibilitäts-Check: ASCENT-max > PREFLIGHT-max > DESCENT-min
    ok_ascent  = False
    ok_descent = False
    if "PREFLIGHT" in phase_stats and "ASCENT" in phase_stats:
        ok_ascent = phase_stats["ASCENT"]["alt_max_m"] > phase_stats["PREFLIGHT"]["alt_max_m"]
    if "ASCENT" in phase_stats and "DESCENT" in phase_stats:
        ok_descent = phase_stats["DESCENT"]["alt_min_m"] < phase_stats["ASCENT"]["alt_max_m"]

    return {
        "baro_rows": int(has_data),
        "max_altitude_m": round(float(df[col].max()), 1),
        "phase_stats": phase_stats,
        "phases_in_log": seen_phases,
        "plausible_ascent": ok_ascent,
        "plausible_descent": ok_descent,
    }

def check_anomalies(df: pd.DataFrame) -> list[dict]:
    issues = []

    # GPS: kein Fix im gesamten Log
    gps_fix_rows = df["lat"].notna().sum()
    if gps_fix_rows == 0:
        issues.append({"sev": "warn", "text": "Kein GPS-Fix im gesamten Log — lat/lon durchgehend leer."})
    else:
        issues.append({"sev": "ok", "text": f"GPS-Fix vorhanden: {gps_fix_rows:,} Zeilen mit Position."})

    # PREFLIGHT-Höhe: Bodenhöhe sollte nahe 0 sein (±200 m Toleranz)
    pre = df[df["phase"] == "PREFLIGHT"]["alt_baro_m"].dropna()
    if not pre.empty and abs(pre.mean()) > 200:
        issues.append({"sev": "warn",
                       "text": f"PREFLIGHT-Baro-Ø = {pre.mean():.1f} m — weit von 0 entfernt. QNH-Kalibrierung prüfen."})
    elif not pre.empty:
        issues.append({"sev": "ok", "text": f"PREFLIGHT-Baro-Ø = {pre.mean():.1f} m — Bodennähe plausibel."})

    # ASCENT beginnt bereits hoch oben (Log-Start mitten im Flug?)
    asc = df[df["phase"] == "ASCENT"]["alt_baro_m"].dropna()
    if not asc.empty and asc.iloc[0] > 1000:
        issues.append({"sev": "warn",
                       "text": f"ASCENT beginnt bei {asc.iloc[0]:.0f} m — Log hat den Start des Aufstiegs nicht erfasst."})

    # Negativer Luftdruck (physikalisch unmöglich)
    neg_pressure = (df["pressure_hpa"] < 0).sum()
    if neg_pressure > 0:
        pmin = df["pressure_hpa"].min()
        issues.append({"sev": "fail",
                       "text": f"Negativer Luftdruck: {neg_pressure} Zeilen, Minimum = {pmin:.2f} hPa. Sensor-Init-Artefakt."})

    # Gyro-Clipping: MPU-6050 Messbereichs-Anschlag
    gyro_cols = ["gyr_x_dps", "gyr_y_dps", "gyr_z_dps"]
    clip_thresh = 249.0
    clipped = sum((df[c].abs() >= clip_thresh).sum() for c in gyro_cols)
    if clipped > 0:
        issues.append({"sev": "warn",
                       "text": f"Gyro clippt bei ±250°/s: {clipped} Samples über allen 3 Achsen. MPU-6050-Messbereich zu eng."})

    # LANDED-Phase fehlt
    if "LANDED" not in df["phase"].values:
        issues.append({"sev": "warn", "text": "Phase LANDED fehlt im Log — Aufzeichnung endet vor/während der Landung."})
    else:
        issues.append({"sev": "ok", "text": "Phase LANDED vorhanden."})

    # Zeitstempel-Rücksprung
    backwards = (df["t_ms"].diff() < 0).sum()
    if backwards > 0:
        issues.append({"sev": "warn",
                       "text": f"{backwards} Zeitstempel-Rücksprung(sprünge) — möglicher Board-Neustart oder millis()-Überlauf."})

    # Logging-Lücken > 5 s
    big_gaps = df["t_ms"].diff()
    big_gaps = big_gaps[big_gaps > 5000]
    if len(big_gaps) > 0:
        issues.append({"sev": "warn",
                       "text": f"{len(big_gaps)} Lücke(n) > 5 s im Log. Größte: {big_gaps.max()/1000:.1f} s."})

    return issues


def sensor_minmax(df: pd.DataFrame) -> list[dict]:
    sensors = {
        "GPS Höhe (alt_gps_m)":     ("alt_gps_m",   "m"),
        "Baro Höhe (alt_baro_m)":   ("alt_baro_m",  "m"),
        "Temp BMP280 (temp_c)":     ("temp_c",       "°C"),
        "Luftdruck (pressure_hpa)": ("pressure_hpa", "hPa"),
        "Temp extern DS18B20":      ("temp_ext_c",   "°C"),
        "UV ADC-Counts (uv_raw)":   ("uv_raw",       "counts"),
        "Beschl. X (acc_x_g)":      ("acc_x_g",      "g"),
        "Beschl. Y (acc_y_g)":      ("acc_y_g",      "g"),
        "Beschl. Z (acc_z_g)":      ("acc_z_g",      "g"),
        "Drehraten X (gyr_x_dps)":  ("gyr_x_dps",    "°/s"),
        "Drehraten Y (gyr_y_dps)":  ("gyr_y_dps",    "°/s"),
        "Drehraten Z (gyr_z_dps)":  ("gyr_z_dps",    "°/s"),
        "GPS Lat":                  ("lat",           "°"),
        "GPS Lon":                  ("lon",           "°"),
        "GPS Sats":                 ("sats",          ""),
    }
    rows = []
    for label, (col, unit) in sensors.items():
        s = df[col].dropna()
        if s.empty:
            rows.append({"sensor": label, "einheit": unit, "n": 0,
                         "min": "–", "max": "–", "mean": "–"})
        else:
            rows.append({
                "sensor": label,
                "einheit": unit,
                "n": len(s),
                "min": round(float(s.min()), 3),
                "max": round(float(s.max()), 3),
                "mean": round(float(s.mean()), 3),
            })
    return rows

# ── HTML-Bericht ────────────────────────────────────────────────────────────

PASS = '<span class="badge pass">✓ OK</span>'
FAIL = '<span class="badge fail">✗ Fehler</span>'
WARN = '<span class="badge warn">⚠ Warnung</span>'

def badge(ok: bool, warn: bool = False) -> str:
    if ok:   return PASS
    if warn: return WARN
    return FAIL

def render_html(ts: dict, hp: dict, mm: list[dict], an: list[dict], fi: dict, df: pd.DataFrame, df_sim: pd.DataFrame, offset_ms: int) -> str:

    # Anomalie-Sektion
    anomaly_rows = ""
    for a in an:
        if a["sev"] == "ok":
            continue
        label = "⚠ Warnung" if a["sev"] == "warn" else "✗ Fehler"
        anomaly_rows += f'<div class="check"><span class="badge {a["sev"]}">{label}</span> <span>{a["text"]}</span></div>\n'
    if not anomaly_rows:
        anomaly_rows = '<p style="color:var(--muted);font-size:13px">Keine Auffälligkeiten.</p>'

    # Höhenprofil-Graph
    df_alt = df[df["alt_baro_m"].notna()].sort_values("t_ms_cont")[["t_ms_cont", "alt_baro_m", "phase"]].copy()
    df_alt["t_plot"] = df_alt["t_ms_cont"] + offset_ms
    if len(df_alt) > 600:
        df_alt = df_alt.iloc[::len(df_alt)//600].copy()

    # df_sim hat bereits t_wall_ms als absolute Uhrzeit
    df_sim_alt = df_sim[["t_wall_ms", "alt_baro_m", "phase"]].copy()
    df_sim_alt = df_sim_alt.rename(columns={"t_wall_ms": "t_plot"})

    t0_g    = int(df_sim_alt["t_plot"].iloc[0])   # 08:30:00
    t1_g    = int(df_alt["t_plot"].iloc[-1])
    alt_min = 0
    alt_max = 40000

    W, H = 860, 320
    PAD_L, PAD_R, PAD_T, PAD_B = 56, 12, 16, 28

    def tx(t): return PAD_L + (t - t0_g) / (t1_g - t0_g) * (W - PAD_L - PAD_R)
    def ty(a): return PAD_T + (1 - (a - alt_min) / (alt_max - alt_min)) * (H - PAD_T - PAD_B)
    def ms_to_hhmm(ms):
        total_s = ms // 1000
        h = total_s // 3600
        m = (total_s % 3600) // 60
        return f"{h:02d}:{m:02d}"

    phase_colors_graph = {
        "PREFLIGHT":           "#898781",
        "ASCENT":              "#2a78d6",
        "DESCENT":             "#1baf7a",
        "LANDED":              "#008300",
        "simulated_preflight": "#c98500",
        "simulated_ascent":    "#eda100",
    }

    def make_path(rows, dashed=False):
        if len(rows) < 2:
            return ""
        pts = [(tx(r["t_plot"]), ty(r["alt_baro_m"])) for _, r in rows.iterrows()]
        col = phase_colors_graph.get(rows["phase"].iloc[0], "#ccc")
        d   = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        dash = ' stroke-dasharray="6,4"' if dashed else ""
        return f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.5" stroke-linejoin="round"{dash}/>'

    # Simulierter Aufstieg (gestrichelt)
    paths_svg = make_path(df_sim_alt, dashed=True)

    # Echte Daten nach Phase aufgeteilt
    cur_phase, cur_rows = None, []
    for _, row in df_alt.iterrows():
        ph = row["phase"]
        if ph != cur_phase:
            if cur_rows:
                paths_svg += make_path(pd.DataFrame(cur_rows), dashed=False)
            cur_rows  = [row]
            cur_phase = ph
        else:
            cur_rows.append(row)
    if cur_rows:
        paths_svg += make_path(pd.DataFrame(cur_rows), dashed=False)

    # Y-Achse: 5 Ticks
    y_ticks_svg = ""
    for i in range(5):
        a    = alt_min + i * (alt_max - alt_min) / 4
        ypos = ty(a)
        y_ticks_svg += (
            f'<line x1="{PAD_L}" y1="{ypos:.1f}" x2="{W-PAD_R}" y2="{ypos:.1f}" '
            f'stroke="var(--grid)" stroke-width="1"/>'
            f'<text x="{PAD_L-4}" y="{ypos+4:.1f}" text-anchor="end" '
            f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>'
        )

    # X-Achse: stündliche Ticks
    x_ticks_svg = ""
    tick_ms = 3600 * 1000
    t_tick  = (t0_g // tick_ms) * tick_ms
    while t_tick <= t1_g:
        if t_tick >= t0_g:
            xpos  = tx(t_tick)
            label = ms_to_hhmm(t_tick)
            x_ticks_svg += (
                f'<line x1="{xpos:.1f}" y1="{PAD_T}" x2="{xpos:.1f}" y2="{H-PAD_B}" '
                f'stroke="var(--grid)" stroke-width="1"/>'
                f'<text x="{xpos:.1f}" y="{H-4}" text-anchor="middle" '
                f'font-size="10" fill="var(--muted)">{label}</text>'
            )
        t_tick += tick_ms

    # Legende
    graph_legend = ""
    for ph, col in phase_colors_graph.items():
        dash_style = "stroke-dasharray:6 4;" if ph == "simulated_ascent" else ""
        graph_legend += (
            f'<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            f'<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" '
            f'stroke="{col}" stroke-width="2" style="{dash_style}"/></svg>'
            f'<span style="font-size:12px;color:var(--secondary)">{ph}</span></span>'
        )

    height_svg = (
        f'<div style="margin-bottom:6px">{graph_legend}</div>'
        f'<svg viewBox="0 0 {W} {H}" width="100%" style="display:block;overflow:visible">'
        f'{y_ticks_svg}{x_ticks_svg}{paths_svg}'
        f'<line x1="{PAD_L}" y1="{PAD_T}" x2="{PAD_L}" y2="{H-PAD_B}" '
        f'stroke="var(--grid)" stroke-width="1"/>'
        f'</svg>'
    )

    # ── Temperatur ────────────────────────────────────────────────────────────
    # Tabellenwerte
    def _temp_stats(series):
        s = series.dropna()
        if s.empty:
            return None
        return {"n": len(s), "min": round(float(s.min()), 1),
                "max": round(float(s.max()), 1),
                "med": round(float(s.median()), 1),
                "mean": round(float(s.mean()), 1)}

    ts_aussen = _temp_stats(df["temp_ext_c"])
    ts_innen  = _temp_stats(df["temp_c"])

    def _temp_stat_row(label, st):
        if st is None:
            return f'<tr><td>{label}</td><td colspan="5" style="color:var(--muted)">keine Daten</td></tr>'
        return (f'<tr><td>{label}</td><td>{st["n"]:,}</td>'
                f'<td>{st["min"]} °C</td><td>{st["max"]} °C</td>'
                f'<td>{st["med"]} °C</td><td>{st["mean"]} °C</td></tr>')

    temp_table_rows = _temp_stat_row("Außen (DS18B20)", ts_aussen) + _temp_stat_row("Innen (BMP280)", ts_innen)

    # Gemeinsame X-Achse: echte t_ms_cont → Wall-Clock
    df_temp = df[["t_ms_cont", "phase", "temp_ext_c", "temp_c", "alt_baro_m"]].copy()
    df_temp["t_plot"] = df_temp["t_ms_cont"] + offset_ms
    df_temp = df_temp.sort_values("t_plot")

    t0_t = int(df_temp["t_plot"].iloc[0])
    t1_t = int(df_temp["t_plot"].iloc[-1])

    # Y-Bereich Temperatur: gemeinsam für beide Kurven
    all_temps = pd.concat([df_temp["temp_ext_c"].dropna(), df_temp["temp_c"].dropna()])
    if all_temps.empty:
        temp_cmp_svg = temp_alt_svg = "<p style='color:var(--muted)'>Keine Temperaturdaten.</p>"
    else:
        temp_lo = float(all_temps.min()) - 5
        temp_hi = float(all_temps.max()) + 5

        W2, H2 = 860, 240
        PL, PR, PT, PB = 46, 12, 16, 28

        def tx2(t): return PL + (t - t0_t) / (t1_t - t0_t) * (W2 - PL - PR)
        def ty_temp(v): return PT + (1 - (v - temp_lo) / (temp_hi - temp_lo)) * (H2 - PT - PB)

        def _make_temp_path(sub, col_y, color, dashed=False):
            pts = [(tx2(r["t_plot"]), ty_temp(r[col_y])) for _, r in sub.iterrows() if pd.notna(r[col_y])]
            if len(pts) < 2:
                return ""
            d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
            dash = ' stroke-dasharray="5,3"' if dashed else ""
            return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5" stroke-linejoin="round"{dash}/>'

        def _x_ticks(tx_fn, t0, t1, h):
            s = ""
            tk = (t0 // (3600*1000)) * (3600*1000)
            while tk <= t1:
                if tk >= t0:
                    x = tx_fn(tk)
                    lbl = ms_to_hhmm(tk)
                    s += (f'<line x1="{x:.1f}" y1="{PT}" x2="{x:.1f}" y2="{h-PB}" '
                          f'stroke="var(--grid)" stroke-width="1"/>'
                          f'<text x="{x:.1f}" y="{h-4}" text-anchor="middle" '
                          f'font-size="10" fill="var(--muted)">{lbl}</text>')
                tk += 3600*1000
            return s

        def _y_ticks_temp(lo, hi, n=5):
            s = ""
            for i in range(n):
                v    = lo + i * (hi - lo) / (n - 1)
                ypos = ty_temp(v)
                s += (f'<line x1="{PL}" y1="{ypos:.1f}" x2="{W2-PR}" y2="{ypos:.1f}" '
                      f'stroke="var(--grid)" stroke-width="1"/>'
                      f'<text x="{PL-4}" y="{ypos+4:.1f}" text-anchor="end" '
                      f'font-size="10" fill="var(--muted)">{v:.0f}°</text>')
            return s

        xt = _x_ticks(tx2, t0_t, t1_t, H2)
        yt = _y_ticks_temp(temp_lo, temp_hi)

        # Graph 1: Außen vs. Innen
        # Downsample
        sub_ext = df_temp[df_temp["temp_ext_c"].notna()]
        sub_int = df_temp[df_temp["temp_c"].notna()]
        if len(sub_ext) > 600: sub_ext = sub_ext.iloc[::len(sub_ext)//600]
        if len(sub_int) > 600: sub_int = sub_int.iloc[::len(sub_int)//600]

        p_ext = _make_temp_path(sub_ext, "temp_ext_c", "#e05c00")
        p_int = _make_temp_path(sub_int, "temp_c",     "#2a78d6")

        leg_cmp = (
            '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#e05c00" stroke-width="2"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Außen (DS18B20)</span></span>'
            '<span style="display:inline-flex;align-items:center;gap:4px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Innen (BMP280)</span></span>'
        )
        temp_cmp_svg = (
            f'<div style="margin-bottom:6px">{leg_cmp}</div>'
            f'<svg viewBox="0 0 {W2} {H2}" width="100%" style="display:block;overflow:visible">'
            f'{yt}{xt}{p_ext}{p_int}'
            f'<line x1="{PL}" y1="{PT}" x2="{PL}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'</svg>'
        )

        # Graph 2: Außentemperatur vs. Höhe (zweite Y-Achse rechts für Höhe)
        alt_lo, alt_hi = 0, float(df_temp["alt_baro_m"].dropna().max()) * 1.05
        PR2 = 52  # extra Platz für rechte Y-Achse

        def tx3(t): return PL + (t - t0_t) / (t1_t - t0_t) * (W2 - PL - PR2)
        def ty_alt2(a): return PT + (1 - (a - alt_lo) / (alt_hi - alt_lo)) * (H2 - PT - PB)
        def ty_ext(v): return PT + (1 - (v - temp_lo) / (temp_hi - temp_lo)) * (H2 - PT - PB)

        xt3 = _x_ticks(tx3, t0_t, t1_t, H2)

        yt_ext = ""
        for i in range(5):
            v    = temp_lo + i * (temp_hi - temp_lo) / 4
            ypos = ty_ext(v)
            yt_ext += (f'<line x1="{PL}" y1="{ypos:.1f}" x2="{W2-PR2}" y2="{ypos:.1f}" '
                       f'stroke="var(--grid)" stroke-width="1"/>'
                       f'<text x="{PL-4}" y="{ypos+4:.1f}" text-anchor="end" '
                       f'font-size="10" fill="var(--muted)">{v:.0f}°</text>')

        yt_alt2 = ""
        for i in range(5):
            a    = alt_lo + i * (alt_hi - alt_lo) / 4
            ypos = ty_alt2(a)
            yt_alt2 += (f'<text x="{W2-PR2+6}" y="{ypos+4:.1f}" text-anchor="start" '
                        f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>')

        sub_ext3 = df_temp[df_temp["temp_ext_c"].notna()].copy()
        sub_alt3 = df_temp[df_temp["alt_baro_m"].notna()].copy()
        if len(sub_ext3) > 600: sub_ext3 = sub_ext3.iloc[::len(sub_ext3)//600]
        if len(sub_alt3) > 600: sub_alt3 = sub_alt3.iloc[::len(sub_alt3)//600]

        def _path_raw(sub, col_y, ty_fn, color):
            pts = [(tx3(r["t_plot"]), ty_fn(r[col_y])) for _, r in sub.iterrows() if pd.notna(r[col_y])]
            if len(pts) < 2: return ""
            d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
            return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5" stroke-linejoin="round"/>'

        p_ext3 = _path_raw(sub_ext3, "temp_ext_c", ty_ext,  "#e05c00")
        p_alt3_d = "M " + " L ".join(f"{tx3(r['t_plot']):.1f},{ty_alt2(r['alt_baro_m']):.1f}" for _, r in sub_alt3.iterrows() if pd.notna(r["alt_baro_m"]))
        p_alt3 = f'<path d="{p_alt3_d}" fill="none" stroke="#2a78d6" stroke-width="1.5" stroke-dasharray="5,3" stroke-linejoin="round"/>' if len(sub_alt3) >= 2 else ""

        leg_alt = (
            '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#e05c00" stroke-width="2"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Außentemp. (°C, links)</span></span>'
            '<span style="display:inline-flex;align-items:center;gap:4px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2" stroke-dasharray="5,3"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Höhe (m, rechts)</span></span>'
        )
        temp_alt_svg = (
            f'<div style="margin-bottom:6px">{leg_alt}</div>'
            f'<svg viewBox="0 0 {W2} {H2}" width="100%" style="display:block;overflow:visible">'
            f'{yt_ext}{xt3}{p_ext3}{p_alt3}{yt_alt2}'
            f'<line x1="{PL}" y1="{PT}" x2="{PL}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'<line x1="{W2-PR2}" y1="{PT}" x2="{W2-PR2}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'</svg>'
        )

    # ── Luftdruck ─────────────────────────────────────────────────────────────
    df_pres = df[["t_ms_cont", "phase", "pressure_hpa", "alt_baro_m"]].copy()
    df_pres["t_plot"] = df_pres["t_ms_cont"] + offset_ms
    df_pres = df_pres.sort_values("t_plot")

    pres_flight = df_pres[df_pres["pressure_hpa"] >= 0]

    def _pres_stat_row(label, s):
        s = s.dropna()
        s = s[s >= 0]
        if s.empty:
            return f'<tr><td>{label}</td><td colspan="5" style="color:var(--muted)">keine Daten</td></tr>'
        return (f'<tr><td>{label}</td><td>{len(s):,}</td>'
                f'<td>{s.min():.1f}</td><td>{s.max():.1f}</td>'
                f'<td>{s.median():.1f}</td><td>{s.mean():.1f}</td></tr>')

    pres_table_rows = _pres_stat_row("Luftdruck (BMP280)", pres_flight["pressure_hpa"])

    if pres_flight.empty:
        pres_alt_svg = "<p style='color:var(--muted)'>Keine Luftdruckdaten.</p>"
        pres_parabola_svg = ""
    else:
        p_lo = 0
        p_hi = float(pres_flight["pressure_hpa"].max()) * 1.05
        t0_p = t0_t
        t1_p = t1_t
        a_hi_p = float(pres_flight["alt_baro_m"].dropna().max()) * 1.05
        PR_P = 52

        def tx_p(t):  return PL + (t - t0_p) / (t1_p - t0_p) * (W2 - PL - PR_P)
        def ty_p(v):  return PT + (1 - (v - p_lo) / (p_hi - p_lo)) * (H2 - PT - PB)
        def ty_ap(a): return PT + (1 - (a - 0) / (a_hi_p - 0)) * (H2 - PT - PB)

        xt_p = ""
        tk = (t0_p // (3600*1000)) * (3600*1000)
        while tk <= t1_p:
            if tk >= t0_p:
                x = tx_p(tk)
                xt_p += (f'<line x1="{x:.1f}" y1="{PT}" x2="{x:.1f}" y2="{H2-PB}" '
                         f'stroke="var(--grid)" stroke-width="1"/>'
                         f'<text x="{x:.1f}" y="{H2-4}" text-anchor="middle" '
                         f'font-size="10" fill="var(--muted)">{ms_to_hhmm(tk)}</text>')
            tk += 3600*1000

        yt_p = ""
        for i in range(5):
            v    = p_lo + i * (p_hi - p_lo) / 4
            ypos = ty_p(v)
            yt_p += (f'<line x1="{PL}" y1="{ypos:.1f}" x2="{W2-PR_P}" y2="{ypos:.1f}" '
                     f'stroke="var(--grid)" stroke-width="1"/>'
                     f'<text x="{PL-4}" y="{ypos+4:.1f}" text-anchor="end" '
                     f'font-size="10" fill="var(--muted)">{v:.0f}</text>')

        yt_ap = ""
        for i in range(5):
            a    = i * a_hi_p / 4
            ypos = ty_ap(a)
            yt_ap += (f'<text x="{W2-PR_P+6}" y="{ypos+4:.1f}" text-anchor="start" '
                      f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>')

        sub_p = pres_flight.copy()
        sub_ap = pres_flight[pres_flight["alt_baro_m"].notna()].copy()
        if len(sub_p)  > 600: sub_p  = sub_p.iloc[::len(sub_p)//600]
        if len(sub_ap) > 600: sub_ap = sub_ap.iloc[::len(sub_ap)//600]

        def _pp(sub, col, ty_fn, color):
            pts = [(tx_p(r["t_plot"]), ty_fn(r[col])) for _, r in sub.iterrows() if pd.notna(r[col])]
            if len(pts) < 2: return ""
            d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
            return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5" stroke-linejoin="round"/>'

        def _pp_dashed(sub, col, ty_fn, color, dashed=False):
            pts = [(tx_p(r["t_plot"]), ty_fn(r[col])) for _, r in sub.iterrows() if pd.notna(r[col])]
            if len(pts) < 2: return ""
            d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
            dash = ' stroke-dasharray="5,3"' if dashed else ""
            return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5" stroke-linejoin="round"{dash}/>'

        leg_p = (
            '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#e05c00" stroke-width="2"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Luftdruck (hPa, links)</span></span>'
            '<span style="display:inline-flex;align-items:center;gap:4px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2" stroke-dasharray="5,3"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Höhe (m, rechts)</span></span>'
        )
        pres_alt_svg = (
            f'<div style="margin-bottom:6px">{leg_p}</div>'
            f'<svg viewBox="0 0 {W2} {H2}" width="100%" style="display:block;overflow:visible">'
            f'{yt_p}{xt_p}{_pp_dashed(sub_p, "pressure_hpa", ty_p, "#e05c00")}{_pp_dashed(sub_ap, "alt_baro_m", ty_ap, "#2a78d6", dashed=True)}{yt_ap}'
            f'<line x1="{PL}" y1="{PT}" x2="{PL}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'<line x1="{W2-PR_P}" y1="{PT}" x2="{W2-PR_P}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'</svg>'
        )

# ── Beschleunigung (IMU-Betrag) ────────────────────────────────────────────
    imu_cols = ["acc_x_g", "acc_y_g", "acc_z_g", "gyr_x_dps", "gyr_y_dps", "gyr_z_dps"]
    df_imu = df[["t_ms_cont", "phase", "alt_baro_m"] + imu_cols].copy()
    df_imu["t_plot"] = df_imu["t_ms_cont"] + offset_ms
    df_imu = df_imu.sort_values("t_plot")

    acc_data  = df_imu[df_imu["acc_x_g"].notna()]
    gyr_data  = df_imu[df_imu["gyr_x_dps"].notna()]

    if not acc_data.empty:
        acc_data = acc_data.copy()
        acc_data["acc_mag"] = (acc_data["acc_x_g"]**2 + acc_data["acc_y_g"]**2 + acc_data["acc_z_g"]**2)**0.5
    if not gyr_data.empty:
        gyr_data = gyr_data.copy()
        gyr_data["gyr_mag"] = (gyr_data["gyr_x_dps"]**2 + gyr_data["gyr_y_dps"]**2 + gyr_data["gyr_z_dps"]**2)**0.5

    def _imu_stat_row(label, series):
        s = series.dropna()
        if s.empty:
            return f'<tr><td>{label}</td><td colspan="5" style="color:var(--muted)">keine Daten</td></tr>'
        return (f'<tr><td>{label}</td><td>{len(s):,}</td>'
                f'<td>{s.min():.2f}</td><td>{s.max():.2f}</td>'
                f'<td>{s.median():.2f}</td><td>{s.mean():.2f}</td></tr>')

    acc_table_rows = _imu_stat_row("Beschleunigung |acc| (g)", acc_data["acc_mag"]) if not acc_data.empty else _imu_stat_row("Beschleunigung |acc| (g)", pd.Series(dtype=float))
    gyr_table_rows = _imu_stat_row("Drehraten |gyr| (°/s)", gyr_data["gyr_mag"]) if not gyr_data.empty else _imu_stat_row("Drehraten |gyr| (°/s)", pd.Series(dtype=float))

    # ── Common dual-axis IMU graph builder ──────────────────────────────────────
    def _imu_graph(mag_series, mag_col, mag_label, mag_unit, mag_color,
                   t0_src, t1_src, df_src, smoothing_window=10):
        """
        Baut einen SVG-Graph: Betrag vs Zeit (links), Höhe gestrichelt (rechts).
        matching pattern of temp/pressure/uv graphs.
        """
        sub = df_src[df_src[mag_col].notna()].copy()
        alt_sub = df_src[df_src["alt_baro_m"].notna()].copy()
        if sub.empty:
            return "<p style='color:var(--muted)'>Keine Daten.</p>"

        W3, H3 = 860, 240
        PL3, PR3, PT3, PB3 = 46, 52, 16, 28
        t0_i = int(sub["t_plot"].iloc[0]) if len(sub) else t0_src
        t1_i = int(sub["t_plot"].iloc[-1]) if len(sub) else t1_src

        mag_lo = 0
        mag_hi = float(sub[mag_col].max()) * 1.15
        a_hi_i = float(alt_sub["alt_baro_m"].dropna().max()) * 1.05 if alt_sub["alt_baro_m"].notna().any() else 10000

        def tx_i(t): return PL3 + (t - t0_i) / (t1_i - t0_i) * (W3 - PL3 - PR3)
        def ty_mag(v): return PT3 + (1 - (v - mag_lo) / (mag_hi - mag_lo)) * (H3 - PT3 - PB3)
        def ty_alt_i(a): return PT3 + (1 - (a - 0) / (a_hi_i - 0)) * (H3 - PT3 - PB3)

        # Smooth with rolling mean
        sub_plot = sub.copy()
        if len(sub_plot) > smoothing_window:
            sub_plot[mag_col] = sub_plot[mag_col].rolling(window=smoothing_window, center=True, min_periods=1).mean()

        # Downsample for SVG path
        if len(sub_plot) > 600:
            sub_ds = sub_plot.iloc[::len(sub_plot)//600].copy()
        else:
            sub_ds = sub_plot
        if len(alt_sub) > 600:
            alt_ds = alt_sub.iloc[::len(alt_sub)//600].copy()
        else:
            alt_ds = alt_sub

        # X ticks
        xt_i = ""
        tk = (t0_i // (3600*1000)) * (3600*1000)
        while tk <= t1_i:
            if tk >= t0_i:
                x = tx_i(tk)
                xt_i += (f'<line x1="{x:.1f}" y1="{PT3}" x2="{x:.1f}" y2="{H3-PB3}" '
                         f'stroke="var(--grid)" stroke-width="1"/>'
                         f'<text x="{x:.1f}" y="{H3-4}" text-anchor="middle" '
                         f'font-size="10" fill="var(--muted)">{ms_to_hhmm(tk)}</text>')
            tk += 3600*1000

        # Y ticks left (magnitude)
        yt_mag = ""
        for i in range(5):
            v    = mag_lo + i * (mag_hi - mag_lo) / 4
            ypos = ty_mag(v)
            yt_mag += (f'<line x1="{PL3}" y1="{ypos:.1f}" x2="{W3-PR3}" y2="{ypos:.1f}" '
                       f'stroke="var(--grid)" stroke-width="1"/>'
                       f'<text x="{PL3-4}" y="{ypos+4:.1f}" text-anchor="end" '
                       f'font-size="10" fill="var(--muted)">{v:.1f}</text>')

        # Y ticks right (altitude)
        yt_alt_i = ""
        for i in range(5):
            a    = i * a_hi_i / 4
            ypos = ty_alt_i(a)
            yt_alt_i += (f'<text x="{W3-PR3+6}" y="{ypos+4:.1f}" text-anchor="start" '
                         f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>')

        # Paths
        pts_mag = [(tx_i(r["t_plot"]), ty_mag(r[mag_col])) for _, r in sub_ds.iterrows() if pd.notna(r[mag_col])]
        p_mag_d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts_mag) if len(pts_mag) >= 2 else ""
        p_mag   = f'<path d="{p_mag_d}" fill="none" stroke="{mag_color}" stroke-width="1.5" stroke-linejoin="round"/>' if p_mag_d else ""

        pts_alt = [(tx_i(r["t_plot"]), ty_alt_i(r["alt_baro_m"])) for _, r in alt_ds.iterrows() if pd.notna(r["alt_baro_m"])]
        p_alt_d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts_alt) if len(pts_alt) >= 2 else ""
        p_alt   = f'<path d="{p_alt_d}" fill="none" stroke="#2a78d6" stroke-width="1.5" stroke-dasharray="5,3" stroke-linejoin="round"/>' if p_alt_d else ""

        leg = (
            f'<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            f'<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="{mag_color}" stroke-width="2"/></svg>'
            f'<span style="font-size:12px;color:var(--secondary)">{mag_label} ({mag_unit}, links)</span></span>'
            f'<span style="display:inline-flex;align-items:center;gap:4px;">'
            f'<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2" stroke-dasharray="5,3"/></svg>'
            f'<span style="font-size:12px;color:var(--secondary)">Höhe (m, rechts)</span></span>'
        )
        return (
            f'<div style="margin-bottom:6px">{leg}</div>'
            f'<svg viewBox="0 0 {W3} {H3}" width="100%" style="display:block;overflow:visible">'
            f'{yt_mag}{xt_i}{p_mag}{p_alt}{yt_alt_i}'
            f'<line x1="{PL3}" y1="{PT3}" x2="{PL3}" y2="{H3-PB3}" stroke="var(--grid)" stroke-width="1"/>'
            f'<line x1="{W3-PR3}" y1="{PT3}" x2="{W3-PR3}" y2="{H3-PB3}" stroke="var(--grid)" stroke-width="1"/>'
            f'</svg>'
        )

    acc_svg = _imu_graph(acc_data, "acc_mag", "Beschleunigung |acc|", "g", "#e67e22",
                          t0_t, t1_t, acc_data, smoothing_window=10)
    gyr_svg = _imu_graph(gyr_data, "gyr_mag", "Drehraten |gyr|", "°/s", "#9b59b6",
                          t0_t, t1_t, gyr_data, smoothing_window=10)

        # Graph 2: simulierter Verlauf (Parabel) — Druck + Höhe
    # Abstieg als Interpolationsbasis: Druck(Höhe)
    desc_ref = df[df["phase"] == "DESCENT"][["alt_baro_m", "pressure_hpa"]].dropna().sort_values("alt_baro_m")
    import numpy as np
    interp_alt  = desc_ref["alt_baro_m"].values
    interp_pres = desc_ref["pressure_hpa"].values

    def _sim_pressure(alt_m):
        return float(np.interp(alt_m, interp_alt, interp_pres))

    # Simulierte Zeitreihe: sim. PREFLIGHT + sim. ASCENT aus df_sim
    df_sim_p = df_sim[df_sim["phase"].isin(["simulated_preflight", "simulated_ascent"])].copy()
    df_sim_p["t_plot"]      = df_sim_p["t_wall_ms"]
    df_sim_p["pressure_sim"]= df_sim_p["alt_baro_m"].apply(_sim_pressure)

    # Zeitachse: von Boardstart (df_sim) bis Ende der echten Daten
    t0_p2 = int(df_sim_p["t_plot"].min())
    t1_p2 = t1_t
    PR_P2 = 52

    def tx_p2(t):  return PL + (t - t0_p2) / (t1_p2 - t0_p2) * (W2 - PL - PR_P2)
    def ty_p2(v):  return PT + (1 - (v - p_lo) / (p_hi - p_lo)) * (H2 - PT - PB)
    def ty_ap2(a): return PT + (1 - (a - 0) / (a_hi_p - 0)) * (H2 - PT - PB)

    xt_p2 = ""
    tk = (t0_p2 // (3600*1000)) * (3600*1000)
    while tk <= t1_p2:
        if tk >= t0_p2:
            x = tx_p2(tk)
            xt_p2 += (f'<line x1="{x:.1f}" y1="{PT}" x2="{x:.1f}" y2="{H2-PB}" '
                      f'stroke="var(--grid)" stroke-width="1"/>'
                      f'<text x="{x:.1f}" y="{H2-4}" text-anchor="middle" '
                      f'font-size="10" fill="var(--muted)">{ms_to_hhmm(tk)}</text>')
        tk += 3600*1000

    yt_p2 = ""
    for i in range(5):
        v    = p_lo + i * (p_hi - p_lo) / 4
        ypos = ty_p2(v)
        yt_p2 += (f'<line x1="{PL}" y1="{ypos:.1f}" x2="{W2-PR_P2}" y2="{ypos:.1f}" '
                  f'stroke="var(--grid)" stroke-width="1"/>'
                  f'<text x="{PL-4}" y="{ypos+4:.1f}" text-anchor="end" '
                  f'font-size="10" fill="var(--muted)">{v:.0f}</text>')

    yt_ap2 = ""
    for i in range(5):
        a    = i * a_hi_p / 4
        ypos = ty_ap2(a)
        yt_ap2 += (f'<text x="{W2-PR_P2+6}" y="{ypos+4:.1f}" text-anchor="start" '
                   f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>')

    def _path2(sub, col, ty_fn, color, dashed=False):
        pts = [(tx_p2(r["t_plot"]), ty_fn(r[col])) for _, r in sub.iterrows() if pd.notna(r[col])]
        if len(pts) < 2: return ""
        d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        dash = ' stroke-dasharray="5,3"' if dashed else ""
        return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5" stroke-linejoin="round"{dash}/>'

    # Echte Kurven
    sub_p2  = pres_flight.copy()
    sub_ap2 = pres_flight[pres_flight["alt_baro_m"].notna()].copy()
    if len(sub_p2)  > 600: sub_p2  = sub_p2.iloc[::len(sub_p2)//600]
    if len(sub_ap2) > 600: sub_ap2 = sub_ap2.iloc[::len(sub_ap2)//600]

    # Simulierte Kurven
    sub_sim_p = df_sim_p.copy()
    if len(sub_sim_p) > 300: sub_sim_p = sub_sim_p.iloc[::len(sub_sim_p)//300]

    p_real_p   = _path2(sub_p2,    "pressure_hpa",  ty_p2,  "#e05c00")
    p_real_alt = _path2(sub_ap2,   "alt_baro_m",    ty_ap2, "#2a78d6",  dashed=True)
    p_sim_p    = _path2(sub_sim_p, "pressure_sim",  ty_p2,  "#e05c00",  dashed=True)
    p_sim_alt  = _path2(sub_sim_p, "alt_baro_m",    ty_ap2, "#2a78d6",  dashed=True)

    leg_p2 = (
        '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
        '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#e05c00" stroke-width="2"/></svg>'
        '<span style="font-size:12px;color:var(--secondary)">Luftdruck (hPa, links)</span></span>'
        '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
        '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#e05c00" stroke-width="2" stroke-dasharray="5,3"/></svg>'
        '<span style="font-size:12px;color:var(--secondary)">Luftdruck simuliert (links)</span></span>'
        '<span style="display:inline-flex;align-items:center;gap:4px;">'
        '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2" stroke-dasharray="5,3"/></svg>'
        '<span style="font-size:12px;color:var(--secondary)">Höhe (m, rechts)</span></span>'
    )
    pres_parabola_svg = (
        f'<div style="margin-bottom:6px">{leg_p2}</div>'
        f'<svg viewBox="0 0 {W2} {H2}" width="100%" style="display:block;overflow:visible">'
        f'{yt_p2}{xt_p2}{p_real_p}{p_sim_p}{p_real_alt}{p_sim_alt}{yt_ap2}'
        f'<line x1="{PL}" y1="{PT}" x2="{PL}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
        f'<line x1="{W2-PR_P2}" y1="{PT}" x2="{W2-PR_P2}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
        f'</svg>'
    )

    # ── UV-Sensor ─────────────────────────────────────────────────────────────
    df_uv = df[["t_ms_cont", "phase", "uv_raw", "alt_baro_m"]].copy()
    df_uv["t_plot"] = df_uv["t_ms_cont"] + offset_ms
    df_uv = df_uv.sort_values("t_plot")

    uv_data = df_uv[df_uv["uv_raw"].notna() & (df_uv["uv_raw"] > 0)]
    uv_s    = df["uv_raw"]

    def _uv_stat_row(label, s):
        s = s.dropna()
        s = s[s > 0]
        if s.empty:
            return f'<tr><td>{label}</td><td colspan="5" style="color:var(--muted)">keine Daten</td></tr>'
        return (f'<tr><td>{label}</td><td>{len(s):,}</td>'
                f'<td>{s.min():.1f}</td><td>{s.max():.1f}</td>'
                f'<td>{s.median():.1f}</td><td>{s.mean():.1f}</td></tr>')

    uv_table_rows = _uv_stat_row("UV ADC-Counts (GUVA-S12SD)", uv_s)

    flight_rows = df_uv

    if uv_data.empty:
        uv_alt_svg = "<p style='color:var(--muted)'>Keine UV-Daten.</p>"
    else:
        uv_lo  = 0
        uv_hi  = float(uv_data["uv_raw"].max()) * 1.1
        t0_uv  = t0_t
        t1_uv  = t1_t
        PR_UV  = 52

        def tx_uv(t): return PL + (t - t0_uv) / (t1_uv - t0_uv) * (W2 - PL - PR_UV)
        def ty_uv(v): return PT + (1 - (v - uv_lo) / (uv_hi - uv_lo)) * (H2 - PT - PB)
        a_hi_uv = float(flight_rows["alt_baro_m"].dropna().max()) * 1.05

        def ty_alt_uv(a):
            return PT + (1 - (a - 0) / (a_hi_uv - 0)) * (H2 - PT - PB)

        xt_uv = ""
        tk = (t0_uv // (3600*1000)) * (3600*1000)
        while tk <= t1_uv:
            if tk >= t0_uv:
                x = tx_uv(tk)
                xt_uv += (f'<line x1="{x:.1f}" y1="{PT}" x2="{x:.1f}" y2="{H2-PB}" '
                          f'stroke="var(--grid)" stroke-width="1"/>'
                          f'<text x="{x:.1f}" y="{H2-4}" text-anchor="middle" '
                          f'font-size="10" fill="var(--muted)">{ms_to_hhmm(tk)}</text>')
            tk += 3600*1000

        yt_uv = ""
        for i in range(5):
            v    = uv_lo + i * (uv_hi - uv_lo) / 4
            ypos = ty_uv(v)
            yt_uv += (f'<line x1="{PL}" y1="{ypos:.1f}" x2="{W2-PR_UV}" y2="{ypos:.1f}" '
                      f'stroke="var(--grid)" stroke-width="1"/>'
                      f'<text x="{PL-4}" y="{ypos+4:.1f}" text-anchor="end" '
                      f'font-size="10" fill="var(--muted)">{v:.0f}</text>')

        yt_alt_uv = ""
        for i in range(5):
            a    = i * a_hi_uv / 4
            ypos = ty_alt_uv(a)
            yt_alt_uv += (f'<text x="{W2-PR_UV+6}" y="{ypos+4:.1f}" text-anchor="start" '
                          f'font-size="10" fill="var(--muted)">{a/1000:.0f}k</text>')

        sub_uv     = uv_data.copy()
        sub_alt_uv = flight_rows[flight_rows["alt_baro_m"].notna()].copy()
        if len(sub_uv)     > 600: sub_uv     = sub_uv.iloc[::len(sub_uv)//600]
        if len(sub_alt_uv) > 600: sub_alt_uv = sub_alt_uv.iloc[::len(sub_alt_uv)//600]

        def _pts(sub, col, ty_fn):
            pts = [(tx_uv(r["t_plot"]), ty_fn(r[col])) for _, r in sub.iterrows() if pd.notna(r[col])]
            if len(pts) < 2: return ""
            d = "M " + " L ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
            return d

        p_uv_d  = _pts(sub_uv,     "uv_raw",    ty_uv)
        p_alt_d = _pts(sub_alt_uv, "alt_baro_m", ty_alt_uv)

        p_uv  = f'<path d="{p_uv_d}"  fill="none" stroke="#9b59b6" stroke-width="1.5" stroke-linejoin="round"/>' if p_uv_d  else ""
        p_alt_uv_path = f'<path d="{p_alt_d}" fill="none" stroke="#2a78d6" stroke-width="1.5" stroke-dasharray="5,3" stroke-linejoin="round"/>' if p_alt_d else ""

        leg_uv = (
            '<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#9b59b6" stroke-width="2"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">UV ADC-Counts (links)</span></span>'
            '<span style="display:inline-flex;align-items:center;gap:4px;">'
            '<svg width="20" height="10"><line x1="0" y1="5" x2="20" y2="5" stroke="#2a78d6" stroke-width="2" stroke-dasharray="5,3"/></svg>'
            '<span style="font-size:12px;color:var(--secondary)">Höhe (m, rechts)</span></span>'
        )
        uv_alt_svg = (
            f'<div style="margin-bottom:6px">{leg_uv}</div>'
            f'<svg viewBox="0 0 {W2} {H2}" width="100%" style="display:block;overflow:visible">'
            f'{yt_uv}{xt_uv}{p_uv}{p_alt_uv_path}{yt_alt_uv}'
            f'<line x1="{PL}" y1="{PT}" x2="{PL}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'<line x1="{W2-PR_UV}" y1="{PT}" x2="{W2-PR_UV}" y2="{H2-PB}" stroke="var(--grid)" stroke-width="1"/>'
            f'</svg>'
        )

    # Phasenfarben
    phase_colors = {
        "PREFLIGHT": "#898781",
        "ASCENT":    "#2a78d6",
        "DESCENT":   "#1baf7a",
        "LANDED":    "#008300",
    }

    # Zeitleiste: Spannweite schließt simulierte Phasen ein
    BOARD_START_MS = 8 * 3600 * 1000 + 40 * 60 * 1000   # 08:40 in ms seit Mitternacht
    bar_t0 = BOARD_START_MS - offset_ms                   # t_ms_cont-äquivalent des Boardstarts
    bar_t1 = ts["t_end_ms"]
    _bar_span = bar_t1 - bar_t0

    def _pct(t_cont):
        return (t_cont - bar_t0) / _bar_span * 100

    # Zeitmarker für Statusleiste
    _events = [("08:40\nBoard",     8 * 3600 * 1000 + 40 * 60 * 1000),
               ("09:00\nStart",     9 * 3600 * 1000),
               ("10:14\nAbstieg",   10 * 3600 * 1000 + 14 * 60 * 1000 + 24 * 1000),
               ("10:59\nLandung",   10 * 3600 * 1000 + 59 * 60 * 1000 + 35 * 1000),
               ("12:20\nAufgehoben", 12 * 3600 * 1000 + 20 * 60 * 1000 + 40 * 1000),
               ("13:13\nEnde",      13 * 3600 * 1000 + 13 * 60 * 1000 + 2 * 1000)]
    markers_html = ""
    for label, wall_ms in _events:
        t_cont = wall_ms - offset_ms
        pct    = _pct(t_cont)
        if -2 <= pct <= 102:
            short = label.split("\n")
            markers_html += (
                f'<div style="position:absolute;left:{pct:.2f}%;top:0;width:1px;height:100%;'
                f'background:var(--primary);opacity:0.35;"></div>'
                f'<div style="position:absolute;left:{pct:.2f}%;bottom:-32px;font-size:11px;'
                f'color:var(--muted);transform:translateX(-50%);white-space:nowrap;text-align:center;line-height:1.3">'
                f'{short[0]}<br><span style="font-size:10px">{short[1]}</span></div>'
            )

    # Simulierte Phasen als Balken-Segmente
    sim_phase_colors = {
        "simulated_preflight": "#c98500",
        "simulated_ascent":    "#eda100",
    }
    bar_html = ""
    for sim_ph, sim_col in sim_phase_colors.items():
        sub = df_sim[df_sim["phase"] == sim_ph].sort_values("t_wall_ms")
        if sub.empty:
            continue
        t0_cont = int(sub["t_wall_ms"].iloc[0])  - offset_ms
        t1_cont = int(sub["t_wall_ms"].iloc[-1]) - offset_ms
        p0 = _pct(t0_cont)
        w  = _pct(t1_cont) - p0
        bar_html += (
            f'<div style="position:absolute;left:{p0:.3f}%;width:{w:.3f}%;height:100%;'
            f'background:{sim_col};opacity:0.6;" title="{sim_ph}"></div>'
        )

    # Echte Daten-Segmente
    for seg in ts["bar_segments"]:
        p0 = _pct(seg["start"] / 100 * (ts["t_end_ms"] - ts["t_start_ms"]) + ts["t_start_ms"])
        w  = seg["end"] / 100 * (ts["t_end_ms"] - ts["t_start_ms"]) + ts["t_start_ms"]
        w  = _pct(w) - p0
        if seg["type"] == "gap":
            gap_s = seg["gap_s"]
            bar_html += (
                f'<div style="position:absolute;left:{p0:.3f}%;width:{w:.3f}%;height:100%;'
                f'background:transparent;" title="Lücke: {gap_s} s"></div>'
            )
        else:
            col   = phase_colors.get(seg["phase"], "#ccc")
            phase = seg["phase"]
            bar_html += (
                f'<div style="position:absolute;left:{p0:.3f}%;width:{w:.3f}%;height:100%;'
                f'background:{col};" title="{phase}"></div>'
            )

    # Legende
    legend_html = ""
    for ph, col in phase_colors.items():
        legend_html += (
            f'<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            f'<span style="display:inline-block;width:10px;height:10px;border-radius:2px;background:{col};"></span>'
            f'<span style="font-size:12px;color:var(--secondary)">{ph}</span></span>'
        )
    for label, sim_col in sim_phase_colors.items():
        title = "Sim. Pre-Flight" if "preflight" in label else "Sim. Aufstieg"
        legend_html += (
            f'<span style="display:inline-flex;align-items:center;gap:4px;margin-right:12px;">'
            f'<span style="display:inline-block;width:10px;height:10px;border-radius:2px;background:{sim_col};opacity:0.6;"></span>'
            f'<span style="font-size:12px;color:var(--secondary)">{title}</span></span>'
        )
    legend_html += (
        '<span style="display:inline-flex;align-items:center;gap:4px;">'
        '<span style="display:inline-block;width:10px;height:10px;border-radius:2px;'
        'background:var(--page);border:1px solid var(--grid);"></span>'
        '<span style="font-size:12px;color:var(--secondary)">Lücke</span></span>'
    )

    # Timing-Tabelle
    timing_rows_html = ""
    for r in ts["timing_rows"]:
        bold = ' style="font-weight:600"' if r["phase"] == "GESAMT" else ""
        timing_rows_html += (
            f'<tr{bold}>'
            f'<td>{r["phase"]}</td>'
            f'<td>{r["rows"]:,}</td>'
            f'<td>{r["t_min_s"]:,.1f}</td>'
            f'<td>{r["t_max_s"]:,.1f}</td>'
            f'<td>{r["dur_s"]:,.1f}</td>'
            f'<td>{r["tick_min_ms"]:,.0f}</td>'
            f'<td>{r["tick_med_ms"]:,.0f}</td>'
            f'<td>{r["tick_max_ms"]:,.0f}</td>'
            f'<td>{r["gaps"]}</td>'
            f'</tr>'
        )

    # Höhenprofil-Bewertung
    hp_asc  = badge(hp["plausible_ascent"])
    hp_desc = badge(hp["plausible_descent"])

    phase_rows = ""
    for ph, s in hp["phase_stats"].items():
        phase_rows += f"""
        <tr>
          <td>{ph}</td>
          <td>{s['rows']:,}</td>
          <td>{s['alt_min_m']:,.1f}</td>
          <td>{s['alt_mean_m']:,.1f}</td>
          <td>{s['alt_max_m']:,.1f}</td>
        </tr>"""

    # Min/Max-Tabelle
    mm_rows = ""
    for r in mm:
        if r["n"] == 0:
            mm_rows += f"<tr><td>{r['sensor']}</td><td>{r['einheit']}</td><td colspan='4' style='color:var(--muted)'>keine Daten</td></tr>"
        else:
            mm_rows += f"<tr><td>{r['sensor']}</td><td>{r['einheit']}</td><td>{r['n']:,}</td><td>{r['min']}</td><td>{r['mean']}</td><td>{r['max']}</td></tr>"

    return f"""<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>Flug-Log Verifikationsbericht</title>
<style>
  :root {{
    --surface:   #fcfcfb;
    --page:      #f9f9f7;
    --primary:   #0b0b0b;
    --secondary: #52514e;
    --muted:     #898781;
    --grid:      #e1e0d9;
    --blue:      #2a78d6;
    --green:     #008300;
    --red:       #d03b3b;
    --yellow:    #b07400;
    --pass-bg:   #e6f4e6;
    --fail-bg:   #fdeaea;
    --warn-bg:   #fef6e0;
  }}
  @media (prefers-color-scheme: dark) {{
    :root {{
      --surface:   #1a1a19;
      --page:      #0d0d0d;
      --primary:   #ffffff;
      --secondary: #c3c2b7;
      --muted:     #898781;
      --grid:      #2c2c2a;
      --blue:      #3987e5;
      --green:     #0ca30c;
      --red:       #e66767;
      --yellow:    #c98500;
      --pass-bg:   #0e2b0e;
      --fail-bg:   #2b0e0e;
      --warn-bg:   #2b220e;
    }}
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{ background: var(--page); color: var(--primary);
          font: 14px/1.6 system-ui, -apple-system, "Segoe UI", sans-serif;
          padding: 32px 24px; max-width: 900px; margin: auto; }}
  h1   {{ font-size: 22px; margin-bottom: 4px; }}
  h2   {{ font-size: 16px; margin: 32px 0 12px; border-bottom: 1px solid var(--grid); padding-bottom: 6px; }}
  .meta {{ color: var(--secondary); font-size: 13px; margin-bottom: 28px; }}
  .kpi-row {{ display: flex; gap: 16px; flex-wrap: wrap; margin-bottom: 8px; }}
  .kpi {{ background: var(--surface); border: 1px solid var(--grid); border-radius: 8px;
           padding: 14px 20px; min-width: 160px; }}
  .kpi .val {{ font-size: 26px; font-weight: 600; color: var(--blue); }}
  .kpi .lbl {{ font-size: 12px; color: var(--secondary); margin-top: 2px; }}
  table {{ width: 100%; border-collapse: collapse; background: var(--surface);
           border: 1px solid var(--grid); border-radius: 8px; overflow: hidden; font-size: 13px; }}
  th {{ background: var(--page); color: var(--secondary); font-weight: 600;
        text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--grid); }}
  td {{ padding: 7px 12px; border-bottom: 1px solid var(--grid); }}
  tr:last-child td {{ border-bottom: none; }}
  .check-list {{ display: flex; flex-direction: column; gap: 8px; }}
  .check {{ display: flex; align-items: baseline; gap: 10px; font-size: 13px; }}
  .check .desc {{ color: var(--secondary); }}
  .badge {{ display: inline-block; padding: 2px 8px; border-radius: 4px;
             font-size: 12px; font-weight: 600; white-space: nowrap; }}
  .badge.pass {{ background: var(--pass-bg); color: var(--green); }}
  .badge.fail {{ background: var(--fail-bg); color: var(--red); }}
  .badge.warn {{ background: var(--warn-bg); color: var(--yellow); }}
  small {{ color: var(--muted); }}
</style>
</head>
<body>

<h1>Flug-Log Verifikationsbericht</h1>
<p class="meta">
  Quelldatei: {fi['name']} &nbsp;·&nbsp;
  {fi['size_kb']:,.0f} KB &nbsp;·&nbsp;
  {fi['raw_rows']:,} Rohzeilen &nbsp;·&nbsp;
  {fi['skipped_rows']:,} verworfen &nbsp;·&nbsp;
  {ts['total_rows']:,} gültig &nbsp;·&nbsp;
  Flugdauer: {ts['duration_min']} min
</p>

<!-- KPI -->
<div class="kpi-row">
  <div class="kpi"><div class="val">{fi['size_kb']:,.0f} KB</div><div class="lbl">Dateigröße</div></div>
  <div class="kpi"><div class="val">{ts['total_rows']:,}</div><div class="lbl">Gültige Zeilen</div></div>
  <div class="kpi"><div class="val">{fi['skipped_rows']:,}</div><div class="lbl">Verworfene Zeilen</div></div>
  <div class="kpi"><div class="val">{ts['duration_min']} min</div><div class="lbl">Aufzeichnungsdauer</div></div>
  <div class="kpi"><div class="val">{ts['median_interval_ms']} ms</div><div class="lbl">Median-Takt</div></div>
  <div class="kpi"><div class="val">{hp['max_altitude_m']:,.0f} m</div><div class="lbl">Max. Baro-Höhe</div></div>
</div>

<!-- 0. Anomalien -->
<h2>0. Anomalien &amp; Auffälligkeiten</h2>
<div class="check-list">
  {anomaly_rows}
</div>

<!-- 1. Zeitstempel -->
<h2>1. Zeitstempel</h2>
<table style="margin-bottom:20px">
  <thead>
    <tr>
      <th>Phase</th>
      <th>Zeilen</th>
      <th>t-Start (s)</th>
      <th>t-Ende (s)</th>
      <th>Dauer (s)</th>
      <th>Takt min (ms)</th>
      <th>Takt med (ms)</th>
      <th>Takt max (ms)</th>
      <th>Lücken (&gt;3×)</th>
    </tr>
  </thead>
  <tbody>{timing_rows_html}</tbody>
</table>

<div style="margin-bottom:8px">{legend_html}</div>
<div style="position:relative;height:28px;border-radius:6px;overflow:visible;
            background:var(--page);border:1px solid var(--grid);margin-bottom:52px;">
  {bar_html}
  {markers_html}
</div>

<!-- 2. Höhenprofil -->
<h2>2. Höhenprofil</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Phase</th><th>Zeilen</th><th>Höhe min (m)</th><th>Höhe Ø (m)</th><th>Höhe max (m)</th></tr></thead>
  <tbody>{phase_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {height_svg}
</div>

<!-- 3. Temperatur -->
<h2>3. Temperatur</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Sensor</th><th>n</th><th>Min</th><th>Max</th><th>Median</th><th>Ø</th></tr></thead>
  <tbody>{temp_table_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;margin-bottom:16px;">
  {temp_cmp_svg}
</div>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {temp_alt_svg}
</div>

<!-- 4. Luftdruck -->
<h2>4. Luftdruck</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Phase</th><th>n</th><th>Min (hPa)</th><th>Max (hPa)</th><th>Median (hPa)</th><th>Ø (hPa)</th></tr></thead>
  <tbody>{pres_table_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;margin-bottom:16px;">
  {pres_alt_svg}
</div>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {pres_parabola_svg}
</div>

<!-- 5. UV-Sensor -->
<h2>5. UV-Sensor</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Sensor</th><th>n</th><th>Min</th><th>Max</th><th>Median</th><th>Ø</th></tr></thead>
  <tbody>{uv_table_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {uv_alt_svg}
</div>

<!-- 6. Beschleunigung -->
<h2>6. Beschleunigung (MPU-6050)</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Sensor</th><th>n</th><th>Min</th><th>Max</th><th>Median</th><th>Ø</th></tr></thead>
  <tbody>{acc_table_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {acc_svg}
</div>

<!-- 7. Drehraten -->
<h2>7. Drehraten (MPU-6050)</h2>
<table style="margin-bottom:20px">
  <thead><tr><th>Sensor</th><th>n</th><th>Min</th><th>Max</th><th>Median</th><th>Ø</th></tr></thead>
  <tbody>{gyr_table_rows}</tbody>
</table>

<div style="background:var(--surface);border:1px solid var(--grid);border-radius:8px;padding:12px 8px 4px;">
  {gyr_svg}
</div>

<!-- 8. Min/Max -->
<h2>8. Sensor-Wertebereiche</h2>
<table>
  <thead><tr><th>Sensor</th><th>Einheit</th><th>n</th><th>Min</th><th>Ø</th><th>Max</th></tr></thead>
  <tbody>{mm_rows}</tbody>
</table>

</body>
</html>"""

def build_simulated(df: pd.DataFrame) -> pd.DataFrame:
    """
    Simulierte Phasen vor dem ersten echten Log-Punkt.
    Zeitachse: Wall-Clock ms seit Mitternacht (t_wall_ms).
    """
    BOARD_START_MS = 8 * 3600 * 1000 + 40 * 60 * 1000   # 08:40
    LAUNCH_MS      = 9 * 3600 * 1000                      # 09:00

    # Board hat bei t_ms=0 angefangen zu zählen → Wanduhr-Offset = Board-Startzeit
    OFFSET_MS = BOARD_START_MS

    asc_first    = df[df["phase"] == "ASCENT"].sort_values("t_ms_cont").iloc[0]
    asc_wall_end = int(asc_first["t_ms_cont"]) + OFFSET_MS

    alt_boden = float(df[df["phase"] == "PREFLIGHT"]["alt_baro_m"].median())
    alt_asc0  = float(asc_first["alt_baro_m"])

    records = []

    # simulated_preflight: Board-Start (08:40) → Flugstart (09:00), Höhe = Boden konstant
    for t in range(BOARD_START_MS, LAUNCH_MS + 1000, 1000):
        h = t // 1000
        records.append({
            "t_wall_ms":  t,
            "phase":      "simulated_preflight",
            "alt_baro_m": alt_boden,
            "utc":        f"{h//3600:02d}:{(h%3600)//60:02d}:{h%60:02d}",
        })

    # simulated_ascent: 09:00 → erster echter ASCENT-Punkt, Höhe linear
    span = asc_wall_end - LAUNCH_MS
    for t in range(LAUNCH_MS, asc_wall_end + 1000, 1000):
        frac = (t - LAUNCH_MS) / span if span > 0 else 0
        h = t // 1000
        records.append({
            "t_wall_ms":  t,
            "phase":      "simulated_ascent",
            "alt_baro_m": round(alt_boden + frac * (alt_asc0 - alt_boden), 1),
            "utc":        f"{h//3600:02d}:{(h%3600)//60:02d}:{h%60:02d}",
        })

    return pd.DataFrame(records), OFFSET_MS


# ── Main ────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    src  = Path(sys.argv[1]) if len(sys.argv) > 1 else CSV
    out  = src.with_suffix("").parent / "report.html"

    print(f"Lade {src} …")
    raw_rows = sum(1 for _ in open(src, encoding="latin-1", errors="replace"))
    df = load(src)

    fi = {
        "name":         src.name,
        "size_kb":      src.stat().st_size / 1024,
        "raw_rows":     raw_rows,
        "skipped_rows": raw_rows - len(df),
    }

    print("Analysiere …")
    ts = check_timestamps(df)
    hp = check_height_profile(df)
    mm = sensor_minmax(df)
    an = check_anomalies(df)

    df_sim, offset_ms = build_simulated(df)
    html = render_html(ts, hp, mm, an, fi, df, df_sim, offset_ms)
    out.write_text(html, encoding="utf-8")
    print(f"Bericht geschrieben → {out}")
