#include "planificador_corto_plazo.h"
#include <syscalls.h>

int quamtun;

extern int conexion_kernel_cpu_dispatch;
extern int conexion_kernel_cpu_interrupt;
extern int ultimo_tid;

extern t_tcb* hilo_actual;  
extern t_pcb* proceso_actual;

extern t_cola_hilo* hilos_cola_ready;
extern pthread_mutex_t * mutex_hilos_cola_ready;
extern sem_t * sem_estado_hilos_cola_ready;

extern t_cola_hilo* hilos_cola_exit;
extern pthread_mutex_t * mutex_hilos_cola_exit;
extern sem_t * sem_estado_hilos_cola_exit;

extern t_cola_hilo* hilos_cola_bloqueados;
extern pthread_mutex_t * mutex_hilos_cola_bloqueados;
extern sem_t * sem_estado_hilos_cola_bloqueados;

extern t_colas_multinivel *colas_multinivel;
extern pthread_mutex_t * mutex_colas_multinivel;
extern sem_t * sem_estado_multinivel;

//extern t_config *config;
extern char* algoritmo;
extern int quantum;

extern pthread_mutex_t * mutex_socket_cpu_dispatch;
extern pthread_mutex_t * mutex_socket_cpu_interrupt;

extern struct timeval tiempo_inicio_quantum; 
extern pthread_mutex_t mutex_tiempo_inicio;

void* planificador_corto_plazo_hilo(void* arg) {
    if (strcmp(algoritmo, "FIFO") == 0) {
        corto_plazo_fifo();
    } else if (strcmp(algoritmo, "PRIORIDADES") == 0) {
        corto_plazo_prioridades();
    } else if (strcmp(algoritmo, "CMN") == 0) {
        corto_plazo_colas_multinivel();
    } else {
        printf("Error: Algoritmo no reconocido.\n");
    }

}

void encolar_hilo_corto_plazo(t_tcb * hilo){

    log_info(logger,"Se crea la peticion para memoria del hilo %d", hilo->tid);
    t_peticion *peticion = malloc(sizeof(t_peticion));
    peticion->tipo = THREAD_CREATE_OP;
    peticion->proceso = obtener_pcb_por_pid(hilo->pid);
    peticion->hilo = hilo;
    peticion->respuesta_recibida = false;
    log_info(logger,"Se encola la peticion para memoria del hilo %d", hilo->tid);
    encolar_peticion_memoria(peticion);

    if (strcmp(algoritmo, "FIFO") == 0) {
        encolar_corto_plazo_fifo(hilo);
    } else if (strcmp(algoritmo, "PRIORIDADES") == 0) {
        encolar_corto_plazo_prioridades(hilo);
    } else if (strcmp(algoritmo, "CMN") == 0) {
        encolar_corto_plazo_multinivel(hilo);
    } else {
        printf("Error: Algoritmo no reconocido.\n");
    }
}

void corto_plazo_fifo(){
    while (1){   
        log_info(logger, "Entraste al While del planificador corto plazo FIFO");
        sem_wait(sem_estado_hilos_cola_ready);
        pthread_mutex_lock(mutex_hilos_cola_ready);
        t_tcb *hilo = desencolar_hilos_fifo();
        hilo->estado = EXEC; 
        pthread_mutex_unlock(mutex_hilos_cola_ready);
        pthread_mutex_lock(mutex_socket_cpu_dispatch);
        log_info(logger, "Se envía el hilo al cpu dispatch");
        enviar_a_cpu_dispatch(hilo->tid, hilo->pid);
        log_info(logger,"Cola de FIFO: Ejecutando hilo TID=%d, PID=%d\n", hilo->tid, hilo->pid);
        recibir_motivo_devolucion_cpu();
        pthread_mutex_unlock(mutex_socket_cpu_dispatch);
    }
}

void encolar_corto_plazo_fifo(t_tcb * hilo){
    pthread_mutex_lock(mutex_hilos_cola_ready);
    list_add(hilos_cola_ready->lista_hilos, hilo);
    pthread_mutex_unlock(mutex_hilos_cola_ready);
    sem_post(sem_estado_hilos_cola_ready);
    log_info(logger, "Se hace un post del semáforo de estado de hilos_cola_ready");
}

t_tcb* desencolar_hilos_fifo(){
    t_tcb *hilo = list_remove(hilos_cola_ready->lista_hilos, 0);
    return hilo;
}

void corto_plazo_prioridades()
{
    while(1){
        log_info(logger, "Entraste al While del planificador corto plazo Prioridades");
        sem_wait(sem_estado_hilos_cola_ready);
        pthread_mutex_lock(mutex_hilos_cola_ready);
        list_sort(hilos_cola_ready->lista_hilos, (void *)comparar_prioridades); 
        t_tcb *hilo = desencolar_hilos_prioridades();
        hilo->estado = EXEC;  
        pthread_mutex_unlock(mutex_hilos_cola_ready);
        pthread_mutex_lock(mutex_socket_cpu_dispatch);
        log_info(logger, "Se envía el hilo al cpu dispatch");
        enviar_a_cpu_dispatch(hilo->tid, hilo->pid);
        log_info(logger,"Cola de PRIORIDADES %d: Ejecutando hilo TID=%d, PID=%d\n", hilo->prioridad,hilo->tid, hilo->pid);
        recibir_motivo_devolucion_cpu();
        pthread_mutex_unlock(mutex_socket_cpu_dispatch);
    }
}

void encolar_corto_plazo_prioridades(t_tcb * hilo){
    pthread_mutex_lock(mutex_hilos_cola_ready);
    int i = 0;
    while (i < list_size(hilos_cola_ready->lista_hilos)) {
        t_tcb *hilo_existente = list_get(hilos_cola_ready->lista_hilos, i);
        if (hilo->prioridad < hilo_existente->prioridad) {
            break;
        }
        i++;
    }
    list_add_in_index(hilos_cola_ready->lista_hilos, i, hilo);
    pthread_mutex_unlock(mutex_hilos_cola_ready);
    sem_post(sem_estado_hilos_cola_ready);
    log_info(logger, "Se hace un post del semáforo de estado de hilos_cola_ready");
}

t_tcb* desencolar_hilos_prioridades()
{
    list_sort(hilos_cola_ready->lista_hilos, (void*)comparar_prioridades);
    t_tcb *hilo = list_remove(hilos_cola_ready->lista_hilos, 0);
    return hilo;
}

int comparar_prioridades(t_tcb *a, t_tcb *b) {
    return a->prioridad - b->prioridad;
}

void corto_plazo_colas_multinivel() {
    while (1) {
        log_info(logger, "Entraste al While del planificador corto plazo CMN");
        int replanificar = 0; // Variable para saber si hay que replanificar
        sem_wait(sem_estado_multinivel);
        pthread_mutex_lock(mutex_colas_multinivel);
        t_nivel_prioridad *nivel_a_ejecutar = NULL;
        t_cola_hilo *cola_a_ejecutar = buscar_cola_menor_prioridad(colas_multinivel, &nivel_a_ejecutar);

        // Si encontramos una cola válida para ejecutar
        if (cola_a_ejecutar != NULL && nivel_a_ejecutar != NULL) {
            // Extraemos el primer hilo en la cola de menor prioridad
            t_tcb *hilo_a_ejecutar = list_remove(cola_a_ejecutar->lista_hilos, 0);
            log_info(logger,"Cola de prioridad %d: Ejecutando hilo TID=%d, PID=%d\n", nivel_a_ejecutar->nivel_prioridad, hilo_a_ejecutar->tid, hilo_a_ejecutar->pid);
            hilo_a_ejecutar->estado = EXEC;  
            ejecutar_round_robin(hilo_a_ejecutar);
            // Indicamos que hay que replanificar, ya que se ha ejecutado un hilo
            replanificar = 1;
        }
        pthread_mutex_unlock(mutex_colas_multinivel);
        if (!replanificar) {
            usleep(100); 
        }
        // Si se ejecutó un hilo, volvemos a planificar
        if (replanificar) {
            continue;
        }
    }
}

t_cola_hilo* buscar_cola_menor_prioridad(t_colas_multinivel *multinivel, t_nivel_prioridad **nivel_a_ejecutar) {
    t_cola_hilo *cola_a_ejecutar = NULL;
    *nivel_a_ejecutar = NULL;

    // Recorremos los niveles de prioridad para encontrar el nivel con la menor prioridad que tenga hilos
    for (int i = 0; i < list_size(multinivel->niveles_prioridad); i++) {
        t_nivel_prioridad *nivel = list_get(multinivel->niveles_prioridad, i);
        t_cola_hilo *cola = nivel->cola_hilos;

        // Verificamos si la cola de hilos no está vacía
        if (!list_is_empty(cola->lista_hilos)) {
            // Si no hemos encontrado una cola válida o el nivel actual tiene menor prioridad
            if (*nivel_a_ejecutar == NULL || nivel->nivel_prioridad < (*nivel_a_ejecutar)->nivel_prioridad) {
                *nivel_a_ejecutar = nivel;
                cola_a_ejecutar = cola;
            }
        }
    }

    return cola_a_ejecutar;
}

void ejecutar_round_robin(t_tcb * hilo_a_ejecutar) {

    // Enviamos el hilo a la CPU mediante el canal de dispatch
    pthread_mutex_lock(mutex_socket_cpu_dispatch);
    log_info(logger, "Se envía el hilo al cpu dispatch");
    enviar_a_cpu_dispatch(hilo_a_ejecutar->tid, hilo_a_ejecutar->pid); // Envía el TID y PID al CPU
    pthread_mutex_unlock(mutex_socket_cpu_dispatch);

    // Lanzamos un nuevo hilo que cuente el quantum y envíe una interrupción si se agota
    pthread_t thread_contador_quantum;
    pthread_create(&thread_contador_quantum, NULL, (void *)enviar_interrupcion_fin_quantum, (void *)hilo_a_ejecutar);
    //algo que cuente el quantum y que haga una resta
    recibir_motivo_devolucion_cpu(); 
    pthread_join(thread_contador_quantum, NULL); // Esperamos que el hilo del quantum termine
}

// Función para contar el quantum y enviar una interrupción si se agota
void enviar_interrupcion_fin_quantum(void *hilo_void) {
    t_tcb* hilo = (t_tcb*) hilo_void;

    pthread_mutex_lock(&mutex_tiempo_inicio);
    gettimeofday(&tiempo_inicio_quantum, NULL); // Marcar el inicio del quantum
    pthread_mutex_unlock(&mutex_tiempo_inicio);

    usleep(hilo->quantum_restante);
    // Si el quantum se agotó, enviamos una interrupción al CPU por el canal de interrupt
    // Si el hilo finalizó, igual se manda la interrupcion por Fin de Quantum, pero la CPU chequea que ese hilo ya terminó y la desestima
    pthread_mutex_lock(mutex_socket_cpu_interrupt);
    //semáforo para modificar el quantum
    enviar_a_cpu_interrupt(hilo->tid, FIN_QUANTUM);
    pthread_mutex_unlock(mutex_socket_cpu_interrupt);
    log_info(logger, "Interrupción por fin de quantum enviada para el TID %d", hilo->tid);
}

void encolar_corto_plazo_multinivel(t_tcb* hilo) {
    int prioridad = hilo->prioridad;

    if (colas_multinivel == NULL) {
        perror("Error: colas_multinivel no está inicializado");
        exit(EXIT_FAILURE);
    }

    if (colas_multinivel->niveles_prioridad == NULL) {
        perror("Error: niveles_prioridad no está inicializado");
        exit(EXIT_FAILURE);
    }

    // Buscar si existe un nivel de prioridad correspondiente
    t_nivel_prioridad* nivel = NULL;
    for (int i = 0; i < list_size(colas_multinivel->niveles_prioridad); i++) {
        t_nivel_prioridad* actual = list_get(colas_multinivel->niveles_prioridad, i);
        if (actual->nivel_prioridad == prioridad) {
            nivel = actual;
            break;
        }
    }
    if (nivel == NULL) {
        printf("Nivel de prioridad %d no existe. Creando nueva cola...\n", prioridad);

        // Crear una nueva cola de hilos
        t_cola_hilo* nueva_cola = malloc(sizeof(t_cola_hilo));
        if (nueva_cola == NULL) {
            perror("Error al asignar memoria para nueva cola de hilos");
            exit(EXIT_FAILURE);
        }

        nueva_cola->nombre_estado = READY;
        nueva_cola->lista_hilos = list_create();
        if (nueva_cola->lista_hilos == NULL) {
            perror("Error al crear lista de hilos en la nueva cola");
            free(nueva_cola);
            exit(EXIT_FAILURE);
        }

        // Crear un nuevo nivel de prioridad y asociarlo con la nueva cola
        t_nivel_prioridad* nuevo_nivel = malloc(sizeof(t_nivel_prioridad));
        if (nuevo_nivel == NULL) {
            perror("Error al asignar memoria para nuevo nivel de prioridad");
            list_destroy(nueva_cola->lista_hilos);
            free(nueva_cola);
            exit(EXIT_FAILURE);
        }

        nuevo_nivel->nivel_prioridad = prioridad;
        nuevo_nivel->cola_hilos = nueva_cola;

        // Agregar el nuevo nivel de prioridad a la lista de niveles
        list_add(colas_multinivel->niveles_prioridad, nuevo_nivel);

        // Encolar el hilo en la nueva cola
        list_add(nueva_cola->lista_hilos, hilo);
        printf("Hilo de prioridad %d encolado en la nueva cola\n", prioridad);
    } else {
        // Si ya existe el nivel, encolamos el hilo en la cola correspondiente
        list_add(nivel->cola_hilos->lista_hilos, hilo);
        printf("Hilo de prioridad %d encolado en la cola existente\n", prioridad);
    }
}

bool nivel_existe_por_prioridad(void* elemento, void* contexto) {
    t_nivel_prioridad* nivel = (t_nivel_prioridad*) elemento;
    int* prioridad_buscada = (int*) contexto;
    return nivel->nivel_prioridad == *prioridad_buscada;
}

void enviar_a_cpu_dispatch(int tid, int pid)
{
    t_paquete * send_handshake = crear_paquete(INFO_HILO);
    agregar_a_paquete(send_handshake, &tid, sizeof(tid)); 
    agregar_a_paquete(send_handshake, &pid, sizeof(pid)); 
    hilo_actual = obtener_tcb_por_tid(tid);
    enviar_paquete(send_handshake, conexion_kernel_cpu_dispatch); 
    eliminar_paquete(send_handshake);
    //Se espera la respuesta, primero el tid y luego el motivo
    recibir_motivo_devolucion_cpu();
}

void enviar_a_cpu_interrupt(int tid, protocolo_socket motivo) {
    t_paquete *send_interrupt;
    switch (motivo)
    {
    case FIN_QUANTUM:
        send_interrupt = crear_paquete(FIN_QUANTUM);
        agregar_a_paquete(send_interrupt, &tid, sizeof(tid)); 
        enviar_paquete(send_interrupt, conexion_kernel_cpu_interrupt);
        log_info(logger, "## - Interrupción por FIN DE QUANTUM del hilo [%d] enviada a CPU", tid);
        eliminar_paquete(send_interrupt);
        break;
    case THREAD_JOIN_OP:
        send_interrupt = crear_paquete(THREAD_JOIN_OP);
        agregar_a_paquete(send_interrupt, &tid, sizeof(tid)); 
        enviar_paquete(send_interrupt, conexion_kernel_cpu_interrupt);
        log_info(logger, "## - Interrupción por THREAD JOIN del hilo [%d] enviada a CPU", tid);
        eliminar_paquete(send_interrupt);
        break;
    default:
        log_warning(logger, "El motivo de envio del hilo con tid [%d] a CPU INTERRUPT es incorrecto.", tid);
        break;
    }
}

void recibir_motivo_devolucion_cpu() {
    struct timeval actual;
    int tiempo_transcurrido;
    
    pthread_mutex_lock(&mutex_tiempo_inicio);
    gettimeofday(&actual, NULL);

    // Calcular tiempo transcurrido en milisegundos
    tiempo_transcurrido = (actual.tv_sec - tiempo_inicio_quantum.tv_sec) * 1000
                        + (actual.tv_usec - tiempo_inicio_quantum.tv_usec) / 1000;
    pthread_mutex_unlock(&mutex_tiempo_inicio);
    
    t_list *paquete_respuesta = recibir_paquete(conexion_kernel_cpu_interrupt);
    int tid;
    protocolo_socket * motivo;
    motivo  = list_remove(paquete_respuesta, 0);
    char* nombre_mutex;
    int tiempo;
    int pid;
    char * nombre_archivo;
    int prioridad;
    int tamanio;
    switch (*motivo) {
        case FINALIZACION:
            log_info(logger, "El hilo %d ha FINALIZADO correctamente\n", tid);
            desbloquear_hilos(tid);
            break;

        case FIN_QUANTUM:
            log_info(logger, "El hilo %d fue desalojado por FIN DE QUANTUM\n", tid);
            actualizar_quantum(int tiempo_transcurrido);
            encolar_corto_plazo_multinivel(hilo_actual);
            break;

        case THREAD_JOIN_OP:
            log_info(logger, "El hilo %d fue bloqueado por THREAD JOIN\n", tid);
            tid = (intptr_t)list_remove(paquete_respuesta, 0);
            // Transicionar el hilo al estado block (se hace en la syscall) y esperar a que termine el otro hilo para poder seguir ejecutando
            actualizar_quantum(int tiempo_transcurrido);
            THREAD_JOIN(tid);
            esperar_desbloqueo_ejecutar_hilo(tid);
            break;

        case MUTEX_CREATE_OP:
            nombre_mutex = list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d está creando un nuevo mutex\n", tid);
            actualizar_quantum(int tiempo_transcurrido);
            MUTEX_CREATE(nombre_mutex);
            break;

        case MUTEX_LOCK_OP:
            nombre_mutex = list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d está intentando adquirir un mutex\n", tid);
            actualizar_quantum(int tiempo_transcurrido);
            MUTEX_LOCK(nombre_mutex);
            break;

        case MUTEX_UNLOCK_OP:
            nombre_mutex = list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d está intentando liberar un mutex\n", tid);
            actualizar_quantum(int tiempo_transcurrido);
            MUTEX_UNLOCK(nombre_mutex);
            break;

        case IO_SYSCALL:
            tiempo = (intptr_t)list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d ejecuta un IO\n", tid);
            actualizar_quantum(int tiempo_transcurrido);
            IO(tiempo, hilo_actual->tid);
            break;   

        case DUMP_MEMORY_OP:
            log_info(logger, "El hilo %d lanza un dump del proceso padre\n", tid);
            pid = proceso_actual->pid;
            actualizar_quantum(int tiempo_transcurrido);
            DUMP_MEMORY(pid);
            break;   

        case PROCESS_CREATE_OP:
            nombre_archivo = list_remove(paquete_respuesta, 0);
            FILE * archivo = fopen(nombre_archivo, "r");
            tamanio = (intptr_t)list_remove(paquete_respuesta, 0);
            prioridad = (intptr_t)list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d inició un PROCESS CREATE\n", tid);
            pid = proceso_actual->pid;
            actualizar_quantum(int tiempo_transcurrido);
            PROCESS_CREATE(archivo, tamanio, prioridad);
            break;   
        
        case PROCESS_EXIT_OP:
            log_info(logger, "El hilo %d inició un PROCESS EXIT\n", tid);
            pid = proceso_actual->pid;
            PROCESS_EXIT();
            break;   
        
        case THREAD_CANCEL_OP:
            tid = (intptr_t)list_remove(paquete_respuesta, 0);
            log_info(logger, "El hilo %d inició un THREAD CANCEL\n", tid);
            THREAD_CANCEL(tid);
            break;   

        case THREAD_EXIT_OP:
            log_info(logger, "El hilo %d inició un THREAD EXIT\n", tid);
            THREAD_EXIT();
            break;   
        case SEGMENTATION_FAULT:
            log_info(logger, "El hilo %d es finalizado por SEGMENTATION FAULT\n", hilo_actual->tid);
            PROCESS_EXIT();
            break;
        default:
            log_warning(logger, "Motivo desconocido para el hilo %d\n", tid);
            break;
    }
}

void actualizar_quantum(int tiempo_transcurrido){
    hilo_actual->quantum_restante -= tiempo_transcurrido;
    if (hilo_actual->quantum_restante < 0) {
                hilo_actual->quantum_restante = quantum;
            }
}

void desbloquear_hilos(int tid) {
    t_tcb* hilo_finalizado = obtener_tcb_por_tid(tid);
    if (!hilo_finalizado) {
        log_warning(logger,"Error: No se encontró el hilo con TID %d\n", tid);
        return;
    }
    // Iterar sobre los hilos en la lista de espera
    for (int i = 0; i < list_size(hilo_finalizado->lista_espera); i++) {
        t_tcb* hilo_bloqueado = list_get(hilo_finalizado->lista_espera, i);

        if (!hilo_bloqueado) {
            log_warning(logger,"El hilo TID %d no bloquea ningún hilo\n", tid);
            continue;
        }
        sem_post(hilo_bloqueado->cant_hilos_block);
        int sem_valor;
        sem_getvalue(hilo_bloqueado->cant_hilos_block, &sem_valor);

        if (sem_valor == 1) {
            printf("Hilo TID %d listo para ejecutar.\n", hilo_bloqueado->tid);
        }
    }
    list_clean(hilo_finalizado->lista_espera);
}

void esperar_desbloqueo_ejecutar_hilo(int tid){
    t_tcb* hilo_esperando = obtener_tcb_por_tid(tid);
    sem_wait(hilo_esperando->cant_hilos_block);
    pthread_mutex_lock(mutex_socket_cpu_dispatch); 
    enviar_a_cpu_dispatch(hilo_esperando->tid, hilo_esperando->pid);   
    pthread_mutex_unlock(mutex_socket_cpu_dispatch); 
}

t_cola_hilo* inicializar_cola_hilo(t_estado estado) {
    char* valor;
    switch (estado) {
        case NEW:   
            valor = "NEW";
            break;
        case READY: 
            valor = "READY";
            break;
        case EXEC:  
            valor = "EXEC";
            break;
        case BLOCK: 
            valor = "BLOCK";
            break;
        case EXIT:  
            valor = "EXIT";
            break;
        default:    
            valor = "DESCONOCIDO";  // En caso de que el valor no sea válido
            break;
    }

    t_cola_hilo *cola = malloc(sizeof(t_cola_hilo));
    if (cola == NULL) {
        perror("Error al asignar memoria para cola de hilos");
        exit(EXIT_FAILURE);
    }

    cola->nombre_estado = estado;
    cola->lista_hilos = list_create();  // Crea una lista vacía
    if (cola->lista_hilos == NULL) {
        perror("Error al crear lista de hilos");
        exit(EXIT_FAILURE);
    }
    return cola;
}

void inicializar_semaforos_corto_plazo(){
    mutex_hilos_cola_ready = malloc(sizeof(pthread_mutex_t));
    if (mutex_hilos_cola_ready == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_hilos_cola_ready, NULL);

    mutex_hilos_cola_exit = malloc(sizeof(pthread_mutex_t));
    if (mutex_hilos_cola_exit == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_hilos_cola_exit, NULL);

    mutex_hilos_cola_bloqueados = malloc(sizeof(pthread_mutex_t));
    if (mutex_hilos_cola_bloqueados == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_hilos_cola_bloqueados, NULL);
    

    mutex_colas_multinivel = malloc(sizeof(pthread_mutex_t));
    if (mutex_colas_multinivel == NULL) {
        perror("Error al asignar memoria para mutex de cola");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(mutex_colas_multinivel, NULL);

    sem_estado_hilos_cola_ready = malloc(sizeof(sem_t));
    if (sem_estado_hilos_cola_ready == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_hilos_cola_ready, 0, 0);

    sem_estado_hilos_cola_exit = malloc(sizeof(sem_t));
    if (sem_estado_hilos_cola_exit == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_hilos_cola_exit, 0, 0);

    sem_estado_hilos_cola_bloqueados = malloc(sizeof(sem_t));
    if (sem_estado_hilos_cola_bloqueados == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_hilos_cola_bloqueados, 0, 0);

    sem_estado_multinivel = malloc(sizeof(sem_t));
    if (sem_estado_multinivel == NULL) {
        perror("Error al asignar memoria para semáforo de cola");
        exit(EXIT_FAILURE);
    }
    sem_init(sem_estado_multinivel, 0, 0);

    log_info(logger,"Mutex's y semáforos de estado para las colas de hilos en READY/EXIT/BLOCK creados\n");
    log_info(logger,"Mutex y semáforo de estado para las colas de hilos Multinivel creados\n");

}

t_colas_multinivel* inicializar_colas_multinivel() {
    log_info(logger,"Cola Multinivel creada\n");
    t_colas_multinivel *colas_multinivel = malloc(sizeof(t_colas_multinivel));
    if (colas_multinivel == NULL) {
        perror("Error al asignar memoria para colas multinivel");
        exit(EXIT_FAILURE);
    }
    colas_multinivel->niveles_prioridad = list_create();
    if (colas_multinivel->niveles_prioridad == NULL) {
        perror("Error al crear lista de niveles de prioridad");
        exit(EXIT_FAILURE);
    }
    return colas_multinivel;
}
