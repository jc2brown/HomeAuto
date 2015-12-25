#include "qshttpd_handler.h"


static int n_request_handlers = 0;
static RequestHandlerInfo *request_handler_infos[16];


#define min(x,y) ((x<y)?(x):(y))

// Returns true if handlerInfo is appropriate for request
int match(RequestHandlerInfo *handlerInfo, Request *request) {
	char *path = handlerInfo->path;
	char *get = request->get;
	int ismatch = ! strncmp(path, get, strlen(path));
	return ismatch;
}


RequestHandler get_request_handler(Request *request) {
	int i;
	int ismatch;
	RequestHandlerInfo *handlerInfo;	
	RequestHandlerInfo *bestHandlerInfo = NULL;
	for ( i = 0; i < n_request_handlers; ++i ) {
		handlerInfo = request_handler_infos[i];
		ismatch = match(handlerInfo, request);
		if ( ismatch ) {
			if ( bestHandlerInfo == NULL || strlen(handlerInfo->path) > strlen(bestHandlerInfo->path)) {
				bestHandlerInfo = handlerInfo;
			}
		}
	}
	return bestHandlerInfo->handler;
}


int add_request_handler(char *path, RequestHandler handler) {
	int i;
	RequestHandlerInfo *handlerInfo;	
	for ( i = 0; i < n_request_handlers; ++i ) {
		handlerInfo = request_handler_infos[i];
		if ( ! strcmp(path, handlerInfo->path) ) {
			return -1;
		}
	}	
	handlerInfo = (RequestHandlerInfo *)malloc(sizeof(RequestHandlerInfo));
	handlerInfo->path = path;
	handlerInfo->handler = handler;
	request_handler_infos[n_request_handlers] = handlerInfo;
	return ++n_request_handlers;
}


void free_request_handlers() {
	int i;
	RequestHandlerInfo *handlerInfo;	
	for ( i = 0; i < n_request_handlers; ++i ) {
		handlerInfo = request_handler_infos[i];
		free(handlerInfo);
	}
}


