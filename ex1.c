#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

enum status {
    ST_OK = 0,
    ST_ERR_NULL_ARG,
    ST_ERR_ALLOC,
    ST_ERR_NO_STRINGS,
    ST_ERR_UNKNOWN_FLAG
};

static size_t newstrlen(const char *s)
{
    size_t n = 0;
    if (s == NULL) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0') return 0;
    for (const char *p = s; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

static int parse_uint(const char *s, unsigned int *out)
{
    unsigned long v;
    if (s == NULL || out == NULL) return 0;
    if (!is_all_digits(s)) return 0;
    v = strtoul(s, NULL, 10);
    if (v > 0xFFFFFFFFUL) return 0;
    *out = (unsigned int)v;
    return 1;
}

static int l_flag(const char *src, size_t *out_len)
{
    if (src == NULL || out_len == NULL) return ST_ERR_NULL_ARG;
    *out_len = newstrlen(src);
    return ST_OK;
}

static int r_flag(const char *src, char **out)
{
    size_t n, i;
    char *r;
    if (src == NULL || out == NULL) return ST_ERR_NULL_ARG;
    n = newstrlen(src);
    r = (char *)malloc(n + 1);
    if (r == NULL) return ST_ERR_ALLOC;
    for (i = 0; i < n; i++) r[i] = src[n - 1 - i];
    r[n] = '\0';
    *out = r;
    return ST_OK;
}

static int u_flag(const char *src, char **out)
{
    size_t n, i;
    char *r;
    if (src == NULL || out == NULL) return ST_ERR_NULL_ARG;
    n = newstrlen(src);
    r = (char *)malloc(n + 1);
    if (r == NULL) return ST_ERR_ALLOC;
    for (i = 0; i < n; i++) {
        if (i % 2 == 1) r[i] = (char)toupper((unsigned char)src[i]);
        else            r[i] = src[i];
    }
    r[n] = '\0';
    *out = r;
    return ST_OK;
}

static int n_flag(const char *src, char **out)
{
    size_t n, i, k = 0;
    char *r;
    if (src == NULL || out == NULL) return ST_ERR_NULL_ARG;
    n = newstrlen(src);
    r = (char *)malloc(n + 1);
    if (r == NULL) return ST_ERR_ALLOC;
    for (i = 0; i < n; i++)
        if (isdigit((unsigned char)src[i])) r[k++] = src[i];
    for (i = 0; i < n; i++)
        if (isalpha((unsigned char)src[i])) r[k++] = src[i];
    for (i = 0; i < n; i++)
        if (!isdigit((unsigned char)src[i]) && !isalpha((unsigned char)src[i]))
            r[k++] = src[i];
    r[k] = '\0';
    *out = r;
    return ST_OK;
}

static int c_flag(const char *const *srcs, size_t cnt,
                  unsigned int seed, char **out)
{
    size_t total = 0, i, k = 0, t;
    char *r;
    size_t *idx;

    if (srcs == NULL || out == NULL) return ST_ERR_NULL_ARG;
    if (cnt == 0) return ST_ERR_NO_STRINGS;

    for (i = 0; i < cnt; i++) {
        if (srcs[i] == NULL) return ST_ERR_NULL_ARG;
        total += newstrlen(srcs[i]);
    }

    r = (char *)malloc(total + 1);
    if (r == NULL) return ST_ERR_ALLOC;

    idx = (size_t *)malloc(cnt * sizeof(size_t));
    if (idx == NULL) { free(r); return ST_ERR_ALLOC; }

    for (i = 0; i < cnt; i++) idx[i] = i;

    srand(seed);

    for (i = cnt; i > 1; i--) {
        size_t j = (size_t)((unsigned)rand() % (unsigned)i);
        size_t tmp = idx[i - 1];
        idx[i - 1] = idx[j];
        idx[j] = tmp;
    }

    for (i = 0; i < cnt; i++) {
        const char *s = srcs[idx[i]];
        size_t len = newstrlen(s);
        for (t = 0; t < len; t++) r[k++] = s[t];
    }
    r[k] = '\0';
    free(idx);
    *out = r;
    return ST_OK;
}

static void print_status_error(enum status st)
{
    switch (st) {
    case ST_OK:               break;
    case ST_ERR_NULL_ARG:     fprintf(stderr, "Error: null argument\n");       break;
    case ST_ERR_ALLOC:        fprintf(stderr, "Error: allocation failed\n");   break;
    case ST_ERR_NO_STRINGS:   fprintf(stderr, "Error: no strings for -c\n");   break;
    case ST_ERR_UNKNOWN_FLAG: fprintf(stderr, "Error: unknown flag\n");        break;
    default:                  fprintf(stderr, "Error: unknown status\n");      break;
    }
}

int main(int argc, char **argv)
{
    const char *flag;
    const char *arg2;
    char *result = NULL;
    size_t len = 0;
    enum status rc = ST_OK;
    int exit_code = EXIT_SUCCESS;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <flag> <string> [seed strings...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    flag = argv[1];
    arg2 = argv[2];

    if (flag[0] != '-' || flag[1] == '\0' || flag[2] != '\0') {
        fprintf(stderr, "Error: bad flag format (expected -X)\n");
        return EXIT_FAILURE;
    }

    switch (flag[1]) {
    case 'l':
        if (argc != 3) {
            fprintf(stderr, "Error: -l expects exactly 2 arguments\n");
            return EXIT_FAILURE;
        }
        rc = l_flag(arg2, &len);
        if (rc == ST_OK) printf("length = %zu\n", len);
        break;

    case 'r':
        if (argc != 3) {
            fprintf(stderr, "Error: -r expects exactly 2 arguments\n");
            return EXIT_FAILURE;
        }
        rc = r_flag(arg2, &result);
        if (rc == ST_OK) printf("%s\n", result);
        break;

    case 'u':
        if (argc != 3) {
            fprintf(stderr, "Error: -u expects exactly 2 arguments\n");
            return EXIT_FAILURE;
        }
        rc = u_flag(arg2, &result);
        if (rc == ST_OK) printf("%s\n", result);
        break;

    case 'n':
        if (argc != 3) {
            fprintf(stderr, "Error: -n expects exactly 2 arguments\n");
            return EXIT_FAILURE;
        }
        rc = n_flag(arg2, &result);
        if (rc == ST_OK) printf("%s\n", result);
        break;

    case 'c': {
        unsigned int seed;
        size_t cnt, i;
        const char **srcs;

        if (argc < 6) {
            fprintf(stderr,
                    "Error: -c usage: %s -c <string> <seed> <s1> <s2> ...\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
        if (!parse_uint(argv[3], &seed)) {
            fprintf(stderr, "Error: bad seed (expected unsigned int)\n");
            return EXIT_FAILURE;
        }

        cnt = (size_t)(argc - 4) + 1;
        srcs = (const char **)malloc(cnt * sizeof(char *));
        if (srcs == NULL) {
            fprintf(stderr, "Error: allocation failed\n");
            return EXIT_FAILURE;
        }
        srcs[0] = arg2;
        for (i = 1; i < cnt; i++) {
            srcs[i] = argv[i + 3];
        }

        rc = c_flag(srcs, cnt, seed, &result);
        free(srcs);
        if (rc == ST_OK) printf("%s\n", result);
        break;
    }

    default:
        rc = ST_ERR_UNKNOWN_FLAG;
        break;
    }

    switch (rc) {
    case ST_OK:
        break;
    default:
        print_status_error(rc);
        exit_code = EXIT_FAILURE;
        break;
    }

    free(result);
    return exit_code;
}