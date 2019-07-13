#include <stdlib.h>

#include "controller.h"

void destroy_control_list(ResourceControl* controls, int ncontrols) {
    for (int i = 0; i < ncontrols; i++) {
        free(controls[i].key);
        free(controls[i].value);
    }
    free(controls);
}
