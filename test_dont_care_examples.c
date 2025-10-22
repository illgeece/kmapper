#include "src/kmap_core.h"
#include <stdio.h>
#include <string.h>

/**
 * Test don't care implementation against established examples
 */

void test_example_1() {
    printf("\n=== Test Example 1: Classic Don't Care Case ===\n");
    printf("Input: minterms = {1, 2, 5}, don't cares = {0, 4, 6}\n");
    printf("Expected: Should use don't cares to create optimal groups\n");
    
    // Create truth table manually for this example
    truth_table_t tt;
    tt.num_vars = 3;
    tt.minterms = (1ULL << 1) | (1ULL << 2) | (1ULL << 5);  // m(1,2,5)
    tt.dont_cares = (1ULL << 0) | (1ULL << 4) | (1ULL << 6); // d(0,4,6)
    tt.minterm_count = 3;
    
    printf("Truth table: ");
    for (int i = 7; i >= 0; i--) {
        if (tt.minterms & (1ULL << i)) printf("1");
        else if (tt.dont_cares & (1ULL << i)) printf("X");
        else printf("0");
    }
    printf("\n");
    
    solution_t solution;
    int result = find_prime_implicants(&tt, &solution);
    
    printf("Solve result: %d\n", result);
    printf("Number of implicants: %d\n", solution.implicant_count);
    
    char output[512];
    if (generate_sop_expression(&solution, tt.num_vars, output, sizeof(output)) == 0) {
        printf("SOP Expression: %s\n", output);
    }
    
    // Verify solution covers all required minterms
    uint64_t covered = 0;
    for (int i = 0; i < solution.implicant_count; i++) {
        covered |= solution.implicants[i].covered_minterms;
    }
    
    bool valid = (covered == tt.minterms);
    printf("Solution covers all minterms: %s\n", valid ? "YES" : "NO");
    
    if (!valid) {
        printf("ERROR: Missing minterms: ");
        uint64_t missing = tt.minterms & ~covered;
        for (int i = 0; i < 8; i++) {
            if (missing & (1ULL << i)) printf("%d ", i);
        }
        printf("\n");
    }
}

void test_example_2() {
    printf("\n=== Test Example 2: Input '1X1X' ===\n");
    printf("Input: 1X1X (minterms at 0,2 with don't cares at 1,3)\n");
    printf("Expected: Should optimize to a single variable\n");
    
    truth_table_t tt;
    int result = parse_input("1X1X", &tt);
    
    printf("Parse result: %d\n", result);
    if (result == 0) {
        printf("Variables: %d\n", tt.num_vars);
        printf("Minterms: 0b");
        for (int i = 3; i >= 0; i--) {
            printf("%d", (tt.minterms >> i) & 1);
        }
        printf(" (positions: ");
        for (int i = 0; i < 4; i++) {
            if (tt.minterms & (1ULL << i)) printf("%d ", i);
        }
        printf(")\n");
        
        printf("Don't cares: 0b");
        for (int i = 3; i >= 0; i--) {
            printf("%d", (tt.dont_cares >> i) & 1);
        }
        printf(" (positions: ");
        for (int i = 0; i < 4; i++) {
            if (tt.dont_cares & (1ULL << i)) printf("%d ", i);
        }
        printf(")\n");
        
        solution_t solution;
        int solve_result = find_prime_implicants(&tt, &solution);
        printf("Solve result: %d\n", solve_result);
        printf("Number of implicants: %d\n", solution.implicant_count);
        
        char output[512];
        if (generate_sop_expression(&solution, tt.num_vars, output, sizeof(output)) == 0) {
            printf("SOP Expression: %s\n", output);
        }
        
        // Verify coverage
        uint64_t covered = 0;
        for (int i = 0; i < solution.implicant_count; i++) {
            covered |= solution.implicants[i].covered_minterms;
        }
        
        bool valid = (covered == tt.minterms);
        printf("Solution covers all minterms: %s\n", valid ? "YES" : "NO");
    }
}

void test_example_3() {
    printf("\n=== Test Example 3: All Don't Cares ===\n");
    printf("Input: XXXX (all don't cares)\n");
    printf("Expected: Should result in constant 1 or minimal expression\n");
    
    truth_table_t tt;
    tt.num_vars = 2;
    tt.minterms = 0;  // No required minterms
    tt.dont_cares = 0b1111;  // All don't cares
    tt.minterm_count = 0;
    
    solution_t solution;
    int result = find_prime_implicants(&tt, &solution);
    
    printf("Solve result: %d\n", result);
    printf("Number of implicants: %d\n", solution.implicant_count);
    
    char output[512];
    if (generate_sop_expression(&solution, tt.num_vars, output, sizeof(output)) == 0) {
        printf("SOP Expression: %s\n", output);
    }
}

void test_example_4() {
    printf("\n=== Test Example 4: No Don't Cares ===\n");
    printf("Input: 1010 (no don't cares)\n");
    printf("Expected: Should work normally, result = A\n");
    
    truth_table_t tt;
    int result = parse_input("1010", &tt);
    
    if (result == 0) {
        solution_t solution;
        find_prime_implicants(&tt, &solution);
        
        char output[512];
        if (generate_sop_expression(&solution, tt.num_vars, output, sizeof(output)) == 0) {
            printf("SOP Expression: %s\n", output);
            printf("Expected: A, Got: %s, Match: %s\n", output, 
                   strcmp(output, "A") == 0 ? "YES" : "NO");
        }
    }
}

int main() {
    printf("Testing Don't Care Logic Implementation\n");
    printf("=======================================\n");
    
    test_example_1();
    test_example_2(); 
    test_example_3();
    test_example_4();
    
    printf("\n=== Don't Care Logic Test Complete ===\n");
    return 0;
}
