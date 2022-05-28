#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_

#include <stdio.h>
#include <stdlib.h>
#include "../../shared/include/shared.h"
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include "pcb.h"
#include "utils.h"
#include <semaphore.h>
#include "serializacion.h"

void crear_y_poner_proceso_en_new(int, void*, int, int);
void* intentar_pasar_de_new_a_ready();
void pasar_de_new_a_ready(); 
void* pasar_de_ready_susp_a_ready();

#endif 