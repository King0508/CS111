/*
 * Implementation of the word_count interface using Pintos lists.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "word_count.h"

/* Initialize the intrusive list. */
void init_words(word_count_list_t *wclist)
{
    list_init(wclist);
}

/* Number of elements in the list. */
size_t len_words(word_count_list_t *wclist)
{
    size_t n = 0;
    for (struct list_elem *it = list_begin(wclist);
         it != list_end(wclist);
         it = list_next(it))
    {
        n++;
    }
    return n;
}

/* Find an existing word node; NULL if not present. */
word_count_t *find_word(word_count_list_t *wclist, char *word)
{
    for (struct list_elem *it = list_begin(wclist);
         it != list_end(wclist);
         it = list_next(it))
    {
        word_count_t *e = list_entry(it, word_count_t, elem);
        if (strcmp(e->word, word) == 0)
            return e;
    }
    return NULL;
}

/* Insert new word with given count, or bump existing. */
word_count_t *add_word_with_count(word_count_list_t *wclist, char *word, int count)
{
    word_count_t *e = find_word(wclist, word);
    if (e)
    {
        e->count += count;
        return e;
    }
    e = malloc(sizeof *e);
    if (!e)
        return NULL;
    e->word = strdup(word);
    if (!e->word)
    {
        free(e);
        return NULL;
    }
    e->count = count;
    list_push_back(wclist, &e->elem);
    return e;
}

word_count_t *add_word(word_count_list_t *wclist, char *word)
{
    return add_word_with_count(wclist, word, 1);
}

/* Print counts in the format expected by merge_counts: "%8d\t%ms". */
void fprint_words(word_count_list_t *wclist, FILE *outfile)
{
    for (struct list_elem *it = list_begin(wclist);
         it != list_end(wclist);
         it = list_next(it))
    {
        word_count_t *e = list_entry(it, word_count_t, elem);
        /* width 8 so it aligns with provided tools; tab before word */
        fprintf(outfile, "%8d\t%s\n", e->count, e->word);
    }
}

/* Adapter: list_sort expects a comparator on list_elems. We’re given a
   comparator on word_count_t*, so unwrap and call through. */
static bool less_list(const struct list_elem *ewc1,
                      const struct list_elem *ewc2, void *aux)
{
    bool (*less)(const word_count_t *, const word_count_t *) =
        (bool (*)(const word_count_t *, const word_count_t *))aux;
    const word_count_t *a = list_entry(ewc1, word_count_t, elem);
    const word_count_t *b = list_entry(ewc2, word_count_t, elem);
    return less(a, b);
}

/* Stable sort by the given comparator (e.g., less_count or less_word). */
void wordcount_sort(word_count_list_t *wclist,
                    bool less(const word_count_t *, const word_count_t *))
{
    list_sort(wclist, less_list, (void *)less);
}
