CC      = mpicc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -fopenmp
LDFLAGS = -fopenmp

SRC_DIR = src
BUILD   = build
OUTDIR  = out

TARGETS = $(BUILD)/movies_mpi $(BUILD)/movies_mpi_opm

all: prepdirs $(TARGETS)

prepdirs:
	mkdir -p $(BUILD) $(OUTDIR)

# Versão MPI simples
$(BUILD)/movies_mpi: $(SRC_DIR)/movies_mpi.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Versão MPI + OpenMP
$(BUILD)/movies_mpi_opm: $(SRC_DIR)/movies_mpi_opm.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BUILD)/*
