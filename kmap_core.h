/**
 * @file kmap_core.h
 * @brief Minimal K-map solver core definitions
 * 
 * Ultra-compact data structures and function declarations for fast
 * Boolean minimization using bit manipulation techniques.
 * 
 * Performance targets:
 * - 2-4 vars: < 1ms
 * - 5-6 vars: < 10ms
 * - Memory: < 1MB total
 * - Binary: < 100KB
 */

#ifndef KMAP_CORE_H
#define KMAP_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* === CONSTANTS === */
#define MAX_VARIABLES 6
#define MAX_CELLS (1 << MAX_VARIABLES)  // 64 cells for 6 variables
#define MAX_GROUPS 32                   // Reasonable limit for terminal display
#define MAX_DONT_CARES 64              // Up to all cells can be don't cares
#define MAX_EXPRESSION_LEN 1024        // Max length for SOP expression

/* === COMPACT DATA STRUCTURES === */

/**
 * @brief Ultra-compact truth table representation
 * 
 * Uses bit vectors for maximum memory efficiency.
 * Total size: 9-73 bytes depending on don't care usage.
 */
typedef struct {
    uint64_t minterms;                          // Bit vector of 1s (minterms)
    uint64_t dont_cares;                        // Bit vector of don't cares  
    uint8_t num_vars;                           // Number of variables (2-6)
    uint8_t minterm_count;                      // Count of 1s (optimization)
} truth_table_t;

/**
 * @brief Prime implicant representation
 * 
 * Compact representation of grouped cells and their Boolean term.
 */
typedef struct {
    uint64_t covered_minterms;                  // Which minterms this covers
    uint8_t literal_mask;                       // Which variables are present
    uint8_t literal_values;                     // Values of present variables
    uint8_t size;                               // Number of minterms covered
} implicant_t;

/**
 * @brief Solution result structure
 */
typedef struct {
    implicant_t implicants[MAX_GROUPS];         // Prime implicants found
    uint8_t implicant_count;                    // Number of implicants
    uint8_t literal_count;                      // Total literals in expression
    uint8_t term_count;                         // Number of terms
} solution_t;

/* === CORE FUNCTION DECLARATIONS === */

/**
 * @brief Main solving function - single entry point
 * @param input Input string (binary, minterm list, etc.)
 * @param output Buffer for result expression
 * @param output_len Size of output buffer
 * @return 0 on success, negative on error
 */
int solve_kmap(const char* input, char* output, int output_len);

/**
 * @brief Parse input string into truth table structure
 * @param input Input string
 * @param tt Output truth table
 * @return 0 on success, negative on error
 */
int parse_input(const char* input, truth_table_t* tt);

/**
 * @brief Find optimal prime implicants using bit manipulation
 * @param tt Truth table
 * @param solution Output solution structure
 * @return 0 on success, negative on error
 */
int find_prime_implicants(const truth_table_t* tt, solution_t* solution);

/**
 * @brief Generate SOP expression from solution
 * @param solution Solution structure
 * @param num_vars Number of variables
 * @param output Output buffer
 * @param output_len Buffer size
 * @return 0 on success, negative on error
 */
int generate_sop_expression(const solution_t* solution, uint8_t num_vars, 
                           char* output, int output_len);

/* === UTILITY FUNCTIONS === */

/**
 * @brief Convert linear index to Gray code
 * @param linear Linear index (0 to 2^n-1)
 * @param num_vars Number of variables
 * @return Gray code value
 */
uint8_t linear_to_gray(uint8_t linear, uint8_t num_vars);

/**
 * @brief Convert Gray code to linear index
 * @param gray Gray code value
 * @param num_vars Number of variables
 * @return Linear index
 */
uint8_t gray_to_linear(uint8_t gray, uint8_t num_vars);

/**
 * @brief Check if two cells are adjacent in K-map
 * @param cell1 First cell index
 * @param cell2 Second cell index
 * @param num_vars Number of variables
 * @return true if adjacent
 */
bool are_adjacent(uint8_t cell1, uint8_t cell2, uint8_t num_vars);

/**
 * @brief Count number of set bits (population count)
 * @param value 64-bit value
 * @return Number of 1 bits
 */
static inline uint8_t popcount(uint64_t value) {
    return __builtin_popcountll(value);
}

/**
 * @brief Find first set bit (count trailing zeros)
 * @param value 64-bit value
 * @return Index of first set bit
 */
static inline uint8_t ctz(uint64_t value) {
    return __builtin_ctzll(value);
}

/* === VALIDATION FUNCTIONS === */

/**
 * @brief Validate truth table structure
 * @param tt Truth table to validate
 * @return true if valid
 */
bool validate_truth_table(const truth_table_t* tt);

/**
 * @brief Validate solution covers all minterms
 * @param tt Original truth table
 * @param solution Solution to validate
 * @return true if solution is complete and correct
 */
bool validate_solution(const truth_table_t* tt, const solution_t* solution);

/* === DEBUG/TESTING FUNCTIONS === */
#ifdef DEBUG
/**
 * @brief Print truth table in binary format
 * @param tt Truth table
 */
void debug_print_truth_table(const truth_table_t* tt);

/**
 * @brief Print solution structure
 * @param solution Solution
 * @param num_vars Number of variables
 */
void debug_print_solution(const solution_t* solution, uint8_t num_vars);
#endif

#endif /* KMAP_CORE_H */
