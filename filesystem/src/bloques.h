// blocks.h
#ifndef BLOCKS_H
#define BLOCKS_H

#include "filesystem.h"

void inicializar_bloques();
void escribir_bloque(int bloque, void *contenido, size_t tamanio);
void *leer_bloque(int bloque);

#endif