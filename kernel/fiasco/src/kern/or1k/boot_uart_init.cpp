IMPLEMENTATION [or1k && serial]:

#include "kernel_console.h"
#include "kernel_uart.h"
#include "static_init.h"

STATIC_INITIALIZER_P(boot_console_init, EARLY_INIT_PRIO);

static void boot_console_init()
{
  Kconsole::init();
  Kernel_uart::init();
  Console::stdout = Kernel_uart::uart();
  Console::stderr = Console::stdout;
  Console::stdin  = Console::stdout;
}

