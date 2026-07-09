import pandas as pd
import sys
from pathlib import Path

CSV_FILE = Path(__file__).parent / "flight.csv"

COLUMNS = [
    "t_ms", "utc", "phase", "fix_q",
    "lat", "lon", "alt_gps_m", "sats",
    "temp_c", "pressure_hpa", "alt_baro_m",
    "temp_ext_c", "uv_raw",
    "acc_x_g", "acc_y_g", "acc_z_g",
    "gyr_x_dps", "gyr_y_dps", "gyr_z_dps",
]

def load(path: Path = CSV_FILE) -> pd.DataFrame:
    df = pd.read_csv(path, header=None, names=COLUMNS, encoding="latin-1", on_bad_lines="skip")
    df["t_ms"] = pd.to_numeric(df["t_ms"], errors="coerce")
    valid_phases = {"PREFLIGHT", "ASCENT", "DESCENT", "LANDED"}
    df = df[df["t_ms"].notna() & df["phase"].isin(valid_phases)].copy()
    df["t_ms"] = df["t_ms"].astype("int64")
    diff = df["t_ms"].diff().fillna(0)
    median_tick = int(df["t_ms"].diff().median())
    offset = (diff.clip(upper=0).abs() + median_tick) * (diff < 0)
    df["t_ms_cont"] = df["t_ms"] + offset.cumsum().astype("int64")
    df["t_s"] = df["t_ms"] / 1000.0
    df["t_s_cont"] = df["t_ms_cont"] / 1000.0
    numeric_cols = ["lat", "lon", "alt_gps_m", "sats",
                    "temp_c", "pressure_hpa", "alt_baro_m",
                    "temp_ext_c", "uv_raw",
                    "acc_x_g", "acc_y_g", "acc_z_g",
                    "gyr_x_dps", "gyr_y_dps", "gyr_z_dps"]
    for col in numeric_cols:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    return df

def summary(df: pd.DataFrame) -> None:
    print(f"Zeilen:  {len(df)}")
    print(f"Phasen:  {df['phase'].value_counts().to_dict()}")
    print(f"Laufzeit: {df['t_ms'].iloc[-1] - df['t_ms'].iloc[0]} ms")

    fix = df[df["lat"].notna()]
    if not fix.empty:
        print(f"GPS-Fix: {len(fix)} Zeilen")
        print(f"  Höhe GPS: min={fix['alt_gps_m'].min():.1f} m  max={fix['alt_gps_m'].max():.1f} m")

    if df["alt_baro_m"].notna().any():
        print(f"  Höhe Baro: min={df['alt_baro_m'].min():.1f} m  max={df['alt_baro_m'].max():.1f} m")

    if df["temp_c"].notna().any():
        print(f"  Temp BMP: min={df['temp_c'].min():.1f} °C  max={df['temp_c'].max():.1f} °C")

    if df["temp_ext_c"].notna().any():
        print(f"  Temp ext: min={df['temp_ext_c'].min():.1f} °C  max={df['temp_ext_c'].max():.1f} °C")

if __name__ == "__main__":
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else CSV_FILE
    df = load(path)
    summary(df)
    print("\nErste 3 Zeilen:")
    print(df.head(3).to_string())
