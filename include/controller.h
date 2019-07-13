#ifndef CONTROLLER_H
#define CONTROLLER_H

/* A resource control (systemd) */
typedef struct ResourceControl {
    char* key;
    char* value;
} ResourceControl;

/*
 * Destroys the ResourceControl struct by deallocating things.
 */
void destroy_control_list(ResourceControl* controls, int ncontrols);

#endif // CONTROLLER_H
