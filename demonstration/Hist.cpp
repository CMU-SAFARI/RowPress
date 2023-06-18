#include "Hist.h"
#include <fstream>
#include <iostream>

#include <cassert>

Hist::Hist(int binsize)
{
    this->binsize = binsize;
}

void Hist::observe_latency(int latency)
{
    int key = latency/binsize;
    if (bins.find(key) == bins.end())
        bins[key] = 1;
    else
        bins[key]++;
}

int Hist::get_bin(int key)
{
    return bins[key];
}

std::vector<int> Hist::get_all_bin_keys()
{
    std::vector<int> keys;

    for(auto kv : bins)
        keys.push_back(kv.first);

    return keys;
}

void Hist::to_csv(std::string filename)
{
    std::cout << "Bin size: " << binsize << std::endl;

    std::ofstream file;
    file.open(filename);
    file << "latency,count" << std::endl;
    for(auto kv : bins)
    {
        if (kv.second == 0)
            continue;
        file << (kv.first * binsize) << "," << kv.second << std::endl;
    }
    file.close();
}

void Hist::clear_all_values()
{
    for (auto kv : bins)
        bins[kv.first] = 0;
}