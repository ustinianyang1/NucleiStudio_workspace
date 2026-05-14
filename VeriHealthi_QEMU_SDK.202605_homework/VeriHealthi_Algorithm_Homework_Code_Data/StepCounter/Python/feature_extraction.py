import os
import glob
import numpy as np
import pandas as pd

def func_calculation(data, func_size, func):
    data_len = len(data)
    half_len = int((func_size - 1) / 2)
    func_data = np.zeros(data_len)
    for i in range(data_len):
        if i <= half_len:
            func_data[i] = func(data[0:2*i+1])
        else:
            if (i + half_len < data_len):
                func_data[i] = func(data[(i - half_len):(i + half_len+1)])
            else:
                func_data[i] = func(data[(2*i+1-data_len):data_len])
    return func_data

def process_file(filepath):
    # Read file
    with open(filepath, mode="r", encoding="utf-8") as f:
        lines = f.readlines()
    
    if len(lines) <= 5:
        return None
        
    cnt = len(lines) - 5
    data = np.zeros(cnt)
    for i, line in enumerate(lines[5:]):
        try:
            data[i] = float(line.strip())
        except ValueError:
            data[i] = 0

    acc_len = cnt // 7
    if acc_len == 0:
        return None
        
    # extract acc x, y, z
    acc_data = np.zeros((acc_len, 4))
    acc_data[:, 0] = data[3:cnt:7]
    acc_data[:, 1] = data[4:cnt:7]
    acc_data[:, 2] = data[5:cnt:7]

    # moving average
    m1 = 15
    m2 = 7
    acc_xyz_mean = np.zeros((acc_len, 4))
    for i in range(3):
        tmp_data = acc_data[:, i]
        tmp_data = func_calculation(tmp_data, m1, np.mean)
        tmp_data = tmp_data.astype(int)
        tmp_data = func_calculation(tmp_data, m2, np.mean)
        tmp_data = tmp_data.astype(int)
        acc_xyz_mean[:, i] = tmp_data
        
    acc_xyz_mean[:, 3] = (acc_xyz_mean[:, 0]**2 + acc_xyz_mean[:, 1]**2 + acc_xyz_mean[:, 2]**2)**0.5
    return acc_xyz_mean

def get_zero_crossing_rate(arr):
    arr_mean_removed = arr - np.mean(arr)
    zero_crossings = np.where(np.diff(np.sign(arr_mean_removed)))[0]
    return len(zero_crossings)

def get_max_fft_amplitude(arr):
    fft_vals = np.abs(np.fft.rfft(arr))
    if len(fft_vals) > 1:
        # Ignore DC component
        return np.max(fft_vals[1:])
    return 0

def extract_features_from_window(window):
    features = {}
    axes = ["x", "y", "z", "mag"]
    
    for i in range(4):
        ax = axes[i]
        w = window[:, i]
        features[f"{ax}_variance"] = np.var(w)
        features[f"{ax}_zcr"] = get_zero_crossing_rate(w)
        features[f"{ax}_energy"] = np.sum(w**2)
        features[f"{ax}_max_fft_amplitude"] = get_max_fft_amplitude(w)
        
    # correlations
    # To handle potential division by zero if variance is 0, we use np.corrcoef
    if np.var(window[:, 0]) > 0 and np.var(window[:, 1]) > 0:
        features["corr_xy"] = np.corrcoef(window[:, 0], window[:, 1])[0, 1]
    else:
        features["corr_xy"] = 0
        
    if np.var(window[:, 0]) > 0 and np.var(window[:, 2]) > 0:
        features["corr_xz"] = np.corrcoef(window[:, 0], window[:, 2])[0, 1]
    else:
        features["corr_xz"] = 0
        
    if np.var(window[:, 1]) > 0 and np.var(window[:, 2]) > 0:
        features["corr_yz"] = np.corrcoef(window[:, 1], window[:, 2])[0, 1]
    else:
        features["corr_yz"] = 0
        
    return features

def main():
    base_dir = "../../AccData"
    categories = {
        "walk": 1,
        "run": 1,
        "others": 0
    }
    
    window_size = 250
    step_size = 125 # 50% overlap for more samples
    
    all_features = []
    all_labels = []
    
    for cat, label in categories.items():
        search_path = os.path.join(base_dir, cat, "*.txt")
        files = glob.glob(search_path)
        for filepath in files:
            acc_xyz_mean = process_file(filepath)
            if acc_xyz_mean is None or len(acc_xyz_mean) < window_size:
                continue
                
            for start in range(0, len(acc_xyz_mean) - window_size + 1, step_size):
                window = acc_xyz_mean[start:start+window_size, :]
                features = extract_features_from_window(window)
                all_features.append(features)
                all_labels.append({"label": label})
                
    features_df = pd.DataFrame(all_features)
    labels_df = pd.DataFrame(all_labels)
    
    features_df.to_csv("features.csv", index=False)
    labels_df.to_csv("labels.csv", index=False)
    print("Done! features.csv and labels.csv have been exported.")

if __name__ == "__main__":
    main()
