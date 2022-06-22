#include "../include/swap.h"

void* atender_pedidos_swap() {

    // Crea directorio si no existe
    if (access(config.PATH_SWAP, F_OK) == 0) {
        log_info(logger, "El directorio %s ya existe", config.PATH_SWAP); //lo podemos borrar
    } else {
        mkdir(config.PATH_SWAP, 0777);
    }

    while(1) {
        sem_wait(&swap_esta_libre);
        sem_wait(&realizar_op_de_swap);
        usleep(config.RETARDO_SWAP * 1000); // Retardo swap
        
        pedido_swap *pedido = list_remove(cola_pedidos_a_swap, 0); //Obtengo pedido

        switch(pedido->co_op) {
            case INIT_PROCESO:
                log_info(logger, "SWAP RECIBE INIT PARA PID %d", pedido->pid);

                char* path = get_file_name(pedido->pid);
                FILE* f = fopen(path,"w");

                if (f == NULL) {
			        log_error(logger, "Error al crear archivo para PID %d en %s", pedido->pid, path);
			        exit(EXIT_FAILURE);
		        } else {
                    ftruncate(fileno(f), pedido->tamanio_proceso);
                    list_add_in_index(lista_files, pedido->pid, f);
                }

                fclose(f);
                free(path);
                break;

            case SWAP_OUT_PAGINA:
                log_info(logger, "SWAP RECIBE SWAP OUT PARA PID %d Y PAGINA %d", pedido->pid, pedido->numero_pagina);
                
                // Copiar pagina a memoria principal

                sem_post(&swap_respondio);
                break;

            case SWAP_IN:
                log_info(logger, "SWAP RECIBE SWAP IN PARA PID %d", pedido->pid);
                break;

            default:
                log_warning_sh(logger, "Operacion desconocida de Swap");
                break;
        }
        sem_post(&swap_esta_libre);
        free(pedido);
    }
}