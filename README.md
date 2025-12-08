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

- **Speedup** ($S_p$):  
  $$
  S_p = \frac{T_1}{T_p}
  $$

- **Eficiência** ($E_p$):  
  $$
  E_p = \frac{S_p}{P}
  $$


Foram medidos os seguintes tempos:

| **Processos** | **Threads** | **Tempo (s)** | **Speedup** | **Eficiência** |
|---------------------------|---------------|-------------|---------------|-------------|-----------------|
|  1             | 1           | 2.896         | 1.00        | 100%           |
| 4             | 1           | 1.062         | 2.73        | 68%            |
| 4             | 4           | 0.047         | 61.89       | 387%           |


📝 Observação importante


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