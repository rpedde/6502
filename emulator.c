#include "emulator.h"

#include "memory.h"
#include "6502.h"
#include "stepwise.h"

int main(int argc, char *argv[]) {
    int option;
    int step = 0;

    while((option = getopt(argc, argv, "d:s")) != -1) {
        switch(option) {
        case 'd':
            debug_level(atoi(optarg));
            break;

        case 's':
            step = 1;
            break;

        default:
            fprintf(stderr,"Srsly?");
            exit(EXIT_FAILURE);
        }
    }

    memory_init();
    debug_level(DBG_DEBUG);
    cpu_init();

    if(step) {
        stepwise_debugger(NULL);
    } else {
    }

    exit(EXIT_SUCCESS);
}
