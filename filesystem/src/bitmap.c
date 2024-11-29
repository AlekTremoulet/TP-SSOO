#include "bitmap.h"

static t_bitarray* bitmap; // Estructura que representa el bitmap en memoria
static pthread_mutex_t mutex_bitmap;
static FILE* bitmap_file;

extern t_config *config;
extern t_log *logger;
void inicializar_bitmap(const char* mount_dir,uint32_t block_count) {
    // Obtener BLOCK_COUNT del archivo de configuración 
    if (block_count == 0) {
        log_error(logger, "No se encontró el valor BLOCK_COUNT en el archivo de configuración.");
        exit(EXIT_FAILURE);
    }

    // Calcular el tamaño del bitmap
    
    size_t tamanio_bitmap;

    if (block_count % 8 == 0) {
        tamanio_bitmap = block_count / 8; // Si es divisible por 8, se divide.
    } else {
        tamanio_bitmap = (block_count + 7) / 8; // Si no es divisible, se redondea para arriba.
    }
    
    size_t path_length = strlen(mount_dir) + strlen("/bitmap.dat") + 1;
    char *path_bitmap = malloc(path_length);
    if (path_bitmap == NULL) {
        log_info(logger, "Error: No se pudo asignar memoria para path_bloques.");
        exit(EXIT_FAILURE);
    }

    // Construir la ruta completa
    snprintf(path_bitmap, path_length, "%s/bitmap.dat", mount_dir);
    log_info(logger, "path bitmap:%s",path_bitmap);
    FILE *archivo_bitmap = fopen(path_bitmap, "rb+");
    if (!archivo_bitmap) {
        archivo_bitmap = fopen(path_bitmap, "wb+");
        if (!archivo_bitmap) {
            log_error(logger, "Error al crear el archivo bitmap.dat");
            exit(EXIT_FAILURE);
        }
        rewind(archivo_bitmap);
        // Inicializar el archivo con ceros
        uint8_t* buffer = calloc(tamanio_bitmap, sizeof(uint8_t)); //Asigna memoria para un buffer inicializado en ceros, cuyo tamaño es igual al bitmap
        fwrite(buffer, sizeof(uint8_t), tamanio_bitmap, archivo_bitmap); //Escribe el buffer de ceros en el archivo para inicializarlo.
        fflush(archivo_bitmap); //Asegura que los datos escritos se guarden físicamente en el archivo.
        free(buffer);
    }



    // Leer o mapear el contenido del bitmap a memoria
    uint8_t* contenido_bitmap = malloc(tamanio_bitmap);       
    rewind(archivo_bitmap);                 
    if (fread(contenido_bitmap, sizeof(uint8_t), tamanio_bitmap, archivo_bitmap) != tamanio_bitmap) {  // Lee los datos del archivo bitmap.dat y los copia en el buffer contenido_bitmap
        log_error(logger, "Error al leer el archivo bitmap.dat");
        exit(EXIT_FAILURE);
    }
    // Crear la estructura t_bitarray
    bitmap = bitarray_create_with_mode((char*)contenido_bitmap, tamanio_bitmap, LSB_FIRST); // Crea un manejador del bitmap (t_bitarray) usando la memoria cargada. uso el cast (char*) porque la funcion funciona con ese tipo de variable
    if (!bitmap) {
        log_info(logger,"Error al inicializar el bitmap");
        exit(EXIT_FAILURE);
    }

    fclose(archivo_bitmap);
    log_info(logger, "Bitmap inicializado correctamente.");
    free(path_bitmap);
    pthread_mutex_init(&mutex_bitmap, NULL); //No necesito bloquear a otros hilos porque no debería haber hilos intentando usar un recurso que aún no existe. Por eso lo pongo al final y no al comienzo
}

bool espacio_disponible(int cantidad) { //asegurar que hay bloques libres antes de intentar reservar uno
    pthread_mutex_lock(&mutex_bitmap);
    int libres = 0;
    for (int i = 0; i < bitarray_get_max_bit(bitmap); i++) {
        if (!bitarray_test_bit(bitmap, i)) {
            libres++;
            if (libres >= cantidad) {
                pthread_mutex_unlock(&mutex_bitmap);
                return true;
            }
        }
    }
    pthread_mutex_unlock(&mutex_bitmap);
    return false;
}
uint32_t reservar_bloque() {
    pthread_mutex_lock(&mutex_bitmap);
    if (!espacio_disponible(1)) {
        pthread_mutex_unlock(&mutex_bitmap);
        log_error(logger, "No hay bloques disponibles para reservar.");
        return -1;
    }
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
    //bitmap_file = fopen("bitmap.dat", "rb+"); el archivo ya se encuentra abierto en realidad
    if (!bitmap_file) {
        log_error(logger, "Error al abrir el archivo bitmap.dat para escribir");
        exit(EXIT_FAILURE);
    }
            // Escribir el bitmap en el archivo
    fseek(bitmap_file, 0, SEEK_SET);
    if (fwrite(bitmap->bitarray, sizeof(uint8_t), bitarray_get_max_bit(bitmap) / 8, bitmap_file) != bitarray_get_max_bit(bitmap) / 8) {
        log_error(logger, "Error al escribir el archivo bitmap.dat");
        exit(EXIT_FAILURE);
    }
    fflush(bitmap_file);

    // Liberar recursos
    bitarray_destroy(bitmap);
    fclose(bitmap_file);

    pthread_mutex_unlock(&mutex_bitmap);
    pthread_mutex_destroy(&mutex_bitmap);
    
}