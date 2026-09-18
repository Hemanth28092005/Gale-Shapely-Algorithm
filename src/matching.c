#include "matching.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

Instance *instance_create(int num_residents, int num_hospitals) {
    if (num_residents < 0 || num_hospitals < 0) return NULL;

    Instance *inst = (Instance *)calloc(1, sizeof(Instance));
    if (!inst) return NULL;

    inst->num_residents = num_residents;
    inst->num_hospitals = num_hospitals;

    if (num_residents > 0) {
        inst->residents = (Resident *)calloc((size_t)num_residents, sizeof(Resident));
        if (!inst->residents) {
            instance_free(inst);
            return NULL;
        }
        for (int i = 0; i < num_residents; i++) {
            inst->residents[i].id = i;
            inst->residents[i].name = NULL;
            inst->residents[i].pref_list = NULL;
            inst->residents[i].pref_count = 0;
            inst->residents[i].next_proposal_idx = 0;
            inst->residents[i].current_match = UNMATCHED;
            if (num_hospitals > 0) {
                inst->residents[i].rank = (int *)malloc((size_t)num_hospitals * sizeof(int));
                if (!inst->residents[i].rank) {
                    instance_free(inst);
                    return NULL;
                }
                for (int h = 0; h < num_hospitals; h++) {
                    inst->residents[i].rank[h] = NOT_RANKED;
                }
            } else {
                inst->residents[i].rank = NULL;
            }
        }
    }

    if (num_hospitals > 0) {
        inst->hospitals = (Hospital *)calloc((size_t)num_hospitals, sizeof(Hospital));
        if (!inst->hospitals) {
            instance_free(inst);
            return NULL;
        }
        for (int j = 0; j < num_hospitals; j++) {
            inst->hospitals[j].id = j;
            inst->hospitals[j].name = NULL;
            inst->hospitals[j].capacity = 0;
            inst->hospitals[j].pref_list = NULL;
            inst->hospitals[j].pref_count = 0;
            inst->hospitals[j].assigned_residents = NULL;
            inst->hospitals[j].assigned_count = 0;
            if (num_residents > 0) {
                inst->hospitals[j].rank = (int *)malloc((size_t)num_residents * sizeof(int));
                if (!inst->hospitals[j].rank) {
                    instance_free(inst);
                    return NULL;
                }
                for (int r = 0; r < num_residents; r++) {
                    inst->hospitals[j].rank[r] = NOT_RANKED;
                }
            } else {
                inst->hospitals[j].rank = NULL;
            }
        }
    }

    return inst;
}

void instance_free(Instance *inst) {
    if (!inst) return;

    if (inst->residents) {
        for (int i = 0; i < inst->num_residents; i++) {
            free(inst->residents[i].name);
            free(inst->residents[i].pref_list);
            free(inst->residents[i].rank);
        }
        free(inst->residents);
    }

    if (inst->hospitals) {
        for (int j = 0; j < inst->num_hospitals; j++) {
            free(inst->hospitals[j].name);
            free(inst->hospitals[j].pref_list);
            free(inst->hospitals[j].rank);
            free(inst->hospitals[j].assigned_residents);
        }
        free(inst->hospitals);
    }

    free(inst);
}

void instance_reset_matching(Instance *inst) {
    if (!inst) return;

    for (int i = 0; i < inst->num_residents; i++) {
        inst->residents[i].current_match = UNMATCHED;
        inst->residents[i].next_proposal_idx = 0;
    }

    for (int j = 0; j < inst->num_hospitals; j++) {
        inst->hospitals[j].assigned_count = 0;
    }
}

bool instance_build_ranks(Instance *inst) {
    if (!inst) return false;

    /* Build resident ranks */
    for (int i = 0; i < inst->num_residents; i++) {
        Resident *r = &inst->residents[i];
        for (int h = 0; h < inst->num_hospitals; h++) {
            r->rank[h] = NOT_RANKED;
        }
        for (int k = 0; k < r->pref_count; k++) {
            int h_id = r->pref_list[k];
            if (h_id < 0 || h_id >= inst->num_hospitals) {
                return false;
            }
            r->rank[h_id] = k;
        }
    }

    /* Build hospital ranks and allocate assigned_residents */
    for (int j = 0; j < inst->num_hospitals; j++) {
        Hospital *h = &inst->hospitals[j];
        for (int r = 0; r < inst->num_residents; r++) {
            h->rank[r] = NOT_RANKED;
        }
        for (int k = 0; k < h->pref_count; k++) {
            int r_id = h->pref_list[k];
            if (r_id < 0 || r_id >= inst->num_residents) {
                return false;
            }
            h->rank[r_id] = k;
        }
        if (h->capacity > 0) {
            free(h->assigned_residents);
            h->assigned_residents = (int *)malloc((size_t)h->capacity * sizeof(int));
            if (!h->assigned_residents) return false;
        }
        h->assigned_count = 0;
    }

    return true;
}

int hospital_get_worst_assigned_resident(const Hospital *h) {
    if (!h || h->assigned_count == 0) return UNMATCHED;

    int worst_resident = h->assigned_residents[0];
    int worst_rank = h->rank[worst_resident];

    for (int i = 1; i < h->assigned_count; i++) {
        int r_id = h->assigned_residents[i];
        int r_rank = h->rank[r_id];
        if (r_rank > worst_rank) {
            worst_rank = r_rank;
            worst_resident = r_id;
        }
    }
    return worst_resident;
}

bool hospital_prefers(const Hospital *h, int r_candidate, int r_current) {
    if (!h) return false;
    if (h->rank[r_candidate] == NOT_RANKED) return false;
    if (r_current == UNMATCHED) return true;
    if (h->rank[r_current] == NOT_RANKED) return true;
    return h->rank[r_candidate] < h->rank[r_current];
}

static double get_time_seconds(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

MatchResult run_matching(Instance *inst, MatchEventCallback callback, void *user_data) {
    MatchResult result = {0, 0, 0, 0.0};
    if (!inst || inst->num_residents == 0) return result;

    instance_reset_matching(inst);

    double start_time = get_time_seconds();

    /* Free resident stack */
    int *free_stack = (int *)malloc((size_t)inst->num_residents * sizeof(int));
    if (!free_stack) return result;
    int stack_top = 0;

    /* Initially, all residents with at least 1 acceptable hospital are free */
    for (int i = 0; i < inst->num_residents; i++) {
        if (inst->residents[i].pref_count > 0) {
            free_stack[stack_top++] = i;
        }
    }

    int step_number = 0;

    while (stack_top > 0) {
        int r_id = free_stack[--stack_top];
        Resident *r = &inst->residents[r_id];

        while (r->current_match == UNMATCHED && r->next_proposal_idx < r->pref_count) {
            int h_id = r->pref_list[r->next_proposal_idx++];
            Hospital *h = &inst->hospitals[h_id];
            step_number++;
            result.num_proposals++;

            if (callback) {
                MatchEvent ev;
                ev.type = EVENT_PROPOSAL;
                ev.step_number = step_number;
                ev.resident_id = r_id;
                ev.hospital_id = h_id;
                ev.bumped_resident_id = UNMATCHED;
                callback(&ev, user_data);
            }

            /* 1. Check if resident is acceptable to hospital */
            if (h->rank[r_id] == NOT_RANKED) {
                result.num_rejections++;
                if (callback) {
                    MatchEvent ev;
                    ev.type = EVENT_REJECT_UNACCEPTABLE;
                    ev.step_number = step_number;
                    ev.resident_id = r_id;
                    ev.hospital_id = h_id;
                    ev.bumped_resident_id = UNMATCHED;
                    callback(&ev, user_data);
                }
                continue;
            }

            /* 2. Check if hospital has capacity 0 */
            if (h->capacity == 0) {
                result.num_rejections++;
                if (callback) {
                    MatchEvent ev;
                    ev.type = EVENT_REJECT_ZERO_CAPACITY;
                    ev.step_number = step_number;
                    ev.resident_id = r_id;
                    ev.hospital_id = h_id;
                    ev.bumped_resident_id = UNMATCHED;
                    callback(&ev, user_data);
                }
                continue;
            }

            /* 3. Hospital has vacant seat */
            if (h->assigned_count < h->capacity) {
                h->assigned_residents[h->assigned_count++] = r_id;
                r->current_match = h_id;

                if (callback) {
                    MatchEvent ev;
                    ev.type = EVENT_ACCEPT_VACANCY;
                    ev.step_number = step_number;
                    ev.resident_id = r_id;
                    ev.hospital_id = h_id;
                    ev.bumped_resident_id = UNMATCHED;
                    callback(&ev, user_data);
                }
                break; /* Successfully matched, stop proposing for r */
            }

            /* 4. Hospital is full: find worst current match */
            int worst_idx = 0;
            int worst_r = h->assigned_residents[0];
            int worst_rank = h->rank[worst_r];

            for (int k = 1; k < h->assigned_count; k++) {
                int curr_r = h->assigned_residents[k];
                int curr_rank = h->rank[curr_r];
                if (curr_rank > worst_rank) {
                    worst_rank = curr_rank;
                    worst_r = curr_r;
                    worst_idx = k;
                }
            }

            /* Does hospital prefer r_id over worst_r? */
            if (h->rank[r_id] < worst_rank) {
                /* Bumping occurs */
                result.num_bumps++;

                if (callback) {
                    MatchEvent ev;
                    ev.type = EVENT_ACCEPT_BUMP;
                    ev.step_number = step_number;
                    ev.resident_id = r_id;
                    ev.hospital_id = h_id;
                    ev.bumped_resident_id = worst_r;
                    callback(&ev, user_data);
                }

                /* Evict worst_r */
                inst->residents[worst_r].current_match = UNMATCHED;
                h->assigned_residents[worst_idx] = r_id;
                r->current_match = h_id;

                /* If worst_r has remaining hospitals, push to stack */
                if (inst->residents[worst_r].next_proposal_idx < inst->residents[worst_r].pref_count) {
                    free_stack[stack_top++] = worst_r;
                }
                break; /* r is now matched */
            } else {
                /* Rejection */
                result.num_rejections++;
                if (callback) {
                    MatchEvent ev;
                    ev.type = EVENT_REJECT_FULL;
                    ev.step_number = step_number;
                    ev.resident_id = r_id;
                    ev.hospital_id = h_id;
                    ev.bumped_resident_id = UNMATCHED;
                    callback(&ev, user_data);
                }
                /* Continue loop: r tries next hospital */
            }
        }
    }

    free(free_stack);

    double end_time = get_time_seconds();
    result.execution_time_ms = (end_time - start_time) * 1000.0;

    return result;
}

void print_instance_summary(const Instance *inst) {
    if (!inst) return;

    printf("=== Instance Summary ===\n");
    printf("Residents: %d, Hospitals: %d\n\n", inst->num_residents, inst->num_hospitals);

    printf("Residents Preference Lists:\n");
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        printf("  %s (id=%d): ", r->name ? r->name : "R", r->id);
        if (r->pref_count == 0) {
            printf("[none]");
        } else {
            for (int k = 0; k < r->pref_count; k++) {
                int hid = r->pref_list[k];
                printf("%s%s", inst->hospitals[hid].name ? inst->hospitals[hid].name : "H",
                       (k == r->pref_count - 1) ? "" : " > ");
            }
        }
        printf("\n");
    }

    printf("\nHospitals Preferences & Capacities:\n");
    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        printf("  %s (id=%d, cap=%d): ", h->name ? h->name : "H", h->id, h->capacity);
        if (h->pref_count == 0) {
            printf("[none]");
        } else {
            for (int k = 0; k < h->pref_count; k++) {
                int rid = h->pref_list[k];
                printf("%s%s", inst->residents[rid].name ? inst->residents[rid].name : "R",
                       (k == h->pref_count - 1) ? "" : " > ");
            }
        }
        printf("\n");
    }
    printf("========================\n\n");
}

void print_matching_results(const Instance *inst, const MatchResult *result) {
    if (!inst) return;

    printf("\n=== Gale-Shapley Matching Results ===\n");
    if (result) {
        printf("Metrics:\n");
        printf("  Proposals:  %lld\n", result->num_proposals);
        printf("  Rejections: %lld\n", result->num_rejections);
        printf("  Bumps:      %lld\n", result->num_bumps);
        printf("  Time:       %.3f ms\n\n", result->execution_time_ms);
    }

    printf("Resident Assignments:\n");
    int matched_residents = 0;
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        const char *r_name = r->name ? r->name : "R";
        if (r->current_match == UNMATCHED) {
            printf("  %-10s -> [UNMATCHED]\n", r_name);
        } else {
            matched_residents++;
            const Hospital *h = &inst->hospitals[r->current_match];
            const char *h_name = h->name ? h->name : "H";
            int rank = r->rank[r->current_match];
            printf("  %-10s -> %-10s (choice #%d)\n", r_name, h_name, rank + 1);
        }
    }

    printf("\nHospital Rosters:\n");
    int total_assigned_seats = 0;
    int total_capacity = 0;
    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        const char *h_name = h->name ? h->name : "H";
        total_assigned_seats += h->assigned_count;
        total_capacity += h->capacity;
        printf("  %-10s (capacity %d/%d): [", h_name, h->assigned_count, h->capacity);
        for (int k = 0; k < h->assigned_count; k++) {
            int rid = h->assigned_residents[k];
            const char *r_name = inst->residents[rid].name ? inst->residents[rid].name : "R";
            printf("%s%s", r_name, (k == h->assigned_count - 1) ? "" : ", ");
        }
        printf("]\n");
    }

    printf("\nSummary: %d/%d residents matched, %d/%d hospital seats filled.\n",
           matched_residents, inst->num_residents, total_assigned_seats, total_capacity);
    printf("=====================================\n\n");
}
