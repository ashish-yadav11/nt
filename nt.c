/* compilation:

gcc -o nt -O3 -Wall -Wextra nt.c

*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define USAGE \
        "Usage:\n" \
        "	nt -h|--help\n" \
        "	nt <time-specification> <notification>\n" \
        "\n" \
        "	time-specification:\n" \
        "		relative - [H,]M[.S] or [Hh][Mm][Ss]\n" \
        "		absolute - [HH]:[MM]\n" \
        "Examples:\n" \
        "	nt 10 '10 minutes up'\n" \
        "	nt 30s '30 seconds up'\n" \
        "	nt 1, '1 hour up'\n" \
        "	nt 5.30 '5 minutes 30 seconds up'\n" \
        "	nt .10 '10 seconds up'\n" \
        "	nt 1,10 '1 hour 10 minutes up'\n" \
        "	nt 11:15 '11:15 up'\n" \
        "	nt 1: '01:00 up'\n" \
        "	nt :5 '00:05 up'"

#define ISDIGIT(X)                      (X >= '0' && X <= '9')

enum { None, Period, Comma, Second, Minute, Hour };

/* the following function assumes n >= 1 */
char *
catarray(int n, char *array[])
{
        char *c;
        char *buf;
        int t = 0;
        int l[n];

        for (int i = 0; i < n; i++)
                t += l[i] = strlen(array[i]);
        c = buf = malloc((t + n) * sizeof(char));
        memcpy(c, array[0], l[0]);
        c += l[0];
        for (int i = 1; i < n; c += l[i], i++) {
                *(c++) = ' ';
                memcpy(c, array[i], l[i]);
        }
        *c = '\0';
        return buf;
}

/* the following function assumes that t points to a buffer of length >= 5 */
int
parseatarg(char *a, char *c, char *t)
{
        switch (c - a) {
                case 0:
                        t[0] = t[1] = '0';
                        break;
                case 1:
                        if (!ISDIGIT(a[0]))
                                return 0;
                        t[0] = '0', t[1] = a[0];
                        break;
                case 2:
                        if (!ISDIGIT(a[0]) || !ISDIGIT(a[1]))
                                return 0;
                        t[0] = a[0], t[1] = a[1];
                        break;
                default:
                        return 0;
        }
        if (ISDIGIT(c[1])) {
                if (ISDIGIT(c[2]))
                        t[2] = c[1], t[3] = c[2];
                else if (c[2] == '\0')
                        t[2] = '0', t[3] = c[1];
                else
                        return 0;
        } else if (c[1] == '\0')
                t[2] = t[3] = '0';
        else
                return 0;
        t[4] = '\0';
        return 1;
}

int
parsetime(char *a, unsigned int *t)
{
        int r = None;
        unsigned int i = 0;

        for (; *a != '\0'; a++) {
                if (ISDIGIT(*a)) {
                        i = 10 * i + *a - '0';
                        continue;
                }
                switch (*a) {
                        case ',':
                                if (r != None)
                                        return 0;
                                r = Comma;
                                *t = 60 * i;
                                i = 0;
                                break;
                        case '.':
                                if (r != None && r != Comma)
                                        return 0;
                                r = Period;
                                *t = 60 * (*t + i);
                                i = 0;
                                break;
                        case 'h':
                                if (r != None)
                                        return 0;
                                r = Hour;
                                *t = 60 * 60 * i;
                                i = 0;
                                break;
                        case 'm':
                                if (r != None && r != Hour)
                                        return 0;
                                r = Minute;
                                *t += 60 * i;
                                i = 0;
                                break;
                        case 's':
                                if (r != None && r != Hour && r != Minute)
                                        return 0;
                                r = Second;
                                *t += i;
                                i = 0;
                                break;
                        default:
                                return 0;
                }
        }
        switch (r) {
                case None:
                        *t = 60 * i;
                        break;
                case Period:
                        *t += i;
                        break;
                case Comma:
                        *t += 60 * i;
                        break;
                default:
                        if (i)
                                return 0;
                        break;
        }
        return *t ? 1 : 0;
}

void
callat(time_t t, char *atarg, char *nt)
{
        int fdr[2], fdw[2];

        if (pipe(fdr) == -1 || pipe(fdw) == -1) {
                perror("callat - pipe");
                free(nt);
                exit(1);
        }
        switch (fork()) {
                case -1:
                        perror("callat - fork");
                        free(nt);
                        exit(1);
                case 0:
                {
                        char *arg[] = { "/usr/bin/at", atarg, NULL };

                        close(fdw[1]);
                        close(fdr[0]);
                        if (fdw[0] != STDIN_FILENO) {
                                if (dup2(fdw[0], STDIN_FILENO) != STDIN_FILENO) {
                                        perror("callat - child - dup2");
                                        free(nt);
                                        exit(1);
                                }
                                close(fdw[0]);
                        }
                        if (fdr[1] != STDOUT_FILENO) {
                                if (dup2(fdr[1], STDOUT_FILENO) != STDOUT_FILENO) {
                                        perror("callat - child - dup2");
                                        free(nt);
                                        exit(1);
                                }
                                close(fdr[1]);
                        }
                        if (dup2(STDOUT_FILENO, STDERR_FILENO) != STDERR_FILENO) {
                                perror("callat - child - dup2");
                                free(nt);
                                exit(1);
                        }
                        execv(arg[0], arg);
                        perror("callat - child - execv");
                        _exit(127);
                }
                default:
                {
                        int f;
                        intmax_t id;
                        char line[128];
                        FILE *stream;

                        close(fdw[0]);
                        close(fdr[1]);
                        if (t >= 0)
                                dprintf(fdw[1], "sleep \"$(( %jd - $(date +%%s) ))\"\n"
                                                "notify-send -t 0 '%s'", (intmax_t)t, nt);
                        else
                                dprintf(fdw[1], "notify-sned -t 0 '%s'", nt);
                        close(fdw[1]);
                        free(nt);
                        if (!(stream = fdopen(fdr[0], "r"))) {
                                close(fdr[0]);
                                perror("callat - fdopen");
                                exit(1);
                        }
                        f = 1;
                        while (fgets(line, sizeof line, stream))
                                if (f && strcmp(line, "warning: commands will be executed using /bin/sh\n") == 0)
                                        f = 0;
                                else if (t >= 0 && sscanf(line, "job %jd", &id) == 1) {
                                        printf("job %jd at %s", id, ctime(&t));
                                        t = -1;
                                } else
                                        fputs(line, stdout);
                        fclose(stream);
                        if (wait(NULL) == -1) {
                                perror("callat - wait");
                                exit(1);
                        }
                }
        }
}

int
main(int argc, char *argv[])
{
        unsigned int t;
        char *c;
        char *nt;

        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
                puts(USAGE);
                return 0;
        }
        if (argc < 3) {
                fputs("nt: not enough arguments\n" USAGE "\n", stderr);
                return 2;
        }
        nt = catarray(argc - 2, argv + 2);
        if (nt[0] == '\0') {
                fputs("nt: notification can't be an empty string\n", stderr);
                free(nt);
                return 2;
        }
        if ((c = strchr(argv[1], ':'))) {
                char atarg[5];

                if (parseatarg(argv[1], c, atarg)) {
                        callat(-1, atarg, nt);
                        return 0;
                }
        } else if (parsetime(argv[1], &t)) {
                char atarg[25];

                snprintf(atarg, sizeof atarg, "now + %u minutes", t / 60);
                callat(time(NULL) + t, atarg, nt);
                return 0;
        }
        fputs("nt: invalid time specification\n" USAGE "\n", stderr);
        free(nt);
        return 2;
}
