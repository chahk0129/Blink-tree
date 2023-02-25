$B^{link}$-tree
==========

This is an open-source implementation of in-memory version of $B^{link}$-tree based on 
papers "[Efficient locking for concurrent operations on B-trees](https://dl.acm.org/doi/10.1145/319628.319663)" (ACM Transactions on Database Systems'81) and
"[Cache-Conscious Concurrency Control of Main-Memory Indexes on Shared-Memory Multiprocessor Systems](https://dl.acm.org/doi/10.5555/645927.672375)" (VLDB'01).


## Optimizations ##

Since it has been designed as a disk-based index, I applied several optimizations for it to deliver good performance in a main-memory system.
1. Node size tuning: use small node sizes (e.g., 512 B or 1 KB) instead of page-access granularity.
2. In-node search: apply linear search instead of binary search to leverage hardware prefetching.
3. Synchronization mechanism: use optimistic latch coupling (OLC) instead of latch coupling.


## How to run ##

```bash
$ mkdir bin
$ make
```
`./bin/test ${num_data} ${num_threads}` runs insert and read operations.

`./bin/range ${num_data} ${num_threads}` loads an index and runs range scans. 
