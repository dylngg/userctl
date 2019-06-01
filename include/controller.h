#ifndef CONTROLLER_H
#define CONTROLLER_H

/* A resource control (systemd) */
typedef struct {
    const char* key;
    const char* values[];
} ResourceControl;

#endif // CONTROLLER_H
