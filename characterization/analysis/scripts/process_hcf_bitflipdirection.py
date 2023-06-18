import os
import sys
import numpy as np
import pandas as pd
from script_utils import *


def get_number_of_bitflipdirection(df: pd.DataFrame(), module: str, pattern: str, tAggON: int, direction: int):
    tmp_df = df.copy(deep=True)
    tmp_df = df[(df['Dir'] == direction) & (df['module']==module) & (df['pattern']==pattern) & (df['tAggON']==tAggON)].count()
    tmp_df.reset_index()
    return tmp_df.iloc[0]


def create_DataFrame_BitFlipDirection_final():
    create_df = pd.DataFrame(columns=['module',
                                      'pattern',
                                      'temperature',
                                      'hc',
                                      'tAggON',
                                      'dir0',
                                      'dir1',
                                      'total_bitflips'])
    return create_df


def add_row_to_df(module: str, pattern: str, temperature: int, hc: int, tAggON: int, dir0: int, dir1: int, total_bitflips: int):
    add_row = [module, pattern, temperature, hc, tAggON, dir0, dir1, total_bitflips]
    return add_row


if __name__ == "__main__":
    data_root = sys.argv[1]
    selected_experiment = "HCFIRST"
    selected_pattern = "single-checkered"   
    selected_temperature_str = "50C"
    selected_module = sys.argv[2]
    
    selected_temperature = 50


    hcf_dfs = []
    for root, dirs, files in os.walk(data_root):
        for file in files:
            filepath = os.path.abspath(os.path.join(root, file))
            module, experiment, pattern, temperature, filename = decode_filepath_bitflipdirection(filepath)
            path_of_interest, path = os.path.split(filepath)
            if module == selected_module and experiment == selected_experiment and pattern == selected_pattern and temperature == selected_temperature and filename.endswith(".csv"):
                current_dir = os.getcwd()
                os.chdir(path_of_interest)
                df_tmp = pd.read_csv(filename)
                
                module, tAggON, itr, ras_scale = decode_hcf_filename(filename)
                df_tmp['module']=module
                df_tmp['pattern']=selected_pattern
                df_tmp['temperature']=selected_temperature
                df_tmp['tAggON']=tAggON
                df_tmp['itr'] = itr
                
                hcf_dfs.append(df_tmp)

                os.chdir(current_dir)

    # delete rows with inexistent hc: https://stackoverflow.com/questions/13413590/how-to-drop-rows-of-pandas-dataframe-whose-value-in-a-certain-column-is-nan
    hcf_tmp = pd.concat(hcf_dfs, ignore_index=True)
    hcf = hcf_tmp[hcf_tmp['Hammer Count'].notna()]
    hcf.reset_index()

    # drop columns not needed
    hcf.drop(columns=['Cacheline', 'Bank', 'Word', 'Byte', 'Bit', 'Row Offset'], axis = 1, inplace = True)

    # reorder columns
    columns = ['module', 'pattern', 'temperature', 'tAggON', 'Pivot Row', 'Hammer Count', 'Dir', 'itr']
    hcf = hcf[columns]
    
    dict = {'module': str, 'pattern': str, 'temperature': int, 'tAggON': int, 'Hammer Count': int, 'Pivot Row': int, 'Dir': int, 'itr': int}
    hcf = hcf.astype(dict)

    # filter out rows with min hc
    min_idx = hcf.groupby(['module', 'pattern', 'temperature', 'tAggON', 'Pivot Row'])['Hammer Count'].idxmin() # https://stackoverflow.com/questions/15705630/get-the-rows-which-have-the-max-value-in-groups-using-groupby
    hcf_minidx = hcf.copy().loc[min_idx].reset_index(drop=True)
    hcf_minidx.drop(['Pivot Row'], axis = 1, inplace = True)
    
    df_final = create_DataFrame_BitFlipDirection_final()

    for i in range(0, len(hcf_minidx)):

        module = hcf_minidx.at[i, 'module']
        pattern = hcf_minidx.at[i, 'pattern']
        temperature = hcf_minidx.at[i, 'temperature']
        hc = hcf_minidx.at[i, 'Hammer Count']
        tAggON = hcf_minidx.at[i, 'tAggON']

        # get bitflips direction 0
        dir0 = get_number_of_bitflipdirection(hcf_minidx, module, pattern, tAggON, 0)
        # get bitflips direction 1
        dir1 = get_number_of_bitflipdirection(hcf_minidx, module, pattern, tAggON, 1)
        # total bitflips
        total_bitflips = dir0 + dir1
        #df_final_dfs.append()
        add_row = add_row_to_df(module, pattern, temperature, hc, tAggON, dir0, dir1, total_bitflips)
        df_final.loc[len(df_final)] = add_row
    
    df_final.reset_index()
    min_idx = df_final.groupby(['module', 'pattern', 'temperature', 'tAggON'])['hc'].idxmin()
    df_final_minidx = df_final.copy().loc[min_idx].reset_index(drop=True)

    selected_pattern = selected_pattern.replace('-','')
    os.system("mkdir -p ../processed_data/hcf_bitflipdirection")
    # path to updated
    df_final_minidx.to_csv("../processed_data/hcf_bitflipdirection/" + "{}_{}_{}_{}_bitflipdirection.csv".format(selected_module, selected_experiment, selected_temperature, selected_pattern), index=False)