/*
 * client.c — Cliente del chatroom
 *
 * Usa dos hilos para poder enviar y recibir mensajes a la vez:
 *   • Hilo 1: Lee mensajes del servidor y los muestra en pantalla.
 *   • Hilo 2 (main): Lee lo que escribe el usuario y lo envía.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "../common.h"
#include "../common.h"

static int server_socket;
static char my_name[MAX_NAME_LEN];

// ─── Hilo receptor ────────────────────────────────────────────────────────────
// Está siempre escuchando mensajes que llegan del servidor.

static void *receive_loop(void *arg) {
    (void)arg;
    Packet pkt;

    while (1) {
        int bytes = recv(server_socket, &pkt, sizeof(Packet), 0);
        if (bytes <= 0) {
            printf("\n[!] Conexión con el servidor perdida.\n");
            exit(0);
        }

        switch (pkt.type) {
            case MSG_CHAT:
                printf("\r[%s]: %s\n> ", pkt.sender, pkt.content);
                break;
            case MSG_PRIVATE:
                printf("\r[PRIVADO de %s]: %s\n> ", pkt.sender, pkt.content);
                break;
            case MSG_JOIN:
            case MSG_LEAVE:
                printf("\r*** %s ***\n> ", pkt.content);
                break;
            case MSG_LIST:
                printf("\r%s\n> ", pkt.content);
                break;
            case MSG_ERROR:
                printf("\r[ERROR]: %s\n> ", pkt.content);
                break;
            default:
                break;
        }
        fflush(stdout);
    }
    return NULL;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <ip_servidor> <tu_nombre>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 Carlos\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    strncpy(my_name, argv[2], MAX_NAME_LEN - 1);

    // 1. Crear socket TCP
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) { perror("socket"); return 1; }

    // 2. Dirección del servidor al que conectarse
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(SERVER_PORT),
    };
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // 3. connect() — establece la conexión TCP con el servidor
    if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }
    printf("Conectado al servidor. ¡Bienvenido al chat, %s!\n", my_name);
    printf("Comandos: /list — ver usuarios | @nombre mensaje — mensaje privado | /quit — salir\n\n");

    // 4. Enviar el nombre al servidor como primer mensaje
    Packet name_pkt = { .type = MSG_JOIN };
    strncpy(name_pkt.sender, my_name, MAX_NAME_LEN);
    send(server_socket, &name_pkt, sizeof(Packet), 0);

    // 5. Lanzar el hilo receptor
    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, receive_loop, NULL);
    pthread_detach(recv_tid);

    // 6. Bucle de envío: leer lo que escribe el usuario
    char input[MAX_MSG_LEN];
    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        // Quitar el salto de línea final
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) continue;

        if (strcmp(input, "/quit") == 0) break;

        Packet pkt;
        strncpy(pkt.sender, my_name, MAX_NAME_LEN);

        if (strcmp(input, "/list") == 0) {
            // Pedir lista de usuarios
            pkt.type = MSG_LIST;
            pkt.content[0] = '\0';

        } else if (input[0] == '@') {
            // Mensaje privado: @nombre texto
            pkt.type = MSG_PRIVATE;
            // Extraer "nombre texto" → enviamos el nombre con @ para que el servidor sepa el destino
            strncpy(pkt.content, input + 1, MAX_MSG_LEN); // ej: "Carlos hola!"

        } else {
            // Mensaje público normal
            pkt.type = MSG_CHAT;
            strncpy(pkt.content, input, MAX_MSG_LEN);
        }

        send(server_socket, &pkt, sizeof(Packet), 0);
    }

    close(server_socket);
    printf("¡Hasta luego!\n");
    return 0;
}