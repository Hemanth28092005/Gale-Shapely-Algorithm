#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *string_duplicate(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

typedef struct {
    char **names;
    int count;
    int capacity;
} NameTable;

static NameTable *name_table_create(int capacity) {
    NameTable *nt = (NameTable *)malloc(sizeof(NameTable));
    if (!nt) return NULL;
    nt->capacity = capacity > 0 ? capacity : 16;
    nt->count = 0;
    nt->names = (char **)malloc((size_t)nt->capacity * sizeof(char *));
    return nt;
}

static void name_table_free(NameTable *nt) {
    if (!nt) return;
    for (int i = 0; i < nt->count; i++) {
        free(nt->names[i]);
    }
    free(nt->names);
    free(nt);
}

static int name_table_find(const NameTable *nt, const char *name) {
    for (int i = 0; i < nt->count; i++) {
        if (strcmp(nt->names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int name_table_get_or_add(NameTable *nt, const char *name, int max_limit) {
    int idx = name_table_find(nt, name);
    if (idx != -1) return idx;

    if (nt->count >= max_limit && max_limit > 0) {
        return -1; /* Exceeds declared count */
    }

    if (nt->count >= nt->capacity) {
        int new_cap = nt->capacity * 2;
        char **new_names = (char **)realloc(nt->names, (size_t)new_cap * sizeof(char *));
        if (!new_names) return -1;
        nt->names = new_names;
        nt->capacity = new_cap;
    }

    nt->names[nt->count] = string_duplicate(name);
    return nt->count++;
}

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

Instance *parse_instance_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "Error: Unable to open input file '%s'\n", filepath);
        return NULL;
    }

    char line[4096];
    int line_num = 0;
    int num_residents = -1;
    int num_hospitals = -1;

    /* 1. Parse header: <N> residents, <M> hospitals */
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;

        /* Look for two numbers with resident and hospital keywords */
        char word1[64], word2[64];
        if (sscanf(trimmed, "%d %63[^,], %d %63s", &num_residents, word1, &num_hospitals, word2) == 4 ||
            sscanf(trimmed, "%d residents %d hospitals", &num_residents, &num_hospitals) == 2 ||
            sscanf(trimmed, "%d %d", &num_residents, &num_hospitals) == 2) {
            break;
        } else {
            fprintf(stderr, "Syntax Error on line %d: Expected '<N> residents, <M> hospitals'\n", line_num);
            fclose(fp);
            return NULL;
        }
    }

    if (num_residents < 0 || num_hospitals < 0) {
        fprintf(stderr, "Error: Header with residents and hospitals counts not found in '%s'\n", filepath);
        fclose(fp);
        return NULL;
    }

    NameTable *res_names = name_table_create(num_residents);
    NameTable *hosp_names = name_table_create(num_hospitals);
    Instance *inst = instance_create(num_residents, num_hospitals);

    if (!res_names || !hosp_names || !inst) {
        if (res_names) name_table_free(res_names);
        if (hosp_names) name_table_free(hosp_names);
        if (inst) instance_free(inst);
        fclose(fp);
        return NULL;
    }

    /* Temporary storage for parsed preferences before resolving IDs */
    typedef struct {
        char *entity_name;
        int capacity;
        char *pref_string;
        bool is_hospital;
    } ParsedEntity;

    int max_entries = num_residents + num_hospitals + 64;
    ParsedEntity *entries = (ParsedEntity *)calloc((size_t)max_entries, sizeof(ParsedEntity));
    int entry_count = 0;

    /* Pre-populate standard R0..Rn-1 and H0..Hm-1 names */
    char buf[64];
    for (int i = 0; i < num_residents; i++) {
        snprintf(buf, sizeof(buf), "R%d", i);
        name_table_get_or_add(res_names, buf, num_residents);
    }
    for (int j = 0; j < num_hospitals; j++) {
        snprintf(buf, sizeof(buf), "H%d", j);
        name_table_get_or_add(hosp_names, buf, num_hospitals);
    }

    /* 2. Read all remaining lines */
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;

        char *colon = strchr(trimmed, ':');
        if (!colon) {
            fprintf(stderr, "Syntax Warning on line %d: Missing colon ':'; skipping.\n", line_num);
            continue;
        }

        *colon = '\0';
        char *entity = trim_whitespace(trimmed);
        char *rest = trim_whitespace(colon + 1);

        bool is_hospital = (rest && (strstr(rest, "cap=") || strstr(rest, "cap =")));
        int cap = 1;
        char *prefs_str = rest;

        if (is_hospital) {
            char *cap_pos = strstr(rest, "cap=");
            if (!cap_pos) cap_pos = strstr(rest, "cap =");
            if (cap_pos) {
                cap = atoi(cap_pos + ((cap_pos[3] == '=') ? 4 : 5));
            }

            char *prefs_pos = strstr(rest, "prefs=");
            if (!prefs_pos) prefs_pos = strstr(rest, "prefs =");
            if (!prefs_pos) prefs_pos = strstr(rest, "prefs:");
            if (prefs_pos) {
                prefs_str = prefs_pos + ((prefs_pos[5] == '=') ? 6 : 7);
            } else {
                /* If no prefs keyword, everything after comma or capacity */
                char *comma = strchr(rest, ',');
                if (comma) prefs_str = comma + 1;
            }
        }

        if (entry_count < max_entries) {
            entries[entry_count].entity_name = string_duplicate(entity);
            entries[entry_count].capacity = cap;
            entries[entry_count].pref_string = string_duplicate(prefs_str ? prefs_str : "");
            entries[entry_count].is_hospital = is_hospital;
            entry_count++;
        }
    }

    fclose(fp);

    /* 3. Register any custom names in NameTables */
    for (int e = 0; e < entry_count; e++) {
        const char *name = entries[e].entity_name;
        if (entries[e].is_hospital) {
            name_table_get_or_add(hosp_names, name, num_hospitals);
        } else {
            name_table_get_or_add(res_names, name, num_residents);
        }
    }

    /* 4. Build Instance entities from parsed lines */
    for (int e = 0; e < entry_count; e++) {
        const char *name = entries[e].entity_name;
        if (entries[e].is_hospital) {
            int h_id = name_table_find(hosp_names, name);
            if (h_id < 0 || h_id >= num_hospitals) continue;

            Hospital *h = &inst->hospitals[h_id];
            h->name = string_duplicate(name);
            h->capacity = entries[e].capacity;

            /* Parse preferences */
            char *token = strtok(entries[e].pref_string, " \t\r\n,");
            int cap_list = 8;
            h->pref_list = (int *)malloc((size_t)cap_list * sizeof(int));
            h->pref_count = 0;

            while (token) {
                if (token[0] != '\0') {
                    int r_id = name_table_get_or_add(res_names, token, num_residents);
                    if (r_id >= 0 && r_id < num_residents) {
                        if (h->pref_count >= cap_list) {
                            cap_list *= 2;
                            h->pref_list = (int *)realloc(h->pref_list, (size_t)cap_list * sizeof(int));
                        }
                        h->pref_list[h->pref_count++] = r_id;
                    }
                }
                token = strtok(NULL, " \t\r\n,");
            }
        } else {
            int r_id = name_table_find(res_names, name);
            if (r_id < 0 || r_id >= num_residents) continue;

            Resident *r = &inst->residents[r_id];
            r->name = string_duplicate(name);

            /* Parse preferences */
            char *token = strtok(entries[e].pref_string, " \t\r\n,");
            int cap_list = 8;
            r->pref_list = (int *)malloc((size_t)cap_list * sizeof(int));
            r->pref_count = 0;

            while (token) {
                if (token[0] != '\0') {
                    int h_id = name_table_get_or_add(hosp_names, token, num_hospitals);
                    if (h_id >= 0 && h_id < num_hospitals) {
                        if (r->pref_count >= cap_list) {
                            cap_list *= 2;
                            r->pref_list = (int *)realloc(r->pref_list, (size_t)cap_list * sizeof(int));
                        }
                        r->pref_list[r->pref_count++] = h_id;
                    }
                }
                token = strtok(NULL, " \t\r\n,");
            }
        }
    }

    /* Fill default names for entities not explicitly named in input file */
    for (int i = 0; i < num_residents; i++) {
        if (!inst->residents[i].name) {
            snprintf(buf, sizeof(buf), "R%d", i);
            inst->residents[i].name = string_duplicate(buf);
        }
    }
    for (int j = 0; j < num_hospitals; j++) {
        if (!inst->hospitals[j].name) {
            snprintf(buf, sizeof(buf), "H%d", j);
            inst->hospitals[j].name = string_duplicate(buf);
        }
    }

    /* Build rank tables */
    if (!instance_build_ranks(inst)) {
        fprintf(stderr, "Error: Failed to build preference rank tables (invalid entity IDs).\n");
        instance_free(inst);
        inst = NULL;
    }

    /* Cleanup */
    for (int e = 0; e < entry_count; e++) {
        free(entries[e].entity_name);
        free(entries[e].pref_string);
    }
    free(entries);
    name_table_free(res_names);
    name_table_free(hosp_names);

    return inst;
}

bool save_instance_file(const Instance *inst, const char *filepath) {
    if (!inst || !filepath) return false;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return false;

    fprintf(fp, "# Hospital/Residents Problem Instance\n");
    fprintf(fp, "%d residents, %d hospitals\n\n", inst->num_residents, inst->num_hospitals);

    fprintf(fp, "# Resident Preferences\n");
    for (int i = 0; i < inst->num_residents; i++) {
        const Resident *r = &inst->residents[i];
        fprintf(fp, "%s:", r->name ? r->name : "R");
        for (int k = 0; k < r->pref_count; k++) {
            int hid = r->pref_list[k];
            fprintf(fp, " %s", inst->hospitals[hid].name ? inst->hospitals[hid].name : "H");
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "\n# Hospital Capacities and Preferences\n");
    for (int j = 0; j < inst->num_hospitals; j++) {
        const Hospital *h = &inst->hospitals[j];
        fprintf(fp, "%s: cap=%d, prefs=", h->name ? h->name : "H", h->capacity);
        for (int k = 0; k < h->pref_count; k++) {
            int rid = h->pref_list[k];
            fprintf(fp, "%s%s", inst->residents[rid].name ? inst->residents[rid].name : "R",
                    (k == h->pref_count - 1) ? "" : " ");
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return true;
}
