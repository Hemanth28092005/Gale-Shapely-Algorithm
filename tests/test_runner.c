#include "../src/matching.h"
#include "../src/verifier.h"
#include "../src/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (Line %d): %s\n", __func__, __LINE__, msg); \
        return false; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    total_tests++; \
    printf("Running %-45s ... ", #fn); \
    if (fn()) { \
        passed_tests++; \
        printf("[PASS]\n"); \
    } else { \
        printf("[FAILED]\n"); \
    } \
} while(0)

/* Test 1: Hand-built 3R x 2H verified against analytical trace */
static bool test_handbuilt_trace(void) {
    Instance *inst = instance_create(3, 2);
    TEST_ASSERT(inst != NULL, "Instance creation failed");

    inst->residents[0].name = (char *)"R0";
    inst->residents[0].pref_count = 2;
    inst->residents[0].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[0].pref_list[0] = 0; inst->residents[0].pref_list[1] = 1;

    inst->residents[1].name = (char *)"R1";
    inst->residents[1].pref_count = 2;
    inst->residents[1].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[1].pref_list[0] = 0; inst->residents[1].pref_list[1] = 1;

    inst->residents[2].name = (char *)"R2";
    inst->residents[2].pref_count = 2;
    inst->residents[2].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[2].pref_list[0] = 1; inst->residents[2].pref_list[1] = 0;

    inst->hospitals[0].name = (char *)"H0";
    inst->hospitals[0].capacity = 1;
    inst->hospitals[0].pref_count = 3;
    inst->hospitals[0].pref_list = (int *)malloc(3 * sizeof(int));
    inst->hospitals[0].pref_list[0] = 0; inst->hospitals[0].pref_list[1] = 1; inst->hospitals[0].pref_list[2] = 2;

    inst->hospitals[1].name = (char *)"H1";
    inst->hospitals[1].capacity = 1;
    inst->hospitals[1].pref_count = 3;
    inst->hospitals[1].pref_list = (int *)malloc(3 * sizeof(int));
    inst->hospitals[1].pref_list[0] = 1; inst->hospitals[1].pref_list[1] = 0; inst->hospitals[1].pref_list[2] = 2;

    TEST_ASSERT(instance_build_ranks(inst), "Building ranks failed");

    MatchResult res = run_matching(inst, NULL, NULL);

    /* Verify exact hand-trace outcomes */
    TEST_ASSERT(inst->residents[0].current_match == 0, "R0 should match to H0");
    TEST_ASSERT(inst->residents[1].current_match == 1, "R1 should match to H1");
    TEST_ASSERT(inst->residents[2].current_match == UNMATCHED, "R2 should be UNMATCHED");

    TEST_ASSERT(inst->hospitals[0].assigned_count == 1, "H0 should have 1 resident");
    TEST_ASSERT(inst->hospitals[0].assigned_residents[0] == 0, "H0 should hold R0");

    TEST_ASSERT(inst->hospitals[1].assigned_count == 1, "H1 should have 1 resident");
    TEST_ASSERT(inst->hospitals[1].assigned_residents[0] == 1, "H1 should hold R1");

    TEST_ASSERT(res.num_proposals == 5, "Proposal count mismatch");
    TEST_ASSERT(res.num_bumps == 2, "Bump count mismatch");
    TEST_ASSERT(res.num_rejections == 1, "Rejection count mismatch");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable == true, "Matching should be stable");
    TEST_ASSERT(vres.blocking_pair_count == 0, "No blocking pairs allowed");
    verification_result_free(&vres);

    /* Prevent freeing static string literals */
    inst->residents[0].name = NULL;
    inst->residents[1].name = NULL;
    inst->residents[2].name = NULL;
    inst->hospitals[0].name = NULL;
    inst->hospitals[1].name = NULL;
    instance_free(inst);
    return true;
}

/* Test 2: Hospital with 0 vacancies from start */
static bool test_edge_zero_capacity(void) {
    Instance *inst = parse_instance_file("tests/fixtures/02_zero_capacity.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 02_zero_capacity.txt");

    run_matching(inst, NULL, NULL);

    /* H0 has capacity 0, so no resident should ever be assigned to H0 */
    TEST_ASSERT(inst->hospitals[0].capacity == 0, "H0 capacity must be 0");
    TEST_ASSERT(inst->hospitals[0].assigned_count == 0, "H0 assigned count must be 0");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable, "Zero-capacity instance should be stable");
    TEST_ASSERT(vres.capacity_constraints_passed, "Capacity constraints must pass");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 3: Resident exhausts entire preference list unmatched */
static bool test_edge_exhausted_unmatched(void) {
    Instance *inst = parse_instance_file("tests/fixtures/03_exhausted_unmatched.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 03_exhausted_unmatched.txt");

    run_matching(inst, NULL, NULL);

    TEST_ASSERT(inst->residents[2].current_match == UNMATCHED, "R2 must be UNMATCHED");
    TEST_ASSERT(inst->residents[2].next_proposal_idx == inst->residents[2].pref_count,
                "R2 should have exhausted their preference list");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable, "Exhausted list matching must be stable");
    TEST_ASSERT(vres.blocking_pair_count == 0, "No blocking pairs should exist");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 4: Instance with no perfect matching */
static bool test_edge_no_perfect_matching(void) {
    Instance *inst = parse_instance_file("tests/fixtures/04_no_perfect_matching.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 04_no_perfect_matching.txt");

    run_matching(inst, NULL, NULL);

    /* Verify unallocated seats and unmatched residents coexist stably */
    int matched_count = 0;
    for (int i = 0; i < inst->num_residents; i++) {
        if (inst->residents[i].current_match != UNMATCHED) matched_count++;
    }
    TEST_ASSERT(matched_count < inst->num_residents, "Matching cannot be perfect");

    /* H2 is never ranked by any resident */
    TEST_ASSERT(inst->hospitals[2].assigned_count == 0, "H2 should remain empty");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable, "Market deficit instance must still be stable");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 5: Ties in capacity usage and cascading bumps */
static bool test_edge_capacity_ties(void) {
    Instance *inst = parse_instance_file("tests/fixtures/05_capacity_ties.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 05_capacity_ties.txt");

    MatchResult res = run_matching(inst, NULL, NULL);
    TEST_ASSERT(res.num_bumps > 0, "Should have experienced bumping");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable, "Cascading bumps matching must be stable");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 6: Spec example from prompt */
static bool test_spec_example(void) {
    Instance *inst = parse_instance_file("tests/fixtures/06_spec_example.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 06_spec_example.txt");

    MatchResult res = run_matching(inst, NULL, NULL);
    TEST_ASSERT(res.num_proposals == 6, "Expected 6 proposals");
    TEST_ASSERT(res.num_bumps == 2, "Expected 2 bumps");

    TEST_ASSERT(inst->residents[0].current_match == 1, "R0 -> H1");
    TEST_ASSERT(inst->residents[1].current_match == 0, "R1 -> H0");
    TEST_ASSERT(inst->residents[2].current_match == 0, "R2 -> H0");
    TEST_ASSERT(inst->residents[3].current_match == 1, "R3 -> H1");
    TEST_ASSERT(inst->residents[4].current_match == UNMATCHED, "R4 -> UNMATCHED");

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable, "Spec example must be stable");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 7: Negative control - intentional blocking pair injection */
static bool test_verifier_catches_blocking_pair(void) {
    Instance *inst = parse_instance_file("tests/fixtures/06_spec_example.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 06_spec_example.txt");

    run_matching(inst, NULL, NULL);

    /* Corrupt the matching: Swap R0 (matched to H1) and R4 (unmatched)
     * R4 wanted H0 and H1, and was bumped. If we put R4 in H1 and make R0 unmatched,
     * R0 strictly prefers H1 (its #1 choice) and H1 prefers R0 over R4 or R3.
     * This forces a blocking pair! */
    inst->residents[0].current_match = UNMATCHED;
    inst->residents[4].current_match = 1;

    /* Update H1's assigned roster to replace R0 with R4 */
    Hospital *h1 = &inst->hospitals[1];
    for (int k = 0; k < h1->assigned_count; k++) {
        if (h1->assigned_residents[k] == 0) {
            h1->assigned_residents[k] = 4;
            break;
        }
    }

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable == false, "Verifier must reject corrupted matching");
    TEST_ASSERT(vres.blocking_pair_count > 0, "Verifier must identify blocking pairs");

    /* Confirm that (R0, H1) was flagged */
    bool found_r0_h1 = false;
    for (int b = 0; b < vres.blocking_pair_count; b++) {
        if (vres.blocking_pairs[b].resident_id == 0 && vres.blocking_pairs[b].hospital_id == 1) {
            found_r0_h1 = true;
            break;
        }
    }
    TEST_ASSERT(found_r0_h1, "Pair (R0, H1) must be detected as a blocking pair");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 8: Negative control - intentional capacity violation */
static bool test_verifier_catches_capacity_violation(void) {
    Instance *inst = parse_instance_file("tests/fixtures/01_simple_3r_2h.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 01_simple_3r_2h.txt");

    run_matching(inst, NULL, NULL);

    /* Illegally assign R2 to H0 beyond H0's capacity of 1 */
    inst->hospitals[0].assigned_count = 2;
    inst->hospitals[0].assigned_residents = (int *)realloc(inst->hospitals[0].assigned_residents, 2 * sizeof(int));
    inst->hospitals[0].assigned_residents[1] = 2;
    inst->residents[2].current_match = 0;

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable == false, "Must fail on capacity excess");
    TEST_ASSERT(vres.capacity_constraints_passed == false, "Capacity check must report false");
    TEST_ASSERT(vres.capacity_violation_count > 0, "Must record capacity violations");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 9: Negative control - unacceptable match injection */
static bool test_verifier_catches_unacceptable_match(void) {
    Instance *inst = parse_instance_file("tests/fixtures/04_no_perfect_matching.txt");
    TEST_ASSERT(inst != NULL, "Failed to parse 04_no_perfect_matching.txt");

    run_matching(inst, NULL, NULL);

    /* Assign resident R1 to H2 (R1 did NOT rank H2) */
    inst->residents[1].current_match = 2;
    inst->hospitals[2].assigned_residents[inst->hospitals[2].assigned_count++] = 1;

    VerificationResult vres = verify_matching(inst);
    TEST_ASSERT(vres.is_stable == false, "Must fail on unacceptable match");
    TEST_ASSERT(vres.individual_rationality_passed == false, "IR check must report false");
    TEST_ASSERT(vres.unacceptability_violation_count > 0, "Must record unacceptability violations");
    verification_result_free(&vres);

    instance_free(inst);
    return true;
}

/* Test 10: Multi-cycle stress and memory lifecycle test */
static bool test_stress_memory_lifecycle(void) {
    for (int trial = 0; trial < 25; trial++) {
        int n = 30 + trial;
        int m = 10 + (trial % 5);
        Instance *inst = instance_create(n, m);
        TEST_ASSERT(inst != NULL, "Stress instance creation failed");

        for (int i = 0; i < n; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "R%d", i);
            inst->residents[i].name = (char *)malloc(32);
            memcpy(inst->residents[i].name, buf, strlen(buf) + 1);
            int k = 3 + (i % (m > 3 ? m - 2 : 1));
            inst->residents[i].pref_count = k;
            inst->residents[i].pref_list = (int *)malloc((size_t)k * sizeof(int));
            for (int p = 0; p < k; p++) inst->residents[i].pref_list[p] = (i + p) % m;
        }

        for (int j = 0; j < m; j++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "H%d", j);
            inst->hospitals[j].name = (char *)malloc(32);
            memcpy(inst->hospitals[j].name, buf, strlen(buf) + 1);
            inst->hospitals[j].capacity = 2 + (j % 3);
            int k = 10 + (j % (n > 10 ? n - 9 : 1));
            inst->hospitals[j].pref_count = k;
            inst->hospitals[j].pref_list = (int *)malloc((size_t)k * sizeof(int));
            for (int p = 0; p < k; p++) inst->hospitals[j].pref_list[p] = (j * 3 + p) % n;
        }

        TEST_ASSERT(instance_build_ranks(inst), "Stress rank build failed");
        run_matching(inst, NULL, NULL);
        VerificationResult vres = verify_matching(inst);
        TEST_ASSERT(vres.is_stable, "Stress run must produce stable matching");
        verification_result_free(&vres);

        instance_free(inst);
    }
    return true;
}

int main(void) {
    printf("========================================================\n");
    printf("     HOSPITAL/RESIDENTS STABLE MATCHING TEST SUITE      \n");
    printf("========================================================\n\n");

    RUN_TEST(test_handbuilt_trace);
    RUN_TEST(test_edge_zero_capacity);
    RUN_TEST(test_edge_exhausted_unmatched);
    RUN_TEST(test_edge_no_perfect_matching);
    RUN_TEST(test_edge_capacity_ties);
    RUN_TEST(test_spec_example);
    RUN_TEST(test_verifier_catches_blocking_pair);
    RUN_TEST(test_verifier_catches_capacity_violation);
    RUN_TEST(test_verifier_catches_unacceptable_match);
    RUN_TEST(test_stress_memory_lifecycle);

    printf("\n========================================================\n");
    printf("Results: %d / %d tests passed.\n", passed_tests, total_tests);
    printf("========================================================\n");

    return (passed_tests == total_tests) ? 0 : 1;
}
