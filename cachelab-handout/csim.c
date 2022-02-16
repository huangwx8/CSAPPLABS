#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef unsigned long long uint64_t;

#define INSTR_BUF_LEN 255
#define MAX_SETS_IN_CACHE 64
#define MAX_LINES_PER_SET 8
#define MAX_BLOCK_SIZE 64


FILE *fp = NULL; // file pointer
char buf[INSTR_BUF_LEN]; // buffer of instructions in file
const char* tracefile = NULL;

int n_setbits = 0; // Number of set index bits (S = 2^s is the number of sets)
int n_lines = 0; // Number of lines per set
int n_blockbits = 0; // Number of block bits (B = 2^b is the block size)
int blocksize; // 2^b

char op; // operation
uint64_t addr; // address operated
int op_size; // size of memory operated

int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;

int verbose = 0;

struct CacheBlock
{
    int valid;
    uint64_t tag;
    uint64_t data[MAX_BLOCK_SIZE];
};

struct CacheBlock lru_cache[MAX_SETS_IN_CACHE * MAX_LINES_PER_SET];

void init_lru_cache() {
	for (int i = 0; i < MAX_SETS_IN_CACHE * MAX_LINES_PER_SET; i++) {
		lru_cache[i].valid = 0;
        lru_cache[i].tag = 0xaabbccdd;
	}
}

struct CacheBlock* find_cacheline(uint64_t index, uint64_t tag) {
    struct CacheBlock* p = &lru_cache[index * n_lines];
    for (int i = 0; i < n_lines; i++, p++) {
        if (p->valid && p->tag == tag) {
            return p;
        }
    }
    return NULL;
}

void insert_cacheline(uint64_t index, uint64_t tag) {
    struct CacheBlock temp;
    temp.tag = tag;
    temp.valid = 1;
    int offset = index * n_lines;

    if (lru_cache[offset + n_lines - 1].valid) {
        if (verbose) {
            printf(" eviction\n");
        }
        eviction_count++;
    }
    for (int i = offset + n_lines - 1; i > offset; i--) {
        lru_cache[i] = lru_cache[i - 1];
    }
    lru_cache[offset] = temp;
}

void adjust_cacheline(struct CacheBlock* block) {
    struct CacheBlock temp = *block;
    int index = (block - lru_cache) / n_lines;
    int offset = index * n_lines;

    // is already head, do not need to adjust
    if (&lru_cache[offset] == block) {
        return;
    }

    for (struct CacheBlock* p = block; (p - lru_cache) > offset; p--) {
        *p = *(p-1);
    }

    lru_cache[offset] = temp;
}

void query(uint64_t index, uint64_t tag) {
	// load or store
	// hit: find node 'tag' in lru_cache, hit++
	// miss: can't find, create a new node, put it at head. 
	// evict: if size > n_lines, delete tail
    if (verbose)
        printf("[%c] index = %llx, tag = %llx\n", op, index, tag);
    struct CacheBlock* p = find_cacheline(index, tag);
    if (p) {
        if (verbose)
            printf(" hit\n");
        hit_count++;
        adjust_cacheline(p);
    }
    else {
        if (verbose)
            printf(" miss\n");
        miss_count++;
        insert_cacheline(index, tag);
    }
}

void modify_query(uint64_t index, uint64_t tag) {
	// load and store
	query(index, tag);
	query(index, tag);
}

void parse_args(int argc, char * argv[])
{
    int ch;
    while ((ch = getopt(argc, argv, "s:E:b:t:v")) != -1)
    {
        switch (ch) 
        {
            case 'v':
                verbose = 1;
                break;
            case 's':
                n_setbits = atoi(optarg);
                break;
            case 'E':
                n_lines = atoi(optarg);
                break;
            case 'b':
	            n_blockbits = atoi(optarg);
	            blocksize = 1 << n_blockbits;
                break;
            case 't':
                tracefile = optarg;
                break;
            case '?':
                goto parse_error;
        }
    }

    if (n_setbits == 0 || n_lines == 0 || n_blockbits == 0) {
        goto parse_error;
    }

    return;

parse_error:
    printf("Usage: ./csim [-v] -s <num> -E <num> -b <num> -t <file>");
    exit(1);
}

void parseline(char line[INSTR_BUF_LEN]) {
	if (line[0] != ' ') {
		op = 'I';
		return;
	}
	sscanf(line, " %c %llx,%d", &op, &addr, &op_size);
}

uint64_t get_index() {
	// get cache index of variable 'addr'
	uint64_t mask = 0;
	mask = ~mask;
	mask = ~(mask << n_setbits);
	return mask & (addr >> n_blockbits);
}

uint64_t get_tag() {
	// get cache tag of variable 'addr'
	return addr >> n_setbits >> n_blockbits;
}

int main(int argc, char** argv)
{
	parse_args(argc, argv);
	init_lru_cache();
	fp = fopen(tracefile, "r");
	if (!fp) {
		printf("File %s isn\'t existing!",tracefile);
        exit(1);
	}
	// read lines
	while (fgets(buf, INSTR_BUF_LEN - 1, fp)) {
		parseline(buf);
		if (op == 'I') continue;
		uint64_t index = get_index();
		uint64_t tag = get_tag();
		if (verbose)
			printf("%c %llx,%d\n", op, addr, op_size);
		if (op == 'L' || op == 'S')
			query(index, tag);
		else if (op == 'M')
			modify_query(index, tag);
		if (verbose)
			printf("\n");
	}
	printSummary(hit_count, miss_count, eviction_count);
	fclose(fp);
}
