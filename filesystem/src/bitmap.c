#include "bitmap.h"

 
void inicializar_bitmap() {
    char *path_bitmap = config_get_string_value(config, "MOUNT_DIR");
    strcat(path_bitmap, "/bitmap.dat");

    // Leer o inicializar bitmap en memoria
    FILE *archivo = fopen(path_bitmap, "rb+");
    if (!archivo) {
        archivo = fopen(path_bitmap, "wb+");
        if (!archivo) {
            log_error(logger, "Error al crear el archivo bitmap.dat");
            exit(EXIT_FAILURE);
        }
    }

    size_t tamanio = (block_count + 7) / 8;
    void *contenido = malloc(tamanio);
    fread(contenido, tamanio, 1, archivo);

    bitmap = bitarray_create_with_mode(contenido, tamanio, LSB_FIRST);
    fclose(archivo);
}

int reservar_bloques(int cantidad) {
    pthread_mutex_lock(&mutex_bitmap);
    int bloques[cantidad];
    int reservados = 0;

    for (int i = 0; i < block_count; i++) {
        if (!bitarray_test_bit(bitmap, i)) {
            bloques[reservados++] = i;
            bitarray_set_bit(bitmap, i);
            if (reservados == cantidad) {
                pthread_mutex_unlock(&mutex_bitmap);
                return bloques[0]; // Devuelve el primer bloque de índices
            }
        }
    }

    pthread_mutex_unlock(&mutex_bitmap);
    return -1; // No hay suficientes bloques libres
}

void liberar_bloque(int bloque) {
    pthread_mutex_lock(&mutex_bitmap);
    bitarray_clean_bit(bitmap, bloque);
    pthread_mutex_unlock(&mutex_bitmap);
}

void actualizar_bitmap() {
    char *path_bitmap = config_get_string_value(config, "MOUNT_DIR");
    strcat(path_bitmap, "/bitmap.dat");

    FILE *archivo = fopen(path_bitmap, "wb+");
    fwrite(bitmap->bitarray, bitmap->size, 1, archivo);
    fclose(archivo);
}