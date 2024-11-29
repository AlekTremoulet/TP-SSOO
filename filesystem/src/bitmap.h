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

void inicializar_bitmap(const char* mount_dir,uint32_t block_count);
int reservar_bloques();
bool espacio_disponible(int cantidad);
void liberar_bloque(uint32_t bloque);
void destruir_bitmap();

#endif