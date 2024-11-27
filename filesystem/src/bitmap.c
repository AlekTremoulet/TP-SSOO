#include "bitmap.h"

static t_bitarray* bitmap; // Estructura que representa el bitmap en memoria
static pthread_mutex_t mutex_bitmap;
static FILE* bitmap_file;

extern t_config *config;
extern t_log *logger;
void inicializar_bitmap(const char* path, t_config* config) {
    // Obtener BLOCK_COUNT del archivo de configuración 
    char* block_count_str = config_get_string_value(config, "BLOCK_COUNT"); //esto tira segmentation fault
    if (block_count_str == NULL) {
        log_error(logger, "No se encontró el valor BLOCK_COUNT en el archivo de configuración.");
        exit(EXIT_FAILURE);
    }

    uint32_t block_count = (uint32_t) strtoul(block_count_str, NULL, 10);

    // Calcular el tamaño del bitmap
    
    size_t tamanio_bitmap;

    if (block_count % 8 == 0) {
        tamanio_bitmap = block_count / 8; // Si es divisible por 8, se divide.
    } else {
        tamanio_bitmap = (block_count + 7) / 8; // Si no es divisible, se redondea para arriba.
    }

    // Abre o crea el archivo bitmap.dat
    FILE* archivo_bitmap = fopen(path, "rb+");
    if (archivo_bitmap == NULL) {
        archivo_bitmap = fopen(path, "wb+");
        log_info(logger, "Error al crear el archivo del bitmap");
        // Inicializar el archivo con ceros
        uint8_t* buffer = calloc(tamanio_bitmap, sizeof(uint8_t)); //Asigna memoria para un buffer inicializado en ceros, cuyo tamaño es igual al bitmap
        fwrite(buffer, sizeof(uint8_t), tamanio_bitmap, archivo_bitmap); //Escribe el buffer de ceros en el archivo para inicializarlo.
        fflush(archivo_bitmap); //Asegura que los datos escritos se guarden físicamente en el archivo.

        free(buffer);
    }

    // Leer o mapear el contenido del bitmap a memoria
    uint8_t* contenido_bitmap = malloc(tamanio_bitmap);                                          // Reserva memoria para almacenar el contenido del bitmap
    fread(contenido_bitmap, sizeof(uint8_t), tamanio_bitmap, archivo_bitmap);                    // Lee los datos del archivo bitmap.dat y los copia en el buffer contenido_bitmap

    // Crear la estructura t_bitarray
    t_bitarray* bitmap = bitarray_create_with_mode(contenido_bitmap, tamanio_bitmap, LSB_FIRST); // Crea un manejador del bitmap (t_bitarray) usando la memoria cargada.
    if (!bitmap) {
        log_info(logger,"Error al inicializar el bitmap");
        exit(EXIT_FAILURE);
    }

    fclose(archivo_bitmap);
    log_info(logger, "Bitmap inicializado correctamente.");
}


uint32_t reservar_bloque() {
    pthread_mutex_lock(&mutex_bitmap);
    for (uint32_t i = 0; i < bitarray_get_max_bit(bitmap); i++) {
        if (!bitarray_test_bit(bitmap, i)) {
            bitarray_set_bit(bitmap, i);
            pthread_mutex_unlock(&mutex_bitmap);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex_bitmap);
    return -1; // No hay bloques disponibles
}

void liberar_bloque(uint32_t bloque) {
    pthread_mutex_lock(&mutex_bitmap);
    bitarray_clean_bit(bitmap, bloque);
    pthread_mutex_unlock(&mutex_bitmap);
}

void destruir_bitmap() {
    pthread_mutex_lock(&mutex_bitmap);

    // Escribir el bitmap en el archivo
    fseek(bitmap_file, 0, SEEK_SET);
    fwrite(bitmap->bitarray, sizeof(uint8_t), bitarray_get_max_bit(bitmap) / 8, bitmap_file);
    fflush(bitmap_file);

    // Liberar recursos
    bitarray_destroy(bitmap);
    fclose(bitmap_file);

    pthread_mutex_unlock(&mutex_bitmap);
    pthread_mutex_destroy(&mutex_bitmap);
}