#ifndef PCB_H_
#define PCB_H_

#include <stdint.h> 
#include <utils/utils.h>


typedef enum  
{
    NEW,
    READY,
    EXEC,
    BLOCK,
    EXIT
}t_estado;

typedef struct{
    uint32_t PC;
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t esp;
}RegistroCPU;

typedef struct {
    uint32_t parametros[2];
    char* ID_instruccion;
    int parametros_validos;
} t_instruccion;

typedef struct {
    int pid;
    int pc;
    t_list *listaTCB;
    t_list *listaMUTEX;
} pcb;

typedef struct {
    int tid;
    int prioridad;
    t_estado estado;
} tcb;

pcb* crear_pcb();
tcb* crear_tcb(int tid, int prioridad);
void cambiar_estado(tcb* tcb_p, t_estado estado);

#endif