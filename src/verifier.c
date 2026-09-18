#include "verifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_blocking_pair(VerificationResult *res, BlockingPair bp) {
    if (res->blocking_pair_count >= res->blocking_pair_capacity) {
        int new_cap = (res->blocking_pair_capacity == 0) ? 8 : res->blocking_pair_capacity * 2;
        BlockingPair *new_arr = (BlockingPair *)realloc(res->blocking_pairs, (size_t)new_cap * sizeof(BlockingPair));
        if (!new_arr) return;
        res->blocking_pairs = new_arr;
        res->blocking_pair_capacity = new_cap;
    }
    res->blocking_pairs[res->blocking_pair_count++] = bp;
}

VerificationResult verify_matching(const Instance *inst) {
    VerificationResult res;
    memset(&res, 0, sizeof(VerificationResult));

    res.is_stable = true;
    res.individual_rationality_passed = true;
    res.capacity_constraints_passed = true;
    res.consistency_passed = true;

    if (!inst) {
        res.is_stable = false;
        return res;
    }

    /* 1. Verify Capacity Constraints & Roster Uniqueness */
    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        if (h->assigned_count < 0 || h->assigned_count > h->capacity) {
            res.capacity_constraints_passed = false;
            res.capacity_violation_count++;
        }

        /* Check for duplicate assignments within the same hospital */
        for (int a = 0; a < h->assigned_count; a++) {
            for (int b = a + 1; b < h->assigned_count; b++) {
                if (h->assigned_residents[a] == h->assigned_residents[b]) {
                    res.capacity_constraints_passed = false;
                    res.capacity_violation_count++;
                }
            }
        }
    }

    /* 2. Verify Assignment Consistency (Bi-directional link) */
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        if (r->current_match != UNMATCHED) {
            if (r->current_match < 0 || r->current_match >= inst->num_hospitals) {
                res.consistency_passed = false;
                res.consistency_violation_count++;
            } else {
                const Hospital *h = &inst->hospitals[r->current_match];
                bool found = false;
                for (int k = 0; k < h->assigned_count; k++) {
                    if (h->assigned_residents[k] == r->id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    res.consistency_passed = false;
                    res.consistency_violation_count++;
                }
            }
        }
    }

    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        for (int k = 0; k < h->assigned_count; k++) {
            int r_id = h->assigned_residents[k];
            if (r_id < 0 || r_id >= inst->num_residents) {
                res.consistency_passed = false;
                res.consistency_violation_count++;
            } else if (inst->residents[r_id].current_match != h->id) {
                res.consistency_passed = false;
                res.consistency_violation_count++;
            }
        }
    }

    /* 3. Verify Individual Rationality (Mutual Acceptability) */
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        if (r->current_match != UNMATCHED) {
            int h_id = r->current_match;
            const Hospital *h = &inst->hospitals[h_id];

            /* Resident must find hospital acceptable */
            if (r->rank[h_id] == NOT_RANKED) {
                res.individual_rationality_passed = false;
                res.unacceptability_violation_count++;
            }

            /* Hospital must find resident acceptable */
            if (h->rank[r->id] == NOT_RANKED) {
                res.individual_rationality_passed = false;
                res.unacceptability_violation_count++;
            }
        }
    }

    /* 4. Exhaustive Search for Blocking Pairs */
    for (int r_id = 0; r_id < inst->num_residents; r_id++) {
        const Resident *r = &inst->residents[r_id];
        const char *r_name = r->name ? r->name : "Resident";

        for (int h_id = 0; h_id < inst->num_hospitals; h_id++) {
            const Hospital *h = &inst->hospitals[h_id];
            const char *h_name = h->name ? h->name : "Hospital";

            /* Skip if already matched to each other */
            if (r->current_match == h_id) {
                continue;
            }

            /* Condition 1: Hospital must be acceptable to resident */
            int r_rank_h = r->rank[h_id];
            if (r_rank_h == NOT_RANKED) {
                continue;
            }

            /* Condition 2: Resident must strictly prefer hospital h over current match */
            int r_curr_rank = (r->current_match == UNMATCHED) ? NOT_RANKED : r->rank[r->current_match];
            bool resident_prefers = false;
            if (r->current_match == UNMATCHED) {
                resident_prefers = true; /* Prefers any acceptable hospital over being unmatched */
            } else if (r_rank_h < r_curr_rank) {
                resident_prefers = true;
            }

            if (!resident_prefers) {
                continue;
            }

            /* Condition 3: Resident must be acceptable to hospital */
            int h_rank_r = h->rank[r_id];
            if (h_rank_r == NOT_RANKED) {
                continue;
            }

            /* Condition 4: Hospital either has vacancy OR prefers r over its worst match */
            if (h->capacity > 0 && h->assigned_count < h->capacity) {
                /* Blocking Pair via Vacancy */
                BlockingPair bp;
                bp.resident_id = r_id;
                bp.hospital_id = h_id;
                bp.reason = BLOCK_REASON_VACANCY;
                bp.resident_rank_of_h = r_rank_h;
                bp.resident_current_match_rank = r_curr_rank;
                bp.hospital_rank_of_r = h_rank_r;
                bp.bumped_resident_id = UNMATCHED;
                bp.worst_resident_rank = NOT_RANKED;

                if (r->current_match == UNMATCHED) {
                    snprintf(bp.description, sizeof(bp.description),
                             "%s prefers %s (choice #%d) over being unmatched; %s has vacancy (%d/%d)",
                             r_name, h_name, r_rank_h + 1, h_name, h->assigned_count, h->capacity);
                } else {
                    const char *curr_h_name = inst->hospitals[r->current_match].name ?
                                              inst->hospitals[r->current_match].name : "current hospital";
                    snprintf(bp.description, sizeof(bp.description),
                             "%s prefers %s (choice #%d) over %s (choice #%d); %s has vacancy (%d/%d)",
                             r_name, h_name, r_rank_h + 1, curr_h_name, r_curr_rank + 1, h_name,
                             h->assigned_count, h->capacity);
                }

                add_blocking_pair(&res, bp);
            } else if (h->capacity > 0 && h->assigned_count == h->capacity) {
                /* Find worst currently assigned resident */
                int worst_r = h->assigned_residents[0];
                int worst_rank = h->rank[worst_r];
                for (int k = 1; k < h->assigned_count; k++) {
                    int curr_r = h->assigned_residents[k];
                    int curr_rank = h->rank[curr_r];
                    if (curr_rank > worst_rank) {
                        worst_rank = curr_rank;
                        worst_r = curr_r;
                    }
                }

                /* Check if hospital prefers r over worst_r */
                if (h_rank_r < worst_rank) {
                    /* Blocking Pair via Preference over Worst Match */
                    BlockingPair bp;
                    bp.resident_id = r_id;
                    bp.hospital_id = h_id;
                    bp.reason = BLOCK_REASON_PREFERS_OVER_WORST;
                    bp.resident_rank_of_h = r_rank_h;
                    bp.resident_current_match_rank = r_curr_rank;
                    bp.hospital_rank_of_r = h_rank_r;
                    bp.bumped_resident_id = worst_r;
                    bp.worst_resident_rank = worst_rank;

                    const char *worst_r_name = inst->residents[worst_r].name ?
                                              inst->residents[worst_r].name : "worst resident";

                    if (r->current_match == UNMATCHED) {
                        snprintf(bp.description, sizeof(bp.description),
                                 "%s prefers %s (choice #%d) over being unmatched; %s prefers %s (rank #%d) over worst match %s (rank #%d)",
                                 r_name, h_name, r_rank_h + 1, h_name, r_name, h_rank_r + 1,
                                 worst_r_name, worst_rank + 1);
                    } else {
                        const char *curr_h_name = inst->hospitals[r->current_match].name ?
                                                  inst->hospitals[r->current_match].name : "current hospital";
                        snprintf(bp.description, sizeof(bp.description),
                                 "%s prefers %s (choice #%d) over %s (choice #%d); %s prefers %s (rank #%d) over worst match %s (rank #%d)",
                                 r_name, h_name, r_rank_h + 1, curr_h_name, r_curr_rank + 1,
                                 h_name, r_name, h_rank_r + 1, worst_r_name, worst_rank + 1);
                    }

                    add_blocking_pair(&res, bp);
                }
            }
        }
    }

    res.is_stable = (res.blocking_pair_count == 0 &&
                     res.capacity_constraints_passed &&
                     res.individual_rationality_passed &&
                     res.consistency_passed);

    return res;
}

void verification_result_free(VerificationResult *result) {
    if (!result) return;
    free(result->blocking_pairs);
    result->blocking_pairs = NULL;
    result->blocking_pair_count = 0;
    result->blocking_pair_capacity = 0;
}

void print_verification_report(const Instance *inst, const VerificationResult *result) {
    if (!inst || !result) return;

    printf("================ STABILITY VERIFICATION ================\n");
    if (result->is_stable) {
        printf("VERDICT: PASS [STABLE MATCHING]\n");
        printf("  - No blocking pairs found (checked %d x %d resident-hospital pairs).\n",
               inst->num_residents, inst->num_hospitals);
        printf("  - Capacity constraints satisfied: YES\n");
        printf("  - Individual rationality satisfied: YES\n");
        printf("  - Bi-directional roster consistency: YES\n");
    } else {
        printf("VERDICT: FAIL [UNSTABLE MATCHING]\n");
        if (!result->capacity_constraints_passed) {
            printf("  [ERROR] Capacity violations detected: %d\n", result->capacity_violation_count);
        }
        if (!result->individual_rationality_passed) {
            printf("  [ERROR] Unacceptable match violations detected: %d\n", result->unacceptability_violation_count);
        }
        if (!result->consistency_passed) {
            printf("  [ERROR] Bi-directional consistency violations detected: %d\n", result->consistency_violation_count);
        }
        if (result->blocking_pair_count > 0) {
            printf("  [ERROR] Found %d blocking pair(s):\n", result->blocking_pair_count);
            for (int i = 0; i < result->blocking_pair_count; i++) {
                const BlockingPair *bp = &result->blocking_pairs[i];
                printf("    %d. (%s, %s): %s\n",
                       i + 1,
                       inst->residents[bp->resident_id].name ? inst->residents[bp->resident_id].name : "R",
                       inst->hospitals[bp->hospital_id].name ? inst->hospitals[bp->hospital_id].name : "H",
                       bp->description);
            }
        }
    }
    printf("========================================================\n\n");
}
