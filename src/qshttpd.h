
#ifndef _QSHTTPD_H
#define _QSHTTPD_H






//////////////////////////////////////////////////////
//
// 	Server Configuration 
//
//////////////////////////////////////////////////////

struct conf_struct {
    char *root;
    int port;
    char *charset;
    char *user;
    char *group;
};
typedef struct conf_struct Conf;


Conf get_conf(void);
void drop_privileges(Conf configuration);
int create_and_bind(Conf configuration);






struct request_struct {
    char *get;
    long resume;
    char *host;
    int src_ip[4];
};
typedef struct request_struct Request;



struct response_struct {
	char *sent;
	int code;
	char *codestr;
	char *file;
	char *mime;
	char *moved;
	char *length;
	char *start;
	char *end;
	char *content;
};
typedef struct response_struct Response;



struct responsecode_struct {
	int code;
	char *str;
};
typedef struct responsecode_struct ResponseCode;

 
void static_request_handler(Request *request, Response *response);

Request *create_request(void);
Response *create_response(void);




//////////////////////////////////////////////////////
//
// 	Request Handler
//
//////////////////////////////////////////////////////


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

