#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

enum status {
    ST_OK = 0,
    ST_ERR_NULL,
    ST_ERR_MEM,
    ST_ERR_OPEN,
    ST_ERR_READ,
    ST_ERR_EMPTY
};

typedef struct {
    size_t ln;
    size_t col;
} Pos;

typedef struct {
    char *path;
    Pos *arr;
    size_t cnt;
    size_t cap;
} FMatch;

typedef struct {
    FMatch *files;
    size_t fcnt;
    size_t total;
} Result;

static size_t str_len(const char *s)
{
    size_t n = 0;
    if (s == NULL) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static size_t str_find(const char *h, size_t hlen, const char *n, size_t nlen, size_t from)
{
    size_t i, j;
    if (nlen == 0 || nlen > hlen || from > hlen - nlen) return (size_t)-1;
    for (i = from; i + nlen <= hlen; i++) {
        for (j = 0; j < nlen && h[i + j] == n[j]; j++) { }
        if (j == nlen) return i;
    }
    return (size_t)-1;
}

static void res_free(Result *res)
{
    size_t i;
    if (res == NULL) return;
    for (i = 0; i < res->fcnt; i++) {
        free(res->files[i].path);
        free(res->files[i].arr);
    }
    free(res->files);
    res->files = NULL;
    res->fcnt = 0;
    res->total = 0;
}

static int push_match(FMatch *fm, size_t ln, size_t col)
{
    if (fm->cnt == fm->cap) {
        size_t ncap = (fm->cap == 0) ? 8 : fm->cap * 2;
        Pos *tmp = (Pos *)realloc(fm->arr, ncap * sizeof(Pos));
        if (tmp == NULL) return ST_ERR_MEM;
        fm->arr = tmp;
        fm->cap = ncap;
    }
    fm->arr[fm->cnt].ln = ln;
    fm->arr[fm->cnt].col = col;
    fm->cnt++;
    return ST_OK;
}

static int read_line(FILE *fp, char **buf, size_t *cap, size_t *out_len)
{
    size_t len = 0;
    int ch;

    if (buf == NULL || cap == NULL || out_len == NULL) return -1;
    if (*buf == NULL) {
        *cap = 128;
        *buf = (char *)malloc(*cap);
        if (*buf == NULL) return -1;
    }

    while ((ch = fgetc(fp)) != EOF) {
        if (len + 1 >= *cap) {
            size_t ncap = *cap * 2;
            char *tmp = (char *)realloc(*buf, ncap);
            if (tmp == NULL) return -1;
            *buf = tmp;
            *cap = ncap;
        }
        if (ch == '\n') {
            (*buf)[len] = '\0';
            *out_len = len;
            return 1;
        }
        (*buf)[len++] = (char)ch;
    }

    if (len == 0) return 0;
    (*buf)[len] = '\0';
    *out_len = len;
    return 1;
}

static int scan_file(const char *path, const char *needle, FMatch *fm)
{
    FILE *fp;
    char *line = NULL;
    size_t cap = 0;
    size_t len = 0;
    size_t lnum = 0;
    size_t nlen = str_len(needle);
    int r;

    fp = fopen(path, "r");
    if (fp == NULL) return ST_ERR_OPEN;

    while ((r = read_line(fp, &line, &cap, &len)) == 1) {
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        lnum++;
        {
            size_t from = 0;
            while (1) {
                size_t pos = str_find(line, len, needle, nlen, from);
                int rc;
                if (pos == (size_t)-1) break;
                rc = push_match(fm, lnum, pos + 1);
                if (rc != ST_OK) {
                    free(line);
                    fclose(fp);
                    return rc;
                }
                from = pos + 1;
            }
        }
    }

    free(line);
    if (r < 0 || ferror(fp)) {
        fclose(fp);
        return ST_ERR_READ;
    }
    fclose(fp);
    return ST_OK;
}

int find_in_files(const char *needle, size_t fcnt, Result *out, ...)
{
    va_list ap;
    Result res;
    size_t i;
    int rc;

    if (needle == NULL || out == NULL) return ST_ERR_NULL;
    if (str_len(needle) == 0) return ST_ERR_EMPTY;
    if (fcnt == 0) {
        out->files = NULL;
        out->fcnt = 0;
        out->total = 0;
        return ST_OK;
    }

    res.files = (FMatch *)calloc(fcnt, sizeof(FMatch));
    if (res.files == NULL) return ST_ERR_MEM;
    res.fcnt = 0;
    res.total = 0;

    va_start(ap, out);
    for (i = 0; i < fcnt; i++) {
        const char *path = va_arg(ap, const char *);
        size_t plen;
        char *copy;
        FMatch *fm;

        if (path == NULL) {
            va_end(ap);
            res_free(&res);
            return ST_ERR_NULL;
        }

        plen = str_len(path);
        copy = (char *)malloc(plen + 1);
        if (copy == NULL) {
            va_end(ap);
            res_free(&res);
            return ST_ERR_MEM;
        }
        for (size_t t = 0; t <= plen; t++) copy[t] = path[t];

        fm = &res.files[res.fcnt];
        fm->path = copy;
        fm->arr = NULL;
        fm->cnt = 0;
        fm->cap = 0;

        rc = scan_file(copy, needle, fm);
        if (rc != ST_OK) {
            free(copy);
            fm->path = NULL;
            free(fm->arr); /* Исправление бага: чистим частично выделенный массив */
            fm->arr = NULL;
            va_end(ap);
            res_free(&res);
            return rc;
        }

        res.total += fm->cnt;
        res.fcnt++;
    }
    va_end(ap);

    *out = res;
    return ST_OK;
}

static void print_res(const Result *res)
{
    size_t i, j;
    for (i = 0; i < res->fcnt; i++) {
        const FMatch *fm = &res->files[i];
        if (fm->cnt == 0) {
            printf("%s: not found\n", fm->path);
        } else {
            printf("%s: %zu match(es)\n", fm->path, fm->cnt);
            for (j = 0; j < fm->cnt; j++) {
                printf("  line %zu, column %zu\n", fm->arr[j].ln, fm->arr[j].col);
            }
        }
    }
    printf("TOTAL: %zu\n", res->total);
}

int main(int argc, char **argv)
{
    Result res;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <needle> <count> <file1> [file2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    {
        const char *needle = argv[1];
        unsigned long fc_ul;
        size_t fc;
        char *end = NULL;

        for (const char *p = argv[2]; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                fprintf(stderr, "Error: bad count\n");
                return EXIT_FAILURE;
            }
        }

        fc_ul = strtoul(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0') {
            fprintf(stderr, "Error: bad count\n");
            return EXIT_FAILURE;
        }
        fc = (size_t)fc_ul;

        if (argc != 3 + (int)fc) {
            fprintf(stderr, "Error: expected %lu paths, got %d\n", fc_ul, argc - 3);
            return EXIT_FAILURE;
        }

        if (fc == 1) {
            rc = find_in_files(needle, 1, &res, argv[3]);
        } else if (fc == 2) {
            rc = find_in_files(needle, 2, &res, argv[3], argv[4]);
        } else if (fc == 3) {
            rc = find_in_files(needle, 3, &res, argv[3], argv[4], argv[5]);
        } else if (fc == 4) {
            rc = find_in_files(needle, 4, &res, argv[3], argv[4], argv[5], argv[6]);
        } else if (fc == 5) {
            rc = find_in_files(needle, 5, &res, argv[3], argv[4], argv[5], argv[6], argv[7]);
        } else {
            fprintf(stderr, "Demo supports up to 5 files\n");
            return EXIT_FAILURE;
        }

        if (rc != ST_OK) {
            switch (rc) {
            case ST_ERR_NULL:  fprintf(stderr, "Error: null arg\n"); break;
            case ST_ERR_MEM:   fprintf(stderr, "Error: alloc\n"); break;
            case ST_ERR_OPEN:  fprintf(stderr, "Error: cannot open file\n"); break;
            case ST_ERR_READ:  fprintf(stderr, "Error: read file\n"); break;
            case ST_ERR_EMPTY: fprintf(stderr, "Error: empty needle\n"); break;
            default:           fprintf(stderr, "Error: unknown\n"); break;
            }
            return EXIT_FAILURE;
        }

        print_res(&res);
        res_free(&res);
    }
    return EXIT_SUCCESS;
}