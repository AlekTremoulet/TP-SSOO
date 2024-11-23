#ifndef BITMAP_H
#define BITMAP_H

#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include "../../filesystem/src/mian.h"

void inicializar_bitmap(const char* path, uint32_t tamanio_bitmap);
int reservar_bloques(int cantidad);
void liberar_bloque(int bloque);
void actualizar_bitmap();

#endif