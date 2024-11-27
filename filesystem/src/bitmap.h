#ifndef BITMAP_H
#define BITMAP_H

#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include "main.h"

void inicializar_bitmap(const char* path,char* block_count_str);
int reservar_bloques(int cantidad);
void liberar_bloque(uint32_t bloque);
void actualizar_bitmap();
void destruir_bitmap();

#endif