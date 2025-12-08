# 🎬 Trabalho 4 – Processamento Paralelo de Dataset de Filmes (MPI + OpenMP)

Desenvolvido para a disciplina de **Programação Paralela**, este projeto realiza o processamento distribuído de um dataset de filmes utilizando:

- **MPI (Message Passing Interface)** — paralelismo de memória distribuída
- **OpenMP** — paralelismo de memória compartilhada
- **Versão híbrida (MPI + OpenMP)**
- Estatísticas sobre avaliação dos filmes em português e **Top 30 filmes mais bem avaliados**
- Análise de desempenho com **tempo**, **speedup** e **eficiência**

---

# 📦 O que este projeto faz

1. **Lê e distribui o CSV de filmes** entre os processos MPI.
2. Cada processo:
   - filtra e processa apenas seu subconjunto de linhas;
   - calcula estatísticas locais (filmes em português com contagem e média de popularidade);
   - mantém um Top-30 local.
3. Os métodos **`MPI_Reduce`** e  **`MPI_Gather`** reúnem resultados parciais.
4. O processo **rank 0** monta o **Top 30 final** entre todos os processos.
5. Versões:
   - `movies_mpi.c` → versão só MPI
   - `movies_mpi_opm.c` → versão híbrida com OpenMP

---

# ✅ Pré-requisitos
Instale os pacotes base (C, MPI,OpenMP)
```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev
```
**Observação:** O projeto foi pensando para WSL.

# ⚙️ Compilação

Versão simplificada
```bash
make
```
Gera os arquivos:
- `build/movies_mpi`
- `build/movies_mpi_opm`

Compilação Manual(opcional)
```bash
mpicc -O2 -Wall -o build/movies_mpi src/movies_mpi.c
mpicc -O2 -Wall -fopenmp -o build/movies_mpi_opm src/movies_mpi_opm.c

```

# ▶️ Execução
Dataset usado:

```bash
data/full_top_rated_movies.csv
```
1) Executar somente MPI
Sequencial - 1 processo
```bash
export OMP_NUM_THREADS=1
mpirun -np 1 ./build/movies_mpi data/full_top_rated_movies.csv
```
Paralelo - 4 processos
```bash
export OMP_NUM_THREADS=1
mpirun -np 4 ./build/movies_mpi data/full_top_rated_movies.csv
```
2) Executar versão híbrida MPI + OpenMP
Exemplo com 4 processos x 4threads
```bash
export OMP_NUM_THREADS=4
mpirun -np 4 ./build/movies_mpi_opm data/full_top_rated_movies.csv

```
A saída de todas as versões é composta por:
- A contagem dos filmes em português
- Média de popularidade destes
- Top30 filmes mais bem avaliados
- Tempo total de execução

# 📊 Análise das Métricas de Desempenho
### 🧮 Fórmulas Utilizadas:

**Speedup (\(S_p\))**

$$
S_p = \frac{T_1}{T_p}
$$

**Eficiência (\(E_p\))**

$$
E_p = \frac{S_p}{P}
$$



Foram medidos os seguintes tempos:
A implementação com apenas MPI mostra um comportamento mais esperado:

- De 1 para 2 processos há uma melhoria real (speedup ≈ 1.48).

- Com 4 processos ainda há ganho, mas menor.

- A partir de 8 processos o desempenho começa a degradar.

- Em 12 processos o speedup cai abaixo de 1 (overhead maior que o benefício).


1) Desempenho do `movie_mpi.c`

| # Processos | Tempo (s)  | Speedup | Eficiência |
|-------------|------------|---------|------------|
| 1           | 0.007978   | 1.000   | 1.000      |
| 2           | 0.005404   | 1.476   | 0.738      |
| 4           | 0.005686   | 1.403   | 0.351      |
| 8           | 0.006428   | 1.241   | 0.155      |
| 12          | 0.009006   | 0.886   | 0.074      |

O programa tem uma pequena janela de escalabilidade eficiente, entre 2 e 4 processos.

Para muitos processos, MPI adiciona overhead de comunicação e sincronizações que superam o ganho.

Também pode haver oversubscribe, especialmente se sua máquina não tem 12 núcleos físicos.

**Resultado:** Escalabilidade moderada para poucos processos, mas negativa para muitos.

---

2) Desempenho do `movie_mpi_opm.c`

A tabela mostra que a versão híbrida (MPI + OpenMP) apresenta piora significativa de desempenho conforme o número de processos aumenta:

- O tempo aumenta em vez de diminuir.

- O speedup cai para menos de 1 já com 2 processos.

- A eficiência despenca rapidamente (de 23% com 2 processos para menos de 1% com 12 processos).

| # Processos | Tempo (s)  | Speedup | Eficiência |
|-------------|------------|---------|------------|
| 1           | 0.016815   | 1.000   | 1.000      |
| 2           | 0.035435   | 0.475   | 0.237      |
| 4           | 0.056498   | 0.298   | 0.074      |
| 8           | 0.117992   | 0.143   | 0.018      |
| 12          | 0.193663   | 0.087   | 0.007      |

Isso indica que o uso combinado de MPI + OpenMP está adicionando mais overhead do que benefício. Possíveis causas:

- O problema é pequeno demais para justificar paralelização híbrida.

- Há custo alto de comunicação entre processos MPI.

- As regiões paralelas OpenMP talvez sejam pequenas, ou existam operações serializadas.

 - O ambiente está oversubscribed, causando competição por CPU.

- A divisão do trabalho não compensa o custo de coordenação.

**Resultado:** Escalabilidade negativa

# 🔁 Resumo - Comandos essenciais

```bash
# 1) compilar
make

# 2) MPI sequencial
mpirun -np 1 ./build/movies_mpi data/full_top_rated_movies.csv

# 3) MPI paralelo
mpirun -np 4 ./build/movies_mpi data/full_top_rated_movies.csv

# 4) Híbrido MPI + OpenMP
export OMP_NUM_THREADS=4
mpirun -np 4 ./build/movies_mpi_opm data/full_top_rated_movies.csv
```
