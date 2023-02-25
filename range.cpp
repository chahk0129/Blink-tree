#include "tree.h"

#include <ctime>
#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include <algorithm>

using Key_t = uint64_t;
using namespace BLINK_TREE;

inline uint64_t _Rdtsc(){
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return (((uint64_t)hi << 32 ) | lo);
}


int main(int argc, char* argv[]){
    if(argc < 3){
	std::cerr << "Usage: " << argv[0] << " num_data num_threads" << std::endl;
	exit(0);
    }

    int num_data = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    Key_t* keys = new Key_t[num_data];
    for(int i=0; i<num_data; i++){
	keys[i] = i+1;
    }
    std::random_shuffle(keys, keys+num_data);

    btree_t<Key_t>* tree = new btree_t<Key_t>();
//    std::cout << "inode_size(" << inode_t<Key_t>::cardinality << "), lnode_btree_size(" << lnode_btree_t<Key_t, Value_t>::cardinality << "), lnode_hash_size(" << lnode_hash_t<Key_t, Value_t>::cardinality << ")" << std::endl;

    struct timespec start, end;
    auto warmup = [&tree, &keys, num_data, num_threads](int tid){
	size_t chunk = num_data / num_threads;
	int from = chunk * tid;
	int to = chunk * (tid + 1);
	if(tid == num_threads-1)
	    to = num_data;

	for(int i=from; i<to; i++){
	    tree->insert(keys[i], (uint64_t)keys[i]);
	}
    };

    auto scan = [&tree, &keys, num_data, num_threads](int tid){
	size_t chunk = num_data / num_threads;
	int from = chunk * tid;
	int to = chunk * (tid + 1);
	if(tid == num_threads - 1)
	    to = num_data;
	uint64_t buf[100];
	int range = 50;

	for(int i=from; i<to; i++){
	    //int range = rand() % 100;
	    //std::cout << range << std::endl;
	    auto ret = tree->range_lookup(keys[i], range, buf);
	}
    };

    std::vector<std::thread> warmup_threads;
    std::cout << "warmup starts" << std::endl;
    for(int i=0; i<num_threads; i++)
	warmup_threads.push_back(std::thread(warmup, i));
    for(auto& t: warmup_threads) t.join();

    auto height = tree->height();
    std::cout << "height of three: " << height+1 << std::endl;

    std::vector<std::thread> scan_threads;
    std::cout << "scan starts" << std::endl;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<num_threads; i++)
	scan_threads.push_back(std::thread(scan, i));
    for(auto& t: scan_threads) t.join();
    clock_gettime(CLOCK_MONOTONIC, &end);

    auto elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    auto tput = (double)num_data / (elapsed/1000000000.0) / 1000000.0;
    std::cout << "elapsed time: " << elapsed/1000000000.0 << " sec" << std::endl;
    std::cout << "throughput: " << tput << " mops/sec" << std::endl;

    height = tree->height();
    std::cout << "height of three: " << height+1 << std::endl;
    return 0;
}
