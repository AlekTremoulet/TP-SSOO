#include "sistema.h"

extern int socket_cliente_cpu;
extern t_memoria *memoria_usuario;

// Función que recibe el paquete y extrae PID y TID
bool recibir_pid_tid(t_list *paquete_recv, int *pid, int *tid) {
    if (list_size(paquete_recv) < 2) {
        log_error(logger, "Paquete incompleto recibido para CONTEXTO_RECEIVE.");
        list_destroy_and_destroy_elements(paquete_recv, (void *)eliminar_paquete);
        return false;
    }

    // Extraer PID
    t_paquete *pid_paquete = list_remove(paquete_recv, 0);
    memcpy(pid, pid_paquete->buffer->stream, sizeof(int));
    eliminar_paquete(pid_paquete);

    // Extraer TID
    t_paquete *tid_paquete = list_remove(paquete_recv, 0);
    memcpy(tid, tid_paquete->buffer->stream, sizeof(int));
    eliminar_paquete(tid_paquete);

    list_destroy(paquete_recv);  // Liberar lista de paquetes
    return true;
}

void enviar_contexto(int pid, int tid) {
    t_pcb *pcb;
    t_tcb *tcb;

    // Busco el PCB y TCB
    if (!obtener_pcb_y_tcb(pid, tid, &pcb, &tcb)) {
        log_error(logger, "No se pudo obtener el contexto para PID %d, TID %d.", pid, tid);
        return; // Si no se encuentran, se registra el error y se termina
    }

    // Creo un paquete para enviar el contexto de ejecución
    t_paquete *paquete = crear_paquete(CONTEXTO_SEND);

    // Agrego los registros del TCB al paquete
    agregar_a_paquete(paquete, &tcb->registro, sizeof(tcb->registro));

    // Agrego la base y el límite del PCB al paquete
    agregar_a_paquete(paquete, &pcb->registro->base, sizeof(pcb->registro->base));
    agregar_a_paquete(paquete, &pcb->registro->limite, sizeof(pcb->registro->limite));

    // Envio el paquete al cliente (CPU)
    enviar_paquete(paquete, socket_cliente_cpu);

    // Libero memoria del paquete
    eliminar_paquete(paquete);

    // Registramos en el log que se envió el contexto
    log_info(logger, "Contexto enviado para PID %d, TID %d.", pid, tid);
}

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

// Funcion auxiliar para buscar y validar el proceso (PCB) y el hilo (TCB) asociados al PID y TID
// t_pcb **pcb_out y t_tcb **tcb_out: Son punteros de salida. Al encontrar el PCB y el TCB, la función los almacena en estas variables para que la función que llama a obtener_pcb_y_tcb pueda utilizarlos.
bool obtener_pcb_y_tcb(int pid, int tid, t_pcb **pcb_out, t_tcb **tcb_out) {
    // Buscar el PCB
    int index_pcb = buscar_pid(memoria_usuario->lista_pcb, pid);
    if (index_pcb == -1) {
        log_error(logger, "PID %d no encontrado en memoria.", pid);
        return false;
    }
    *pcb_out = list_get(memoria_usuario->lista_pcb, index_pcb);

    // Buscar el TCB dentro del PCB
    int index_tcb = buscar_tid((*pcb_out)->listaTCB, tid);
    if (index_tcb == -1) {
        log_error(logger, "TID %d no encontrado en el proceso PID %d.", tid, pid);
        return false;
    }
    *tcb_out = list_get((*pcb_out)->listaTCB, index_tcb);

    return true;
} // TODO: verificar manejo de list_get con Santi

// Actualizar los registros de un hilo (TCB) en memoria con los valores proporcionados por la CPU
void actualizar_contexto_ejecucion() {
    t_list *paquete_recv_list;
    t_pcb *pcb;
    t_tcb *tcb;
    int pid, tid;
    RegistroCPU registros_actualizados;

    // Recibo el paquete de la CPU
    paquete_recv_list = recibir_paquete(socket_cliente_cpu);

    // Valido que el paquete contiene los elementos necesarios
    if (!paquete_recv_list || list_size(paquete_recv_list) < 3) {
        log_error(logger, "Paquete incompleto recibido para actualizar contexto.");
        liberar_lista_paquetes(paquete_recv_list);
        enviar_error_actualizacion(); // Envio error a la CPU
        return;
    }

    // Obtengo el PID
    t_paquete *pid_paquete = list_remove(paquete_recv_list, 0);
    memcpy(&pid, pid_paquete->buffer->stream, sizeof(int));
    eliminar_paquete(pid_paquete);

    // Obtengo el TID
    t_paquete *tid_paquete = list_remove(paquete_recv_list, 0);
    memcpy(&tid, tid_paquete->buffer->stream, sizeof(int));
    eliminar_paquete(tid_paquete);

    // Obtengo los registros actualizados
    t_paquete *registros_paquete = list_remove(paquete_recv_list, 0);
    memcpy(&registros_actualizados, registros_paquete->buffer->stream, sizeof(RegistroCPU));
    eliminar_paquete(registros_paquete);

    // Libero lista de paquetes
    list_destroy(paquete_recv_list);

    // Busco el PCB y TCB correspondientes
    if (!obtener_pcb_y_tcb(pid, tid, &pcb, &tcb)) {
        enviar_error_actualizacion(); // Envio error a la CPU
        return;
    }

    // Actualizo los registros en el TCB
    memcpy(&(tcb->registro), &registros_actualizados, sizeof(RegistroCPU));
    log_info(logger, "Registros actualizados para PID %d, TID %d.", pid, tid);

    // Respondemos a la CPU con un paquete de confirmación
    t_paquete *paquete_ok = crear_paquete(OK_MEMORIA);
    const char *mensaje_ok = "Actualización exitosa";

    agregar_a_paquete(paquete_ok, mensaje_ok, strlen(mensaje_ok) + 1);
    enviar_paquete(paquete_ok, socket_cliente_cpu);
    eliminar_paquete(paquete_ok);
}

void enviar_error_actualizacion() {
    t_paquete *paquete_error = crear_paquete(ERROR_MEMORIA);
    const char *mensaje_error = "Error al actualizar contexto de ejecución";
    agregar_a_paquete(paquete_error, mensaje_error, strlen(mensaje_error) + 1);
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
void crear_proceso(t_pcb *pcb){
    int index = agregar_a_tabla_particion_fija(pcb);
    elemento_particiones_fijas *aux = list_get(memoria_usuario->tabla_particiones_fijas, index);
    pcb->registro->base=aux->base; //guarda la direccion de inicio del segmento en el registro "base"
    pcb->registro->limite=aux->size;
    //agregar pcb a la lista global
    list_add(memoria_usuario->lista_pcb, pcb);
    
    t_list_iterator *iterator = list_iterator_create(pcb->listaTCB); // iterator para listaTCB
    t_tcb * tcb_aux;
    
    while(list_iterator_has_next(iterator)){ // llena la lista de tcb con los tcb que vinieron dentro del pcb
        tcb_aux = list_iterator_next(iterator);
        list_add(memoria_usuario->lista_tcb, tcb_aux);
    }
}
void crear_thread(t_tcb *tcb){
    int index_pid;
    t_pcb *pcb_aux;
    index_pid = buscar_tid(memoria_usuario->lista_pcb, tcb->tid);

    pcb_aux = list_get(memoria_usuario->lista_pcb, index_pid);
    list_add(pcb_aux->listaTCB, tcb);
    list_add(memoria_usuario->lista_tcb, tcb);

}
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
	
	t_paquete *paquete_send;
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
}