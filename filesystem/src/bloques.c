#include "bloques.h"
extern t_config* config;
extern t_log* logger;
extern int retardo_acceso;
static FILE* metadata_file;
int index_block=0;
extern uint32_t block_count;
extern int block_size;
extern retardo_acceso;
extern char* mount_dir;

void inicializar_bloques(uint32_t block_count, int block_size, char* mount_dir) {
    if (mount_dir == NULL) {
        log_info(logger, "Error: No se encontró 'MOUNT_DIR' en la configuración.");
        return exit(EXIT_FAILURE);
    }

    size_t path_length = strlen(mount_dir) + strlen("/bloques.dat") + 1;
    char *path_bloques = malloc(path_length);
    if (path_bloques == NULL) {
        log_info(logger, "Error: No se pudo asignar memoria para path_bloques.");
        exit(EXIT_FAILURE);
    }
    

    // Construir la ruta completa
    snprintf(path_bloques, path_length, "%s/bloques.dat", mount_dir);
    log_info(logger, "path bloques:%s",path_bloques);
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
    free(path_bloques);
    log_info(logger, "Bloques inicializado correctamente.");

    
}

void escribir_bloque(int bloque, void *contenido, size_t tamanio) {
    int block_count = config_get_int_value(config, "BLOCK_COUNT"); // Obtiene el total de bloques
    if (bloque < 0 || bloque >= block_count) {
        log_error(logger, "El índice de bloque %d está fuera del rango permitido.", bloque);
        return; // No continúa la operación si el bloque es inválido
    }
    if (tamanio > block_size) {
        log_error(logger, "El tamaño de escritura excede el tamaño del bloque.");
        exit(EXIT_FAILURE);
    }
    usleep(retardo_acceso * 1000); // Retardo simulado
    memcpy(bloques + (bloque * block_size), contenido, tamanio);
    log_info(logger, "Escribiendo bloque %d con %lu bytes", bloque, tamanio);
}

void *leer_bloque(int bloque) {
    // Valida bloque dentro del rango
    int block_count = config_get_int_value(config, "BLOCK_COUNT"); // Obtiene el número total de bloques
    if (bloque < 0 || bloque >= block_count) {
        log_error(logger, "El índice de bloque %d está fuera del rango permitido (0 - %d).", bloque, block_count - 1);
        return NULL; // Devuelve NULL si el bloque es inválido
    }

    // Simula retardo de acceso al bloque
    usleep(retardo_acceso * 1000);

    // Calcula la dirección del bloque dentro del archivo mapeado
    void *direccion_bloque = bloques + (bloque * block_size);

    log_info(logger, "Bloque %d leído correctamente. Dirección: %p, Tamaño: %d bytes", bloque, direccion_bloque, block_size);

    return direccion_bloque;
}


void crear_archivo_metadata(uint32_t block_count,  int block_size, char* dir_files, char* nombre_archivo) {
    if (block_count == 0) {
        log_error(logger, "No se encontró el valor BLOCK_COUNT en el archivo de configuración.");
        exit(EXIT_FAILURE);
    }

    //recibo aca el nombre del archivo
    size_t path_length = strlen(dir_files) + strlen(nombre_archivo) + 2;
    char *path_metadata = malloc(path_length);
    if (path_metadata == NULL) {
        log_error(logger, "Error: No se pudo asignar memoria para path_metadata.\n");
        exit(EXIT_FAILURE);
    }
    snprintf(path_metadata, path_length, "%s/%s", dir_files, nombre_archivo);
    if (!metadata_file) {
        metadata_file = fopen(path_metadata, "wb+");
        if (!metadata_file) {
            log_error(logger, "Error al crear el archivo metadata");
            free(path_metadata);
            exit(EXIT_FAILURE);
        }
        index_block++;
        size_t buffer_size = snprintf(NULL, 0, "SIZE=%u\nINDEX_BLOCK=%d\n", block_size, index_block) + 1;
        char *buffer = malloc(buffer_size);
        if (buffer == NULL) {
            log_error(logger, "Error al asignar memoria para el buffer.");
            fclose(metadata_file);
            free(path_metadata);
            exit(EXIT_FAILURE);
        }
        snprintf(buffer, buffer_size, "SIZE=%d\nINDEX_BLOCK=%d\n", block_size, index_block);
        if (fwrite(buffer, sizeof(char), strlen(buffer), metadata_file) != strlen(buffer)) {
            log_error(logger, "Error al escribir en el archivo metadata.");
            fclose(metadata_file);
            free(path_metadata);
            exit(EXIT_FAILURE);
        }

        fclose(metadata_file);
        log_info(logger, "Archivo metadata creado con éxito: %s", path_metadata);
    } else {
        log_info(logger, "Archivo metadata ya existe: %s", path_metadata);
        fclose(metadata_file);
    }
    log_info(logger, "Creación Archivo: “## Archivo Creado: <%s> - Tamaño: <%d>", nombre_archivo, block_size);
    free(path_metadata);


}