#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 8192
#define MAX_FIELDS 64
#define TOPK 30   // top 30 geral

typedef struct {
    char title[256];
    char lang[16];
    char release_date[32];
    double rating;
    double popularity;
    int vote_count;
} Film;

// Remove espaços e aspas duplas do início/fim
void trim_field(char *s) {
    if (!s) return;

    char *start = s;
    while (*start && (isspace((unsigned char)*start) || *start == '"'))
        start++;

    char *end = start + strlen(start);
    while (end > start &&
           (isspace((unsigned char)end[-1]) || end[-1] == '"'))
        end--;

    *end = '\0';

    if (start != s)
        memmove(s, start, end - start + 1);
}

// Split de CSV respeitando aspas
int split_csv_quoted(char *line, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    char *start = line;
    int in_quotes = 0;

    while (*p && count < max_fields) {
        if (*p == '"') {
            in_quotes = !in_quotes;
        } else if (*p == ',' && !in_quotes) {
            *p = '\0';
            fields[count++] = start;
            start = p + 1;
        } else if ((*p == '\n' || *p == '\r') && !in_quotes) {
            *p = '\0';
        }
        p++;
    }
    if (start <= p && count < max_fields) {
        fields[count++] = start;
    }
    return count;
}

// Insere candidato no top K local
void try_insert_topk(Film *top, const Film *cand) {
    if (cand->rating < 0.0) return;

    int pos = -1;
    for (int i = 0; i < TOPK; i++) {
        if (top[i].rating < 0.0 || cand->rating > top[i].rating) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return;

    for (int i = TOPK - 1; i > pos; i--) {
        top[i] = top[i - 1];
    }
    top[pos] = *cand;
}

// comparador para qsort (ordem decrescente)
int cmp_films_desc(const void *a, const void *b) {
    const Film *fa = (const Film *)a;
    const Film *fb = (const Film *)b;

    if (fa->rating < fb->rating) return 1;
    if (fa->rating > fb->rating) return -1;

    if (fa->popularity < fb->popularity) return 1;
    if (fa->popularity > fb->popularity) return -1;

    return 0;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) {
            fprintf(stderr,
                    "Uso: mpirun -np <P> ./build/movies_mpi data/full_top_rated_movies.csv\n");
        }
        MPI_Finalize();
        return 1;
    }

    const char *filename = argv[1];

    FILE *f = fopen(filename, "r");
    if (!f) {
        if (rank == 0) perror("Erro ao abrir arquivo");
        MPI_Finalize();
        return 1;
    }

    char line[MAX_LINE];

    // ======== 1) Ler cabeçalho =========
    if (!fgets(line, MAX_LINE, f)) {
        if (rank == 0) fprintf(stderr, "Erro: arquivo vazio.\n");
        fclose(f);
        MPI_Finalize();
        return 1;
    }

    char *fields[MAX_FIELDS];
    int n_fields = split_csv_quoted(line, fields, MAX_FIELDS);

    int idx_lang = -1;
    int idx_pop  = -1;
    int idx_rating = -1;
    int idx_original_title = -1;
    int idx_release_date = -1;

    for (int i = 0; i < n_fields; i++) {
        trim_field(fields[i]);
        if (strcmp(fields[i], "original_language") == 0) idx_lang = i;
        else if (strcmp(fields[i], "popularity") == 0)   idx_pop = i;
        else if (strcmp(fields[i], "vote_average") == 0) idx_rating = i;
        else if (strcmp(fields[i], "original_title") == 0) idx_original_title = i;
        else if (strcmp(fields[i], "release_date") == 0) idx_release_date = i;
    }

    if (idx_lang < 0 || idx_pop < 0 || idx_rating < 0 ||
        idx_original_title < 0 || idx_release_date < 0) {
        if (rank == 0) {
            fprintf(stderr,
                    "Nao encontrei colunas necessárias.\n"
                    "lang=%d pop=%d rating=%d original_title=%d release_date=%d\n",
                    idx_lang, idx_pop, idx_rating, idx_original_title, idx_release_date);
        }
        fclose(f);
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        printf("Colunas detectadas: lang=%d pop=%d rating=%d original_title=%d release_date=%d\n",
               idx_lang, idx_pop, idx_rating, idx_original_title, idx_release_date);
    }

    // ======== 2) Estatísticas locais =========
    long count_pt_local = 0;
    double sum_pop_local = 0.0;

    Film top_local[TOPK];
    for (int i = 0; i < TOPK; i++) {
        top_local[i].rating = -1.0;
        top_local[i].title[0] = '\0';
        top_local[i].lang[0] = '\0';
        top_local[i].release_date[0] = '\0';
        top_local[i].popularity = 0.0;
        top_local[i].vote_count = 0;
    }

    long line_num = 0;

    // ======== 3) Leitura paralela das linhas =========
    while (fgets(line, MAX_LINE, f)) {

        if ((line_num % size) != rank) {
            line_num++;
            continue;
        }

        char line_copy[MAX_LINE];
        strncpy(line_copy, line, MAX_LINE);
        line_copy[MAX_LINE - 1] = '\0';

        char *cols[MAX_FIELDS];
        int nf = split_csv_quoted(line_copy, cols, MAX_FIELDS);
        if (nf <= idx_original_title) {
            line_num++;
            continue;
        }

        char *lang  = cols[idx_lang];
        char *s_pop = cols[idx_pop];
        char *s_rating = cols[idx_rating];
        char *s_title = cols[idx_original_title];
        char *s_rel   = cols[idx_release_date];

        trim_field(lang);
        trim_field(s_pop);
        trim_field(s_rating);
        trim_field(s_title);
        trim_field(s_rel);

        if (strlen(lang) == 0) {
            line_num++;
            continue;
        }

        double pop    = atof(s_pop);
        double rating = atof(s_rating);

        // estatísticas apenas para filmes PT
        if (strcmp(lang, "pt") == 0) {
            count_pt_local++;
            sum_pop_local += pop;
        }

        // TOP 30 geral (sem filtrar idioma)
        Film cand;
        cand.rating = rating;
        cand.popularity = pop;
        cand.vote_count = 0;

        strncpy(cand.title, s_title, sizeof(cand.title) - 1);
        cand.title[sizeof(cand.title) - 1] = '\0';

        strncpy(cand.lang, lang, sizeof(cand.lang) - 1);
        cand.lang[sizeof(cand.lang) - 1] = '\0';

        strncpy(cand.release_date, s_rel, sizeof(cand.release_date) - 1);
        cand.release_date[sizeof(cand.release_date) - 1] = '\0';

        try_insert_topk(top_local, &cand);

        line_num++;
    }

    fclose(f);

    // ======== 4) Redução global =========
    long count_pt_global = 0;
    double sum_pop_global = 0.0;

    MPI_Reduce(&count_pt_local, &count_pt_global, 1, MPI_LONG, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&sum_pop_local, &sum_pop_global, 1, MPI_DOUBLE, MPI_SUM, 0,
               MPI_COMM_WORLD);

    // ======== 5) Coleta do TOP K ========
    Film *all_tops = NULL;
    if (rank == 0) {
        all_tops = (Film *)malloc(sizeof(Film) * TOPK * size);
    }

    MPI_Gather(top_local, TOPK * sizeof(Film), MPI_BYTE,
               all_tops, TOPK * sizeof(Film), MPI_BYTE,
               0, MPI_COMM_WORLD);

    // ======== 6) Rank 0 monta e exibe resultado final =========
    if (rank == 0) {
        printf("\n=== Filmes em Português ===\n");
        printf("Total de filmes PT: %ld\n", count_pt_global);

        if (count_pt_global > 0) {
            double media_pop = sum_pop_global / count_pt_global;
            printf("Média de popularidade dos filmes PT: %.4f\n\n", media_pop);
        }

        int total_cand = TOPK * size;
        qsort(all_tops, total_cand, sizeof(Film), cmp_films_desc);

        printf("=== TOP %d filmes mais bem avaliados (geral) ===\n", TOPK);

        int printed = 0;
        for (int i = 0; i < total_cand && printed < TOPK; i++) {
            if (all_tops[i].rating < 0.0) continue;

            printf(
                "%2d) Nota = %.2f | Popularidade = %.2f | Idioma = %s | Data = %s | "
                "Título = %s\n",
                printed + 1,
                all_tops[i].rating,
                all_tops[i].popularity,
                all_tops[i].lang,
                all_tops[i].release_date,
                all_tops[i].title
            );

            printed++;
        }

        free(all_tops);
    }

    MPI_Finalize();
    return 0;
}
