#ifndef COMMON_H
#define COMMON_H

// ─── Configuración general ───────────────────────────────────────────────────
#define SERVER_PORT   8080
#define MAX_CLIENTS   50
#define MAX_MSG_LEN   1024
#define MAX_NAME_LEN  32

// ─── Tipos de mensaje ────────────────────────────────────────────────────────
// Cada paquete que viaja por la red lleva un "tipo" para saber qué hacer con él
typedef enum {
    MSG_CHAT    = 1,   // Mensaje normal de chat
    MSG_JOIN    = 2,   // Un usuario se ha unido
    MSG_LEAVE   = 3,   // Un usuario se ha ido
    MSG_LIST    = 4,   // Pedir lista de usuarios conectados
    MSG_ERROR   = 5,   // Error del servidor
    MSG_PRIVATE = 6,   // Mensaje privado (@usuario texto)
} MessageType;

// ─── Estructura del paquete ───────────────────────────────────────────────────
// Todo lo que viaja por la red tiene este formato fijo ("protocolo")
typedef struct {
    MessageType type;
    char sender[MAX_NAME_LEN];   // Quién lo envía
    char content[MAX_MSG_LEN];   // El contenido del mensaje
} Packet;

#endif