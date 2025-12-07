#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE   8192
#define MAX_FIELDS 64
#define TOPK       10

typedef struct {
    char title[256];
    double rating;
    double popularity;
    int vote_count;
} Film;

// Split simples baseado em delimitador (não trata aspas/CSV complexo)
int split_line(char *line, char **fields, int max_fields, char delim) {
    int count = 0;
    char *ptr = line;
    char *start = ptr;

    while (*ptr && count < max_fields) {
        if (*ptr == delim || *ptr == '\n' || *ptr == '\r') {
            *ptr = '\0';
            fields[count++] = start;
            start = ptr + 1;
        }
        ptr++;
    }
    if (start < ptr && count < max_fields) {
        fields[count++] = start;
    }
    return count;
}

// Tenta inserir um filme no top 10 local (ordenado por nota decrescente)
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

// comparador para qsort (ordem decrescente por rating, depois popularidade)
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
            fprintf(stderr, "Uso: mpirun -np <P> ./build/movies_mpi_opm data/full_top_rated_movies.csv\n");
        }
        MPI_Finalize();
        return 1;
    }

    const char *filename = argv[1];
    char delim = ','; // separador do CSV original

    FILE *f = fopen(filename, "r");
    if (!f) {
        if (rank == 0) perror("Erro ao abrir arquivo");
        MPI_Finalize();
        return 1;
    }

    char line[MAX_LINE];

    // ======== 1) Ler cabeçalho e detectar índices das colunas ========
    if (!fgets(line, MAX_LINE, f)) {
        if (rank == 0) fprintf(stderr, "Erro: arquivo vazio.\n");
        fclose(f);
        MPI_Finalize();
        return 1;
    }

    char *fields[MAX_FIELDS];
    int n_fields = split_line(line, fields, MAX_FIELDS, delim);

    int idx_lang = -1;
    int idx_pop  = -1;
    int idx_rating = -1;
    int idx_original_title = -1;

    for (int i = 0; i < n_fields; i++) {
        if (strcmp(fields[i], "original_language") == 0) idx_lang = i;
        else if (strcmp(fields[i], "popularity") == 0)   idx_pop = i;
        else if (strcmp(fields[i], "vote_average") == 0) idx_rating = i;
        else if (strcmp(fields[i], "original_title") == 0) idx_original_title = i;
    }

    if (idx_lang < 0 || idx_pop < 0 || idx_rating < 0 || idx_original_title < 0) {
        if (rank == 0) {
            fprintf(stderr,
                    "Nao encontrei colunas necessárias.\n"
                    "original_language=%d popularity=%d vote_average=%d original_title=%d\n",
                    idx_lang, idx_pop, idx_rating, idx_original_title);
        }
        fclose(f);
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        printf("[OMP] Colunas detectadas: lang=%d pop=%d rating=%d original_title=%d\n",
               idx_lang, idx_pop, idx_rating, idx_original_title);
    }

    // ======== 2) Ler e armazenar apenas linhas pertencentes a este rank ========
    long global_line_num = 0; // linhas de dados (sem cabeçalho)

    char **local_lines = NULL;
    long   local_capacity = 0;
    long   local_count = 0;

    while (fgets(line, MAX_LINE, f)) {
        if ((global_line_num % size) == rank) {
            if (local_count == local_capacity) {
                long new_cap = (local_capacity == 0) ? 1024 : local_capacity * 2;
                char **tmp = (char **)realloc(local_lines, new_cap * sizeof(char *));
                if (!tmp) {
                    fprintf(stderr, "Rank %d: erro de memória ao alocar linhas.\n", rank);
                    fclose(f);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
                local_lines = tmp;
                local_capacity = new_cap;
            }

            local_lines[local_count] = malloc(strlen(line) + 1);
            if (!local_lines[local_count]) {
                fprintf(stderr, "Rank %d: erro de memória ao duplicar linha.\n", rank);
                fclose(f);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            strcpy(local_lines[local_count], line);
            local_count++;
        }
        global_line_num++;
    }

    fclose(f);

    // ======== 3) Estatísticas locais com OpenMP (count + soma popularidade) ========
    long   count_pt_local = 0;
    double sum_pop_local  = 0.0;

    #pragma omp parallel for reduction(+:count_pt_local,sum_pop_local) schedule(static)
    for (long i = 0; i < local_count; i++) {
        char line_copy[MAX_LINE];
        strncpy(line_copy, local_lines[i], MAX_LINE);
        line_copy[MAX_LINE - 1] = '\0';

        char *cols[MAX_FIELDS];
        int nf = split_line(line_copy, cols, MAX_FIELDS, delim);
        if (nf <= idx_original_title) continue;

        const char *lang  = cols[idx_lang];
        const char *s_pop = cols[idx_pop];

        if (!lang || strlen(lang) == 0) continue;

        if (strcmp(lang, "pt") == 0) {
            double pop = atof(s_pop);
            count_pt_local++;
            sum_pop_local += pop;
        }
    }

    // ======== 4) Descobrir top 10 local (loop sequencial) ========
    Film top_local[TOPK];
    for (int i = 0; i < TOPK; i++) {
        top_local[i].rating = -1.0;
        top_local[i].title[0] = '\0';
        top_local[i].popularity = 0.0;
        top_local[i].vote_count = 0;
    }

    for (long i = 0; i < local_count; i++) {
        char line_copy[MAX_LINE];
        strncpy(line_copy, local_lines[i], MAX_LINE);
        line_copy[MAX_LINE - 1] = '\0';

        char *cols[MAX_FIELDS];
        int nf = split_line(line_copy, cols, MAX_FIELDS, delim);
        if (nf <= idx_original_title) continue;

        const char *lang     = cols[idx_lang];
        const char *s_pop    = cols[idx_pop];
        const char *s_rating = cols[idx_rating];
        const char *s_title  = cols[idx_original_title];

        if (!lang || strlen(lang) == 0) continue;
        if (strcmp(lang, "pt") != 0) continue;

        double pop    = atof(s_pop);
        double rating = atof(s_rating);

        Film cand;
        cand.rating = rating;
        cand.popularity = pop;
        cand.vote_count = 0;
        strncpy(cand.title, s_title, sizeof(cand.title) - 1);
        cand.title[sizeof(cand.title) - 1] = '\0';

        try_insert_topk(top_local, &cand);
    }

    // liberar memória das linhas locais
    for (long i = 0; i < local_count; i++) {
        free(local_lines[i]);
    }
    free(local_lines);

    // ======== 5) Redução MPI para estatísticas globais ========
    long   count_pt_global = 0;
    double sum_pop_global  = 0.0;

    MPI_Reduce(&count_pt_local, &count_pt_global, 1, MPI_LONG,   MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&sum_pop_local, &sum_pop_global,   1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // ======== 6) Coleta do top 10 local de todos os ranks ========
    Film *all_tops = NULL;
    if (rank == 0) {
        all_tops = (Film *)malloc(sizeof(Film) * TOPK * size);
    }

    MPI_Gather(top_local,
               TOPK * (int)sizeof(Film),
               MPI_BYTE,
               all_tops,
               TOPK * (int)sizeof(Film),
               MPI_BYTE,
               0,
               MPI_COMM_WORLD);

    // ======== 7) Rank 0 monta resultado final ========
    if (rank == 0) {
        printf("\n[OMP] === Filmes em Português ===\n");
        printf("Total de filmes PT: %ld\n", count_pt_global);
        if (count_pt_global > 0) {
            double media_pop = sum_pop_global / (double)count_pt_global;
            printf("Média de popularidade dos filmes PT: %.4f\n\n", media_pop);
        }

        int total_cand = TOPK * size;
        qsort(all_tops, total_cand, sizeof(Film), cmp_films_desc);

        printf("[OMP] === TOP 10 filmes PT (por vote_average) ===\n");
        int printed = 0;
        for (int i = 0; i < total_cand && printed < TOPK; i++) {
            if (all_tops[i].rating < 0.0) continue;

            printf("%2d) Nota = %.2f | Popularidade = %.2f | Título = %s\n",
                   printed + 1,
                   all_tops[i].rating,
                   all_tops[i].popularity,
                   all_tops[i].title);
            printed++;
        }

        free(all_tops);
    }

    MPI_Finalize();
    return 0;
}
