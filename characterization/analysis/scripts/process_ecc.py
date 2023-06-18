import os, sys
import pandas as pd
import warnings
warnings.filterwarnings("ignore")

from script_utils import *

def get_filename(type, attack_time, tAggON, module):
    if(type == 'single'):
        rp_filename = module + '_7711_258_60ms'
    elif(type == 'double'):
        rp_filename = module + '_3855_258_60ms'
    return rp_filename

def get_bfs_per_word(filename, itr):
    df = pd.read_csv(filename + "_itr" + str(itr) + ".csv")
    df['bit_loc'] = df['Cacheline']*512 + df['Word']*64 + df['Byte']*8 + df['Bit']
    df['Row'] = df['Pivot Row'] + df['Row Offset']
    df = df.drop(['Cacheline', 'Word',  'Byte', 'Bit', 'Dir', 'Hammer Count', 'Bank', 'Row Offset', 'Pivot Row'], axis=1)
    df = df.drop_duplicates()
    df['ECC Word'] = (df['bit_loc'] / 64).astype('int')
    df = df.drop(['bit_loc'], axis=1)
    df = df.groupby(['Row', 'ECC Word']).size().reset_index(name='bfs per word')
    bfs_per_word_dict = df['bfs per word'].value_counts().to_dict()
    return bfs_per_word_dict, itr

def get_worst_res(filename):
    if(not os.path.exists(filename + "_itr0.csv")):
        return dict(), -1
    df = pd.read_csv(filename + "_itr0.csv")
    itr = 0
    max_df_len = len(df.index)
    for i in range(1, 5):
        df = pd.read_csv(filename + "_itr" + str(i) + ".csv")
        df_len = len(df.index)
        if(df_len > max_df_len):
            itr = i
    return get_bfs_per_word(filename, itr)

def main():
    try:
        data_root = sys.argv[1]
        module = sys.argv[2]
    except Exception as e:
        print("Usage: python3 parse_ecc_log.py path/to/raw/data/dir module_name")
        raise

    output_folder = "../processed_data/ecc/"
    temp = "80C"
    attack_time = "60ms"
    tAggON = "RP_trefi"
    test = "BER"

    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
        
    for type in ['single', 'double']:
        log = open(output_folder + "/" + module + "_" + type + ".log", 'w')
        try:
            rp_filename = get_filename(type, attack_time, tAggON, module)
            filename =  data_root + "/" + module + "/" + test + "/" + type + "-checkered/" + temp + "/" + rp_filename

            log.write("Type: " + type + "\tAttack time: " + str(attack_time) + " \ttAggON: " + tAggON + "\n")
            bfs_per_word_dict, itr = get_worst_res(filename)

            if(itr == -1 or len(bfs_per_word_dict.items()) == 0):
                log.write("No bitflips\n")
            else:
                for item in bfs_per_word_dict.items():
                    log.write(str(item[0]) + "," + str(item[1]) + "\n")
            log.close()
        except Exception as e:
            log.write("Type: " + type + "\tAttack time: " + str(attack_time) + " \ttAggON: " + tAggON + "\n")
            log.write(str(e))
            log.close()

if __name__ == "__main__":
  main()