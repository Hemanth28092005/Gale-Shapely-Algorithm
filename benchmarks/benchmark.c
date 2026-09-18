#include "../src/matching.h"
#include "../src/verifier.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static double get_hires_time_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return ((double)counter.QuadPart / (double)freq.QuadPart) * 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((double)ts.tv_sec * 1000.0) + ((double)ts.tv_nsec * 1e-6);
#endif
}

/* Fisher-Yates shuffle helper */
static void shuffle(int *array, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

static Instance *generate_random_instance(int n, int m, int r_pref_len, int h_pref_len, int avg_cap) {
    Instance *inst = instance_create(n, m);
    if (!inst) return NULL;

    int *all_hospitals = (int *)malloc((size_t)m * sizeof(int));
    for (int j = 0; j < m; j++) all_hospitals[j] = j;

    for (int i = 0; i < n; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "R%d", i);
        size_t len = strlen(buf);
        inst->residents[i].name = (char *)malloc(len + 1);
        memcpy(inst->residents[i].name, buf, len + 1);

        int k = (r_pref_len <= m) ? r_pref_len : m;
        inst->residents[i].pref_count = k;
        inst->residents[i].pref_list = (int *)malloc((size_t)k * sizeof(int));

        shuffle(all_hospitals, m);
        for (int p = 0; p < k; p++) {
            inst->residents[i].pref_list[p] = all_hospitals[p];
        }
    }
    free(all_hospitals);

    int *all_residents = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) all_residents[i] = i;

    for (int j = 0; j < m; j++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "H%d", j);
        size_t len = strlen(buf);
        inst->hospitals[j].name = (char *)malloc(len + 1);
        memcpy(inst->hospitals[j].name, buf, len + 1);

        inst->hospitals[j].capacity = avg_cap + (rand() % 3 - 1);
        if (inst->hospitals[j].capacity < 1) inst->hospitals[j].capacity = 1;

        int k = (h_pref_len <= n) ? h_pref_len : n;
        inst->hospitals[j].pref_count = k;
        inst->hospitals[j].pref_list = (int *)malloc((size_t)k * sizeof(int));

        shuffle(all_residents, n);
        for (int p = 0; p < k; p++) {
            inst->hospitals[j].pref_list[p] = all_residents[p];
        }
    }
    free(all_residents);

    instance_build_ranks(inst);
    return inst;
}

int main(int argc, char **argv) {
    srand(42); /* Fixed seed for reproducibility */

    const char *csv_n_path = "benchmarks/benchmark_n.csv";
    const char *csv_l_path = "benchmarks/benchmark_l.csv";

    if (argc >= 2) csv_n_path = argv[1];
    if (argc >= 3) csv_l_path = argv[2];

    printf("========================================================\n");
    printf("     HOSPITAL/RESIDENTS BENCHMARK EXPERIMENT SUITE      \n");
    printf("========================================================\n\n");

    /* Experiment 1: Runtime vs Market Size N (Residents) */
    printf("--> Experiment 1: Scaling with Market Size N (M = N / 2)\n");
    FILE *fp_n = fopen(csv_n_path, "w");
    if (!fp_n) {
        fprintf(stderr, "Error opening %s\n", csv_n_path);
        return 1;
    }
    fprintf(fp_n, "N,M,Trials,AvgTimeMs,AvgProposals,AvgBumps\n");

    int test_sizes[] = {50, 100, 200, 500, 1000, 2000, 3500, 5000};
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

    for (int idx = 0; idx < num_sizes; idx++) {
        int n = test_sizes[idx];
        int m = n / 2;
        int trials = (n >= 2000) ? 3 : 5;
        int r_pref = (m < 25) ? m : 25;
        int h_pref = (n < 50) ? n : 50;
        int avg_cap = (n / m) + 1;

        double total_time = 0.0;
        long long total_props = 0;
        long long total_bumps = 0;

        for (int t = 0; t < trials; t++) {
            Instance *inst = generate_random_instance(n, m, r_pref, h_pref, avg_cap);
            double t0 = get_hires_time_ms();
            MatchResult res = run_matching(inst, NULL, NULL);
            double t1 = get_hires_time_ms();

            total_time += (t1 - t0);
            total_props += res.num_proposals;
            total_bumps += res.num_bumps;

            instance_free(inst);
        }

        double avg_time = total_time / trials;
        double avg_props = (double)total_props / trials;
        double avg_bumps = (double)total_bumps / trials;

        printf("  N=%5d | M=%4d | Time: %8.3f ms | Proposals: %7.1f | Bumps: %6.1f\n",
               n, m, avg_time, avg_props, avg_bumps);
        fprintf(fp_n, "%d,%d,%d,%.4f,%.1f,%.1f\n", n, m, trials, avg_time, avg_props, avg_bumps);
    }
    fclose(fp_n);
    printf("Saved Experiment 1 data to '%s'\n\n", csv_n_path);

    /* Experiment 2: Runtime vs Average Preference List Length L */
    printf("--> Experiment 2: Scaling with Resident Preference Length L (N=2000, M=500)\n");
    FILE *fp_l = fopen(csv_l_path, "w");
    if (!fp_l) {
        fprintf(stderr, "Error opening %s\n", csv_l_path);
        return 1;
    }
    fprintf(fp_l, "L,N,M,Trials,AvgTimeMs,AvgProposals,AvgBumps\n");

    int test_l[] = {2, 5, 10, 20, 40, 80, 150, 300};
    int num_l = sizeof(test_l) / sizeof(test_l[0]);
    int fix_n = 2000;
    int fix_m = 500;
    int fix_cap = 5;
    int fix_h_pref = 150;
    int trials_l = 3;

    for (int idx = 0; idx < num_l; idx++) {
        int l = test_l[idx];
        double total_time = 0.0;
        long long total_props = 0;
        long long total_bumps = 0;

        for (int t = 0; t < trials_l; t++) {
            Instance *inst = generate_random_instance(fix_n, fix_m, l, fix_h_pref, fix_cap);
            double t0 = get_hires_time_ms();
            MatchResult res = run_matching(inst, NULL, NULL);
            double t1 = get_hires_time_ms();

            total_time += (t1 - t0);
            total_props += res.num_proposals;
            total_bumps += res.num_bumps;

            instance_free(inst);
        }

        double avg_time = total_time / trials_l;
        double avg_props = (double)total_props / trials_l;
        double avg_bumps = (double)total_bumps / trials_l;

        printf("  L=%3d | N=%4d, M=%3d | Time: %8.3f ms | Proposals: %7.1f | Bumps: %6.1f\n",
               l, fix_n, fix_m, avg_time, avg_props, avg_bumps);
        fprintf(fp_l, "%d,%d,%d,%d,%.4f,%.1f,%.1f\n",
                l, fix_n, fix_m, trials_l, avg_time, avg_props, avg_bumps);
    }
    fclose(fp_l);
    printf("Saved Experiment 2 data to '%s'\n\n", csv_l_path);

    printf("========================================================\n");
    printf("Benchmark suite completed successfully.\n");
    printf("========================================================\n");

    return 0;
}
