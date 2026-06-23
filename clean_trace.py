import re
import os

def clean_trace():
    txt_path = "dos_Tessla.txt"
    filtered_path = "dos_Tessla_filtered.txt"
    
    if not os.path.exists(txt_path):
        print(f"Error: {txt_path} not found.")
        return
        
    with open(txt_path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()
        
    # Step 1: Filter only lines matching the 'time: var = val' format
    valid_lines = [l for l in lines if re.match(r'^\d+:\s+\w+\s+=\s+\d+', l.strip())]
    
    # Step 2: Enforce monotonically increasing timestamps with reset detection and outlier drop
    cleaned_lines = []
    last = None
    
    for l in valid_lines:
        m = re.match(r"^(\d+):", l)
        if not m:
            continue
        t = int(m.group(1))
        
        if last is None:
            cleaned_lines.append(l)
            last = t
        elif t >= last and t < last + 20000:
            cleaned_lines.append(l)
            last = t
        elif t < 1000 and last > 1000:
            # Board reset detected: start fresh to keep timestamps strictly increasing
            cleaned_lines = [l]
            last = t
            
    with open(filtered_path, "w", encoding="utf-8") as f:
        f.writelines(cleaned_lines)
    print(f"Successfully cleaned and wrote {len(cleaned_lines)} lines to {filtered_path}.")

if __name__ == "__main__":
    clean_trace()
