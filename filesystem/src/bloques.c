/*

#include "bloques.h"
extern t_config* config;
extern t_log* logger;
void inicializar_bloques() {
    char *path_bloques = config_get_string_value(config, "MOUNT_DIR");
    strcat(path_bloques, "/bloques.dat");

    FILE *archivo = fopen(path_bloques, "rb+");
    if (!archivo) {
        archivo = fopen(path_bloques, "wb+");
        if (!archivo) {
            log_error(logger, "Error al crear el archivo bloques.dat");
            exit(EXIT_FAILURE);
        }

        ftruncate(fileno(archivo), block_size * block_count);
    }

    bloques = mmap(NULL, block_size * block_count, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(archivo), 0);
    if (bloques == MAP_FAILED) {
        log_error(logger, "Error al mapear bloques.dat en memoria");
        exit(EXIT_FAILURE);
    }

    fclose(archivo);
}

void escribir_bloque(int bloque, void *contenido, size_t tamanio) {
    usleep(retardo_acceso * 1000); // Retardo simulado
    memcpy(bloques + (bloque * block_size), contenido, tamanio);
}

void *leer_bloque(int bloque) {
    usleep(retardo_acceso * 1000); // Retardo simulado
    return bloques + (bloque * block_size);
}

*/