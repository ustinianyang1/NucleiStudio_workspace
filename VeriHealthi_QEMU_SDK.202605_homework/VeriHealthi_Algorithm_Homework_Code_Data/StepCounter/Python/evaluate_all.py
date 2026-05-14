import os
import glob
import numpy as np
import itertools
from step_counter_simulation import BPNetwork, count_steps_for_file
import torch

def evaluate_all():
    base_dir = "../../AccData"
    
    # Load model
    model_path = "model.pth"
    model = BPNetwork(19)
    if os.path.exists(model_path):
        model.load_state_dict(torch.load(model_path))
    model.eval()

    import re
    # gather all txt files
    all_files = glob.glob(os.path.join(base_dir, "**/*.txt"), recursive=True)
    all_files = [f for f in all_files if "IMU_" in f]
    
    file_info = []
    for f in all_files:
        m = re.search(r'_step(\d+)\.txt', f)
        if m:
            true_steps = int(m.group(1))
            file_info.append((f, true_steps))
            
    print(f"Found {len(file_info)} files testing...")
    
    base_thresh = 4096 // 10
    base_pvd = 4096 // 14
    
    # Define Parameter Grid
    grid = {
        'STEP_ACC_DIFF_THRESHOLD': [base_thresh - 100, base_thresh, base_thresh + 100],
        'PEAK_VALLEY_DIFFERENCE': [base_pvd - 50, base_pvd, base_pvd + 50],
        'TIME_THRESHOLD1': [3, 4, 5],
        'TIME_THRESHOLD2': [30, 40, 50]
    }
    
    keys = list(grid.keys())
    combinations = list(itertools.product(*(grid[k] for k in keys)))
    print(f"Total Combinations: {len(combinations)}")
    
    best_combo = None
    best_mae = float('inf')
    best_mape = float('inf')
    
    # Try a few combinations to find minimum MAE
    for idx, combo in enumerate(combinations):
        params = dict(zip(keys, combo))
        
        err_abs = 0
        err_pct = 0
        valid_files = 0
        
        for fp, true_steps in file_info:
            pred_steps = count_steps_for_file(fp, model, params)
            err_abs += abs(pred_steps - true_steps)
            if true_steps > 0:
                err_pct += abs(pred_steps - true_steps) / true_steps
                valid_files += 1
                
        mae = err_abs / len(file_info) if len(file_info) > 0 else 0
        mape = err_pct / valid_files if valid_files > 0 else 0
        
        if mae < best_mae:
            best_mae = mae
            best_mape = mape
            best_combo = params
            print(f"New Best MAE: {mae:.4f}, MAPE: {mape:.4f} @ {params}")

    print("="*40)
    print("BEST PARAMS FOUND:")
    print(best_combo)
    print(f"BEST MAE: {best_mae:.4f}")
    print(f"BEST MAPE: {best_mape:.4f}")

if __name__ == "__main__":
    evaluate_all()