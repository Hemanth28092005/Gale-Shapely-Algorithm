#include "matching.h"
#include "verifier.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MatchEvent *events;
    int count;
    int capacity;
    bool verbose;
    const Instance *inst;
} TraceCollector;

static void trace_callback(const MatchEvent *ev, void *user_data) {
    TraceCollector *tc = (TraceCollector *)user_data;
    if (!tc) return;

    const Instance *inst = tc->inst;
    const char *r_name = (inst && inst->residents[ev->resident_id].name) ?
                         inst->residents[ev->resident_id].name : "Resident";
    const char *h_name = (inst && inst->hospitals[ev->hospital_id].name) ?
                         inst->hospitals[ev->hospital_id].name : "Hospital";

    if (tc->verbose) {
        switch (ev->type) {
            case EVENT_PROPOSAL:
                printf("[Step %3d] %s proposes to %s\n", ev->step_number, r_name, h_name);
                break;
            case EVENT_ACCEPT_VACANCY:
                printf("           -> %s provisionally ACCEPTS %s (seat available: %d/%d)\n",
                       h_name, r_name, inst->hospitals[ev->hospital_id].assigned_count,
                       inst->hospitals[ev->hospital_id].capacity);
                break;
            case EVENT_ACCEPT_BUMP: {
                const char *bumped_name = (inst && inst->residents[ev->bumped_resident_id].name) ?
                                          inst->residents[ev->bumped_resident_id].name : "Resident";
                printf("           -> %s BUMPS %s to accept %s (better preference rank)\n",
                       h_name, bumped_name, r_name);
                break;
            }
            case EVENT_REJECT_UNACCEPTABLE:
                printf("           -> %s REJECTS %s (resident is not on hospital's preference list)\n",
                       h_name, r_name);
                break;
            case EVENT_REJECT_ZERO_CAPACITY:
                printf("           -> %s REJECTS %s (hospital has 0 capacity)\n",
                       h_name, r_name);
                break;
            case EVENT_REJECT_FULL:
                printf("           -> %s REJECTS %s (hospital is full of more preferred residents)\n",
                       h_name, r_name);
                break;
        }
    }

    /* Collect for JSON export */
    if (tc->capacity > 0) {
        if (tc->count >= tc->capacity) {
            tc->capacity *= 2;
            tc->events = (MatchEvent *)realloc(tc->events, (size_t)tc->capacity * sizeof(MatchEvent));
        }
        tc->events[tc->count++] = *ev;
    }
}

static void export_json_trace(const Instance *inst, const TraceCollector *tc, const MatchResult *res,
                              const VerificationResult *v_res, const char *filepath) {
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "Error: Unable to open JSON output file '%s'\n", filepath);
        return;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"num_residents\": %d,\n", inst->num_residents);
    fprintf(fp, "  \"num_hospitals\": %d,\n", inst->num_hospitals);

    /* Residents */
    fprintf(fp, "  \"residents\": [\n");
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        fprintf(fp, "    {\"id\": %d, \"name\": \"%s\", \"current_match\": %d, \"prefs\": [",
                r->id, r->name, r->current_match);
        for (int k = 0; k < r->pref_count; k++) {
            fprintf(fp, "%d%s", r->pref_list[k], (k == r->pref_count - 1) ? "" : ", ");
        }
        fprintf(fp, "]}%s\n", (i == inst->num_residents - 1) ? "" : ",");
    }
    fprintf(fp, "  ],\n");

    /* Hospitals */
    fprintf(fp, "  \"hospitals\": [\n");
    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        fprintf(fp, "    {\"id\": %d, \"name\": \"%s\", \"capacity\": %d, \"prefs\": [",
                h->id, h->name, h->capacity);
        for (int k = 0; k < h->pref_count; k++) {
            fprintf(fp, "%d%s", h->pref_list[k], (k == h->pref_count - 1) ? "" : ", ");
        }
        fprintf(fp, "]}%s\n", (j == inst->num_hospitals - 1) ? "" : ",");
    }
    fprintf(fp, "  ],\n");

    /* Steps */
    fprintf(fp, "  \"steps\": [\n");
    for (int s = 0; s < tc->count; s++) {
        const MatchEvent *ev = &tc->events[s];
        const char *type_str = "PROPOSAL";
        if (ev->type == EVENT_ACCEPT_VACANCY) type_str = "ACCEPT_VACANCY";
        else if (ev->type == EVENT_ACCEPT_BUMP) type_str = "ACCEPT_BUMP";
        else if (ev->type == EVENT_REJECT_UNACCEPTABLE) type_str = "REJECT_UNACCEPTABLE";
        else if (ev->type == EVENT_REJECT_FULL) type_str = "REJECT_FULL";
        else if (ev->type == EVENT_REJECT_ZERO_CAPACITY) type_str = "REJECT_ZERO_CAPACITY";

        fprintf(fp, "    {\"step\": %d, \"type\": \"%s\", \"resident\": %d, \"hospital\": %d, \"bumped\": %d}%s\n",
                ev->step_number, type_str, ev->resident_id, ev->hospital_id, ev->bumped_resident_id,
                (s == tc->count - 1) ? "" : ",");
    }
    fprintf(fp, "  ],\n");

    /* Metrics */
    fprintf(fp, "  \"metrics\": {\n");
    fprintf(fp, "    \"proposals\": %lld,\n", res->num_proposals);
    fprintf(fp, "    \"rejections\": %lld,\n", res->num_rejections);
    fprintf(fp, "    \"bumps\": %lld,\n", res->num_bumps);
    fprintf(fp, "    \"time_ms\": %.4f,\n", res->execution_time_ms);
    fprintf(fp, "    \"stable\": %s\n", (v_res && v_res->is_stable) ? "true" : "false");
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");
    fclose(fp);
    printf("Successfully wrote execution trace to '%s'\n", filepath);
}

static char *string_duplicate(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

Instance *create_handbuilt_3r_2h_instance(void) {
    Instance *inst = instance_create(3, 2);
    if (!inst) return NULL;

    /* R0 */
    inst->residents[0].name = string_duplicate("R0");
    inst->residents[0].pref_count = 2;
    inst->residents[0].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[0].pref_list[0] = 0; /* H0 */
    inst->residents[0].pref_list[1] = 1; /* H1 */

    /* R1 */
    inst->residents[1].name = string_duplicate("R1");
    inst->residents[1].pref_count = 2;
    inst->residents[1].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[1].pref_list[0] = 0; /* H0 */
    inst->residents[1].pref_list[1] = 1; /* H1 */

    /* R2 */
    inst->residents[2].name = string_duplicate("R2");
    inst->residents[2].pref_count = 2;
    inst->residents[2].pref_list = (int *)malloc(2 * sizeof(int));
    inst->residents[2].pref_list[0] = 1; /* H1 */
    inst->residents[2].pref_list[1] = 0; /* H0 */

    /* H0 (capacity 1): R0 > R1 > R2 */
    inst->hospitals[0].name = string_duplicate("H0");
    inst->hospitals[0].capacity = 1;
    inst->hospitals[0].pref_count = 3;
    inst->hospitals[0].pref_list = (int *)malloc(3 * sizeof(int));
    inst->hospitals[0].pref_list[0] = 0; /* R0 */
    inst->hospitals[0].pref_list[1] = 1; /* R1 */
    inst->hospitals[0].pref_list[2] = 2; /* R2 */

    /* H1 (capacity 1): R1 > R0 > R2 */
    inst->hospitals[1].name = string_duplicate("H1");
    inst->hospitals[1].capacity = 1;
    inst->hospitals[1].pref_count = 3;
    inst->hospitals[1].pref_list = (int *)malloc(3 * sizeof(int));
    inst->hospitals[1].pref_list[0] = 1; /* R1 */
    inst->hospitals[1].pref_list[1] = 0; /* R0 */
    inst->hospitals[1].pref_list[2] = 2; /* R2 */

    instance_build_ranks(inst);
    return inst;
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --input, -i <file>    Specify input problem instance file\n");
    printf("  --verbose, -v         Print step-by-step proposal and bumping trace\n");
    printf("  --verify              Run stability and individual rationality checks\n");
    printf("  --benchmark           Run performance benchmark suite\n");
    printf("  --json <file>         Export JSON execution trace for animated visualizer\n");
    printf("  --help, -h            Show this help message and exit\n\n");
    printf("If no input file is specified, the built-in 3-resident / 2-hospital\n");
    printf("reference instance is executed.\n");
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *json_file = NULL;
    bool verbose = false;
    bool verify = false;
    bool benchmark = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 || strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                input_file = argv[++i];
            } else {
                fprintf(stderr, "Error: --input requires a file argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--verify") == 0) {
            verify = true;
        } else if (strcmp(argv[i], "--benchmark") == 0) {
            benchmark = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            if (i + 1 < argc) {
                json_file = argv[++i];
            } else {
                fprintf(stderr, "Error: --json requires a file argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (benchmark) {
        printf("Running built-in benchmark runner...\n");
        printf("For full benchmarks, use 'make benchmark' or 'python benchmarks/run_benchmarks.py'\n");
        /* Quick benchmark */
        int sizes[] = {100, 200, 500, 1000};
        for (int s = 0; s < 4; s++) {
            int n = sizes[s];
            int m = n / 2;
            Instance *inst = instance_create(n, m);
            for (int i = 0; i < n; i++) {
                char buf[32];
                snprintf(buf, sizeof(buf), "R%d", i);
                inst->residents[i].name = string_duplicate(buf);
                int k = (m < 20) ? m : 20;
                inst->residents[i].pref_count = k;
                inst->residents[i].pref_list = (int *)malloc((size_t)k * sizeof(int));
                for (int p = 0; p < k; p++) inst->residents[i].pref_list[p] = (i + p) % m;
            }
            for (int j = 0; j < m; j++) {
                char buf[32];
                snprintf(buf, sizeof(buf), "H%d", j);
                inst->hospitals[j].name = string_duplicate(buf);
                inst->hospitals[j].capacity = 2;
                int k = (n < 50) ? n : 50;
                inst->hospitals[j].pref_count = k;
                inst->hospitals[j].pref_list = (int *)malloc((size_t)k * sizeof(int));
                for (int p = 0; p < k; p++) inst->hospitals[j].pref_list[p] = (j * 2 + p) % n;
            }
            instance_build_ranks(inst);
            MatchResult res = run_matching(inst, NULL, NULL);
            VerificationResult vres = verify_matching(inst);
            printf("N=%4d, M=%4d | Time: %7.3f ms | Proposals: %6lld | Bumps: %5lld | Stable: %s\n",
                   n, m, res.execution_time_ms, res.num_proposals, res.num_bumps,
                   vres.is_stable ? "YES" : "NO");
            verification_result_free(&vres);
            instance_free(inst);
        }
        return 0;
    }

    Instance *inst = NULL;
    if (input_file) {
        printf("Loading problem instance from '%s'...\n", input_file);
        inst = parse_instance_file(input_file);
        if (!inst) {
            fprintf(stderr, "Failed to load instance from '%s'.\n", input_file);
            return 1;
        }
    } else {
        printf("No input file specified. Running hand-built 3-resident / 2-hospital reference instance.\n");
        inst = create_handbuilt_3r_2h_instance();
        if (!inst) {
            fprintf(stderr, "Failed to create built-in instance.\n");
            return 1;
        }
        /* Default to verbose and verify for the reference instance */
        verbose = true;
        verify = true;
    }

    print_instance_summary(inst);

    TraceCollector tc;
    memset(&tc, 0, sizeof(TraceCollector));
    tc.inst = inst;
    tc.verbose = verbose;
    if (json_file) {
        tc.capacity = 64;
        tc.events = (MatchEvent *)malloc((size_t)tc.capacity * sizeof(MatchEvent));
    }

    if (verbose) {
        printf("--- Execution Proposal Trace ---\n");
    }

    MatchResult result = run_matching(inst, (verbose || json_file) ? trace_callback : NULL, &tc);

    if (verbose) {
        printf("--------------------------------\n");
    }

    print_matching_results(inst, &result);

    VerificationResult v_result;
    memset(&v_result, 0, sizeof(VerificationResult));
    int exit_code = 0;

    if (verify) {
        v_result = verify_matching(inst);
        print_verification_report(inst, &v_result);
        if (!v_result.is_stable) {
            exit_code = 2;
        }
    }

    if (json_file) {
        export_json_trace(inst, &tc, &result, verify ? &v_result : NULL, json_file);
    }

    /* Cleanup */
    if (tc.events) free(tc.events);
    if (verify) verification_result_free(&v_result);
    instance_free(inst);

    return exit_code;
}
