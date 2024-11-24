#ifndef BITMAP_H
#define BITMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include "../../filesystem/src/mian.h"

void inicializar_bitmap(const char* path, uint32_t tamanio_bitmap);
uint32_t reservar_bloque();
void liberar_bloque(uint32_t bloque);
void destruir_bitmap();


#endif