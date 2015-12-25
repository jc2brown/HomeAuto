// QSHTTPD 
// Modified for dynamic applications

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include "qshttpd.h"



#define DEBUG 0

#if DEBUG
#define dbgprintf printf
#else
#define dbgprintf(fmt, ...) 
#endif





//////////////////////////////////////////////////////
//
// 	Server Configuration 
//
//////////////////////////////////////////////////////


int is_option(char *buffer, char *option_name) {
    // Find the first uppercase alphabetic char
    while (*buffer < 65 || *buffer > 90) {
        buffer++;
    }
    
    // key=value must be longer than key
    if (strlen(buffer) > strlen(option_name)) {
        return (strncmp(buffer, option_name, strlen(option_name)) == 0);
    } else {   
        return 0;
    }
}

char * get_value(char * buffer) {
    buffer = strchr(buffer, '=') + 1;
    while (*buffer == ' ' || *buffer == '\t') {
        buffer++;
    }
    
    return buffer;
}

Conf get_conf() {
    FILE *conffile;
    char buffer[500];
    char *tmp;
    Conf conf;
    
    conffile = fopen ("qshttpd.conf", "r");

    while (fgets (buffer , 500, conffile)) {
        if (is_option(buffer, "ROOT")) {
            tmp = get_value(buffer);
            conf.root = calloc(1, strlen(tmp));
            strncpy(conf.root, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "PORT")) {
            conf.port = atoi(get_value(buffer));
        } else if (is_option(buffer, "USER")) {
            tmp = get_value(buffer);
            conf.user = calloc(1, strlen(tmp));
            strncpy(conf.user, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "GROUP")) {
            tmp = get_value(buffer);
            conf.group = calloc(1, strlen(tmp));
            strncpy(conf.group, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "CHARSET")) {
            tmp = get_value(buffer);
            conf.charset = calloc(1, strlen(tmp) + 1);
            strncpy(conf.charset, tmp, strlen(tmp) -1);
        }
    }
    
    fclose (conffile);
    
    return conf;
}


//Chroot and change user and group to nobody. Got this function from Simple HTTPD 1.0.
void drop_privileges(Conf configuration) {
    struct passwd *pwd;
    struct group *grp;

    if ((pwd = getpwnam(configuration.user)) == 0) {
        fprintf(stderr, "User not found in /etc/passwd\n");
        exit(EXIT_FAILURE);
    }

    if ((grp = getgrnam(configuration.group)) == 0) {
        fprintf(stderr, "Group not found in /etc/group\n");
        exit(EXIT_FAILURE);
    }
    
    if (chdir(configuration.root) != 0) {
        fprintf(stderr, "chdir(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (chroot(configuration.root) != 0) {
        fprintf(stderr, "chroot(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (setgid(grp->gr_gid) != 0) {
        fprintf(stderr, "setgid(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (setuid(pwd->pw_uid) != 0) {
        fprintf(stderr, "setuid(...) failed\n");
        exit(EXIT_FAILURE);
    }
}



//////////////////////////////////////////////////////
//
// 	Request Handlers
//
//////////////////////////////////////////////////////


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




//////////////////////////////////////////////////////
//
// 	Socket setup
//
//////////////////////////////////////////////////////

#define BACKLOG 10

int alive;

//void qshttpd_sigterm_handler(int s) {
//	alive = 0;
//}


void sigchld_handler(int s) {
   while(waitpid(-1, NULL, WNOHANG) > 0);
}

int create_and_bind(Conf configuration) {
    struct sigaction sa;
    int sockfd, yes=1;
    struct sockaddr_in my_addr;

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(configuration.port);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);

	// Use non-blocking I/O on socket
	//int flags = fcntl(sockfd, F_GETFL);
	//if ( ! ( flags & O_NONBLOCK ) ) {
	//	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	//}
	
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    drop_privileges(configuration);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
    return sockfd;
}











//Sockets stuff
static int sockfd;
static int new_fd;
static struct sockaddr_in their_addr;
static socklen_t sin_size;


//Other global variables
static int buffer_counter;
static char * buffer;
static int buffer_chunks;
static FILE *openfile;

void read_chunk() {
    fread (buffer,1,1048576,openfile);
    buffer_counter++;
}


Request *create_request() {
    Request *request = (Request *)malloc(sizeof(Request));
}
void destroy_request(Request *request) {	
	if ( request == NULL ) {
		return;
	}	
	free(request);
}


#define N_RESPONSECODES 5

ResponseCode responsecodes[N_RESPONSECODES] = {
	{ 200, "200 OK" },
	{ 204, "204 No Content" },
	{ 206, "206 Partial Content" },
	{ 301, "301 Moved Permanently" },
	{ 404, "404 Not Found" },
};

char *get_responsecode_text(int code) {
	int i;
	for ( i = 0; i < N_RESPONSECODES; ++i ) {
		if ( responsecodes[i].code == code ) {
			return responsecodes[i].str;
		}
	}
	return 0;
}


Response *create_response() {	
    Response *response = (Response *)malloc(sizeof(Response));    
    response->sent = (char *)malloc(5000);
    strcpy(response->sent, "");
    response->file = (char *)malloc(200);
    strcpy(response->file, "");
    response->mime = (char *)malloc(100);
    strcpy(response->mime, "");
    response->moved = (char *)malloc(200);
    strcpy(response->moved, "");
    response->length = (char *)malloc(100);
    strcpy(response->length, "");
    response->start = (char *)malloc(100);
    strcpy(response->start, "");
    response->end = (char *)malloc(100);
    strcpy(response->end, "");
    response->content = (char *)malloc(4000);
    strcpy(response->content, "");
    
    response->code = 0;
}

void destroy_response(Response *response) {	
	if ( response == NULL ) {
		return;
	}	
    free(response->sent);
    free(response->codestr);
    free(response->file);
    free(response->mime);
    free(response->moved);
    free(response->length);
    free(response->start);
    free(response->end);		
    free(response->content);	
	free(response);
}



Request *process_request (char *raw_in, struct sockaddr_in their_addr) {
	
	Request *request = create_request();
    
    char *line = strtok(raw_in, "\n\r");
    do {
        if (strncmp(line, "GET", 3) == 0) {
            line += 4;
            int path_length = strpbrk(line, " ") - line;
            request->get = calloc(1, path_length + 1);
            strncpy(request->get, line, path_length);
        } else if (strncmp(line, "Range: bytes=", 13) == 0){
            line += 13;
            request->resume = atoi(line);
        } else if (strncmp(line, "Host:", 5) == 0) {
            line += 6;
            request->host = malloc(strlen(line) + 1);
            strncpy(request->host, line, strlen(line) + 1);
        }
    } while ((line = strtok(NULL, "\n\r")) != NULL);
                
	request->src_ip[0] = (their_addr.sin_addr.s_addr >> 0) & 0x00FF;
	request->src_ip[1] = (their_addr.sin_addr.s_addr >> 8) & 0x00FF;
	request->src_ip[2] = (their_addr.sin_addr.s_addr >> 16) & 0x00FF;
	request->src_ip[3] = (their_addr.sin_addr.s_addr >> 24) & 0x00FF;
				
    return request;    
}




void static_request_handler(Request *request, Response *response) {	

    char *ext=NULL, *extf;
    long filesize, range=0;
    
    			
	// If requested path is a directory
	if (opendir(request->get)) {
		// Last char is /
		if (request->get[strlen(request->get)-1] == '/'){
			// The user knows this is a directory so we serve the directory index
			strcat(request->get, "index.html");
			openfile=fopen (request->get, "r");
			if (openfile){
				response->code = 200;
			} else {
				//Here should be some kind of directory listing
				strcpy(response->file, "/404.html");
				openfile = fopen (response->file, "r");
				response->code = 404;
			}
		// Last char isn't / so we redirect the browser to the directory
		} else {
			response->code = 301;
			strcpy(response->moved, "Location: http://");
			strcat(response->moved, request->host);
			strcat(response->moved, request->get);
			strcat(response->moved, "/");
		}

	// Requested path isn't a directory
	} else {
		openfile=fopen (request->get, "rb");
		if (openfile){
			/*dbgprintf("> %d\n", response->code);
			if ( response->code == 0) {			
				response->code = 200;
				dbgprintf(">> %d\n", response->code);
			}*/
		} else {
			strcpy(response->file, "/404.html");
			openfile = fopen (response->file, "r");
			response->code = 404;
		}
	}
	
	
	if ( response->code != 301 ) {
		fseek (openfile , 0 , SEEK_END);
		filesize = ftell (openfile);
		rewind (openfile);
		if (range > 0) {
			sprintf(response->end, "%ld", filesize);
			filesize = filesize - range;
			sprintf(response->start, "%ld", range);
			fseek (openfile , range , SEEK_SET);
		}
		buffer_chunks = filesize/1048576;
		if(filesize%1048576 > 0){
			buffer_chunks++;
		}
		sprintf(response->length, "%ld", filesize);		
		buffer_counter = 0;
		buffer = (char*) malloc (sizeof(char)*1048576);
	}

	if ( response->code != 404 && response->code != 301 ) {
		ext = strtok(request->get, ".");
		while(ext != NULL){
			ext = strtok(NULL, ".");
			if (ext != NULL){
				extf = ext;
			}
		}
	} else {
		extf="html";
	}

	/* Maybe I should read mime types from a file. At least for now, add here what you need.*/

	if (strcmp(extf, "html") == 0){
		strcpy (response->mime, "text/html");
	} else if(strcmp(extf, "jpg") == 0){
		strcpy (response->mime, "image/jpeg");
	} else if(strcmp(extf, "gif") == 0){
		strcpy (response->mime, "image/gif");
	} else if(strcmp(extf, "css") == 0){
		strcpy (response->mime, "text/css");
	} else if(strcmp(extf, "js") == 0){
		strcpy (response->mime, "text/javascript");
	} else {
		strcpy(response->mime, "application/octet-stream");
	}
            
}


void print_configuration(Conf *configuration) {
    dbgprintf("Server Configuration:\n");
    dbgprintf("  root=%s\n", configuration->root);
    dbgprintf("  port=%d\n", configuration->port);
    dbgprintf("  charset=%s\n", configuration->charset);
    dbgprintf("  user=%s\n", configuration->user);
    dbgprintf("  group=%s\n", configuration->group);    
}

void print_request(Request *request) {	
	dbgprintf("\n%d.%d.%d.%d GET %s%s\n", 
		request->src_ip[0], 
		request->src_ip[1], 
		request->src_ip[2], 
		request->src_ip[3], 
		request->host, request->get);	
}


void finalize_response(Response *response, Conf *configuration) {
		
	if ( strlen(response->length) == 0 ) {
		sprintf(response->length, "%d", strlen(response->content));
	}
	
	if ( response->code == 0 ) {
		if ( strlen(response->length) == 0 ) {
			response->code = 204;
		} else {
			response->code = 200;						
		}
	}
             
	strcpy(response->sent, "HTTP/1.1 ");
	strcat(response->sent, get_responsecode_text(response->code));
	strcat(response->sent, "\nServer: qshttpd 0.3.0\n");
	if ( response->code == 301 ) {    
		strcat(response->sent, response->moved);
		strcat(response->sent, "\n");
	}

	strcat(response->sent, "Content-Length: ");
	if ( response->code != 301 ) {    
		strcat(response->sent, response->length);
	} else {
		strcat(response->sent, "0");
	}
	
	if ( response->code == 206 ) {    
		strcat(response->sent, "\nContent-Range: bytes ");
		strcat(response->sent, response->start);
		strcat(response->sent, "-");
		strcat(response->sent, response->end);
		strcat(response->sent, "/");
		strcat(response->sent, response->end);
	}
	strcat(response->sent, "\nConnection: close\nContent-Type: ");
	strcat(response->sent, response->mime);
	strcat(response->sent, "; charset=");
	strcat(response->sent, configuration->charset);
	strcat(response->sent, "\n\n");
	
}



void send_response(Response *response, int fd) {
	write(new_fd, response->sent, strlen(response->sent));
	write(new_fd, response->content, strlen(response->content));
				
	while (buffer_counter < buffer_chunks) {
		read_chunk();
		write(new_fd, buffer, 1048576);
	}
}


int start_httpd() {
	char in_raw[3000];
    Request *request;
    Response *response;
    
    int httpd_pid;
    
    //signal(SIGTERM, qshttpd_sigterm_handler);
    
    if ( (httpd_pid = fork()) ) {
		return httpd_pid;
	}
	
    dbgprintf("QSHTTPD\n\n");
    Conf configuration = get_conf();
    
    print_configuration(&configuration);
    
    add_request_handler("", static_request_handler);
    
    sockfd = create_and_bind(configuration);

	alive = 1;
    while ( alive ) {
				
        sin_size = sizeof(struct sockaddr_in);
        
        
		dbgprintf("sockfd=%d\n", sockfd);
		dbgprintf("their_addr=%p\n", &their_addr);
		dbgprintf("sin_size=%d\n", sin_size);
		
		
		dbgprintf("accept\n\n");
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		dbgprintf("%d\n\n", new_fd);
        
        if ( new_fd == EAGAIN || new_fd == EWOULDBLOCK ) {
			// doesn't seem to get here
			dbgprintf("hmmm1\n\n");
			continue;
		}
        if (new_fd == -1) {
			dbgprintf("hmmm2\n\n");
            //perror("accept");
            continue;
        }
			
		dbgprintf("fork\n\n");
        
        if ( fork() ) {
			dbgprintf("close\n\n");
            close(sockfd);
        
			// 
			dbgprintf("response\n\n");
			response = create_response();	
				
			// Read request from socket
            if (read(new_fd, in_raw, 3000) == -1) {
				// When does this happen? What should we do?
                perror("ERROR :: qshttpd.c :: start_httpd :: read(...) == -1)\n");      
            } else {				
                request = process_request(in_raw, their_addr);   				
				print_request(request);	
				RequestHandler request_handler = get_request_handler(request);				
                request_handler(request, response);                           
            }
            
            finalize_response(response, &configuration);

			send_response(response, new_fd);
                        
            close(new_fd);
            
            destroy_request(request);
            destroy_response(response);
            
            exit(0);
        }
        close(new_fd);
    }
    dbgprintf("QSHTTPD done\n");
    return 0;
}







