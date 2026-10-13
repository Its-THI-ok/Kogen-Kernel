#include <debug/debug.h>
#include <attributes.h>

#include <stddef.h>
#include <stdint.h>

#include <tools/string.h>
#include <info/system.h>
#include "platform.h"

/*
 * Função chamada se o Kernel entrar em estado de pânico
 */
cold void panic(char reason[512])
{
	debug_write(SYSTEM_NAME, "Platform", "!! PANIC !! - execution halted : %s", reason);

    // Desliga as interrupções (cli) e para a execução (hlt)
	__asm__ volatile("cli\n" "hlt");
}

cold void initialize_platform(void)
{
    // ...
}