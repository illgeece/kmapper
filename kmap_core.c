/**
 * @file kmap_core.c
 * @brief Minimal K-map solver implementation
 * 
 * High-performance Boolean minimization using bit manipulation.
 * Optimized for speed and minimal memory usage.
 */

#include "kmap_core.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* Forward declarations for static functions */
static int parse_binary_string(const char* input, truth_table_t* tt);
static int parse_minterm_list(const char* input, truth_table_t* tt);
static int find_groups_with_dont_cares(uint64_t minterms, uint64_t dont_cares, 
                                      uint8_t num_vars, implicant_t* groups, 
                                      uint8_t* group_count);
static bool is_valid_4cell_group(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t num_vars);
static void remove_redundant_implicants(solution_t* solution);

/* === GRAY CODE LOOKUP TABLES === */
/* Pre-computed for maximum speed */

static const uint8_t gray_2var[4] = {0, 1, 3, 2};
static const uint8_t gray_3var[8] = {0, 1, 3, 2, 6, 7, 5, 4};
static const uint8_t gray_4var[16] = {
    0, 1, 3, 2, 6, 7, 5, 4, 12, 13, 15, 14, 10, 11, 9, 8
};

/* Reverse lookup tables for Gray to linear conversion */
static const uint8_t linear_2var[4] = {0, 1, 3, 2};
static const uint8_t linear_3var[8] = {0, 1, 3, 2, 7, 6, 4, 5};
static const uint8_t linear_4var[16] = {
    0, 1, 3, 2, 7, 6, 4, 5, 15, 14, 12, 13, 8, 9, 11, 10
};

/* === UTILITY FUNCTION IMPLEMENTATIONS === */

uint8_t linear_to_gray(uint8_t linear, uint8_t num_vars) {
    if (linear >= (1 << num_vars)) return 0;
    
    switch (num_vars) {
        case 2: return gray_2var[linear];
        case 3: return gray_3var[linear];
        case 4: return gray_4var[linear];
        case 5: 
        case 6:
            /* For 5-6 variables, use algorithmic conversion */
            return linear ^ (linear >> 1);
        default:
            return 0;
    }
}

uint8_t gray_to_linear(uint8_t gray, uint8_t num_vars) {
    if (gray >= (1 << num_vars)) return 0;
    
    switch (num_vars) {
        case 2: return linear_2var[gray];
        case 3: return linear_3var[gray];
        case 4: return linear_4var[gray];
        case 5:
        case 6: {
            /* Algorithmic conversion for larger cases */
            uint8_t result = gray;
            for (uint8_t i = 1; i < num_vars; i++) {
                result ^= (gray >> i);
            }
            return result;
        }
        default:
            return 0;
    }
}

bool are_adjacent(uint8_t cell1, uint8_t cell2, uint8_t num_vars) {
    if (cell1 >= (1 << num_vars) || cell2 >= (1 << num_vars)) {
        return false;
    }
    
    /* Two cells are adjacent if they differ by exactly one bit */
    uint8_t diff = cell1 ^ cell2;
    return popcount(diff) == 1;
}

/* === VALIDATION FUNCTIONS === */

bool validate_truth_table(const truth_table_t* tt) {
    if (!tt) return false;
    if (tt->num_vars < 2 || tt->num_vars > MAX_VARIABLES) return false;
    
    uint64_t max_mask = (1ULL << (1 << tt->num_vars)) - 1;
    
    /* Check that minterms and don't cares don't overlap */
    if (tt->minterms & tt->dont_cares) return false;
    
    /* Check that values don't exceed possible range */
    if ((tt->minterms | tt->dont_cares) & ~max_mask) return false;
    
    /* Validate minterm count */
    if (tt->minterm_count != popcount(tt->minterms)) return false;
    
    return true;
}

bool validate_solution(const truth_table_t* tt, const solution_t* solution) {
    if (!tt || !solution) return false;
    if (!validate_truth_table(tt)) return false;
    
    uint64_t covered = 0;
    
    /* Check that all implicants together cover exactly the minterms */
    for (uint8_t i = 0; i < solution->implicant_count; i++) {
        covered |= solution->implicants[i].covered_minterms;
    }
    
    /* Must cover all minterms, no more, no less */
    return covered == tt->minterms;
}

/* === INPUT PARSING FUNCTIONS === */

/**
 * @brief Detect input format and delegate to appropriate parser
 */
int parse_input(const char* input, truth_table_t* tt) {
    if (!input || !tt) return -1;
    
    /* Initialize truth table */
    memset(tt, 0, sizeof(truth_table_t));
    
    /* Skip whitespace */
    while (isspace(*input)) input++;
    if (*input == '\0') return -1;
    
    /* Detect format */
    if (strchr(input, ',')) {
        /* Minterm list format: "0,1,3,5" */
        return parse_minterm_list(input, tt);
    } else if (strspn(input, "01Xx-") == strlen(input)) {
        /* Binary string format: "10110" */
        return parse_binary_string(input, tt);
    } else {
        /* Unknown format */
        return -1;
    }
}

/**
 * @brief Parse binary string format
 */
static int parse_binary_string(const char* input, truth_table_t* tt) {
    size_t len = strlen(input);
    
    /* Determine number of variables from string length */
    uint8_t num_vars = 0;
    while ((1U << num_vars) < len) num_vars++;
    
    if ((1U << num_vars) != len || num_vars < 2 || num_vars > MAX_VARIABLES) {
        return -1;
    }
    
    tt->num_vars = num_vars;
    tt->minterms = 0;
    tt->dont_cares = 0;
    tt->minterm_count = 0;
    
    /* Parse each character - reverse bit order for standard representation */
    for (size_t i = 0; i < len; i++) {
        uint64_t bit = 1ULL << (len - 1 - i);  /* Reverse bit order */
        
        switch (input[i]) {
            case '1':
                tt->minterms |= bit;
                tt->minterm_count++;
                break;
            case '0':
                /* Already 0, do nothing */
                break;
            case 'X':
            case 'x':
            case '-':
                tt->dont_cares |= bit;
                break;
            default:
                return -1; /* Invalid character */
        }
    }
    
    return 0;
}

/**
 * @brief Parse minterm list format
 */
static int parse_minterm_list(const char* input, truth_table_t* tt) {
    char* input_copy = strdup(input);
    if (!input_copy) return -1;
    
    tt->minterms = 0;
    tt->dont_cares = 0;
    tt->minterm_count = 0;
    
    uint8_t max_minterm = 0;
    char* token = strtok(input_copy, ",");
    
    while (token) {
        /* Skip whitespace */
        while (isspace(*token)) token++;
        
        char* endptr;
        long value = strtol(token, &endptr, 10);
        
        if (*endptr != '\0' || value < 0 || value >= MAX_CELLS) {
            free(input_copy);
            return -1;
        }
        
        uint8_t minterm = (uint8_t)value;
        if (minterm > max_minterm) max_minterm = minterm;
        
        tt->minterms |= (1ULL << minterm);
        tt->minterm_count++;
        
        token = strtok(NULL, ",");
    }
    
    free(input_copy);
    
    /* Determine number of variables from highest minterm */
    tt->num_vars = 2;
    while ((1U << tt->num_vars) <= max_minterm) tt->num_vars++;
    
    if (tt->num_vars > MAX_VARIABLES) return -1;
    
    return 0;
}

/* === MAIN SOLVING FUNCTION === */

int solve_kmap(const char* input, char* output, int output_len) {
    if (!input || !output || output_len <= 0) return -1;
    
    truth_table_t tt;
    solution_t solution;
    
    /* Parse input */
    int result = parse_input(input, &tt);
    if (result != 0) return result;
    
    /* Validate parsed truth table */
    if (!validate_truth_table(&tt)) return -2;
    
    /* Handle trivial cases */
    if (tt.minterm_count == 0) {
        strncpy(output, "0", output_len - 1);
        output[output_len - 1] = '\0';
        return 0;
    }
    
    if (tt.minterms == ((1ULL << (1 << tt.num_vars)) - 1)) {
        strncpy(output, "1", output_len - 1);
        output[output_len - 1] = '\0';
        return 0;
    }
    
    /* Find prime implicants */
    result = find_prime_implicants(&tt, &solution);
    if (result != 0) return result;
    
    /* Validate solution */
    if (!validate_solution(&tt, &solution)) return -3;
    
    /* Generate SOP expression */
    result = generate_sop_expression(&solution, tt.num_vars, output, output_len);
    if (result != 0) return result;
    
    return 0;
}

/* === DEBUG FUNCTIONS === */
#ifdef DEBUG
void debug_print_truth_table(const truth_table_t* tt) {
    if (!tt) return;
    
    printf("Truth Table (%d vars):\n", tt->num_vars);
    printf("Minterms:   ");
    for (int i = (1 << tt->num_vars) - 1; i >= 0; i--) {
        printf("%c", (tt->minterms & (1ULL << i)) ? '1' : '0');
    }
    printf("\n");
    
    printf("Don't cares:");
    for (int i = (1 << tt->num_vars) - 1; i >= 0; i--) {
        printf("%c", (tt->dont_cares & (1ULL << i)) ? 'X' : '0');
    }
    printf("\n");
    
    printf("Count: %d minterms\n", tt->minterm_count);
}

void debug_print_solution(const solution_t* solution, uint8_t num_vars) {
    if (!solution) return;
    
    printf("Solution (%d implicants):\n", solution->implicant_count);
    
    for (uint8_t i = 0; i < solution->implicant_count; i++) {
        const implicant_t* imp = &solution->implicants[i];
        
        printf("  Implicant %d: covers %d minterms, mask=0x%02X, values=0x%02X\n",
               i, imp->size, imp->literal_mask, imp->literal_values);
        
        printf("    Minterms: ");
        for (uint8_t j = 0; j < (1 << num_vars); j++) {
            if (imp->covered_minterms & (1ULL << j)) {
                printf("%d ", j);
            }
        }
        printf("\n");
    }
    
    printf("Total: %d terms, %d literals\n", 
           solution->term_count, solution->literal_count);
}
#endif

/* === CORE GROUPING ALGORITHM === */

/**
 * @brief Correct don't care aware grouping algorithm
 * 
 * This algorithm properly implements don't care optimization:
 * 1. Only covers required minterms (not don't cares)
 * 2. Uses don't cares to form larger, more optimal groups
 * 3. Each don't care can be treated as 0 or 1 to maximize benefit
 */
static int find_groups_with_dont_cares(uint64_t minterms, uint64_t dont_cares, 
                                      uint8_t num_vars, implicant_t* groups, 
                                      uint8_t* group_count) {
    uint64_t remaining_minterms = minterms;  /* Only these need to be covered */
    uint64_t available_for_grouping = minterms | dont_cares;  /* These can be used in groups */
    uint64_t total_cells = (1ULL << num_vars);
    
    *group_count = 0;
    
    /* First pass: find pairs that include at least one required minterm */
    for (uint8_t cell1 = 0; cell1 < total_cells && *group_count < MAX_GROUPS; cell1++) {
        if (!(available_for_grouping & (1ULL << cell1))) continue;
        if (!(remaining_minterms & (1ULL << cell1))) continue; /* Must start with a required minterm */
        
        /* Look for adjacent cells to form pairs */
        for (uint8_t cell2 = cell1 + 1; cell2 < total_cells; cell2++) {
            if (!(available_for_grouping & (1ULL << cell2))) continue;
            
            /* Check if they're adjacent (differ by exactly 1 bit) */
            if (are_adjacent(cell1, cell2, num_vars)) {
                /* Found a pair - create implicant */
                uint8_t diff = cell1 ^ cell2;
                uint64_t group_mask = (1ULL << cell1) | (1ULL << cell2);
                
                groups[*group_count].covered_minterms = group_mask & minterms; /* Only count actual minterms */
                groups[*group_count].literal_mask = ((1 << num_vars) - 1) & ~diff;
                groups[*group_count].literal_values = cell1 & groups[*group_count].literal_mask;
                groups[*group_count].size = popcount(groups[*group_count].covered_minterms);
                
                /* Only add if this group covers at least one required minterm */
                if (groups[*group_count].covered_minterms != 0) {
                    (*group_count)++;
                    
                    /* Remove covered minterms from remaining */
                    remaining_minterms &= ~groups[*group_count - 1].covered_minterms;
                    
                    /* Remove used cells from available to prevent reuse */
                    available_for_grouping &= ~group_mask;
                    break; /* Move to next cell1 */
                }
            }
        }
    }
    
    /* Try to find larger groups (4-cell groups) for remaining minterms */
    for (uint8_t cell1 = 0; cell1 < total_cells && *group_count < MAX_GROUPS && remaining_minterms; cell1++) {
        if (!(remaining_minterms & (1ULL << cell1))) continue;
        
        /* Look for groups of 4 adjacent cells */
        for (uint8_t cell2 = cell1 + 1; cell2 < total_cells; cell2++) {
            if (!are_adjacent(cell1, cell2, num_vars)) continue;
            if (!(available_for_grouping & (1ULL << cell2))) continue;
            
            for (uint8_t cell3 = cell2 + 1; cell3 < total_cells; cell3++) {
                if (!are_adjacent(cell1, cell3, num_vars) && !are_adjacent(cell2, cell3, num_vars)) continue;
                if (!(available_for_grouping & (1ULL << cell3))) continue;
                
                for (uint8_t cell4 = cell3 + 1; cell4 < total_cells; cell4++) {
                    if (!(available_for_grouping & (1ULL << cell4))) continue;
                    
                    /* Check if all 4 cells form a valid rectangle */
                    uint64_t group_mask = (1ULL << cell1) | (1ULL << cell2) | (1ULL << cell3) | (1ULL << cell4);
                    uint8_t diff_bits = cell1 ^ cell2 ^ cell3 ^ cell4;
                    
                    /* For a valid 4-cell group, exactly 2 variables should differ */
                    if (popcount(diff_bits) == 2 && is_valid_4cell_group(cell1, cell2, cell3, cell4, num_vars)) {
                        uint64_t covered_minterms = group_mask & minterms;
                        
                        if (covered_minterms != 0) {
                            groups[*group_count].covered_minterms = covered_minterms;
                            groups[*group_count].literal_mask = ((1 << num_vars) - 1) & ~diff_bits;
                            groups[*group_count].literal_values = cell1 & groups[*group_count].literal_mask;
                            groups[*group_count].size = popcount(covered_minterms);
                            
                            (*group_count)++;
                            remaining_minterms &= ~covered_minterms;
                            available_for_grouping &= ~group_mask;
                            goto next_cell1; /* Found a 4-cell group, move on */
                        }
                    }
                }
            }
        }
        next_cell1:;
    }
    
    /* Final pass: individual minterms that couldn't be grouped */
    for (uint8_t cell = 0; cell < total_cells && *group_count < MAX_GROUPS; cell++) {
        if (remaining_minterms & (1ULL << cell)) {
            /* Single cell implicant */
            groups[*group_count].covered_minterms = (1ULL << cell);
            groups[*group_count].literal_mask = (1 << num_vars) - 1;
            groups[*group_count].literal_values = cell;
            groups[*group_count].size = 1;
            
            (*group_count)++;
        }
    }
    
    return *group_count;
}

/**
 * @brief Check if 4 cells form a valid rectangular group
 */
static bool is_valid_4cell_group(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t num_vars) {
    (void)num_vars; /* Unused for now */
    
    /* Simple validation: all pairs should have consistent bit differences */
    uint8_t diff1 = c1 ^ c2;
    uint8_t diff2 = c1 ^ c3; 
    uint8_t diff3 = c1 ^ c4;
    
    /* For a valid rectangle, the differences should be consistent */
    return (diff1 != 0) && (diff2 != 0) && (diff3 != 0) && 
           ((diff1 & diff2) == 0 || (diff1 & diff3) == 0 || (diff2 & diff3) == 0);
}


/**
 * @brief Remove redundant implicants (those covered by larger ones)
 */
static void remove_redundant_implicants(solution_t* solution) {
    for (int i = 0; i < solution->implicant_count; i++) {
        if (solution->implicants[i].size == 0) continue; /* Already removed */
        
        for (int j = 0; j < solution->implicant_count; j++) {
            if (i == j || solution->implicants[j].size == 0) continue;
            
            /* Check if implicant i is covered by implicant j */
            uint64_t i_minterms = solution->implicants[i].covered_minterms;
            uint64_t j_minterms = solution->implicants[j].covered_minterms;
            
            if ((i_minterms & j_minterms) == i_minterms && 
                solution->implicants[j].size > solution->implicants[i].size) {
                /* Implicant i is redundant - mark for removal */
                solution->implicants[i].size = 0;
                break;
            }
        }
    }
    
    /* Compact the array by removing marked implicants */
    int write_pos = 0;
    for (int read_pos = 0; read_pos < solution->implicant_count; read_pos++) {
        if (solution->implicants[read_pos].size > 0) {
            if (write_pos != read_pos) {
                solution->implicants[write_pos] = solution->implicants[read_pos];
            }
            write_pos++;
        }
    }
    solution->implicant_count = write_pos;
}

/**
 * @brief Main prime implicant finding function
 */
int find_prime_implicants(const truth_table_t* tt, solution_t* solution) {
    if (!tt || !solution) return -1;
    if (!validate_truth_table(tt)) return -2;
    
    /* Initialize solution */
    memset(solution, 0, sizeof(solution_t));
    
    /* Handle trivial cases */
    if (tt->minterm_count == 0) {
        return 0; /* No implicants needed */
    }
    
    if (tt->minterm_count == 1) {
        /* Single minterm - create single-cell implicant */
        uint8_t minterm_pos = ctz(tt->minterms);
        solution->implicants[0].covered_minterms = 1ULL << minterm_pos;
        solution->implicants[0].literal_mask = (1 << tt->num_vars) - 1;
        solution->implicants[0].literal_values = minterm_pos;
        solution->implicants[0].size = 1;
        solution->implicant_count = 1;
        return 0;
    }
    
    /* Use correct don't care aware grouping algorithm */
    find_groups_with_dont_cares(tt->minterms, tt->dont_cares, tt->num_vars, 
                                solution->implicants, &solution->implicant_count);
    
    /* Remove redundant implicants */
    remove_redundant_implicants(solution);
    
    /* Calculate solution statistics */
    solution->term_count = solution->implicant_count;
    solution->literal_count = 0;
    for (int i = 0; i < solution->implicant_count; i++) {
        solution->literal_count += popcount(solution->implicants[i].literal_mask);
    }
    
    return 0;
}

/**
 * @brief Generate SOP (Sum of Products) expression from solution
 */
int generate_sop_expression(const solution_t* solution, uint8_t num_vars,
                           char* output, int output_len) {
    if (!solution || !output || output_len <= 0) return -1;
    if (num_vars > 8) return -2; /* Safety check */
    
    /* Variable names */
    const char var_names[] = "ABCDEFGH";
    
    output[0] = '\0'; /* Start with empty string */
    
    /* Handle empty solution */
    if (solution->implicant_count == 0) {
        strncpy(output, "0", output_len - 1);
        output[output_len - 1] = '\0';
        return 0;
    }
    
    /* Generate each term */
    for (int i = 0; i < solution->implicant_count; i++) {
        const implicant_t* imp = &solution->implicants[i];
        
        /* Add OR operator between terms (except for first term) */
        if (i > 0) {
            if ((int)strlen(output) + 3 >= output_len) return -3; /* Buffer too small */
            strcat(output, " + ");
        }
        
        /* Generate the product term */
        bool first_literal = true;
        
        for (uint8_t var = 0; var < num_vars; var++) {
            uint8_t var_bit = 1 << var;
            
            /* Check if this variable is included in the implicant */
            if (imp->literal_mask & var_bit) {
                /* Add AND operator between literals (except for first) */
                if (!first_literal) {
                    if ((int)strlen(output) + 1 >= output_len) return -3;
                    strcat(output, "&");
                }
                
                /* Add variable (complemented if value is 0) */
                if (!(imp->literal_values & var_bit)) {
                    /* Complemented variable */
                    if ((int)strlen(output) + 2 >= output_len) return -3;
                    strcat(output, "~");
                }
                
                /* Variable name */
                if ((int)strlen(output) + 2 >= output_len) return -3;
                char var_str[2] = {var_names[var], '\0'};
                strcat(output, var_str);
                
                first_literal = false;
            }
        }
        
        /* Handle the case where all variables are eliminated (constant 1) */
        if (first_literal) {
            if ((int)strlen(output) + 2 >= output_len) return -3;
            strcat(output, "1");
        }
    }
    
    /* Handle the case where we have an empty expression */
    if (strlen(output) == 0) {
        strncpy(output, "1", output_len - 1);
        output[output_len - 1] = '\0';
    }
    
    return 0;
}

