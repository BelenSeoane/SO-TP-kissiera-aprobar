#include "../include/manejo_memoria.h"


uint32_t asignar_memoria_y_estructuras(int pid, int tamanio_proceso) {

    // Despues delegar a funciones para hacer más prolijo
    uint32_t cant_max_marcos = (uint32_t) ceil((double)config.MARCOS_POR_PROCESO); //A todos se les asigna esto independientemente de su tamaño
    uint32_t cant_marcos = (uint32_t) ceil((double)tamanio_proceso / config.TAM_PAGINA);
    uint32_t cant_tablas_segundo_nivel = (uint32_t) ceil((double)cant_marcos / config.ENTRADAS_POR_TABLA);
    uint32_t cant_entradas_tabla_primer_nivel = config.ENTRADAS_POR_TABLA;

    // Cada proceso tiene una tabla de primer nivel
    Tabla_Primer_Nivel* tabla_primer_nivel = malloc(sizeof(Tabla_Primer_Nivel));
    tabla_primer_nivel->pid = pid;
    tabla_primer_nivel->entradas_tabla_primer_nivel = list_create();

    for (int i = 0; i < cant_entradas_tabla_primer_nivel; i++) {

        Entrada_Tabla_Primer_Nivel* entrada_primer_nivel = malloc(sizeof(Entrada_Tabla_Primer_Nivel));
            
        // Por cada entrada, creo una tabla de segundo nivel que va a ser la asociada
        Tabla_Segundo_Nivel* tabla_segundo_nivel = malloc(sizeof(Tabla_Segundo_Nivel));
        tabla_segundo_nivel->entradas_tabla_segundo_nivel = list_create();

        // Cada tabla de segundo nivel va a tener la misma cantidad de entradas
        // aunque algunas no las use
        for(int k = 0; k < config.ENTRADAS_POR_TABLA; k++) {
                           
            // Esto despues se llenaria cuando se carga la pagina en el espacio de usuario
            Entrada_Tabla_Segundo_Nivel* entrada = malloc(sizeof(Entrada_Tabla_Segundo_Nivel));
            entrada->marco = 0;
            entrada->bit_presencia = 0;
            entrada->bit_modificado = 0;
            entrada->bit_uso = 0;
            entrada->bit_puntero = 0;
            list_add(tabla_segundo_nivel->entradas_tabla_segundo_nivel, entrada);
        } 
            
        // La tabla se 2do nivel esta completa, la agrego a la lista
		pthread_mutex_lock(&mutex_lista_segundo_nivel);
        entrada_primer_nivel->index_tabla_segundo_nivel = list_add(lista_tablas_segundo_nivel, tabla_segundo_nivel);
        pthread_mutex_unlock(&mutex_lista_segundo_nivel);
    
        // Con la direccion que me dio, creo la entrada de 1er nivel y la agrego a la tabla
        list_add(tabla_primer_nivel->entradas_tabla_primer_nivel, entrada_primer_nivel);
    
    }
    
    // La tabla de primer nivel esta llena, la agrego a la lista
    pthread_mutex_lock(&mutex_lista_primer_nivel);
    list_add_in_index(lista_tablas_primer_nivel, pid, tabla_primer_nivel);
    pthread_mutex_unlock(&mutex_lista_primer_nivel);

    // Estos valores son los reales que necesita el proceso. 
    // Pero la tabla se llena con todas las entradas aunque no las use
    log_info(logger, "Asigno %d marcos", cant_marcos);
    log_info(logger, "Asigno %d entradas en la tabla de primer nivel", cant_entradas_tabla_primer_nivel);
    log_info(logger, "Asigno %d tablas de segundo nivel\n\n", cant_tablas_segundo_nivel);

    verificar_memoria();

    // Pedir INIT a swap
    pedido_swap *pedido = malloc(sizeof(pedido_swap));
	pedido->co_op = INIT_PROCESO;
    pedido->pid = pid;
    pedido->tamanio_proceso = tamanio_proceso;
    sem_init(&pedido->pedido_finalizado, 0, 0);

	pthread_mutex_lock(&mutexColaSwap);
	list_add(cola_pedidos_a_swap, pedido);
	pthread_mutex_unlock(&mutexColaSwap);
    sem_post(&realizar_op_de_swap);

    sem_wait(&pedido->pedido_finalizado);
    return pid;
}

void verificar_memoria() {

    pthread_mutex_lock(&mutex_lista_primer_nivel);
    pthread_mutex_lock(&mutex_lista_segundo_nivel);

    //log_info(logger, "Hay %d tablas de primer nivel", list_size(lista_tablas_primer_nivel));
    //log_info(logger, "Hay %d tablas de segundo nivel\n\n", list_size(lista_tablas_segundo_nivel));

    mostrar_lista_tablas_primer_nivel();

    pthread_mutex_unlock(&mutex_lista_primer_nivel);
    pthread_mutex_unlock(&mutex_lista_segundo_nivel);

}


void mostrar_lista_tablas_primer_nivel() {

    log_info(logger, "-- ESTADO TABLAS DE PAGINAS --");

    t_list_iterator* iterator = list_iterator_create(lista_tablas_primer_nivel);
    Tabla_Primer_Nivel* elem_iterado;

    while(list_iterator_has_next(iterator)) {
        elem_iterado = list_iterator_next(iterator);
        log_info(logger, "PID %d", elem_iterado->pid);

        t_list_iterator* iterator2 = list_iterator_create(elem_iterado->entradas_tabla_primer_nivel);
        Entrada_Tabla_Primer_Nivel* elem_iterado_2;
        
        while(list_iterator_has_next(iterator2)) {
            elem_iterado_2 = list_iterator_next(iterator2);
            Tabla_Segundo_Nivel* tabla_segundo_nivel = list_get(lista_tablas_segundo_nivel, elem_iterado_2->index_tabla_segundo_nivel);
            log_info(logger, "\tIndex en lista 2do nivel: %d", elem_iterado_2->index_tabla_segundo_nivel);
            
            t_list_iterator* iterator3 = list_iterator_create(tabla_segundo_nivel->entradas_tabla_segundo_nivel);
            Entrada_Tabla_Segundo_Nivel* elem_iterado_3;
            
            while(list_iterator_has_next(iterator3)) {
                elem_iterado_3 = list_iterator_next(iterator3);
                log_info(logger, "\tM %d | P %d | M %d | U %d |* %d", elem_iterado_3->marco, elem_iterado_3->bit_presencia, elem_iterado_3->bit_modificado, elem_iterado_3->bit_uso, elem_iterado_3->bit_puntero);
            }

            list_iterator_destroy(iterator3);
            
        }
        
        list_iterator_destroy(iterator2);
        printf("\n"); // Horrible despues sacar

    }

    list_iterator_destroy(iterator);

}

 
void finalizar_estructuras_del_proceso_y_avisar_a_kernel(int index_tabla_primer_nivel, int socket){
    
    pthread_mutex_lock(&mutex_lista_primer_nivel);
    Tabla_Primer_Nivel* t_primer_nivel = list_get(lista_tablas_primer_nivel, index_tabla_primer_nivel);
    pthread_mutex_unlock(&mutex_lista_primer_nivel);

    int marcos_a_liberar = paginas_con_marco_cargado_presente(index_tabla_primer_nivel);
    pthread_mutex_lock(&mutex_lista_segundo_nivel);

    log_info(logger,"Tengo que liberar %d páginas del proceso %d", marcos_a_liberar, index_tabla_primer_nivel);
    for(int i=0; i<t_primer_nivel->entradas_tabla_primer_nivel->elements_count; i++){
        Entrada_Tabla_Primer_Nivel * entrada_primer_nivel = list_get(t_primer_nivel->entradas_tabla_primer_nivel, i);
        Tabla_Segundo_Nivel * tabla_segundo_nivel = list_get(lista_tablas_segundo_nivel, entrada_primer_nivel->index_tabla_segundo_nivel);

        for(int j=0; j<tabla_segundo_nivel->entradas_tabla_segundo_nivel->elements_count; j++){

            Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel = list_get(tabla_segundo_nivel->entradas_tabla_segundo_nivel, j);
            entrada_segundo_nivel -> bit_presencia;
            if(entrada_segundo_nivel->bit_presencia == 1) {

                entrada_segundo_nivel->marco = 0;
                entrada_segundo_nivel->bit_presencia = 0;
                entrada_segundo_nivel->bit_modificado = 0;
                entrada_segundo_nivel->bit_uso = 0;
                entrada_segundo_nivel->bit_puntero = 0;

                log_info(logger, "Liberación de páginas del proceso  %d : Página %d de la tabla %d de segundo nivel liberada\n\n", index_tabla_primer_nivel, j, i);
			} 
        }
        
    }
    
    log_info(logger, "Ya reinicé estructuras, ahora mando confirmación a kernel");
    pthread_mutex_unlock(&mutex_lista_segundo_nivel);
    int bytes_a_enviar = sizeof(int);
    void* a_enviar = malloc(bytes_a_enviar);
    int* codigo = malloc(sizeof(int));
    *codigo = 1;
    memcpy(a_enviar, &(*codigo), sizeof(int));
    send(socket, a_enviar, sizeof(int), 0);
    free(codigo);
    free(a_enviar);   

} 

int solicitar_pagina_a_swap(int pid, int numero_pagina) {

    int frame = buscar_frame_libre();

    pedido_swap *pedido = malloc(sizeof(pedido_swap));
	pedido->co_op = SWAP_IN_PAGINA;
    pedido->pid = pid;
    pedido->numero_pagina = numero_pagina;
    pedido->frame_libre = frame;
    sem_init(&pedido->pedido_finalizado, 0, 0);

	pthread_mutex_lock(&mutexColaSwap);
	list_add(cola_pedidos_a_swap, pedido);
	pthread_mutex_unlock(&mutexColaSwap);
    sem_post(&realizar_op_de_swap);

    log_info(logger, "Envie pedido de pagina a swap");
    sem_wait(&pedido->pedido_finalizado);

    return frame; 

}
/*
int cargar_pagina_en_memoria(int pid) {

    int frame = buscar_frame_libre();
    
    pthread_mutex_lock(&mutex_memoria);
    memcpy(memoria_principal + frame*config.TAM_PAGINA, pagina_en_intercambio, config.TAM_PAGINA); //esto lo va a hacer swap
    pthread_mutex_unlock(&mutex_memoria);
    
    log_info(logger, "Se copio la pagina en el frame %d", frame);
    return frame; 
}*/

void actualizar_tabla_de_paginas(int index_tabla_segundo_nivel, int entrada_tabla_segundo_nivel, int marco) {

    pthread_mutex_lock(&mutex_lista_segundo_nivel);
	Tabla_Segundo_Nivel* t_segundo_nivel = list_get(lista_tablas_segundo_nivel, index_tabla_segundo_nivel);
    
    t_list_iterator* iterator = list_iterator_create(t_segundo_nivel->entradas_tabla_segundo_nivel);
    Entrada_Tabla_Segundo_Nivel* elem_iterado;
            
    int i = 0;
    while(list_iterator_has_next(iterator) && i!=entrada_tabla_segundo_nivel) {
        elem_iterado = list_iterator_next(iterator);
        i++;
    }

    log_info(logger, "i es %d", i);
    elem_iterado->marco = marco;
    elem_iterado->bit_presencia = 1;
    elem_iterado->bit_modificado = 0;
    elem_iterado->bit_uso = 1;
    elem_iterado->bit_puntero = 0;
	pthread_mutex_unlock(&mutex_lista_segundo_nivel);

    list_iterator_destroy(iterator);
    log_info(logger, "Se actualizo la tabla de paginas\n\n");

}

/*bool comparator_orden_de_carga(void* elem1, void* elem2){
    Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel_1 = elem1;
    Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel_2 = elem2;
    return entrada_segundo_nivel_1->orden_de_carga < entrada_segundo_nivel_2->orden_de_carga;
    
}*/

/*void ordenar_lista_con_paginas_cargadas_segun_orden_de_carga(){
    list_sort(lista_paginas_cargadas_en_orden, comparator_orden_de_carga);
    Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel_iter;
    log_info(logger,"---\n\nInicio comprobación lista de carga de paginas ordenada----\n");
    for(int i=0; i<lista_paginas_cargadas_en_orden->elements_count;i++){
        entrada_segundo_nivel_iter = list_get(lista_paginas_cargadas_en_orden, i);
        log_info(logger,"El orden de carga de la entrada es: %d", entrada_segundo_nivel_iter->orden_de_carga);
    }
    log_info(logger,"---\n\nFin comprobación lista de carga de paginas ordenada----\n\n");
}*/

void generar_lista_de_paginas_cargadas(int index_tabla_primer_nivel){

    pthread_mutex_lock(&mutex_lista_primer_nivel);
    Tabla_Primer_Nivel* t_primer_nivel = list_get(lista_tablas_primer_nivel, index_tabla_primer_nivel);
    pthread_mutex_unlock(&mutex_lista_primer_nivel);

    pthread_mutex_lock(&mutex_lista_segundo_nivel);
    for(int i=0; i<t_primer_nivel->entradas_tabla_primer_nivel->elements_count; i++){
        
        Entrada_Tabla_Primer_Nivel * entrada_primer_nivel = list_get(t_primer_nivel->entradas_tabla_primer_nivel, i);
        Tabla_Segundo_Nivel * tabla_segundo_nivel = list_get(lista_tablas_segundo_nivel, entrada_primer_nivel->index_tabla_segundo_nivel);

        for(int j=0; j<tabla_segundo_nivel->entradas_tabla_segundo_nivel->elements_count; j++){

            Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel = list_get(tabla_segundo_nivel->entradas_tabla_segundo_nivel, j);
            if(entrada_segundo_nivel->bit_presencia == 1) {
                list_add(lista_paginas_cargadas,entrada_segundo_nivel);
			} 
        }
        
    }
    pthread_mutex_unlock(&mutex_lista_segundo_nivel);
    log_info(logger, "Lista con páginas cargadas populada.\n\n");
}

void actualizar_bit_modificado(int pid, int marco) {

    pthread_mutex_lock(&mutex_lista_primer_nivel);
    Tabla_Primer_Nivel* t_primer_nivel = list_get(lista_tablas_primer_nivel, pid);
    pthread_mutex_unlock(&mutex_lista_primer_nivel);

    pthread_mutex_lock(&mutex_lista_segundo_nivel);
    for(int i=0; i<t_primer_nivel->entradas_tabla_primer_nivel->elements_count; i++){
        
        Entrada_Tabla_Primer_Nivel * entrada_primer_nivel = list_get(t_primer_nivel->entradas_tabla_primer_nivel, i);
        Tabla_Segundo_Nivel * tabla_segundo_nivel = list_get(lista_tablas_segundo_nivel, entrada_primer_nivel->index_tabla_segundo_nivel);

        for(int j=0; j<tabla_segundo_nivel->entradas_tabla_segundo_nivel->elements_count; j++){

            Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel = list_get(tabla_segundo_nivel->entradas_tabla_segundo_nivel, j);
            // Se podria poner para que tmb el bit de presencia sea 1 pero deberia alcanzar
            if(entrada_segundo_nivel->marco == marco) {
                entrada_segundo_nivel->bit_presencia = 1;
			} 
        }    
    }
    pthread_mutex_unlock(&mutex_lista_segundo_nivel);
    log_info(logger, "Se actualizo el bit de modificado del marco %d del proceso %d", marco, pid);
}

void actualizar_bit_puntero(index){
    index++;

    if (index >= (lista_paginas_cargadas->elements_count)){
        index = 0;
    }

    Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel = list_get(lista_paginas_cargadas, index);
    entrada_segundo_nivel-> bit_puntero = 1;
}

int buscar_index_puntero_para_aplicar_algoritmo(){

    int index = 0;
    for(int i=0; i<lista_paginas_cargadas->elements_count; i++){

        Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel = list_get(lista_paginas_cargadas, i);
        if(entrada_segundo_nivel->bit_puntero == 1) {
            index = i;
        }
    }
    return index;
}

int aplicar_algoritmo_de_sustitucion_clock(){
    
    int marco = -1;
    Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel;
    int index_puntero = buscar_index_puntero_para_aplicar_algoritmo();
    bool primera_vez = true;

    while(marco == -1){
        entrada_segundo_nivel = list_get(lista_paginas_cargadas, index_puntero);

        //Desasigno el bit inicial de puntero
        if (primera_vez){
            entrada_segundo_nivel->bit_puntero = 0;
            primera_vez = false;
        }
        //Si el bit de uso es 0 es el marco a sustituir, sino, le doy una oportunidad y sigo
        if(entrada_segundo_nivel->bit_uso == 0) {
            marco = entrada_segundo_nivel->marco;
            break;
        } else{
            entrada_segundo_nivel->bit_uso = 0;
        }
        index_puntero++;
        if (index_puntero >= (lista_paginas_cargadas->elements_count)){
            index_puntero = 0;
        }
    }
    log_info(logger, "Algoritmo Clock Finalizado: el marco a descargar es %d", marco);
    actualizar_bit_puntero(index_puntero);
    return marco;
}

int aplicar_algoritmo_de_sustitucion_clock_modificado(){

    int marco = -1;
    //TODO
    /*Entrada_Tabla_Segundo_Nivel * entrada_segundo_nivel;
    int index_puntero = buscar_index_puntero_para_aplicar_algoritmo();
    bool primera_vez = true;

    while(marco == -1){
        entrada_segundo_nivel = list_get(lista_paginas_cargadas, index_puntero);

        //Desasigno el bit inicial de puntero
        if (primera_vez){
            entrada_segundo_nivel->bit_puntero = 0;
            primera_vez = false;
        }

        //Si el bit de uso es 0 es el marco a sustituir, sino, le doy una oportunidad y sigo
        if(entrada_segundo_nivel->bit_uso == 0) {
            marco = entrada_segundo_nivel->marco;
            break;
        } else{
            entrada_segundo_nivel->bit_uso = 0;
        }

        index_puntero++;
        if (index_puntero >= (lista_paginas_cargadas->elements_count)){
            index_puntero = 0;
        }
    }*/
    return marco;
}