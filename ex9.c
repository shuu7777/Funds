#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

enum status {
    ST_OK = 0,
    ST_ERR_NULL,
    ST_ERR_BASE,
    ST_ERR_EPS,
    ST_ERR_COUNT,
    ST_ERR_VALUE,
    ST_ERR_PARSE
};

static int is_valid_base(int base)
{
    return (base >= 2 && base <= 36) ? 1 : 0;
}

static int is_valid_eps(double eps)
{
    return (eps > 0.0 && eps < 1.0) ? 1 : 0;
}

static int is_valid_fraction(double x)
{
    return (x > 0.0 && x < 1.0) ? 1 : 0;
}

static int is_finite_in_base(double x, int base, double eps)
{
    int i;
    for (i = 0; i < 100; i++) {
        double y;
        double frac;
        
        y = x * (double)base;
        
        if (y - floor(y) < eps || ceil(y) - y < eps) {
            return 1;
        }
        
        frac = y - floor(y);
        x = frac;
    }
    return 0;
}

int check_fractions(int base, double eps, size_t count, int *out, ...)
{
    va_list ap;
    size_t i;

    if (out == NULL) return ST_ERR_NULL;
    if (!is_valid_base(base)) return ST_ERR_BASE;
    if (!is_valid_eps(eps)) return ST_ERR_EPS;
    if (count == 0) return ST_ERR_COUNT;

    va_start(ap, out);
    for (i = 0; i < count; i++) {
        double x = va_arg(ap, double);
        if (!is_valid_fraction(x)) {
            va_end(ap);
            return ST_ERR_VALUE;
        }
        out[i] = is_finite_in_base(x, base, eps);
    }
    va_end(ap);

    return ST_OK;
}

static int parse_int(const char *s, int *out)
{
    long v;
    char *end = NULL;
    const char *p;

    if (s == NULL || out == NULL || *s == '\0') return 0;
    if (s[0] == '-' || s[0] == '+') return 0;

    for (p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }

    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (v < 0 || v > 2147483647L) return 0;

    *out = (int)v;
    return 1;
}

static int parse_double(const char *s, double *out)
{
    double v;
    char *end = NULL;
    const char *p;
    int dot = 0;
    int digits = 0;

    if (s == NULL || out == NULL || *s == '\0') return 0;
    if (s[0] == '-') return 0;

    p = s;
    while (*p) {
        if (*p == '.') {
            if (dot) return 0;
            dot = 1;
        } else if (isdigit((unsigned char)*p)) {
            digits++;
        } else {
            return 0;
        }
        p++;
    }
    if (digits == 0) return 0;

    v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;
    
    *out = v;
    return 1;
}

static void print_error(int rc)
{
    switch (rc) {
    case ST_ERR_NULL:  fprintf(stderr, "Error: null arg\n"); break;
    case ST_ERR_BASE:  fprintf(stderr, "Error: base must be in [2;36]\n"); break;
    case ST_ERR_EPS:   fprintf(stderr, "Error: eps must be in (0;1)\n"); break;
    case ST_ERR_COUNT: fprintf(stderr, "Error: count must be > 0\n"); break;
    case ST_ERR_VALUE: fprintf(stderr, "Error: value out of (0;1)\n"); break;
    case ST_ERR_PARSE: fprintf(stderr, "Error: bad numeric format\n"); break;
    default:           fprintf(stderr, "Error: unknown\n"); break;
    }
}

int main(int argc, char **argv)
{
    int base;
    double eps;
    size_t count;
    int *results;
    int rc;
    size_t i;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <base> <eps> <count> <f1> [f2 ... fN]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!parse_int(argv[1], &base)) {
        print_error(ST_ERR_PARSE);
        return EXIT_FAILURE;
    }
    if (!parse_double(argv[2], &eps)) {
        print_error(ST_ERR_PARSE);
        return EXIT_FAILURE;
    }
    
    {
        int c;
        if (!parse_int(argv[3], &c) || c <= 0) {
            print_error(ST_ERR_COUNT);
            return EXIT_FAILURE;
        }
        count = (size_t)c;
    }

    if ((size_t)argc != 4 + count) {
        fprintf(stderr, "Error: expected %zu fractions, got %d\n",
                count, argc - 4);
        return EXIT_FAILURE;
    }

    for (i = 0; i < count; i++) {
        double v;
        if (!parse_double(argv[4 + i], &v)) {
            fprintf(stderr, "Error: bad fraction format: %s\n", argv[4 + i]);
            return EXIT_FAILURE;
        }
    }

    results = (int *)malloc(count * sizeof(int));
    if (results == NULL) {
        fprintf(stderr, "Error: alloc\n");
        return EXIT_FAILURE;
    }

    {
        double *vals = (double *)malloc(count * sizeof(double));
        if (vals == NULL) {
            free(results);
            fprintf(stderr, "Error: alloc\n");
            return EXIT_FAILURE;
        }
        for (i = 0; i < count; i++) {
            parse_double(argv[4 + i], &vals[i]);
        }

        if (count == 1) {
            rc = check_fractions(base, eps, 1, results, vals[0]);
        } else if (count == 2) {
            rc = check_fractions(base, eps, 2, results, vals[0], vals[1]);
        } else if (count == 3) {
            rc = check_fractions(base, eps, 3, results, vals[0], vals[1], vals[2]);
        } else if (count == 4) {
            rc = check_fractions(base, eps, 4, results, vals[0], vals[1], vals[2], vals[3]);
        } else if (count == 5) {
            rc = check_fractions(base, eps, 5, results, vals[0], vals[1], vals[2], vals[3], vals[4]);
        } else {
            fprintf(stderr, "Error: demo supports up to 5 fractions\n");
            free(vals);
            free(results);
            return EXIT_FAILURE;
        }

        free(vals);
    }

    if (rc != ST_OK) {
        print_error(rc);
        free(results);
        return EXIT_FAILURE;
    }

    for (i = 0; i < count; i++) {
        printf("%zu: %s\n", i + 1, results[i] ? "finite" : "infinite");
    }

    free(results);
    return EXIT_SUCCESS;
}