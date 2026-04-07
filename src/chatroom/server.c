/*
 * server.c — Servidor central del chatroom
 *
 * Cada cliente que se conecta recibe su propio hilo (thread), por lo que
 * varios usuarios pueden hablar al mismo tiempo sin bloquearse entre sí.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "../common.h"

// ─── Estado global del servidor ───────────────────────────────────────────────

typedef struct {
    int   socket;                  // El "canal" TCP con ese cliente
    char  name[MAX_NAME_LEN];      // Su apodo
    int   active;                  // 1 = conectado, 0 = libre
} Client;

// Lista de todos los clientes y un mutex para acceder a ella de forma segura
// (varios hilos pueden intentar modificarla a la vez)
static Client   clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Envía un paquete a UN cliente
static void send_packet(int socket, const Packet *pkt) {
    send(socket, pkt, sizeof(Packet), 0);
}

// Reenvía un paquete a TODOS los clientes conectados (excepto al que lo originó)
static void broadcast(const Packet *pkt, int exclude_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != exclude_socket) {
            send_packet(clients[i].socket, pkt);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Envía un mensaje privado a un usuario por su nombre
static int send_private(const char *target_name, const Packet *pkt) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, target_name) == 0) {
            send_packet(clients[i].socket, pkt);
            pthread_mutex_unlock(&clients_mutex);
            return 1; // encontrado
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return 0; // no existe ese usuario
}

// Construye y envía la lista de usuarios al cliente que la pidió
static void send_user_list(int socket) {
    char list[MAX_MSG_LEN] = "Usuarios conectados: ";

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            strncat(list, clients[i].name, MAX_NAME_LEN);
            strncat(list, " ", 2);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    Packet pkt = { .type = MSG_LIST };
    strncpy(pkt.sender, "Servidor", MAX_NAME_LEN);
    strncpy(pkt.content, list, MAX_MSG_LEN);
    send_packet(socket, &pkt);
}

// ─── Hilo por cliente ─────────────────────────────────────────────────────────
// Esta función se ejecuta de forma independiente para cada usuario conectado.

static void *handle_client(void *arg) {
    Client *me = (Client *)arg;
    Packet pkt;

    printf("[+] '%s' se ha conectado.\n", me->name);

    // Anunciar al resto que alguien llegó
    Packet join_pkt = { .type = MSG_JOIN };
    strncpy(join_pkt.sender, "Servidor", MAX_NAME_LEN);
    snprintf(join_pkt.content, MAX_MSG_LEN, "%s se ha unido al chat.", me->name);
    broadcast(&join_pkt, me->socket);

    // Bucle principal: esperar mensajes de este cliente
    while (1) {
        int bytes = recv(me->socket, &pkt, sizeof(Packet), 0);

        // Si recv devuelve 0 o negativo, el cliente se desconectó
        if (bytes <= 0) break;

        if (pkt.type == MSG_CHAT) {
            printf("[chat] %s: %s\n", pkt.sender, pkt.content);
            broadcast(&pkt, me->socket);  // reenviar a todos los demás

        } else if (pkt.type == MSG_LIST) {
            send_user_list(me->socket);

        } else if (pkt.type == MSG_PRIVATE) {
            // El formato del contenido es: "@nombre mensaje"
            // Extraemos el nombre de destino y el mensaje
            char target[MAX_NAME_LEN], message[MAX_MSG_LEN];
            sscanf(pkt.content, "%s %[^\n]", target, message);

            strncpy(pkt.content, message, MAX_MSG_LEN);
            if (!send_private(target, &pkt)) {
                Packet err = { .type = MSG_ERROR };
                strncpy(err.sender, "Servidor", MAX_NAME_LEN);
                snprintf(err.content, MAX_MSG_LEN, "Usuario '%s' no encontrado.", target);
                send_packet(me->socket, &err);
            }
        }
    }

    // ── Limpieza al desconectarse ─────────────────────────────────────────────
    printf("[-] '%s' se ha desconectado.\n", me->name);

    pthread_mutex_lock(&clients_mutex);
    me->active = 0;
    close(me->socket);
    pthread_mutex_unlock(&clients_mutex);

    Packet leave_pkt = { .type = MSG_LEAVE };
    strncpy(leave_pkt.sender, "Servidor", MAX_NAME_LEN);
    snprintf(leave_pkt.content, MAX_MSG_LEN, "%s ha abandonado el chat.", me->name);
    broadcast(&leave_pkt, -1);

    return NULL;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    // 1. Crear el socket del servidor
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    // Permite reutilizar el puerto inmediatamente tras reiniciar el servidor
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Configurar dirección y puerto
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,   // escuchar en todas las interfaces
        .sin_port        = htons(SERVER_PORT),
    };

    // 3. Bind: "reservar" ese puerto para nuestro proceso
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    // 4. Listen: ponerse a escuchar conexiones entrantes
    listen(server_fd, 10);
    printf("Servidor escuchando en puerto %d...\n", SERVER_PORT);

    // 5. Bucle de aceptación: esperar nuevos clientes
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        // accept() bloquea aquí hasta que alguien se conecta
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) { perror("accept"); continue; }

        // El primer mensaje del cliente es su nombre
        Packet name_pkt;
        if (recv(client_fd, &name_pkt, sizeof(Packet), 0) <= 0) {
            close(client_fd);
            continue;
        }

        // Buscar un hueco libre en la lista de clientes
        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) { slot = i; break; }
        }

        if (slot == -1) {
            // Servidor lleno
            pthread_mutex_unlock(&clients_mutex);
            Packet err = { .type = MSG_ERROR };
            strncpy(err.sender, "Servidor", MAX_NAME_LEN);
            strncpy(err.content, "Servidor lleno. Inténtalo más tarde.", MAX_MSG_LEN);
            send_packet(client_fd, &err);
            close(client_fd);
            continue;
        }

        clients[slot].socket = client_fd;
        clients[slot].active = 1;
        strncpy(clients[slot].name, name_pkt.sender, MAX_NAME_LEN);
        pthread_mutex_unlock(&clients_mutex);

        // Lanzar un hilo para ese cliente
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, &clients[slot]);
        pthread_detach(tid);  // el hilo se limpiará solo al terminar
    }

    close(server_fd);
    return 0;
}