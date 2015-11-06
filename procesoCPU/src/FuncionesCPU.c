#include "procesoCPU.h"

char* CONFIG_PATH =
		"/home/utnso/Escritorio/git/tp-2015-2c-killthepony/procesoCPU/Debug/config.txt";
char* LOGGER_PATH = "log.txt";

//////////////////////////////////////HILO RESPONDE PORCENTAJE///////////////////

void* hilo_responder_porcentaje() {

	//int numero = *cantidad_cpu;
	t_msg* msg ;
	socket_planificador_especial = conectar_con_planificador_especial();

	if (socket_planificador_especial>0) {
		while (true) {
			log_trace(logger, "[PORCENTAJE] Esperando peticiones para responder el CPU_PORCENTAJE_UTILIZACION del planificador");
			msg = recibir_mensaje(socket_planificador_especial);

			if(msg!=NULL){
				if(msg->header.id ==CPU_PORCENTAJE_UTILIZACION){
					destroy_message(msg);
					log_trace(logger, "[PORCENTAJE] Nuevo mensaje del planificador pidiendo CPU_PORCENTAJE_UTILIZACION");
					enviar_porcentaje_a_planificador();

				}else{
					log_error(logger, "[PORCENTAJE] mensaje desconocido");
					break;
				}
			}else{
				log_error(logger, "[PORCENTAJE]: El planificador se desconecto");
				break;
			}


		}
	} else
		log_trace(logger,
				"[PORCENTAJE] Error del socket especial al conectarse con la memoria y el planificador. \n");

	return NULL;
}

///////////////////////////////////////FUNCION ENVIAR PORCENTAJE/////////////////////////

///////////////////////////////////////PORCENTAJE////////////////////////////////////////

void* hilo_porcentaje( numero) {

	porcentaje_a_planificador[numero] = 0;
	sleep(60);
	if (RETARDO() != 0) {
		porcentaje_a_planificador[numero] = (porcentaje[numero] * 100)
				/ (60 / RETARDO());
	} else {
		porcentaje_a_planificador[numero] = (porcentaje[numero] * 100)
				/ (60 / RETARDO_MINIMO());
	}

	porcentaje = 0;

	return NULL;
}
///////////////////////////////////////HILOS////////////////////////////////////////////
void* hilo_cpu(int *numero_hilo) {

	int numero = *numero_hilo;

	t_msg* mensaje_planificador = NULL;

	//int* cantidad_sentencias_ejecutadas = malloc(10*(sizeof(int)));

	socket_memoria[numero] = conectar_con_memoria(numero);
	socket_planificador[numero] = conectar_con_planificador(numero);

	if (socket_memoria[numero]>0 && socket_planificador[numero]>0) {

		while (true) {

			log_trace(logger, "[HILO #%d] Esperando peticiones del planificador", numero);
			mensaje_planificador = recibir_mensaje(socket_planificador[numero]);
			if(mensaje_planificador!=NULL){
				log_trace(logger, "[HILO #%d] Nuevo mensaje del planificador", numero);
				procesar_mensaje_planif(mensaje_planificador, numero); //pasarle socket_planificador y de memoria
			}else{
				log_error(logger, "[HILO #%d] Error al recibir mensaje del planif", numero);
				break;
			}

		}
	} else
		log_trace(logger,"[HILO #%d] Error al conectarse con la memoria y el planificador.", numero);

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
int procesar_mensaje_planif(t_msg* msg, int numero) {
	//print_msg(msg);
	t_pcb* pcb = NULL;
	t_resultado_pcb mensaje_planificador;

	switch (msg->header.id) {
	case PCB_A_EJECUTAR:
		destroy_message(msg);

		pcb = recibir_mensaje_pcb(socket_planificador[numero]);
		//pcb_print(pcb);

		log_trace(logger, "[HILO #%d] Ejecutando %s, PC: %d, cantSentAEjec: %d, cant_sent: %d",numero,
				pcb->path, pcb->pc, pcb->cant_a_ejectuar, pcb->cant_sentencias);

		mensaje_planificador = ejecutar(pcb, socket_memoria[numero], numero);

		log_trace(logger, "[HILO #%d] Fin ejecucion %s, PC: %d, cantSentAEjec: %d, cant_sent: %d",numero,
				pcb->path, pcb->pc, pcb->cant_a_ejectuar, pcb->cant_sentencias);
		avisar_a_planificador(mensaje_planificador, socket_planificador[numero], numero);

		free(pcb);
		break;

	}

	destroy_message(msg);
	return 0;
}

int pcb_tiene_que_seguir_ejecutando(t_pcb* pcb) {
	return pcb->pc < pcb->cant_sentencias;
}

/*
 * formatear para que quede solo texto
 */

char* get_texto_solo(char* texto) {
	char* txt;
	//borro la commilla de principio y final
	int len = strlen(texto) - 2;
	txt = malloc(len + 1);
	memset(txt, 0, len); //inicializo en 0
	memcpy(txt, texto + 1, len);
	txt[len] = '\0';
	return txt;
}

char* leer_hasta_el_final(char* texto){
	int i=0;
	while(texto[i]!='\n' && texto[i]!='\0' ){
		i++;
	}

	char* rs = string_substring_until(texto, i);
	return rs;
}

char* leer_hasta_espacio(char* texto){
	int i=0;
	while(texto[i]!='\n' && texto[i]!='\0' && texto[i]!=' ' ){
		i++;
	}

	char* rs = string_substring_until(texto, i);
	return rs;
}

char** splitear_sentencia(char* sent){
	//char** rs = string_n_split(sent, 2, " ");
	char**rs = malloc(4*(sizeof(char*)));
	rs[0] = NULL;rs[1] = NULL;rs[2] = NULL;
	rs[3] = NULL;//este ultimo es necesario para que sepa cuando cortar al hacer el free_split

	rs[0] = leer_hasta_espacio(sent);//nombre ej: iniciar, leer, escribir, entrada-salida, finalizar

	//si no es el finalizar, el segundo param si o si es un nro
	if(!string_starts_with(sent, "finalizar")){
		rs[1] = leer_hasta_espacio(sent+strlen(rs[0])+1);

		//si comienza con escribir leo el siguiente param que es el contenido de la pagina
		//ej: escribir 1 "hola que talco!"
		if(string_starts_with(sent, "escribir")){

			char* texto_con_commillas = leer_hasta_el_final(sent + strlen(rs[0]) + strlen(rs[1]) + 2);
			rs[2] = get_texto_solo(texto_con_commillas);
			free(texto_con_commillas);
		}
	}




	return rs;
}
/*
 * el param seria p.e: escribir 3 "hola"
 */
t_sentencia* sentencia_crear(char* sentencia, int pid, int hilo) {
	t_sentencia* sent = malloc(sizeof(*sent));
	sent->hilo = hilo;
	sent->pid = pid;
	sent->tiempo = 0;


	char** split = splitear_sentencia(sentencia);

	if (string_starts_with(sentencia, "iniciar ")) {
		sent->sentencia = iniciar;
		sent->cant_paginas = atoi(split[1]);
	} else if (string_starts_with(sentencia, "leer ")) {
		sent->sentencia = leer;
		sent->pagina = atoi(split[1]);
	} else if (string_starts_with(sentencia, "escribir ")) {
		sent->sentencia = escribir;
		sent->pagina = atoi(split[1]);

		sent->texto = string_duplicate(split[2]);
	} else if (string_starts_with(sentencia, "entrada-salida ")) {
		sent->sentencia = io;
		sent->tiempo = atoi(split[1]);
	} else if (string_starts_with(sentencia, "finalizar")) {
		sent->sentencia = final;
	} else {
		printf("Error para desconocidoooooooooooooooooooooooooooooooooo\n");
		sent->sentencia = error;
	}

	free_split(split);
	return sent;
}

int sent_ejecutar_iniciar(t_sentencia* sent, int socket_mem) {

	int rs = 0;
	t_msg* msg = NULL;
	msg = argv_message(MEM_INICIAR, 2, sent->pid, sent->cant_paginas);
	log_trace(logger, "[HILO #%d] Enviando mensaje MemIniciar %d", sent->hilo, sent->cant_paginas);
	enviar_y_destroy_mensaje(socket_mem, msg);

	log_trace(logger, "[HILO #%d] Esperando Rta MemIniciar %d", sent->hilo, sent->cant_paginas);
	msg = recibir_mensaje(socket_mem);
	if (msg != NULL) {

		if (msg->header.id == MEM_OK) {
			//el argv[0] es el estado
			log_trace(logger, "[HILO #%d] Rta mensaje MemIniciar %d OK", sent->hilo,  msg->argv[0]);
			rs = 0; // OK
		} else {
			rs = -1; // NO OK
		}
		destroy_message(msg);
		return rs;
	} else {
		log_trace(logger, "[HILO #%d] RTA MemIniciar NULL", sent->hilo);
		return -2;
	}
}

int sent_ejecutar_finalizar(t_sentencia* sent, int socket_mem) {
	int rs = 0;
	t_msg* msg = NULL;
	msg = argv_message(MEM_FINALIZAR, 1, sent->pid);
	log_trace(logger, "[HILO #%d] Enviando mensaje MEM_FINALIZAR ", sent->hilo);
	enviar_y_destroy_mensaje(socket_mem, msg);

	log_trace(logger, "[HILO #%d] Esperando Rta MEM_FINALIZAR", sent->hilo);
	msg = recibir_mensaje(socket_mem);
	if (msg != NULL) {

		if (msg->header.id == MEM_OK) {
			//el argv[0] es el estado
			log_trace(logger, "[HILO #%d] Rta mensaje MEM_FINALIZAR OK", sent->hilo);
			rs = 0; // OK
		} else {
			rs = -1; // NO OK
		}
		destroy_message(msg);
		return rs;
	} else {
		log_trace(logger, "[HILO #%d] RTA MEM_FINALIZAR NULL", sent->hilo);
		return -2;
	}
}

int sent_ejecutar_escribir(t_sentencia* sent, int socket_mem) {
	int rs = 0;

	t_msg* msg = NULL;
	msg = string_message(MEM_ESCRIBIR, sent->texto, 2, sent->pid, sent->pagina);
	log_trace(logger, "[HILO #%d] Enviando mensaje MEM_ESCRIBIR %d %s", sent->hilo, sent->pagina,
			sent->texto);
	enviar_y_destroy_mensaje(socket_mem, msg);

	log_trace(logger, "[HILO #%d] Esperando rta mensaje MEM_ESCRIBIR %d",sent->hilo, sent->pagina,
			sent->texto);
	msg = recibir_mensaje(socket_mem);
	if (msg != NULL) {
		if (msg->header.id == MEM_OK) {
			//en el stream esta el contenido de la pagina
			log_trace(logger, "[HILO #%d] Rta mensaje MEM_ESCRIBIR %d", sent->hilo, msg->argv[0]);
			rs = 0; // OK
		} else {
			rs = -1;
		}
		destroy_message(msg);

		return rs;
	} else {
		log_trace(logger, "[HILO #%d] RTA MEM_ESCRIBIR NULL", sent->hilo);
		return -2;
	}
}

char* sent_ejecutar_leer(t_sentencia* sent, int socket_mem) {
	char* pagina = NULL;

	t_msg* msg = NULL;
	msg = argv_message(MEM_LEER, 2, sent->pid, sent->pagina);
	log_trace(logger, "[HILO #%d] Enviando mensaje MEM_LEER %d", sent->hilo,  sent->pagina);
	enviar_y_destroy_mensaje(socket_mem, msg);

	log_trace(logger, "[HILO #%d] Esperando rta mensaje MEM_LEER %d",sent->hilo, sent->pagina);
	msg = recibir_mensaje(socket_mem);
	if (msg != NULL) {
		if (msg->header.id == MEM_OK) {
			//en el stream esta el contenido de la pagina
			log_trace(logger, "[HILO #%d] Rta mensaje: MEM_LEER %s", sent->hilo, msg->stream);
			pagina = string_duplicate(msg->stream);

		} else {
			pagina = NULL;
		}
		destroy_message(msg);

		return pagina;
	} else {
		log_trace(logger, "[HILO #%d] RTA MEM_LEER NULL", sent->hilo);
		return NULL;
	}
}

int sent_ejecutar(t_sentencia* sent, int socket_mem) {
	porcentaje = porcentaje + 1;
	char* pagina = NULL;
	int st = 0;
	switch (sent->sentencia) {
	case iniciar:
		st = sent_ejecutar_iniciar(sent, socket_mem);
		break;
	case leer:
		pagina = sent_ejecutar_leer(sent, socket_mem);
		FREE_NULL(pagina)
		;
		break;
	case escribir:
		sent_ejecutar_escribir(sent, socket_mem);
		break;
	case io:

		break;
	case error:
		log_trace(logger, "[HILO #%d] Error de sentencia en archivo", sent->hilo);
		break;
	case final:
		sent_ejecutar_finalizar(sent, socket_mem);
		break;
	default:
		log_trace(logger, "[HILO #%d] case default", sent->hilo);
		break;
	}

	return st;

	//return 0;
}

void sent_free(t_sentencia* sent) {
	if (sent->sentencia == escribir)
		FREE_NULL(sent->texto);
	FREE_NULL(sent);
}

t_resultado_pcb ejecutar(t_pcb* pcb, int socket_mem, int hilo) {

	bool es_entrada_salida = false;
	int st = 0;

	t_resultado_pcb resultado;

	int cantidad_a_ejecutar = pcb->cant_a_ejectuar;
	int contador = 0;

	char* mcod = file_get_mapped(pcb->path);
	char** sents = string_split(mcod, "\n");
	t_sentencia* sent = NULL;
	e_sentencia ultima_sentencia_ejecutada;
	sent = sentencia_crear(sents[pcb->pc], pcb->pid, hilo);

	while ((sent->sentencia != final) && (!es_entrada_salida)
			&& (cantidad_a_ejecutar != contador) && (st == 0)) {

		sleep(RETARDO());

		if (sent->sentencia != io) {

			porcentaje = porcentaje + 1;
			st = sent_ejecutar(sent, socket_mem);

			ultima_sentencia_ejecutada = sent->sentencia;
			sent_free(sent);
			pcb->pc++;
			sent = sentencia_crear(sents[pcb->pc], pcb->pid, hilo);
			contador = contador + 1;

		} else {
			es_entrada_salida = true;
		}
	}

	if ((sent->sentencia == final) && (cantidad_a_ejecutar != contador)) {
		sleep(RETARDO());
		porcentaje = porcentaje + 1;
		sent_ejecutar(sent, socket_mem);
		ultima_sentencia_ejecutada = sent->sentencia;


	}

	file_mmap_free(mcod, pcb->path);

	free_split(sents);
	resultado.pcb = pcb;
	if (st == 0)
		resultado.sentencia = ultima_sentencia_ejecutada;
	else
		resultado.sentencia = error;
	resultado.tiempo = sent->tiempo;
	resultado.cantidad_sentencias = contador;
	sent_free(sent);
	return resultado;

}

int avisar_a_planificador(t_resultado_pcb respuesta, int socket_planif, int hilo) {
	t_msg* mensaje_a_planificador;

	int i = 0;
	//printf("Sentencia: %d - IO: %d - FINAL: %d - ERROR: %d ",respuesta.sentencia,io,final,error);

	switch (respuesta.sentencia) {
	case io:
		mensaje_a_planificador = argv_message(PCB_IO, 4, respuesta.pcb->pid,
				respuesta.sentencia, respuesta.tiempo,
				respuesta.cantidad_sentencias);
				log_trace(logger, "[HILO #%d] EnviarAlPlanif PCB_IO, PID:%d, sent:%d, t: %d, cant_sent_ejec:%d",hilo, respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo, respuesta.cantidad_sentencias);
		break;
	case final:
		mensaje_a_planificador = argv_message(PCB_FINALIZAR, 4,
				respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo,
				respuesta.cantidad_sentencias);
		log_trace(logger, "[HILO #%d] ************** PCB_FINALIZAR, PID:%d *****************", hilo, respuesta.pcb->pid);
		log_trace(logger, "[HILO #%d] EnviarAlPlanif PCB_FINALIZAR, PID:%d, sent:%d, t: %d, cant_sent_ejec:%d",hilo, respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo, respuesta.cantidad_sentencias);
		break;
	case error:
		mensaje_a_planificador = argv_message(PCB_ERROR, 4, respuesta.pcb->pid,
				respuesta.sentencia, respuesta.tiempo,
				respuesta.cantidad_sentencias);
		log_trace(logger, "[HILO #%d] EnviarAlPlanif PCB_ERROR, PID:%d, sent:%d, t: %d, cant_sent_ejec:%d",hilo, respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo, respuesta.cantidad_sentencias);
		break;
	default: //si no entra a ninguno de los cases, significa que termino por fin de quantum
		mensaje_a_planificador = argv_message(PCB_FIN_QUANTUM, 4,
				respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo,
				respuesta.cantidad_sentencias);
		log_trace(logger, "[HILO #%d] EnviarAlPlanif PCB_FIN_QUANTUM, PID:%d, sent:%d, t: %d, cant_sent_ejec:%d",hilo, respuesta.pcb->pid, respuesta.sentencia, respuesta.tiempo, respuesta.cantidad_sentencias);
		break;
	}

	i = enviar_y_destroy_mensaje(socket_planif, mensaje_a_planificador);

	return i;
}

int enviar_porcentaje_a_planificador() {

	int i,l;
	t_msg* mensaje_a_planificador;
	/*mensaje_a_planificador = argv_message(CPU_PORCENTAJE_UTILIZACION,
			CANTIDAD_HILOS(), porcentaje_a_planificador[0],
			porcentaje_a_planificador[1], porcentaje_a_planificador[2],
			porcentaje_a_planificador[3], porcentaje_a_planificador[4],
			porcentaje_a_planificador[5]);*/
	for (l=0;l<CANTIDAD_HILOS();l++){
		log_trace(logger, "HILO #%d: CPU_PORCENTAJE_UTILIZACION: %d", l,
				porcentaje_a_planificador[l]);
		mensaje_a_planificador = argv_message(CPU_PORCENTAJE_UTILIZACION, 2, l,
				porcentaje_a_planificador[l]);
		i = enviar_y_destroy_mensaje(socket_planificador_especial,
				mensaje_a_planificador);

	}


	return i;

}

int conectar_con_memoria(int numero) {
	int sock;
	sock = client_socket(IP_MEMORIA(), PUERTO_MEMORIA());


	if (sock < 0) {
		log_trace(logger, "[HILO #%d] Error al conectar con  admin Mem. %s:%d",numero,IP_MEMORIA(), PUERTO_MEMORIA());
	} else {
		log_trace(logger, "[HILO #%d] Conectado con admin Mem. %s:%d", numero, IP_MEMORIA(), PUERTO_MEMORIA());
	}

	//envio handshake
	//envio un msj con el id del proceso
	t_msg* msg = string_message(CPU_NUEVO, "[HILO #%d] Hola soy un CPU ", 1, numero);
	if (enviar_mensaje(sock, msg) > 0) {
		log_trace(logger, "[HILO #%d] Mensaje enviado OK", numero                                                           );
	}
	destroy_message(msg);

	//enviar_mensaje_cpu(sock);

	return sock;
}

int conectar_con_planificador(int numero) {
	int sock;
	sock = client_socket(IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());

	if (sock < 0) {
		log_trace(logger, "[HILO #%d] Error al conectar con el planificador. %s:%d",numero,
				IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());
	} else {
		log_trace(logger, "[HILO #%d] Conectado con planificador. %s:%d",numero,
				IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());
	}

	//envio handshake
	//envio un msj con el id del proceso
	t_msg* msg = string_message(CPU_NUEVO, "[HILO #%d] Hola soy un CPU", 1, numero);
	if (enviar_mensaje(sock, msg) > 0) {
		log_trace(logger, "[HILO #%d] Mensaje enviado OK", numero);
	}
	destroy_message(msg);

	//enviar_mensaje_cpu(sock);

	return sock;
}

int conectar_con_planificador_especial() {
	int sock;
	sock = client_socket(IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());

	if (sock < 0) {
		log_trace(logger, "[PORCENTAJE] Error al conectar con el planificador especial. %s:%d",
				IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());
	} else {
		log_trace(logger, "[PORCENTAJE] Conectado con planificador especial. %s:%d",
				IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());
	}

	//envio handshake
	//envio un msj con el id del proceso
	t_msg* msg = string_message(CPU_ESPECIAL, "[PORCENTAJE] Hola soy un CPU", 1, -1);
	if (enviar_mensaje(sock, msg) > 0) {
		log_trace(logger, "[PORCENTAJE] Mensaje enviado OK");
	}
	destroy_message(msg);

	//enviar_mensaje_cpu(sock);

	return sock;
}

char ipmem[15];
char ipplanif[15];
int puertomem;
int puertoplanif;
void config_inicializar(){
	memset(ipmem, 0, 15);
	strcpy(ipmem, config_get_string_value(cfg, "IP_MEMORIA"));
	puertomem = config_get_int_value(cfg, "PUERTO_MEMORIA");

	memset(ipplanif, 0, 15);
	strcpy(ipplanif, config_get_string_value(cfg, "IP_PLANIFICADOR"));
	puertoplanif = config_get_int_value(cfg, "PUERTO_PLANIFICADOR");
}

int inicializar() {

	cfg = config_create(CONFIG_PATH);
	printf("IP planif: %s:%d\n", IP_PLANIFICADOR(), PUERTO_PLANIFICADOR());
	config_inicializar();

	clean_file(LOGGER_PATH);
	logger = log_create(LOGGER_PATH, "procesoCPU", true, LOG_LEVEL_TRACE);

	pthread_mutex_init(&mutex, NULL);

	return 0;
}

int finalizar() {

	config_destroy(cfg);
	log_destroy(logger);
	pthread_mutex_destroy(&mutex);
	return 0;
}

//////////////////////////////////////////////////FUNCIONES CONFIGURACION////////////////////////


char* IP_MEMORIA() {
	return ipmem;
}
int PUERTO_MEMORIA() {
	return puertomem;
}

char* IP_PLANIFICADOR() {
	return ipplanif;
}
int PUERTO_PLANIFICADOR() {
	return puertoplanif;
}

int CANTIDAD_HILOS() {
	return config_get_int_value(cfg, "CANTIDAD_HILOS");
}

int RETARDO() {
	return config_get_int_value(cfg, "RETARDO");
}

int RETARDO_MINIMO() {
	return config_get_int_value(cfg, "RETARDO_MINIMO");
}

