/*
 * Word count application with one process per input file.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "word_count.h"
#include "word_helpers.h"

/*
 * Read stream of counts and accumulate globally.
 * Expects lines like: "%8d\t%ms\n" (count, tab, word)
 */
void merge_counts(word_count_list_t *wclist, FILE *count_stream) {
    char *word;
    int count;
    int rv;
    while ((rv = fscanf(count_stream, "%8d\t%ms\n", &count, &word)) == 2) {
        add_word_with_count(wclist, word, count);
        free(word);
    }
    if ((rv == EOF) && (feof(count_stream) == 0)) {
        perror("could not read counts");
    } else if (rv != EOF) {
        fprintf(stderr, "read ill-formed count (matched %d)\n", rv);
    }
}

static int run_child_and_pipe(const char *path, int pfd[2]) {
    if (pipe(pfd) < 0) { perror("pipe"); return -1; }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(pfd[0]); close(pfd[1]); return -1; }

    if (pid == 0) {
        /* Child: close read end, process file, emit to write end. */
        close(pfd[0]);

        FILE *in = fopen(path, "r");
        if (!in) { perror(path); _exit(1); }

        word_count_list_t local;
        init_words(&local);
        count_words(&local, in);
        fclose(in);

        FILE *out = fdopen(pfd[1], "w");
        if (!out) { perror("fdopen"); _exit(1); }
        fprint_words(&local, out);
        fclose(out);           /* also closes pfd[1] */
        _exit(0);
    }

    /* Parent: close write end, keep read end for merge. */
    close(pfd[1]);
    return pid;
}

int main(int argc, char *argv[]) {
    word_count_list_t word_counts;
    init_words(&word_counts);

    if (argc <= 1) {
        count_words(&word_counts, stdin);
    } else {
        int n = argc - 1;
        int *rfds = calloc(n, sizeof *rfds);
        pid_t *pids = calloc(n, sizeof *pids);
        if (!rfds || !pids) { fprintf(stderr, "oom\n"); return 1; }

        for (int i = 0; i < n; i++) {
            int pfd[2];
            pid_t pid = run_child_and_pipe(argv[i + 1], pfd);
            if (pid < 0) { rfds[i] = -1; pids[i] = -1; continue; }
            rfds[i] = pfd[0];  /* parent read end */
            pids[i] = pid;
        }

        /* Merge each child's stream, then close read end. */
        for (int i = 0; i < n; i++) {
            if (rfds[i] >= 0) {
                FILE *rin = fdopen(rfds[i], "r");
                if (!rin) { perror("fdopen"); close(rfds[i]); continue; }
                merge_counts(&word_counts, rin);
                fclose(rin);   /* closes rfds[i] */
            }
        }

        /* Reap children. */
        for (int i = 0; i < n; i++) {
            if (pids[i] > 0) {
                int status;
                (void)waitpid(pids[i], &status, 0);
            }
        }
        free(rfds);
        free(pids);
    }

    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
