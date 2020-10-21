#include "v16x_server.h"

V16X_server v16x_server;

void SHAL_SYSTEM::system_shutdown()
{
    v16x_server.server_shutdown();
}

void configure()
{
    v16x_server.configure();
}

void loop()
{
    v16x_server.loop();
}
