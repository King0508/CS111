/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_p.c"
#endif
#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_p.c"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "word_count.h"

/* Initialize list and mutex. */
void init_words(word_count_list_t *wclist)
{
    list_init(&wclist->lst);
    pthread_mutex_init(&wclist->lock, NULL);
}

/* Length with brief lock while traversing. */
size_t len_words(word_count_list_t *wclist)
{
    size_t n = 0;
    pthread_mutex_lock(&wclist->lock);
    for (struct list_elem *it = list_begin(&wclist->lst);
         it != list_end(&wclist->lst);
         it = list_next(it))
    {
        n++;
    }
    pthread_mutex_unlock(&wclist->lock);
    return n;
}

/* Find needs the lock to read the shared list safely. */
word_count_t *find_word(word_count_list_t *wclist, char *word)
{
    word_count_t *res = NULL;
    pthread_mutex_lock(&wclist->lock);
    for (struct list_elem *it = list_begin(&wclist->lst);
         it != list_end(&wclist->lst);
         it = list_next(it))
    {
        word_count_t *e = list_entry(it, word_count_t, elem);
        if (strcmp(e->word, word) == 0)
        {
            res = e;
            break;
        }
    }
    pthread_mutex_unlock(&wclist->lock);
    return res;
}

/* Add or increment, holding the lock only while touching the list. */
word_count_t *add_word(word_count_list_t *wclist, char *word)
{
    pthread_mutex_lock(&wclist->lock);

    /* Search */
    word_count_t *e = NULL;
    for (struct list_elem *it = list_begin(&wclist->lst);
         it != list_end(&wclist->lst);
         it = list_next(it))
    {
        word_count_t *t = list_entry(it, word_count_t, elem);
        if (strcmp(t->word, word) == 0)
        {
            e = t;
            break;
        }
    }

    if (e)
    {
        e->count++;
        pthread_mutex_unlock(&wclist->lock);
        return e;
    }

    /* Insert */
    e = malloc(sizeof *e);
    if (!e)
    {
        pthread_mutex_unlock(&wclist->lock);
        return NULL;
    }
    e->word = strdup(word);
    if (!e->word)
    {
        free(e);
        pthread_mutex_unlock(&wclist->lock);
        return NULL;
    }
    e->count = 1;
    list_push_back(&wclist->lst, &e->elem);

    pthread_mutex_unlock(&wclist->lock);
    return e;
}

/* Print without allowing concurrent mutation while iterating. */
void fprint_words(word_count_list_t *wclist, FILE *outfile)
{
    pthread_mutex_lock(&wclist->lock);
    for (struct list_elem *it = list_begin(&wclist->lst);
         it != list_end(&wclist->lst);
         it = list_next(it))
    {
        word_count_t *e = list_entry(it, word_count_t, elem);
        fprintf(outfile, "%8d\t%s\n", e->count, e->word);
    }
    pthread_mutex_unlock(&wclist->lock);
}

/* Sort safely by temporarily taking the lock around list_sort. */
static bool less_list(const struct list_elem *a,
                      const struct list_elem *b, void *aux)
{
    bool (*less)(const word_count_t *, const word_count_t *) =
        (bool (*)(const word_count_t *, const word_count_t *))aux;
    const word_count_t *wa = list_entry(a, word_count_t, elem);
    const word_count_t *wb = list_entry(b, word_count_t, elem);
    return less(wa, wb);
}

void wordcount_sort(word_count_list_t *wclist,
                    bool less(const word_count_t *, const word_count_t *))
{
    pthread_mutex_lock(&wclist->lock);
    list_sort(&wclist->lst, less_list, (void *)less);
    pthread_mutex_unlock(&wclist->lock);
}
