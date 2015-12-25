
#define BACKLOG 10


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
