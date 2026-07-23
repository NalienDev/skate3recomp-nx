// netinet/ip.h - Nintendo Switch / devkitA64 (libnx) shim
//
// libnx provides the core BSD socket headers (sys/socket.h, netinet/in.h,
// arpa/inet.h, netinet/tcp.h) but not <netinet/ip.h>, which defines raw IP
// header structures. The rexglue socket code includes it defensively but does
// not use struct ip / struct iphdr, so pulling in <netinet/in.h> is sufficient.
#pragma once

#include <netinet/in.h>
