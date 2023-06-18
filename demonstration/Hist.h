#include <unordered_map>
#include <vector>
#include <string>

/**
 * A class that collects latency values into bins.
 * The bins are stored in a hash table.
 * The key is the bin number, and the value is the number of latency values
 * that fall into that bin.
*/
class Hist
{
private:
    // collect observed latency values into bins
    // bins[0] --> latency values [0, 10)
    // bins[1] --> latency values [10, 20)
    // ...
    // bins[X] --> latency values [X*10, X*10+10)
    std::unordered_map<int, int> bins;
    int binsize = 10;
public:
    Hist(int binsize);
    void observe_latency(int latency);
    int get_bin(int key);
    std::vector<int> get_all_bin_keys();
    void to_csv(std::string filename);
    void clear_all_values();
};
