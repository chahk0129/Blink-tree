#include "tree.h"

#include <ctime>
#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include <algorithm>

using Key_t = uint64_t;

inline uint64_t _Rdtsc(){
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return (((uint64_t)hi << 32 ) | lo);
}

using namespace BLINK_TREE;

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

    std::vector<std::thread> inserting_threads;
    std::vector<std::thread> searching_threads;
    std::vector<std::thread> mixed_threads;

    std::vector<uint64_t> notfound_keys[num_threads];

    btree_t<Key_t>* tree = new btree_t<Key_t>();
    std::cout << "inode_size(" << inode_t<Key_t>::cardinality << "), lnode_size(" << lnode_t<Key_t>::cardinality << ")" << std::endl;

    struct timespec start, end;

    std::atomic<uint64_t> insert_time = 0;
    std::atomic<uint64_t> search_time = 0;

    size_t chunk = num_data / num_threads;
    auto insert = [&tree, &keys, &insert_time, num_data, num_threads](int tid){
	size_t chunk = num_data / num_threads;
	int from = chunk * tid;
	int to = chunk * (tid + 1);
	for(int i=from; i<to; i++){
	    tree->insert(keys[i], (uint64_t)&keys[i]);
	}
    };
    auto search = [&tree, &keys, &notfound_keys, num_data, num_threads](int tid){
	size_t chunk = num_data / num_threads;
	int from = chunk * tid;
	int to = chunk * (tid + 1);
	for(int i=from; i<to; i++){
	    auto ret = tree->lookup(keys[i]);
	    if(ret != (uint64_t)&keys[i]){
		notfound_keys[tid].push_back(i);
	    }
	}
    };

    std::vector<std::thread> insert_threads;
    std::cout << "Insertion starts" << std::endl;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<num_threads; i++)
	insert_threads.push_back(std::thread(insert, i));
    for(auto& t: insert_threads)
	t.join();
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    std::cout << "elapsed time: " << elapsed/1000000000.0 << " sec" << std::endl;
    std::cout << "throughput: " << num_data / (double)(elapsed/1000000000.0) / 1000000 << " mops/sec" << std::endl;


    std::vector<std::thread> search_threads;
    std::cout << "Search starts" << std::endl;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<num_threads; i++)
	search_threads.push_back(std::thread(search, i));
    for(auto& t: search_threads)
	t.join();
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    std::cout << "elapsed time: " << elapsed/1000000000.0 << " sec" << std::endl;
    std::cout << "throughput: " << num_data / (double)(elapsed/1000000000.0) / 1000000 << " mops/sec" << std::endl;

    bool not_found = false;
    for(int i=0; i<num_threads; i++){
	for(auto& it: notfound_keys[i]){
	    auto ret = tree->lookup(keys[it]);
	    if(ret != (uint64_t)&keys[it]){
		std::cout << "key " << keys[it] << " not found" << std::endl;
		not_found = true;
	    }
	}
    }

    if(not_found){
	std::cout << "finding it anyway\n\n" << std::endl;
	for(int i=0; i<num_threads; i++){
	    for(auto& it: notfound_keys[i]){
		auto ret = tree->find_anyway(keys[it]);
		if(ret != (uint64_t)&keys[it])
		    std::cout << "key " << keys[it] << " not found" << std::endl;
		else{
		    // lower key
		    std::cout << "lower key find_anyway" << std::endl;
		    ret = tree->find_anyway(keys[it]-1);
		    if(ret != (uint64_t)&keys[it]-1)
			std::cout << "key " << keys[it]-1 << " not found" << std::endl;
		}
	    }
	}
    }

    tree->sanity_check();
    auto height = tree->height();
    std::cout << "Height of tree: " << height+1 << std::endl;
    auto util = tree->utilization();
    std::cout << "Utilization of leaf nodes: " << util*100 << " \%" << std::endl;
//    tree->print_internal();
//    tree->print_leaf();
    return 0;
}
