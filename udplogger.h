// (c) 2018-2019 HomeAccessoryKid
#ifndef __UDPLOGGER_H__
#define __UDPLOGGER_H__

#include <semphr.h>

//use udp-client to collect this output
//use udplog_init(prio) to set up //is prio=3 a good idea??

//#define UDPLOG(format, ...)             udplogstring_len+=sprintf(udplogstring+udplogstring_len,format,##__VA_ARGS__);
#define UDPLOG(format, ...)  do {   if( xSemaphoreTake( xUDPlogSemaphore, ( TickType_t ) 1 ) == pdTRUE ) { \
                                        udplogstring_len+=sprintf(udplogstring+udplogstring_len,format,##__VA_ARGS__); \
                                        xSemaphoreGive( xUDPlogSemaphore ); \
                                    } else printf("skipped a UDPLOG\n"); \
                                } while(0)
#define UDPLGP(format, ...)  do {printf(format,##__VA_ARGS__); \
                                 UDPLOG(format,##__VA_ARGS__); \
                                } while(0)
#define INIT "UDPlog init message. Use udplog-client to receive other log messages\n"

void udplog_send(void *pvParameters);
void udplog_init(int prio);
extern SemaphoreHandle_t xUDPlogSemaphore;
extern char udplogstring[];
extern int  udplogstring_len;

#endif //__UDPLOGGER_H__
