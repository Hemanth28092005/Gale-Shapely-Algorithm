#ifndef PARSER_H
#define PARSER_H

#include "matching.h"

/* Parses an input instance file into a dynamically allocated Instance.
 * Supports format:
 *   <N> residents, <M> hospitals
 *   R0: H1 H0 H2
 *   ...
 *   H0: cap=2, prefs=R1 R0 R2 R3 R4
 *   ...
 * Returns pointer to Instance on success, NULL on syntax/semantic error.
 */
Instance *parse_instance_file(const char *filepath);

/* Saves an Instance to a file in the standard plain-text format */
bool save_instance_file(const Instance *inst, const char *filepath);

#endif /* PARSER_H */
