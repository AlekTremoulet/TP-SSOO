#include "bloques.h"
extern t_config* config;
extern t_log* logger;
extern retardo_acceso;

void inicializar_bloques(int block_count, int block_size, t_config *config) {
    char *mount_dir = config_get_string_value(config, "MOUNT_DIR");
    if (mount_dir == NULL) {
        fprintf(stderr, "Error: No se encontró 'MOUNT_DIR' en la configuración.\n");
        return 1;
    }

    size_t path_length = strlen(mount_dir) + strlen("/bloques.dat") + 1;

    char *path_bloques = malloc(path_length);
    if (path_bloques == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para path_bloques.\n");
        return 1;
    }

    // Construir la ruta completa
    snprintf(path_bloques, path_length, "%s/bloques.dat", mount_dir);

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
    free(path_bloques);
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

