/*
 * Word count application with one thread per input file.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "word_count.h"
#include "word_helpers.h"

struct thread_arg
{
    const char *path;
    word_count_list_t *dst;
};

static void *worker(void *arg)
{
    struct thread_arg *a = (struct thread_arg *)arg;
    FILE *f = fopen(a->path, "r");
    if (!f)
    {
        perror(a->path);
        return NULL;
    }
    count_words(a->dst, f); /* tokenization + add_word happen here */
    fclose(f);
    return NULL;
}

int main(int argc, char *argv[])
{
    word_count_list_t word_counts;
    init_words(&word_counts);

    if (argc <= 1)
    {
        count_words(&word_counts, stdin);
    }
    else
    {
        int n = argc - 1;
        pthread_t *tids = calloc(n, sizeof *tids);
        struct thread_arg *args = calloc(n, sizeof *args);
        if (!tids || !args)
        {
            fprintf(stderr, "oom\n");
            return 1;
        }

        for (int i = 0; i < n; i++)
        {
            args[i].path = argv[i + 1];
            args[i].dst = &word_counts;
            if (pthread_create(&tids[i], NULL, worker, &args[i]) != 0)
            {
                perror("pthread_create");
                tids[i] = 0;
            }
        }
        for (int i = 0; i < n; i++)
        {
            if (tids[i])
                pthread_join(tids[i], NULL);
        }
        free(tids);
        free(args);
    }

    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
}
