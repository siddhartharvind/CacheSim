#include <ctype.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef char byte;

enum MesiState {
    UNASSIGNED = -1,
    MODIFIED,  // 0
    EXCLUSIVE, // 1
    SHARED,    // 2
    INVALID    // 3
};

enum InstrType {
    READ   = 0,
    WRITE // 1
};

struct cache {
    byte           address; // This is the address in memory.
    byte           value;   // This is the value stored in cached memory.
    enum MesiState state;
};

struct decoded_inst {
    enum InstrType  type;
    byte            address;
    byte            value;   // Only used for WR
};

typedef struct cache cache;
typedef struct decoded_inst decoded;


/*
 * This is a very basic C cache simulator.
 * The input files for each "Core" must be named core_1.txt, core_2.txt, core_3.txt ... core_n.txt
 * Input files consist of the following instructions:
 * - RD <address>
 * - WR <address> <val>
 */

byte *memory;

// Decode instruction lines
decoded decode_inst_line(char *buffer) {
    decoded inst;
    char inst_type[2];
    sscanf(buffer, "%s", inst_type);

    if (strcmp(inst_type, "RD") == 0) {
        inst.type = READ;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;

    } else if (strcmp(inst_type, "WR") == 0) {
        inst.type = WRITE;
        int addr  = 0;
        int val   = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = val;

    }
    return inst;
}


// Helper function to print the cachelines
void print_cachelines(cache *c, int cache_size) {
    for (int i = 0; i < cache_size; ++i) {
        cache cacheline = c[i];
        printf("Address: %d, State: %d, Value: %d\n", cacheline.address, cacheline.state, cacheline.value);
    }
}


// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads) {
    // Initialize a CPU level cache that holds about 2 bytes of data.
    int cache_size = 2;
    cache *c = malloc((sizeof *c) * cache_size);

    #pragma omp parallel num_threads(num_threads)
    {
        int thread_num = omp_get_thread_num();

        // Read Input file
        char file_name[20];
        sprintf(file_name, "input_%d.txt", thread_num);

        FILE *inst_file = fopen(file_name, "r");
        char inst_line[20];

        // Decode instructions and execute them.
        while (fgets(inst_line, sizeof inst_line, inst_file)) {
            decoded inst = decode_inst_line(inst_line);
            /*
             * Cache Replacement Algorithm
             */
            int hash = inst.address % cache_size;
            cache cacheline = c[hash];

            /*
             * This is where you will implement the coherency check.
             * For now, we will simply grab the latest data from memory.
             */
            if (cacheline.address != inst.address) {
                // Flush current cacheline to memory
                memory[cacheline.address] = cacheline.value;

                // Assign new cacheline
                cacheline.address = inst.address;
                cacheline.state   = UNASSIGNED;

                // This is where it reads value of the address from memory
                cacheline.value = memory[inst.address];
                if (inst.type == WRITE) {
                    cacheline.value = inst.value;
                }

                cacheline.state = EXCLUSIVE;
                c[hash] = cacheline;
            }

            else switch (cacheline.state)
            {
                case INVALID: {
                    // Flush current cacheline to memory
                    memory[cacheline.address] = cacheline.value;

                    // Assign new cacheline
                    cacheline.address = inst.address;
                    cacheline.state = EXCLUSIVE;

                    // Read value of address from memory
                    cacheline.value = memory[inst.address];
                    if (inst.type == WRITE) {
                        cacheline.value = inst.value;
                    }

                    c[hash] = cacheline;
                    break;
                }


                case SHARED: {
                    int is_modified = 0;
                    int memory_value;

                    // Checking if any other thread has modified the cacheline
                    #pragma omp atomic read
                    memory_value = memory[inst.address];

                    if (cacheline.value != memory_value) {
                        is_modified = 1;
                    }

                    if (is_modified) {
                        // Update cacheline in local cache
                        cacheline.value = memory[inst.address];
                        cacheline.state = MODIFIED;

                    } else {
                        if (inst.type == WRITE) {
                            // Modify the cacheline
                            cacheline.value = inst.value;
                            cacheline.state = SHARED;
                        }
                    }
                    break;
                }

                case MODIFIED: {
                    if (inst.type == WRITE) {
                        // Modify the cacheline
                        cacheline.value = inst.value;
                        cacheline.state = MODIFIED;
                    }
                    break;
                }

                default: {
                    continue;
                }
            }

            switch (inst.type) {
                case READ:
                    printf("Thread %d: RD %d: %d\n", thread_num, cacheline.address, cacheline.value);
                    break;

                case WRITE:
                    printf("Thread %d: WR %d: %d\n", thread_num, cacheline.address, cacheline.value);
                    break;
            }
        }

        fclose(inst_file);
    }

    free(c);
}


int main(int argc, char *argv[]) {

    // Usage
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num-threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *endptr = NULL;
    int num_threads = (int) strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid number of threads: `%s`\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // Initialize Global memory
    // Let's assume the memory module holds about 24 bytes of data.
    int memory_size = 24;
    memory = malloc((sizeof *memory) * memory_size);
    cpu_loop(num_threads);
    free(memory);
}
