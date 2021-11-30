#include "v16x_server.h"

V16X_server v16x_server;

void SHAL_SYSTEM::system_shutdown()
{
    v16x_server.server_shutdown();
}

void configure()
{
    SHAL_SYSTEM::init();
    v16x_server.configure();
    SHAL_SYSTEM::run_thread_process(V16X_server::fire_process);
}

void loop()
{
    v16x_server.loop();
}
