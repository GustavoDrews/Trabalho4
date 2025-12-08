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


1) Desempenho do `movie_mpi.c`
   
| # Processos(p) | Tempo (s) | Speedup S(p) | Eficiência E(p) |
|------------:|----------:|-------------:|----------------:|
| 1           | 1.031555  | 1.000        | 100.0%          |
| 2           | 0.794190  | 1.299        | 64.9%           |
| 4           | 1.033550  | 0.998        | 25.0%           |
| 6           | 1.311308  | 0.787        | 13.1%           |
| 12          | 0.244019  | 4.227        | 35.2%           |


1) Desempenho do `movie_mpi_opm.c`
   
| # Processos(p) | Tempo (s) | Speedup S(p) | Eficiência E(p) |
|------------:|----------:|-------------:|----------------:|
| 1           | 0.010630  | 1.000        | 100.0%          |
| 2           | 0.014282  | 0.744        | 37.2%           |
| 4           | 0.072719  | 0.146        | 3.7%            |
| 6           | 0.090551  | 0.117        | 2.0%            |
| 12          | 0.253293  | 0.042        | 0.3%            |

📝 Observação importante
- `movies_mpi_opm` - otimizado, muito rápido sozinho

  - Speedup cai conforme aumenta np.

  - Eficiência despenca de 100% para 0.3%.
  
➡ Essa versão é tão rápida que o overhead de paralelização mata o ganho.
Em termos de desempenho: rodar com np = 1 é o melhor cenário disparado.

- `movies_mpi` - normal

  - Escala um pouco de 1 → 2 processos.

  - Se atrapalha em 4 e 6 processos.

  - Tem um bom ganho em 12 processos (provavelmente explorando melhor o hardware, mesmo oversubscrito).

➡ Paralelização só “compensa” mesmo em np = 2 e em np = 12 com esses dados.

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