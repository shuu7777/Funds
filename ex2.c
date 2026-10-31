#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    unsigned int id;
    char *name;
    char *surname;
    double salary;
} Employee;

enum status {
    ST_OK = 0,
    ST_ERR_ARGS,
    ST_ERR_OPEN_IN,
    ST_ERR_OPEN_OUT,
    ST_ERR_ALLOC,
    ST_ERR_PARSE,
    ST_ERR_EMPTY,
    ST_ERR_WRITE
};

static int is_latin_word(const char *s)
{
    if (s == NULL || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            return 0;
    }
    return 1;
}

static int parse_uint(const char *s, unsigned int *out)
{
    unsigned long v;
    char *end = NULL;
    size_t len = 0;
    
    if (s == NULL || *s == '\0' || out == NULL) return 0;
    
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
        len++;
    }
    
    if (len > 10) return 0; 
    
    v = strtoul(s, &end, 10);
    if (end == s || *end != '\0') return 0;
    if (v > 0xFFFFFFFFUL) return 0;
    
    *out = (unsigned int)v;
    return 1;
}

static int parse_salary(const char *s, double *out)
{
    double val = 0.0;
    double fraction = 0.1;
    int has_dot = 0;
    int digits = 0;
    
    if (s == NULL || *s == '\0' || out == NULL) return 0;
    if (s[0] == '-') return 0;
    
    for (const char *p = s; *p; p++) {
        if (*p == '.') {
            if (has_dot) return 0;
            has_dot = 1;
        } else if (isdigit((unsigned char)*p)) {
            if (!has_dot) {
                val = val * 10.0 + (*p - '0');
            } else {
                val += (*p - '0') * fraction;
                fraction *= 0.1;
            }
            digits++;
        } else {
            return 0;
        }
    }
    
    if (digits == 0) return 0;
    *out = val;
    return 1;
}

static void employees_free(Employee *arr, size_t n)
{
    size_t i;
    if (arr == NULL) return;
    for (i = 0; i < n; i++) {
        free(arr[i].name);
        free(arr[i].surname);
    }
    free(arr);
}

static int split_ws(char *line, char *tokens[4], int *out_cnt)
{
    int cnt = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (cnt >= 4) return ST_ERR_PARSE;
        tokens[cnt++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = '\0'; p++; }
    }
    *out_cnt = cnt;
    return ST_OK;
}

static int cmp_asc(const void *pa, const void *pb)
{
    const Employee *a = (const Employee *)pa;
    const Employee *b = (const Employee *)pb;
    int c;

    if (a->salary < b->salary) return -1;
    if (a->salary > b->salary) return  1;

    c = strcmp(a->surname, b->surname);
    if (c != 0) return c;

    c = strcmp(a->name, b->name);
    if (c != 0) return c;

    if (a->id < b->id) return -1;
    if (a->id > b->id) return  1;
    return 0;
}

static int cmp_desc(const void *pa, const void *pb)
{
    const Employee *a = (const Employee *)pa;
    const Employee *b = (const Employee *)pb;
    int c;

    if (a->salary > b->salary) return -1;
    if (a->salary < b->salary) return  1;

    c = strcmp(a->surname, b->surname);
    if (c != 0) return -c;

    c = strcmp(a->name, b->name);
    if (c != 0) return -c;

    if (a->id > b->id) return -1;
    if (a->id < b->id) return  1;
    return 0;
}

static int read_next_line(FILE *fp, char *buf, size_t bufsz)
{
    while (fgets(buf, (int)bufsz, fp) != NULL) {
        size_t len = strlen(buf);
        if (len + 1 == bufsz && buf[len - 1] != '\n') {
            int ch;
            while ((ch = fgetc(fp)) != EOF && ch != '\n') { }
            return -1;
        }
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        {
            size_t i = 0;
            while (buf[i] == ' ' || buf[i] == '\t') i++;
            if (buf[i] == '\0') continue;
        }
        return 1;
    }
    if (ferror(fp)) return -1;
    return 0;
}

static int parse_employee(const char *line, Employee *emp)
{
    char buf[1024];
    char *tokens[4];
    int cnt = 0;
    size_t len = strlen(line);
    unsigned int id;
    double salary;

    if (len + 1 > sizeof(buf)) return ST_ERR_PARSE;
    memcpy(buf, line, len + 1);

    if (split_ws(buf, tokens, &cnt) != ST_OK) return ST_ERR_PARSE;
    if (cnt != 4) return ST_ERR_PARSE;

    if (!parse_uint(tokens[0], &id))         return ST_ERR_PARSE;
    if (!is_latin_word(tokens[1]))           return ST_ERR_PARSE;
    if (!is_latin_word(tokens[2]))           return ST_ERR_PARSE;
    if (!parse_salary(tokens[3], &salary))   return ST_ERR_PARSE;

    emp->id = id;
    emp->salary = salary;
    emp->name = (char *)malloc(strlen(tokens[1]) + 1);
    emp->surname = (char *)malloc(strlen(tokens[2]) + 1);
    if (emp->name == NULL || emp->surname == NULL) {
        free(emp->name);
        free(emp->surname);
        emp->name = emp->surname = NULL;
        return ST_ERR_ALLOC;
    }
    strcpy(emp->name, tokens[1]);
    strcpy(emp->surname, tokens[2]);
    return ST_OK;
}

static int write_employees(FILE *fp, const Employee *arr, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (fprintf(fp, "%u %s %s %.10g\n",
                    arr[i].id, arr[i].name, arr[i].surname, arr[i].salary) < 0)
            return ST_ERR_WRITE;
    }
    return ST_OK;
}

int main(int argc, char **argv)
{
    const char *in_path, *flag, *out_path;
    int asc;
    FILE *fin = NULL, *fout = NULL;
    Employee *arr = NULL;
    size_t n = 0, cap = 0;
    enum status rc;
    int exit_code = EXIT_SUCCESS;
    char line[1024];
    int r;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <in> <flag: -a|-d|a|d> <out>\n", argv[0]);
        return EXIT_FAILURE;
    }
    in_path = argv[1];
    flag = argv[2];
    out_path = argv[3];

    if ((flag[0] != '-' && flag[0] != '/') ||
        (flag[1] != 'a' && flag[1] != 'd') ||
        flag[2] != '\0') {
        fprintf(stderr, "Error: bad flag (expected -a|-d|/a|/d)\n");
        return EXIT_FAILURE;
    }
    asc = (flag[1] == 'a');

    fin = fopen(in_path, "r");
    if (fin == NULL) {
        fprintf(stderr, "Error: cannot open input file '%s'\n", in_path);
        return EXIT_FAILURE;
    }

    while ((r = read_next_line(fin, line, sizeof(line))) == 1) {
        Employee emp;
        rc = parse_employee(line, &emp);
        if (rc == ST_ERR_ALLOC) {
            fprintf(stderr, "Error: allocation failed\n");
            employees_free(arr, n);
            fclose(fin);
            return EXIT_FAILURE;
        }
        if (rc != ST_OK) {
            fprintf(stderr, "Error: bad line in input: %s\n", line);
            employees_free(arr, n);
            fclose(fin);
            return EXIT_FAILURE;
        }
        if (n == cap) {
            size_t newcap = (cap == 0) ? 8 : cap * 2;
            Employee *tmp = (Employee *)realloc(arr, newcap * sizeof(Employee));
            if (tmp == NULL) {
                free(emp.name);
                free(emp.surname);
                fprintf(stderr, "Error: allocation failed\n");
                employees_free(arr, n);
                fclose(fin);
                return EXIT_FAILURE;
            }
            arr = tmp;
            cap = newcap;
        }
        arr[n++] = emp;
    }
    if (r < 0) {
        fprintf(stderr, "Error: read error or line too long\n");
        employees_free(arr, n);
        fclose(fin);
        return EXIT_FAILURE;
    }
    fclose(fin);
    fin = NULL;

    if (n == 0) {
        fprintf(stderr, "Error: input file is empty\n");
        employees_free(arr, n);
        return EXIT_FAILURE;
    }

    qsort(arr, n, sizeof(Employee), asc ? cmp_asc : cmp_desc);

    fout = fopen(out_path, "w");
    if (fout == NULL) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", out_path);
        employees_free(arr, n);
        return EXIT_FAILURE;
    }
    if (write_employees(fout, arr, n) != ST_OK) {
        fprintf(stderr, "Error: write failed\n");
        exit_code = EXIT_FAILURE;
    }
    fclose(fout);

    employees_free(arr, n);
    return exit_code;
}