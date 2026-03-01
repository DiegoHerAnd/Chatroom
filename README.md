# Chatroom

Proyecto multiusuario desarrollado en C, basado en arquitectura cliente-servidor y comunicación mediante sockets TCP. El servidor central gestiona múltiples conexiones simultáneas, permitiendo el envío y recepción de mensajes en tiempo real a través de un modelo de mensaje estructurado.

---

## Arquitectura general

El proyecto tiene 3 archivos fuente y un Makefile:

| Archivo | Descripción |
|---|---|
| `common.h` | Definiciones compartidas — el "lenguaje" entre servidor y cliente |
| `server.c` | El servidor central |
| `client.c` | El programa que ejecuta cada usuario |

---

## Comunicación TCP

TCP funciona como una llamada telefónica: primero se establece la conexión, luego ambos extremos pueden hablar en los dos sentidos y al final se cuelga.

**En el servidor:**
- `socket()` — crea el "teléfono"
- `bind()` — le asigna un número (el puerto 8080)
- `listen()` — se pone a esperar llamadas
- `accept()` — acepta una conexión entrante (bloquea hasta que alguien llame)

**En el cliente:**
- `socket()` — crea su teléfono
- `connect()` — llama al servidor (IP + puerto)

A partir de ahí, ambos lados usan `send()` y `recv()` para intercambiar datos, exactamente igual que leer y escribir en un archivo.

---

## Protocolo: qué viaja por el cable

En lugar de enviar texto plano, se define un **paquete con estructura fija** (`Packet` en `common.h`):

```
┌─────────────┬───────────────┬──────────────────────────┐
│  tipo (int) │  remitente    │  contenido del mensaje   │
│  MSG_CHAT   │  "Carlos"     │  "Hola a todos!"         │
└─────────────┴───────────────┴──────────────────────────┘
```

Así el servidor sabe de un vistazo qué hacer: reenviar a todos, responder con la lista de usuarios, gestionar un mensaje privado, etc.

---

## Múltiples usuarios simultáneos

Sin más magia, `accept()` solo puede atender a uno. La solución es lanzar un **hilo (thread)** por cada cliente que se conecta. Cada hilo actúa como un empleado dedicado exclusivamente a un usuario:

```
Servidor principal
     │
     ├── Hilo 1 → atiende a Ana
     ├── Hilo 2 → atiende a Carlos
     └── Hilo 3 → atiende a María
```

Como todos los hilos comparten la lista de clientes, se utiliza un **mutex** (un candado) para evitar que la modifiquen a la vez y corrompan los datos.

---

## Envío y recepción simultáneos en el cliente

Sin ninguna solución adicional, mientras el usuario escribe no podría ver mensajes entrantes. Se resuelve con **dos hilos**:

- El hilo principal lee del teclado y envía al servidor.
- Un segundo hilo escucha continuamente y muestra los mensajes que llegan.

---

## Cómo probarlo

```bash
# Compilar
make

# Terminal 1 — arrancar el servidor
./server

# Terminal 2 — primer usuario
./client 127.0.0.1 Ana

# Terminal 3 — segundo usuario
./client 127.0.0.1 Carlos
```

**Comandos disponibles dentro del chat:**

| Comando | Acción |
|---|---|
| `Hola a todos!` | Mensaje público a todos los usuarios |
| `/list` | Ver quién está conectado |
| `@Ana ¿qué tal?` | Mensaje privado a un usuario |
| `/quit` | Salir del chat |
