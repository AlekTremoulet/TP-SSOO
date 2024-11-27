#include <main.h>

t_log *logger;
int retardo_acceso;

int main() {

    pthread_t tid_memoria;

	argumentos_thread arg_memoria;

    logger = log_create("filesystem.log", "filesystem", 1, LOG_LEVEL_DEBUG);
    t_config *config = config_create("config/filesystem.config");

    void *ret_value; // no es mejor un NULL?

    //conexiones

   	arg_memoria.puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
	
	// Leer parámetros del archivo de configuración
    int block_count = config_get_int_value(config, "BLOCK_COUNT");
    int block_size = config_get_int_value(config, "BLOCK_SIZE");
    retardo_acceso = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
	char* block_count_str = config_get_string_value(config, "BLOCK_COUNT");
	char* mount_dir = config_get_string_value(config, "MOUNT_DIR");
	// Inicializar estructuras
    inicializar_bitmap(mount_dir,block_count_str);
    inicializar_bloques(block_count,block_size,mount_dir);
	
    //conexiones
	pthread_create(&tid_memoria, NULL, conexion_memoria, (void *)&arg_memoria);
	//conexiones

    //espero fin conexiones
	
	pthread_join(tid_memoria, ret_value);

	//espero fin conexiones
	return 0;
}

void *conexion_memoria(void* arg_memoria)
{
	argumentos_thread * args = arg_memoria; 
	t_paquete *handshake_send;
	t_list *handshake_recv;
	char * handshake_texto = "conexion filesystem";
	t_config *config = config_create("config/filesystem.config");

	int server = iniciar_servidor(args->puerto);
	log_info(logger, "Servidor listo para recibir al cliente memoria");
	int socket_cliente_memoria = esperar_cliente(server);

	//HANDSHAKE
	handshake_send = crear_paquete(HANDSHAKE);
	agregar_a_paquete (handshake_send, handshake_texto , strlen(handshake_texto)+1);
	//HANDSHAKE_end


		while(true){
			int cod_op = recibir_operacion(socket_cliente_memoria);
			switch (cod_op)
			{
				case HANDSHAKE:
					handshake_recv = recibir_paquete(socket_cliente_memoria);
					log_info(logger, "me llego: memoria");
					list_iterate(handshake_recv, (void*) iterator);
					enviar_paquete(handshake_send, socket_cliente_memoria);
					break;
				case INIT_BITMAP:
					log_info(logger, "Solicitud para inicializar el Bitmap recibida");

					char* mount_dir = config_get_string_value(config, "MOUNT_DIR");
					if (mount_dir == NULL) {
						log_error(logger, "Error: No se encontró 'MOUNT_DIR' en la configuración.");
						return (void*)EXIT_FAILURE;
					}

					char* block_count_str = config_get_string_value(config, "BLOCK_COUNT");
					if (block_count_str == NULL) {
						log_error(logger, "Error: No se encontró 'BLOCK_COUNT' en la configuración.");
						return (void*)EXIT_FAILURE;
					}

					inicializar_bitmap(mount_dir, block_count_str);
					break;
				case -1:
					log_error(logger, "el cliente se desconecto. Terminando servidor");
					return (void *)EXIT_FAILURE;
					break;
				default:
					log_warning(logger,"Operacion desconocida. No quieras meter la pata");
					break;
			}
		}
		
	close(server);
	close(socket_cliente_memoria);
	config_destroy(config);
    return (void *)EXIT_SUCCESS;
}
