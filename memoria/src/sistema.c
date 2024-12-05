#include "sistema.h"

extern int socket_cliente_cpu;
extern t_memoria *memoria_usuario;


/// @brief Read memory
/// @param direccion 
/// @return devuelve el contenido de la direccion, o -1 para error
uint32_t read_memory(uint32_t direccion){
    uint32_t * aux;

    if(direccion < 0 || direccion > memoria_usuario->size){
        log_error(logger, "Direccion %d invalida", direccion);
        return -1;
    }

    aux = memoria_usuario->espacio;
    return aux[direccion];
}
/// @brief Write memory
/// @param direccion
/// @param valor
/// @return devuelve 0 para ok o -1 para error
int write_memory(uint32_t direccion, uint32_t valor){
    uint32_t * aux;
    
    if(direccion < 0 || direccion > memoria_usuario->size){
        log_error(logger, "Direccion %d invalida", direccion);
        return -1;
    }
    aux = memoria_usuario->espacio;
    aux[direccion] = valor;

    return 0;
}

void enviar_contexto(){
    t_list *paquete_recv_list;
    t_paquete *paquete_recv;
    t_paquete *paquete_send;
    int pid;
    int tid;
    int PC;

    
    paquete_recv_list = recibir_paquete(socket_cliente_cpu);

    paquete_recv = list_remove(paquete_recv_list, 0);
    pid = (intptr_t)paquete_recv->buffer->stream;
    paquete_recv = list_remove(paquete_recv_list, 0);
    tid = (intptr_t)paquete_recv->buffer->stream;
    paquete_recv = list_remove(paquete_recv_list, 0);
    PC = (intptr_t)paquete_recv->buffer->stream;

    if(pid <= 0 || tid < 0 || PC < 0){
        log_error(logger, "CPU envio parametros incorrectos pidiendo el contexto de ejecucion");
    }
    int index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if(index_pcb==-1){
        error_contexto("No se encontro el PID en memoria");
    }
    t_pcb *pcb_aux = list_get(memoria_usuario->lista_pcb, index_pcb);
    int index_thread = buscar_tid(pcb_aux->listaTCB, tid);
    if(index_thread==-1){
        error_contexto("No se encontro el TID en el proceso");
    }
    //t_tcb *tcb_aux = list_get(pcb_aux->listaTCB, index_thread); NO SE USA

    agregar_a_paquete(paquete_send, pcb_aux->registro, sizeof(pcb_aux->registro));
    enviar_paquete(paquete_send, socket_cliente_cpu);
    eliminar_paquete(paquete_send);
}
void recibir_contexto(){

    t_list *paquete_recv_list;
    t_paquete *paquete_recv;
    t_paquete *paquete_send;
    RegistroCPU *registro;
    int pid;
    int index_pcb;
    t_pcb *aux_pcb;

    paquete_recv_list = recibir_paquete(socket_cliente_cpu);
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(registro, paquete_recv->buffer->stream, sizeof(RegistroCPU));
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(&pid, paquete_recv->buffer->stream, sizeof(int));
    
    index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if(index_pcb==-1){
        error_contexto("No se encontro el PID en memoria");
    }
    aux_pcb = list_get(memoria_usuario->lista_pcb, index_pcb);
    aux_pcb->registro = registro;

    paquete_send = crear_paquete(CONTEXTO_SEND);
    char *msj = malloc(100);
    strcpy(msj, "contexto recibido");
    agregar_a_paquete(paquete_send, msj, sizeof(msj));
    //log_info(logger, msj);
    log_info(logger, "%s", msj);
}

t_paquete *obtener_contexto(int pid, int tid) {
    
    if (pid <= 0 || tid < 0) {
        log_error(logger, "PID o TID inválidos al solicitar contexto (PID: %d, TID: %d)", pid, tid);
        return NULL;
    }

    // Buscar PCB por PID
    int index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if (index_pcb == -1) {
        log_error(logger, "No se encontró el PID %d en memoria.", pid);
        return NULL;
    }
    t_pcb *pcb = list_get(memoria_usuario->lista_pcb, index_pcb);

    // // Buscar TCB por TID dentro del PCB
    // int index_tcb = buscar_tid(pcb->listaTCB, tid);
    // if (index_tcb == -1) {
    //     log_error(logger, "No se encontró el TID %d en el proceso PID %d.", tid, pid);
    //     return NULL;
    // }
    // t_tcb *tcb = list_get(pcb->listaTCB, index_tcb);

    // Preparar paquete con el contexto completo
    t_paquete *paquete_send = crear_paquete(CONTEXTO_SEND);

    // Agregar registros del TCB
    agregar_a_paquete(paquete_send, &pcb->registro, sizeof(pcb->registro));

    log_info(logger, "Contexto de ejecución (PID: %d, TID: %d) preparado exitosamente.", pid, tid);
    return paquete_send;
}

void actualizar_contexto_ejecucion() {
    t_list *paquete_recv_list;
    t_paquete *paquete_recv;
    int pid, tid;
    RegistroCPU registros_actualizados;

    // Recibir paquete desde la CPU
    paquete_recv_list = recibir_paquete(socket_cliente_cpu);

    if (!paquete_recv_list || list_size(paquete_recv_list) < 3) {
        log_error(logger, "Paquete incompleto recibido para actualizar contexto de ejecución.");
        liberar_lista_paquetes(paquete_recv_list);
        return;
    }

    // Extraer PID
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(&pid, paquete_recv->buffer->stream, sizeof(int));
    eliminar_paquete(paquete_recv);

    // Extraer TID
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(&tid, paquete_recv->buffer->stream, sizeof(int));
    eliminar_paquete(paquete_recv);

    // Extraer registros actualizados
    paquete_recv = list_remove(paquete_recv_list, 0);
    memcpy(&registros_actualizados, paquete_recv->buffer->stream, sizeof(RegistroCPU));
    eliminar_paquete(paquete_recv);
    liberar_lista_paquetes(paquete_recv_list);

    // Validar PID y TID
    int index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if (index_pcb == -1) {
        log_error(logger, "No se encontró el PID %d en memoria para actualizar contexto.", pid);
        enviar_error_actualizacion();
        return;
    }
    t_pcb *pcb = list_get(memoria_usuario->lista_pcb, index_pcb);

    // int index_tcb = buscar_tid(pcb->listaTCB, tid);
    // if (index_tcb == -1) {
    //     log_error(logger, "No se encontró el TID %d en el proceso PID %d para actualizar contexto.", tid, pid);
    //     enviar_error_actualizacion();
    //     return;
    // }
    // t_tcb *tcb = list_get(pcb->listaTCB, index_tcb);

    // Actualizar registros en el TCB
    memcpy(&(pcb->registro), &registros_actualizados, sizeof(RegistroCPU));
    log_info(logger, "Registros actualizados para PID %d, TID %d.", pid, tid);

    // Responder OK al cliente
    t_paquete *paquete_ok = crear_paquete(OK_MEMORIA);
    const char *mensaje_ok = "Actualización exitosa";
    //agregar_a_paquete(paquete_ok, mensaje_ok, strlen(mensaje_ok) + 1);
    agregar_a_paquete(paquete_ok, (void *)mensaje_ok, strlen(mensaje_ok) + 1);
    enviar_paquete(paquete_ok, socket_cliente_cpu);
    eliminar_paquete(paquete_ok);
}

void enviar_error_actualizacion() {
    t_paquete *paquete_error = crear_paquete(ERROR_MEMORIA);
    const char *mensaje_error = "Error al actualizar contexto de ejecución";
    //agregar_a_paquete(paquete_error, mensaje_error, strlen(mensaje_error) + 1);
    agregar_a_paquete(paquete_error, (void *)mensaje_error, strlen(mensaje_error) + 1);
    enviar_paquete(paquete_error, socket_cliente_cpu);
    eliminar_paquete(paquete_error);
}

void liberar_lista_paquetes(t_list *lista) {
    if (!lista) return;
    list_destroy_and_destroy_elements(lista, (void *)eliminar_paquete);
}

//retorna index de pid en la lista de PCB
int buscar_pid(t_list *lista, int pid){
    t_pcb *elemento;
    t_list_iterator * iterator = list_iterator_create(lista);

    while(list_iterator_has_next(iterator)){
        elemento = list_iterator_next(iterator);
        if(elemento->pid == pid){
            return list_iterator_index(iterator);
        }
    }
    list_iterator_destroy(iterator);
    return -1;
}
//retorna index de tid en la lista de threads
int buscar_tid(t_list *lista, int tid){
    t_tcb *elemento;
    t_list_iterator * iterator = list_iterator_create(lista);

    while(list_iterator_has_next(iterator)){
        elemento = list_iterator_next(iterator);
        if(elemento->tid == tid){
            return list_iterator_index(iterator);
        }
    }
    list_iterator_destroy(iterator);
    return -1;
}
void error_contexto(char * error){
    log_error(logger, error);
    t_paquete *send = crear_paquete(ERROR_MEMORIA);
    enviar_paquete(send, socket_cliente_cpu);
    //agregar_a_paquete(send, error, sizeof(error)); se puede mandar error sin mensaje. Sino se complejiza el manejo de la respuesta del lado de cpu
    eliminar_paquete(send);
    return;
}
/// @brief busca un lugar vacio en la tabla y agrega el tid
/// @returns index donde guardo el tid
/// @param tcb 
int agregar_a_tabla_particion_fija(t_pcb *pcb){
    
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_particiones_fijas);
    elemento_particiones_fijas *aux;
    

    while(list_iterator_has_next(iterator)) {
        aux = list_iterator_next(iterator);
        if (aux->libre_ocupado==0){
            aux->libre_ocupado = pcb->pid; //no liberar aux, sino se pierde el elemento xd
            break;
        }
    }return list_iterator_index(iterator);
    list_iterator_destroy(iterator);
}
int agregar_a_dinamica(t_pcb *pcb){
    
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_huecos);
    elemento_huecos *aux_hueco, *aux_hueco_nuevo;
    elemento_procesos *aux_proceso;
    int index;

    aux_proceso->pid = pcb->pid;
    
    while(list_iterator_has_next(iterator)) {
        aux_hueco = list_iterator_next(iterator); // siguiente hueco
        if (aux_hueco->size >= pcb->memoria_necesaria){ // entra mi proceso en el hueco?
            
            //seteo variables del nuevo proceso
            aux_proceso->inicio = aux_hueco->inicio;
            aux_proceso->size = pcb->memoria_necesaria;

            //add proceso a tabla de procesos
            index = list_add(memoria_usuario->tabla_procesos, aux_proceso);
            
            //creo un hueco nuevo a partir del usado
            aux_hueco_nuevo->inicio = aux_hueco->inicio+aux_proceso->size;
            aux_hueco_nuevo->size = aux_hueco->size-aux_proceso->size;

            //reemplazo el hueco usado por el nuevo (que es mas chico)
            aux_hueco = aux_hueco_nuevo;

            //se me quemo el cerebro
            list_iterator_destroy(iterator);
            return index;
        }
    }
    log_error(logger, "No hay huecos libres");
    return -1;
}
void remover_proceso_de_tabla_dinamica(int pid){
    int index = buscar_en_dinamica(pid);

    elemento_huecos *aux_hueco, *aux_hueco_iterator;
    elemento_procesos *aux_proceso;

    aux_proceso = list_remove(memoria_usuario->tabla_procesos, index);
    aux_hueco->inicio = aux_proceso->inicio;
    aux_hueco->size = aux_proceso->size;

    free(aux_proceso);

    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_huecos);

    while(list_iterator_has_next(iterator)){
        aux_hueco_iterator = list_iterator_next(iterator);
        if(aux_hueco_iterator->inicio < aux_hueco->inicio){
            list_iterator_add(iterator, aux_hueco);
            list_iterator_destroy(iterator);
            break;
        }
    }
    consolidar_huecos();
}
void consolidar_huecos(){
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_huecos);
    elemento_huecos *aux_iterator, *aux_previous;

    aux_previous = list_iterator_next(iterator);
    while(list_iterator_has_next(iterator)){
        aux_iterator = list_iterator_next(iterator);
        if (aux_iterator->inicio == aux_previous->inicio + aux_previous->size){
            log_info(logger, "Se unieron 2 huecos");
            aux_previous->size += aux_iterator->size;

            list_iterator_remove(iterator);
            free(aux_iterator);
        }else{
            aux_previous = aux_iterator;
        }
    }
    return;
}
/// @brief busca el TID en la tabla de particiones fijas y devuelve el index
/// @param tid 
/// @return index o -1 -> error
int buscar_en_tabla_fija(int pid){
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_particiones_fijas);
    elemento_particiones_fijas *aux;
    while(list_iterator_has_next(iterator)){
        aux = list_iterator_next(iterator);
        if(aux->libre_ocupado == pid){
            return list_iterator_index(iterator);
        }
    }return -1;
}
void inicializar_tabla_particion_fija(t_list *particiones){
    elemento_particiones_fijas * aux = malloc(sizeof(elemento_particiones_fijas));
    aux->libre_ocupado = 0; // elemento libre
    aux->base = 0;
    aux->size = 0;
    uint32_t acumulador = 0;
    t_list_iterator *iterator_particiones = list_iterator_create(particiones);
    t_list_iterator *iterator_tabla = list_iterator_create(memoria_usuario->tabla_particiones_fijas);

    while(list_iterator_has_next(iterator_particiones)){
        aux->libre_ocupado = 0;
        aux->base = acumulador;
        aux->size = (int)list_iterator_next(iterator_particiones);
        acumulador += (uint32_t)aux->size;
        list_iterator_add(iterator_tabla, aux);
    }

    list_iterator_destroy(iterator_particiones);
    list_iterator_destroy(iterator_tabla);
    return;
}
void init_tablas_dinamicas(){
    elemento_huecos * aux = malloc(sizeof(elemento_huecos));
    aux->inicio = 0;
    aux->size = memoria_usuario->size;

    memoria_usuario->tabla_huecos = list_create();
    list_add(memoria_usuario->tabla_huecos, aux);

    memoria_usuario->tabla_procesos = list_create();

    return;
}
int buscar_en_dinamica(int pid){
    t_list_iterator *iterator = list_iterator_create(memoria_usuario->tabla_procesos);
    elemento_procesos *aux;
    while(list_iterator_has_next(iterator)){
        aux = list_iterator_next(iterator);
        if(aux->pid == pid){
            return list_iterator_index(iterator);
        }
    }return -1;
}

void crear_proceso(t_pcb *pcb) {
    switch(memoria_usuario->tipo_particion) {
        case FIJAS:
            int index_fija = agregar_a_tabla_particion_fija(pcb);
            elemento_particiones_fijas *aux_fija = list_get(memoria_usuario->tabla_particiones_fijas, index_fija);
            pcb->registro->base = aux_fija->base;  // Guardo la dirección de inicio en el registro base
            pcb->registro->limite = aux_fija->size;
            
            // Agrego el PCB a la lista global
            list_add(memoria_usuario->lista_pcb, pcb);
            
            // Agrego TCBs de este PCB a la lista global
            t_list_iterator *iterator_fija = list_iterator_create(pcb->listaTCB);
            t_tcb *tcb_aux_fija;
            while (list_iterator_has_next(iterator_fija)) {
                tcb_aux_fija = list_iterator_next(iterator_fija);
                list_add(memoria_usuario->lista_tcb, tcb_aux_fija);
            }
            list_iterator_destroy(iterator_fija);
            break;
        
        case DINAMICAS:
            int index_dinamica = agregar_a_dinamica(pcb);
            if (index_dinamica == -1) {
                log_error(logger, "No se pudo asignar memoria para el proceso PID %d en memoria dinámica.", pcb->pid);
                return; // Si no se pudo asignar, finaliza
            }

            // Agrego PCB a la lista global
            list_add(memoria_usuario->lista_pcb, pcb);

            // Agrego TCBs de este PCB a la lista global
            t_list_iterator *iterator_dinamica = list_iterator_create(pcb->listaTCB);
            t_tcb *tcb_aux_dinamica;
            while (list_iterator_has_next(iterator_dinamica)) {
                tcb_aux_dinamica = list_iterator_next(iterator_dinamica);
                list_add(memoria_usuario->lista_tcb, tcb_aux_dinamica);
            }
            list_iterator_destroy(iterator_dinamica);
            break;
        
        default:
            log_error(logger, "Tipo de memoria no reconocido.");
            break;
    }
}

void crear_thread(t_tcb *tcb){
    int index_pid;
    t_pcb *pcb_aux;
    index_pid = buscar_pid(memoria_usuario->lista_pcb, tcb->pid);

    pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
    list_add(pcb_aux->listaTCB, tcb);
    list_add(memoria_usuario->lista_tcb, tcb);

}

/*
void fin_proceso(int pid){ // potencialmente faltan semaforos
    int index_pid, index_tid;
    t_pcb *pcb_aux;
    t_tcb *tcb_aux;
    
    switch(memoria_usuario->tipo_particion){

        case FIJAS:
            elemento_particiones_fijas *aux;
            index_pid = buscar_en_tabla_fija(pid);
            if(index_pid!=(-1)){
                aux = list_get(memoria_usuario->tabla_particiones_fijas, index_pid);
                aux->libre_ocupado = 0;
            }
            index_pid = buscar_pid(memoria_usuario->lista_pcb, pid);
            pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
            
            t_list_iterator *iterator = list_iterator_create(pcb_aux->listaTCB);
            while(list_iterator_has_next(iterator)){
                tcb_aux = list_iterator_next(iterator);
                index_tid = buscar_tid(memoria_usuario->lista_tcb, tcb_aux->tid);
                tcb_aux = list_remove(memoria_usuario->lista_tcb, index_tid);
            }
            //mutex?
            pcb_aux = list_remove(memoria_usuario->lista_pcb, index_pid);
            //mutex?
            
        case DINAMICAS:
            //falta hacer
            break;
    }
    free(tcb_aux);
    free(pcb_aux);
} */

void fin_proceso(int pid) {
    int index_pid, index_tid;
    t_pcb *pcb_aux;
    t_tcb *tcb_aux;

    switch(memoria_usuario->tipo_particion) {
        case FIJAS:
            elemento_particiones_fijas *aux;
            index_pid = buscar_en_tabla_fija(pid);
            if(index_pid!=(-1)){
                aux = list_get(memoria_usuario->tabla_particiones_fijas, index_pid);
                aux->libre_ocupado = 0;
            }
            index_pid = buscar_pid(memoria_usuario->lista_pcb, pid);
            pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
            
            t_list_iterator *iterator = list_iterator_create(pcb_aux->listaTCB);
            while(list_iterator_has_next(iterator)){
                tcb_aux = list_iterator_next(iterator);
                index_tid = buscar_tid(memoria_usuario->lista_tcb, tcb_aux->tid);
                tcb_aux = list_remove(memoria_usuario->lista_tcb, index_tid);
            }
            //mutex?
            pcb_aux = list_remove(memoria_usuario->lista_pcb, index_pid);
            //mutex?

            break;
            
        case DINAMICAS:
            remover_proceso_de_tabla_dinamica(pid);
            log_info(logger, "Proceso PID %d liberado de memoria dinámica.", pid);
            break;
        
        default:
            log_error(logger, "Tipo de memoria no reconocido.");
            break;
    }

    free(tcb_aux);
    free(pcb_aux);
}

void fin_thread(int tid){
    int index_tid, index_pid;
    int pid;
    t_tcb *tcb_aux;
    t_pcb *pcb_aux;
    index_tid = buscar_tid(memoria_usuario->lista_tcb, tid);

    tcb_aux = list_get(memoria_usuario->lista_tcb, index_tid);
    pid = tcb_aux->pid;

    index_pid = buscar_pid(memoria_usuario->lista_pcb, pid);
    pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);

    t_list_iterator *iterator = list_iterator_create(pcb_aux->listaTCB);
    while(list_iterator_has_next(iterator)){
        tcb_aux = list_iterator_next(iterator);
        if (tcb_aux->tid == tid){
            list_iterator_remove(iterator);
            break;
        }
    }
    list_remove(memoria_usuario->lista_tcb, index_tid);
    free(tcb_aux);
}
int obtener_instruccion(int PC, int tid){ // envia el paquete instruccion a cpu. Si falla, retorna -1
	if(PC<0){
        log_error(logger, "PC invalido");
		return -1;
	}
	
	t_paquete *paquete_send = malloc(sizeof(t_paquete));
    t_tcb *tcb_aux;
	int index_tid;
	char * instruccion;
    int pid;

    index_tid = buscar_tid(memoria_usuario->lista_tcb, tid); //consigo tcb
    tcb_aux = list_get(memoria_usuario->lista_tcb, index_tid);

	instruccion = list_get(tcb_aux->instrucciones, PC);

	paquete_send = crear_paquete(OBTENER_INSTRUCCION);
	agregar_a_paquete(paquete_send, instruccion, sizeof(instruccion));
	enviar_paquete(paquete_send, socket_cliente_cpu);
	eliminar_paquete(paquete_send);

    return 0;
}