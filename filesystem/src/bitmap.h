#ifndef BITMAP_H
#define BITMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <utils/utils.h>
#include "../../filesystem/src/mian.h"

void inicializar_bitmap();
int reservar_bloques(int cantidad);
void liberar_bloque(int bloque);
void actualizar_bitmap();

#endif