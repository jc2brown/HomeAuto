#ifndef QSHTTPD_HANDLER_H
#define QSHTTPD_HANDLER_H


typedef void (*RequestHandler)(Request *request, Response *response); 

struct RequestHandlerInfo_struct {
	char *path;
	RequestHandler handler;	
};
typedef struct RequestHandlerInfo_struct RequestHandlerInfo;


int add_request_handler(char *path, RequestHandler handler) ;
RequestHandler get_request_handler(Request *request);
void free_request_handlers(void) ;

#endif

