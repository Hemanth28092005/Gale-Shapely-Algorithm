#ifndef MATCHING_H
#define MATCHING_H

#include <stdbool.h>
#include <stddef.h>

#define NOT_RANKED (-1)
#define UNMATCHED  (-1)

typedef struct {
    int id;                 /* 0-indexed resident ID */
    char *name;             /* e.g. "R0" or human name */
    int *pref_list;         /* Array of hospital IDs in strict descending preference */
    int pref_count;         /* Number of acceptable hospitals */
    int *rank;              /* rank[h] = 0-based rank of hospital h, or NOT_RANKED */
    int next_proposal_idx;  /* Index in pref_list of next hospital to propose to */
    int current_match;      /* Hospital ID currently matched to, or UNMATCHED */
} Resident;

typedef struct {
    int id;                 /* 0-indexed hospital ID */
    char *name;             /* e.g. "H0" or hospital name */
    int capacity;           /* Maximum resident capacity c_j >= 0 */
    int *pref_list;         /* Array of resident IDs in strict descending preference */
    int pref_count;         /* Number of acceptable residents */
    int *rank;              /* rank[r] = 0-based rank of resident r, or NOT_RANKED */
    int *assigned_residents;/* Array of currently assigned resident IDs (size: capacity) */
    int assigned_count;     /* Number of currently assigned residents */
} Hospital;

typedef struct {
    int num_residents;
    int num_hospitals;
    Resident *residents;
    Hospital *hospitals;
} Instance;

typedef enum {
    EVENT_PROPOSAL,
    EVENT_ACCEPT_VACANCY,
    EVENT_ACCEPT_BUMP,
    EVENT_REJECT_UNACCEPTABLE,
    EVENT_REJECT_FULL,
    EVENT_REJECT_ZERO_CAPACITY
} MatchEventType;

typedef struct {
    MatchEventType type;
    int step_number;
    int resident_id;
    int hospital_id;
    int bumped_resident_id; /* UNMATCHED if not a bump event */
} MatchEvent;

typedef void (*MatchEventCallback)(const MatchEvent *event, void *user_data);

typedef struct {
    long long num_proposals;
    long long num_rejections;
    long long num_bumps;
    double execution_time_ms;
} MatchResult;

/* Instance lifecycle */
Instance *instance_create(int num_residents, int num_hospitals);
void instance_free(Instance *inst);
void instance_reset_matching(Instance *inst);

/* Helper to populate preference ranks after pref_list has been filled */
bool instance_build_ranks(Instance *inst);

/* Hospital helper routines */
int hospital_get_worst_assigned_resident(const Hospital *h);
bool hospital_prefers(const Hospital *h, int r_candidate, int r_current);

/* Gale-Shapley matching algorithm (resident-optimal) */
MatchResult run_matching(Instance *inst, MatchEventCallback callback, void *user_data);

/* Output formatting */
void print_instance_summary(const Instance *inst);
void print_matching_results(const Instance *inst, const MatchResult *result);

#endif /* MATCHING_H */
