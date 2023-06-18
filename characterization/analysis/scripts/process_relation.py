import os, sys
import pandas as pd
import warnings
warnings.filterwarnings("ignore")

from script_utils import *

def get_filenames(test, type):
    rp_filenames = ["_1000000","_100000","_10000","_1000","_100","_10","_1","_200000","_20000","_20","_2338","_258","_2","_500000","_50000","_5000","_500","_50","_5"]
    rh_filename = "_0"
    return rp_filenames, rh_filename

def get_ret_df(main_folder, ret_filename, module, type):
    filename =  main_folder + "/" + module + "/RETENTION_FAILURE/" + type + "-checkered/80C/" + ret_filename
    df = pd.read_csv(filename + "_itr0.csv")
    itr = 0
    max_df_len = len(df.index)
    for i in range(1, 5):
        df = pd.read_csv(filename + "_itr" + str(i) + ".csv")
        df_len = len(df.index)
        if(df_len > max_df_len):
            itr = i

    filename =  main_folder + "/" + module + "/RETENTION_FAILURE/" + type + "-checkered/80C/" + ret_filename
    df = pd.read_csv(filename + "_itr" + str(itr) + ".csv")
    df['bit_loc'] = df['Cacheline']*512 + df['Word']*64 + df['Byte']*8 + df['Bit']
    df = df.drop(['Cacheline', 'Word',  'Byte', 'Bit', 'Dir', 'Hammer Count', 'Bank'], axis=1)
    return df

def get_df(main_folder, module, test, type, temp, res_filename):
    filename = main_folder + '/' + module + '/' + test + '/' + type + '-checkered/' + temp + '/' + res_filename
    if(not os.path.exists(filename + "_itr0.csv")):
        return pd.DataFrame(), filename + "_itr0.csv"
    hcf_all_df = pd.DataFrame()
    for i in range(5):
        df = pd.read_csv(filename + "_itr" + str(i) + ".csv")
        df['bit_loc'] = df['Cacheline']*512 + df['Word']*64 + df['Byte']*8 + df['Bit']
        df = df.drop(['Cacheline', 'Word',  'Byte', 'Bit', 'Dir', 'Bank'], axis=1)
        df['itr'] = i
        hcf_all_df = hcf_all_df.append(df, ignore_index=True)

    idx = hcf_all_df.groupby(['Pivot Row'])['Hammer Count'].transform(min) == hcf_all_df['Hammer Count']
    hcf_df = hcf_all_df[idx].copy()
    hcf_df = hcf_df.drop(['itr', 'Hammer Count'], axis=1)
    hcf_df = hcf_df.drop_duplicates()
    return hcf_df, "found"

def main():
    try:
        data_root = sys.argv[1]
        module = sys.argv[2]
    except Exception as e:
        print("Usage: python3 parse_relation_log.py path/to/raw/data/dir module_name")
        raise

    type = "single"
    temp = "50C"
    test = "HCFIRST"

    output_folder = "../processed_data/relation/"

    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    title = module + "_" + test + "_" + type + "_" + temp
    log = open(output_folder + title + ".log", "w")

    rp_filenames, rh_filename = get_filenames(test, type)
    rp_filenames = [module + rp_filename for rp_filename in rp_filenames]
    rh_filename = module + rh_filename
    ret_filename = module + "_4096"

    ret_df = get_ret_df(data_root, ret_filename, module, type)
    rh_df, found = get_df(data_root, module, test, type, temp, rh_filename)
    if(found != "found"):
        log.write("(not found - " + found + ")")
        log.close()
        exit()

    total_rh_bfs = len(rh_df.index)
    total_ret_bfs = len(ret_df.index)

    log.write("tAggON,rp_bfs,rp_rh_bfs,rp_ret_bfs,rh_bfs,ret_bfs\n")
    for i, rp_filename in enumerate(rp_filenames):
        rp_df, found = get_df(data_root, module, test, type, temp, rp_filename)
        tAggON = str(ras_scale_to_tAggON_ns(int(rp_filename.split("_")[1])))

        if(found != "found"):
            log.write(tAggON + ",0,NaN,NaN," + str(total_rh_bfs) + ',' + str(total_ret_bfs) + '\n')
            continue
        
        total_rp_bfs = len(rp_df.index)
        log.write(tAggON + ',' + str(total_rp_bfs))
        
        if(total_rp_bfs == 0):
            log.write(",NaN,NaN," + str(total_rh_bfs) + ',' + str(total_ret_bfs) + '\n')
            continue

        overlap_rh_df = pd.merge(rh_df, rp_df, how ='inner')
        overlap_rh_bfs = len(overlap_rh_df.index)
        log.write("," + str(overlap_rh_bfs))

        overlap_ret_df = pd.merge(ret_df, rp_df, how ='inner')
        overlap_ret_bfs = len(overlap_ret_df.index)
        log.write("," + str(overlap_ret_bfs))

        log.write(',' + str(total_rh_bfs) + ',' + str(total_ret_bfs) + '\n')

    log.close()

if __name__ == "__main__":
  main()