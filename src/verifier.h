#ifndef VERIFIER_H
#define VERIFIER_H

#include "matching.h"
#include <stdbool.h>

typedef enum {
    BLOCK_REASON_VACANCY,
    BLOCK_REASON_PREFERS_OVER_WORST
} BlockingReason;

typedef struct {
    int resident_id;
    int hospital_id;
    BlockingReason reason;
    int resident_rank_of_h;
    int resident_current_match_rank; /* NOT_RANKED if resident was unmatched */
    int hospital_rank_of_r;
    int bumped_resident_id;          /* UNMATCHED if vacancy */
    int worst_resident_rank;         /* NOT_RANKED if vacancy */
    char description[256];
} BlockingPair;

typedef struct {
    bool is_stable;
    bool individual_rationality_passed;
    bool capacity_constraints_passed;
    bool consistency_passed;

    int blocking_pair_count;
    int blocking_pair_capacity;
    BlockingPair *blocking_pairs;

    int capacity_violation_count;
    int unacceptability_violation_count;
    int consistency_violation_count;
} VerificationResult;

/* Runs complete verification checks on the instance's current matching */
VerificationResult verify_matching(const Instance *inst);

/* Frees dynamically allocated memory in VerificationResult */
void verification_result_free(VerificationResult *result);

/* Formats and prints comprehensive verification report to stdout */
void print_verification_report(const Instance *inst, const VerificationResult *result);

#endif /* VERIFIER_H */
