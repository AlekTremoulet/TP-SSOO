#ifndef BLOCKS_H
#define BLOCKS_H

#include "bloques.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include "main.h"

extern t_config *config;
extern t_log *logger;


void *bloques = NULL;
int block_size = 4; 

void inicializar_bloques(int block_count, int block_size, char* mount_dir);
void escribir_bloque(int bloque, void *contenido, size_t tamanio);
void *leer_bloque(int bloque);

#endif
