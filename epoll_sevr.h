#ifndef _EPOLL_SERVER_H
#define _EPOLL_SERVER_H

void p_error_(char *s);
void epoll_run(int port);
void solve_the_accept(int lfd,int epfd);
int init_sock_bind_listen_fd(int port,int epfd);
void dis_connect(int cfd,int epfd);
void solve_the_read(int cfd,int epfd);
void solve_http_request(const char *head,int cfd);
void send_the_error_page(int cfd,int status,char *disc,char *text);
void send_respond_head(int cfd,int status,const char *disc,const char *type,long len) ;
void send_the_dir(int cfd,char *dirname);
void send_the_reg(int cfd,char *filename);
int get_line(int sock, char *buf, int size);
int hexit(char c);
void encode_str(char* to, int tosize, const char* from);
void decode_str(char *to, char *from);
const char *get_file_type(const char *name);

#endif
