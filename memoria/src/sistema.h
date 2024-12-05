#ifndef SISTEMA_H_
#define SISTEMA_H_

#include <stdint.h> 
#include <utils/utils.h>
#include <main.h>

#define SIZE_PARTICION 40
#define OK_MEMORIA 1

//cpu
void enviar_contexto(void);
uint32_t read_memory(uint32_t direccion);
int write_memory(uint32_t direccion, uint32_t valor);
void recibir_contexto();
t_paquete *obtener_contexto(int pid, int tid);
void actualizar_contexto_ejecucion(void);
void enviar_error_actualizacion();
int buscar_pid(t_list *lista, int pid);
int buscar_tid(t_list *lista, int tid);

void error_contexto(char *error);

void crear_proceso(t_pcb *pcb);
void fin_proceso(int pid);

void inicializar_tabla_particion_fija(t_list *particiones);
void init_tablas_dinamicas();
int buscar_en_tabla_fija(int tid);
int buscar_en_dinamica(int pid);
int agregar_a_tabla_particion_fija(t_pcb *pcb);
int agregar_a_dinamica(t_pcb *pcb);

void remover_proceso_de_tabla_dinamica(int pid);
void consolidar_huecos();

void crear_thread(t_tcb *tcb);
void fin_thread(int tid);

int obtener_instruccion(int PC, int tid);
void liberar_lista_paquetes(t_list *lista);

typedef struct arg_peticion_memoria{
    int socket;
    protocolo_socket cod_op;
}arg_peticion_memoria;

#endif