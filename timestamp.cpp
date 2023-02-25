#include "tree.h"

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <vector>
#include <thread>
#include <algorithm>

using namespace std;
using namespace BLINK_TREE;
using Key_t = uint64_t;
using Value_t = uint64_t;

inline uint64_t Rdtsc(){
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return (((uint64_t) hi << 32) | lo);
}

enum{
    OP_INSERT,
    OP_READ,
    OP_SCAN
};

template <typename Key_t, typename Value_t>
struct kvpair_t{
    Key_t key;
    Value_t value;
};

int main(int argc, char* argv[]){
    if(argc < 3){
	std::cerr << "Usage: " << argv[0] << " num_data num_threads (mode) ---- mode is optional" << std::endl;
	std::cerr << "\t insert-only if mode is not given" << std::endl;
	std::cerr << "\t otherwise, 0=scan-only, 1=read-only, 2-mixed" << std::endl;
	exit(0);
    }
    int num_data = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    int mode = 0; 
    if(argc == 4){
	mode = atoi(argv[3]);
	// 0: scan only
	// 1: read only
	// 2: balanced
    }

    btree_t<Key_t>* tree = new btree_t<Key_t>();
    std::vector<kvpair_t<Key_t, Value_t>> keys[num_threads];
    for(int i=0; i<num_threads; i++){
	size_t chunk = num_data / num_threads;
	keys[i].reserve(chunk);
    }

    auto load = [tree, num_data, num_threads, &keys](int tid){
	int sensor_id = 0;
	size_t chunk = num_data / num_threads;
	kvpair_t<Key_t, Value_t>* kv = new kvpair_t<Key_t, Value_t>[chunk];

	for(int i=0; i<chunk; i++){
	    kv[i].key = ((Rdtsc() << 16) | sensor_id++ << 6) | tid;
	    kv[i].value = reinterpret_cast<Value_t>(&kv[i].key);
	    keys[tid].push_back(kv[i]);
	    tree->insert(kv[i].key, kv[i].value);
	    if(sensor_id == 1024)
		sensor_id = 0;
	}
    };

    struct timespec start, end;

    std::vector<std::thread> load_threads;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<num_threads; i++)
	load_threads.push_back(std::thread(load, i));
    for(auto& t: load_threads) t.join();
    clock_gettime(CLOCK_MONOTONIC, &end);

    auto elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    auto tput = (double)num_data / (elapsed/1000000000.0) / 1000000.0;
    std::cout << "Insertion: " << tput << " mops/sec" << std::endl;

    std::vector<std::pair<kvpair_t<Key_t, Value_t>, std::pair<int, int>>> ops;
    ops.reserve(num_data);
    if(mode == 0){ // scan only
	std::cout << "Scan 100%" << std::endl;
	for(int i=0; i<num_threads; i++){
	    for(auto& v: keys[i]){
		int r = rand() % 100;
		ops.push_back(std::make_pair(v, std::make_pair(OP_SCAN, r)));
	    }
	}
    }
    else if(mode == 1){ // read only
	std::cout << "Read 100%" << std::endl;
	for(int i=0; i<num_threads; i++){
	    for(auto& v: keys[i]){
		ops.push_back(std::make_pair(v, std::make_pair(OP_READ, 0)));
	    }
	}
    }
    else if(mode == 2){ // balanced
	std::cout << "Insert 50%, Short scan 30%, Long scan 10%, Read 10%" << std::endl;
	for(int i=0; i<num_threads; i++){
	    for(auto& v: keys[i]){
		int r = rand() % 100;
		if(r < 50)
		    ops.push_back(std::make_pair(v, std::make_pair(OP_INSERT, 0)));
		else if(r < 80){
		    int range = rand() % 5 + 5;
		    ops.push_back(std::make_pair(v, std::make_pair(OP_SCAN, range)));
		}
		else if(r < 90){
		    int range = rand() % 90 + 10;
		    ops.push_back(std::make_pair(v, std::make_pair(OP_SCAN, range)));
		}
		else 
		    ops.push_back(std::make_pair(v, std::make_pair(OP_READ, 0)));
	    }
	}
    }
    else{ 
	std::cout << "Invalid workload configuration" << std::endl;
	exit(0);
    }

    std::sort(ops.begin(), ops.end(), [](auto& a, auto& b){
	    return a.first.key < b.first.key;
	    });
    std::random_shuffle(ops.begin(), ops.end());

    auto scan = [tree, num_data, num_threads, ops](int tid){
	size_t chunk = num_data / num_threads;
	size_t start = chunk * tid;
	size_t end = chunk * (tid+1);
	if(end > num_data)
	    end = num_data;

	for(auto i=start; i<end; i++){
	    uint64_t buf[ops[i].second.second];
	    auto ret = tree->range_lookup(ops[i].first.key, ops[i].second.second, buf);
	}
    };

    auto read = [tree, num_data, num_threads, ops](int tid){
	size_t chunk = num_data / num_threads;
	size_t start = chunk * tid;
	size_t end = chunk * (tid+1);
	if(end > num_data)
	    end = num_data;

	for(auto i=start; i<end; i++){
	    auto ret = tree->lookup(ops[i].first.key);
	}
    };


    auto mix = [tree, num_data, num_threads, ops](int tid){
	size_t chunk = num_data / num_threads;
	size_t start = chunk * tid;
	size_t end = chunk * (tid+1);
	if(end > num_data)
	    end = num_data;

	int sensor_id = 0;
	std::vector<uint64_t> v;
	v.reserve(5);

	for(auto i=start; i<end; i++){
	    auto op = ops[i].second.first;
	    if(op == OP_INSERT){
		auto key = ((Rdtsc() << 16) | sensor_id++ << 6) | tid;
		if(sensor_id == 1024) sensor_id = 0;
		tree->insert(key, (uint64_t)&key);
	    }
	    else if(op == OP_SCAN){
		uint64_t buf[ops[i].second.second];
		auto ret = tree->range_lookup(ops[i].first.key, ops[i].second.second, buf);
	    }
	    else{
		auto ret = tree->lookup(ops[i].first.key);
	    }
	}
    };

    std::vector<std::thread> threads;
    if(mode == 0){
	clock_gettime(CLOCK_MONOTONIC, &start);
	for(int i=0; i<num_threads; i++)
	    threads.push_back(std::thread(scan, i));
	for(auto& t: threads) t.join();
	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
	tput = (double)num_data / (elapsed/1000000000.0) / 1000000.0;
	std::cout << "Scan: " << tput << " mops/sec" << std::endl;
    }
    else if(mode == 1){
	clock_gettime(CLOCK_MONOTONIC, &start);
	for(int i=0; i<num_threads; i++)
	    threads.push_back(std::thread(read, i));
	for(auto& t: threads) t.join();
	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
	tput = (double)num_data / (elapsed/1000000000.0) / 1000000.0;
	std::cout << "Read: " << tput << " mops/sec" << std::endl;
    }
    else{
	clock_gettime(CLOCK_MONOTONIC, &start);
	for(int i=0; i<num_threads; i++)
	    threads.push_back(std::thread(mix, i));
	for(auto& t: threads) t.join();
	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
	tput = (double)num_data / (elapsed/1000000000.0) / 1000000.0;
	std::cout << "Mix: " << tput << " mops/sec" << std::endl;
    }
    return 0;
}

