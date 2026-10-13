// Imports
#include <stdint.h>
#include <debug/debug.h>

/*
 * Função primária do Kernel em C, chamada pelo boot.asm
 */
void kmain() {
    debug_init(); // Inicia o Sistema de Debug do Kernel
    
    for (int i = 0; i < 9; i++) {
        debug_write("Kernel", "mist-sama", "semen de cavalos %d", i);    
    }

    for (;;);
}