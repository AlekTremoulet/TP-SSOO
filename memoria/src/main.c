#include <main.h>

t_log *logger;
int socket_cliente_cpu; //necesito que sea global para usarlo desde sistema.c
t_memoria *memoria_usuario;
t_list *lista_pcb_memoria;

int main(int argc, char* argv[]) {

    pthread_t tid_cpu;
    pthread_t tid_kernel;
    pthread_t tid_fs;

    argumentos_thread arg_cpu;
    argumentos_thread arg_kernel;
    argumentos_thread arg_fs;

    logger = log_create("memoria.log", "Memoria", 1, LOG_LEVEL_DEBUG);
    t_config *config = config_create("config/memoria.config");

    void *ret_value;

	//inicializar memoria
	t_list *particiones = list_create();
	char ** particiones_string = config_get_array_value(config, "PARTICIONES");
	cargar_lista_particiones(particiones, particiones_string);
	string_array_destroy(particiones_string);
	inicializar_memoria(config_get_int_value(config, "TIPO_PARTICION"), config_get_int_value(config, "TAM_MEMORIA"), particiones); //1 fija 0 dinamica

    //conexiones
	arg_cpu.puerto = config_get_string_value(config, "PUERTO_CPU");
    arg_kernel.puerto = config_get_string_value(config, "PUERTO_KERNEL");
    arg_fs.puerto = config_get_string_value(config, "PUERTO_FILESYSTEM");
    arg_fs.ip = config_get_string_value(config, "IP_FILESYSTEM");

    //conexiones
	pthread_create(&tid_cpu, NULL, conexion_cpu, (void *)&arg_cpu);
	pthread_create(&tid_kernel, NULL, server_multihilo_kernel, (void *)&arg_kernel);
	pthread_create(&tid_fs, NULL, cliente_conexion_filesystem, (void *)&arg_fs);
	//conexiones

	intptr_t ret_value_int = (intptr_t)ret_value;
    //espero fin conexiones
	pthread_join(tid_cpu, ret_value);
	log_info(logger,"conexion con cpu cerrada con status code: %ld", ret_value_int);
	pthread_join(tid_kernel, ret_value);
	log_info(logger,"conexion con server muiltihilo kernel cerrada con status code: %ld", ret_value_int);
	pthread_join(tid_fs, ret_value);
	log_info(logger,"conexion con filesystem cerrada con status code: %ld", ret_value_int);
	//espero fin conexiones

}
void inicializar_memoria(particiones tipo_particion, int size, t_list *particiones){
	memoria_usuario = malloc(sizeof(t_memoria));
	memoria_usuario->lista_pcb = list_create();
	memoria_usuario->lista_tcb = list_create();
	memoria_usuario->espacio=malloc(size*sizeof(uint32_t));
	memoria_usuario->size = size;

	switch(tipo_particion){
		case DINAMICAS: // particiones dinamicas
			log_error(logger, "particiones dinamicas no implementadas todavia");
			break;
		
		case FIJAS: // fijas
			memoria_usuario->tipo_particion = FIJAS;
			memoria_usuario->tabla_particiones_fijas = list_create();
			inicializar_tabla_particion_fija(particiones);
			
			break;
	}
}
void cargar_lista_particiones(t_list * particiones, char **particiones_array){
	
	char* elemento = malloc(sizeof(char*)*20);
	elemento = *particiones_array;
	for(int i=0;particiones_array[i]!=NULL;i++){
		list_add(particiones, particiones_array[i]);
	}
}
void *server_multihilo_kernel(void* arg_server){

	argumentos_thread * args = arg_server;
	pthread_t aux_thread;
	t_list *lista_t_peticiones = list_create();
	void* ret_value;
	protocolo_socket cod_op;
	int flag = 1; //1: operar, 0: TERMINATE

	int server = iniciar_servidor(args->puerto); //abro server
	log_info(logger, "Servidor listo para recibir nueva peticion");
	
	while (flag)
	{
		log_info(logger, "esperando nueva peticion");
		int socket_cliente_kernel = esperar_cliente(server); //pausado hasta que llegue una peticion nueva (nuevo cliente)
	
		cod_op = recibir_operacion(socket_cliente_kernel);
		switch (cod_op)
		{
			case PROCESS_CREATE_OP:
				pthread_create(&aux_thread, NULL, peticion_kernel_NEW_PROCESS, (void *)&socket_cliente_kernel);
				list_add(lista_t_peticiones, &aux_thread);
				log_info(logger, "nueva peticion");
				break;
			case THREAD_CREATE_OP:
				pthread_create(&aux_thread, NULL, peticion_kernel_NEW_THREAD, (void *)&socket_cliente_kernel);
				list_add(lista_t_peticiones, &aux_thread);
				log_info(logger, "nueva peticion");
				break;
			case PROCESS_EXIT_OP:
				pthread_create(&aux_thread, NULL, peticion_kernel_END_PROCESS, (void *)&socket_cliente_kernel);
				list_add(lista_t_peticiones, &aux_thread);
				log_info(logger, "nueva peticion");
				break;
			case THREAD_EXIT_OP:
				pthread_create(&aux_thread, NULL, peticion_kernel_END_THREAD, (void *)&socket_cliente_kernel);
				list_add(lista_t_peticiones, &aux_thread);
				log_info(logger, "nueva peticion");
				break;
			case TERMINATE:
				log_error(logger, "TERMINATE recibido de KERNEL");
				flag=0;
				break;
			default:
				log_warning(logger,"Peticion invalida %d", cod_op);
				break;
		}
		
	}
	int size = list_size(lista_t_peticiones);
	for(int i=0;i<size;i++){ //en caso de que el while de arriba termine, espera a todas las peticiones antes de finalizar el server
		pthread_t *aux = list_remove(lista_t_peticiones, 0);
		pthread_join(*aux, ret_value);
		log_info(logger, "peticion de kernel terminada con status code: %d", (int)ret_value);
	}

	close(server);
    pthread_exit(EXIT_SUCCESS);
}
void *peticion_kernel_NEW_PROCESS(void* arg_peticion){
	int socket = (int)arg_peticion;
	t_pcb *pcb;
	//atender peticion
	t_list * paquete_list;
	t_paquete * paquete_recv;
	t_paquete * paquete_send;

	paquete_list = recibir_paquete(socket);
	pcb = list_remove(paquete_list, 0);
	crear_proceso(pcb);
	
	//notificar resultado a kernel
	paquete_send = crear_paquete(OK);
	enviar_paquete(paquete_send, socket);

	eliminar_paquete(paquete_send);
	eliminar_paquete(paquete_recv);
	list_destroy(paquete_list);
	close(socket); //cerrar socket
	return(void*)EXIT_SUCCESS; //finalizar hilo
}
void *peticion_kernel_NEW_THREAD(void* arg_peticion){
	int socket = (int)arg_peticion;
	t_tcb *tcb;
	int pid;
	//atender peticion
	t_list * paquete_list;
	t_paquete * paquete_recv;
	t_paquete * paquete_send;

	paquete_list = recibir_paquete(socket);
	tcb = list_remove(paquete_list, 0);
	pid = (intptr_t)list_remove(paquete_list, 0);
	crear_thread(tcb);
	
	//notificar resultado a kernel
	paquete_send = crear_paquete(OK);
	enviar_paquete(paquete_send, socket);

	eliminar_paquete(paquete_send);
	eliminar_paquete(paquete_recv);
	list_destroy(paquete_list);
	close(socket); //cerrar socket
	return(void*)EXIT_SUCCESS; //finalizar hilo
}
void *peticion_kernel_END_PROCESS(void* arg_peticion){
	int socket = (int)arg_peticion;
	int pid;
	//atender peticion
	t_list * paquete_list;
	t_paquete * paquete_recv;
	t_paquete * paquete_send;

	paquete_list = recibir_paquete(socket);
	pid = (intptr_t)list_remove(paquete_list, 0);
	fin_proceso(pid);
	
	//notificar resultado a kernel
	paquete_send = crear_paquete(OK);
	enviar_paquete(paquete_send, socket);

	eliminar_paquete(paquete_send);
	eliminar_paquete(paquete_recv);
	list_destroy(paquete_list);
	close(socket); //cerrar socket
	return(void*)EXIT_SUCCESS; //finalizar hilo
}
void *peticion_kernel_END_THREAD(void* arg_peticion){
	int socket = (int)arg_peticion;
	int tid;
	//atender peticion
	t_list * paquete_list;
	t_paquete * paquete_recv;
	t_paquete * paquete_send;

	paquete_list = recibir_paquete(socket);
	tid = (intptr_t)list_remove(paquete_list, 0);
	fin_thread(tid);
	
	//notificar resultado a kernel
	paquete_send = crear_paquete(OK);
	enviar_paquete(paquete_send, socket);

	eliminar_paquete(paquete_send);
	eliminar_paquete(paquete_recv);
	list_destroy(paquete_list);
	close(socket); //cerrar socket
	return(void*)EXIT_SUCCESS; //finalizar hilo
}
void *conexion_cpu(void* arg_cpu)
{
	argumentos_thread *args = arg_cpu; 
	t_paquete *handshake_send;
	t_list *paquete_recv;
	char * handshake_texto = "conexion con memoria";
	
	t_config *config = config_create("config/memoria.config");
	int delay = config_get_int_value(config, "RETARDO_RESPUESTA");
	
	int server = iniciar_servidor(args->puerto);
	log_info(logger, "Servidor listo para recibir al cliente CPU");
	socket_cliente_cpu = esperar_cliente(server);

	//HANDSHAKE
	handshake_send = crear_paquete(HANDSHAKE);
	agregar_a_paquete (handshake_send, handshake_texto , strlen(handshake_texto)+1);
	//HANDSHAKE_end

	uint32_t direccion;
	uint32_t valor;
	int PC;
	int tid;
	while(true){
		int cod_op = recibir_operacion(socket_cliente_cpu);
		sleep(delay); // retardo en peticion / cpu
		switch (cod_op)
		{
			case HANDSHAKE:
				paquete_recv = recibir_paquete(socket_cliente_cpu);
				log_info(logger, "me llego:cpu");
				list_iterate(paquete_recv, (void*) iterator);
				enviar_paquete(handshake_send, socket_cliente_cpu);
				list_destroy(paquete_recv);
				break;
			case CONTEXTO_RECEIVE:
				// Recibo el paquete y extraigo pid y tid
				t_list *paquete_recv = recibir_paquete(socket_cliente_cpu);

				int pid, tid;
				if (recibir_pid_tid(paquete_recv, &pid, &tid)) {
					enviar_contexto(pid, tid);
    			}
				break;
			case CONTEXTO_UPDATE:
				actualizar_contexto_ejecucion();
				break;
			case CONTEXTO_SEND:
				recibir_contexto();
				break;
			case OBTENER_INSTRUCCION:
				paquete_recv = recibir_paquete(socket_cliente_cpu);
				PC = (intptr_t)list_remove(paquete_recv, 0);
				tid = (intptr_t)list_remove(paquete_recv, 0);
				obtener_instruccion(PC, tid);
				break;
			case READ_MEM:
				paquete_recv = recibir_paquete(socket_cliente_cpu);
				direccion = (uint32_t)list_remove(paquete_recv, 0);
				valor = (uint32_t)read_memory(direccion);
				if(valor != -1){
					t_paquete *paquete_send = crear_paquete(OK);
					agregar_a_paquete(paquete_send, &valor, sizeof(uint32_t));
					enviar_paquete(paquete_send, socket_cliente_cpu);
					eliminar_paquete(paquete_send);
				}else {
					t_paquete *paquete_send = crear_paquete(ERROR_MEMORIA);
					log_error(logger, "Error escribiendo en memoria");
					enviar_paquete(paquete_send, socket_cliente_cpu);
					eliminar_paquete(paquete_send);
				}
				break;
			case WRITE_MEM:
				paquete_recv = recibir_paquete(socket_cliente_cpu);
				direccion = (uint32_t)list_remove(paquete_recv, 0);
				valor = (uint32_t)list_remove(paquete_recv, 0);
				if(valor != -1){
					write_memory(direccion, valor);
					t_paquete *paquete_send = crear_paquete(OK);
					agregar_a_paquete(paquete_send, &valor, sizeof(uint32_t));
					enviar_paquete(paquete_send, socket_cliente_cpu);
					eliminar_paquete(paquete_send);
				}else {
					t_paquete *paquete_send = crear_paquete(ERROR_MEMORIA);
					log_error(logger, "Error escribiendo en memoria");
					enviar_paquete(paquete_send, socket_cliente_cpu);
					eliminar_paquete(paquete_send);
				}
				break;
			case -1:
				log_error(logger, "el cliente se desconecto. Terminando servidor");
				return (void *)EXIT_FAILURE;
				break;
			default:
				log_warning(logger,"Operacion desconocida. No quieras meter la pata");
				break;
		}eliminar_paquete(handshake_send);
	}
		
	close(server);
	close(socket_cliente_cpu);
    pthread_exit(EXIT_SUCCESS);
}
void *cliente_conexion_filesystem(void * arg_fs){

	argumentos_thread * args = arg_fs;
	t_paquete* send_handshake;
	int conexion_memoria_fs;
	protocolo_socket op;
	char* valor = "conexion memoria";
	int flag=1;
	do
	{
		conexion_memoria_fs = crear_conexion(args->ip, args->puerto);
		sleep(1);

	}while(conexion_memoria_fs == -1);
	
	send_handshake = crear_paquete(HANDSHAKE);
	agregar_a_paquete (send_handshake, valor , strlen(valor)+1);
	
	while(flag){
		enviar_paquete(send_handshake, conexion_memoria_fs);
		sleep(1);
		op = recibir_operacion(conexion_memoria_fs);
		switch (op)
		{
		case HANDSHAKE:
			log_info(logger, "recibi handshake de filesystem");
			break;
		case TERMINATE:
			flag = 0;
			break;

		default:
			break;
		}
	}

	eliminar_paquete(send_handshake);
	liberar_conexion(conexion_memoria_fs);
    return (void *)EXIT_SUCCESS;
}