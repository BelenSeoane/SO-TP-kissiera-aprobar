#include "../include/swap.h"

void* atender_pedidos_swap() {

    // Crea directorio si no existe
    if (access(config.PATH_SWAP, F_OK) == 0) {
        log_info(logger, "El directorio %s ya existe. Borrando directorio...", config.PATH_SWAP);
        if(rmdir(config.PATH_SWAP) == -1) {
            log_error(logger, "El directorio %s no pudo borrarse porque no está vacío", config.PATH_SWAP);
        }
    }
    mkdir(config.PATH_SWAP, 0777);

    while(1) {
        sem_wait(&swap_esta_libre);
        sem_wait(&realizar_op_de_swap);

        log_info(logger, "CONECTANDO CON SWAP...");
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
		        }
                
                ftruncate(fileno(f), pedido->tamanio_proceso);             

                fclose(f);
                free(path);

                break;

            case SWAP_IN_PAGINA:
                log_info(logger, "SWAP recibe SWAP IN para proceso %d, pag %d y la va a cargar en el frame %d", pedido->pid, pedido->numero_pagina, pedido->frame_libre);   

                char* file_name = get_file_name(pedido->pid);
                FILE *fp = fopen(file_name, "r");

                if(fp == NULL) {
                    log_error(logger, "Error al abrir archivo %s, cerrando modulo swap", file_name);
                    exit(1);
                }
      
                fseek(fp, pedido->numero_pagina * config.TAM_PAGINA, SEEK_SET);
                log_info(logger, "El puntero esta en la pos: %ld y va a leer %d bytes", ftell(fp), config.TAM_PAGINA);

                void* buffer = malloc(config.TAM_PAGINA);
                fread(buffer, config.TAM_PAGINA, 1, fp);

                pthread_mutex_lock(&mutex_memoria);
                memcpy(memoria_principal + (pedido->frame_libre * config.TAM_PAGINA), buffer, config.TAM_PAGINA);
                pthread_mutex_unlock(&mutex_memoria);

                //log_info(logger, "Despues de leer el puntero esta en la pos: %ld", ftell(fp));
                log_info(logger, "Se copio la pagina %d a memoria en el frame %d", pedido->numero_pagina, pedido->frame_libre);

                free(file_name);
                free(buffer);
                fclose(fp);
                
                break;

            case SWAP_OUT_PAGINA:
                log_info(logger, "SWAP recibe SWAP OUT para proceso %d y pagina %d", pedido->pid, pedido->numero_pagina);
                log_info(logger, "La pagina a escribir se encuentra en el marco %d", pedido->frame_libre);

                char* file_name_swap_out = get_file_name(pedido->pid);
                FILE *fp_swap_out = fopen(file_name_swap_out, "rw");

                if(fp_swap_out == NULL) {
                    log_error(logger, "Error al abrir archivo %s, cerrando modulo swap", file_name_swap_out);
                    exit(1);
                }
      
                fseek(fp_swap_out, pedido->numero_pagina * config.TAM_PAGINA, SEEK_SET);
                log_info(logger, "El puntero esta en la pos: %ld y va a escribir %d bytes", ftell(fp_swap_out), config.TAM_PAGINA);

                void* buffer_swap_out = malloc(config.TAM_PAGINA);

                pthread_mutex_lock(&mutex_memoria);
                memcpy(buffer_swap_out, memoria_principal + (pedido->frame_libre * config.TAM_PAGINA), config.TAM_PAGINA);
                pthread_mutex_unlock(&mutex_memoria);

                fwrite(buffer_swap_out, config.TAM_PAGINA, 1, fp_swap_out);

                log_info(logger, "Se escribio el frame %d en la pagina %d del archivo swap de %d", pedido->frame_libre, pedido->numero_pagina, pedido->pid);

                free(file_name_swap_out);
                free(buffer_swap_out);
                fclose(fp_swap_out);

                break;

            case ELIMINAR_ARCHIVO_SWAP: ;
                char* archivo_a_eliminar = get_file_name(pedido->pid);

                if (remove(archivo_a_eliminar) == 0) {
                    log_info(logger, "El archivo %s ha sido eliminado", archivo_a_eliminar);
                } else {
                    log_error(logger, "Error al eliminar el archivo %s", archivo_a_eliminar);
                }

                free(archivo_a_eliminar);

                break;

            default:
                log_warning_sh(logger, "Operacion desconocida de Swap");
                break;
        }
        sem_post(&pedido->pedido_finalizado);
        sem_post(&swap_esta_libre);
        
        sem_destroy(&pedido->pedido_finalizado);
        free(pedido);
    }
}