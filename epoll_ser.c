#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "epoll_sevr.h"
#define EPOLL_MAX 2000
#define SERVER_PORT 8888




// get the type of the file
//used in the (send the head) func
const char *get_file_type(const char *name)
{
    char* dot;

    //from the back to the front to find the "."
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
//printf the error and exit
void p_error_(char *s)
{
    printf("%s\n",s);
    exit(1);
}


void epoll_run(int port)
{   
    //create the RB_tree
    int epfd = epoll_create(EPOLL_MAX);
    if(epfd == -1)
        p_error_("epoll_create error");

    int lfd = init_sock_bind_listen_fd(port,epfd);
    struct epoll_event allevents[EPOLL_MAX];
    while(1)
    {
        //unblock to wait
        int tasks = epoll_wait(epfd,allevents,EPOLL_MAX,0);
        if(tasks == -1)
            p_error_("epoll_wait error");
        int i =0;
        for(i =0;i<tasks;++i)
        {
            //the allevents[i] is not a EPOLLIN event
            if(!(allevents[i].events & EPOLLIN))
                continue;
            

            int sockfd = allevents[i].data.fd;
            //if fd is lfd,solve the accept
            //the solve_the_accept includes accept and add to the tee
            if(sockfd == lfd)
            {
                solve_the_accept(lfd,epfd);
                //the lfd is the only task
                if(tasks-1 == 0)
                    break;
            }
            else
            {
                printf("========read:tasks:%d=====\n",tasks);
                //solve the read task
                solve_the_read(sockfd,epfd);
                printf("=====read_end:tasks:%d====\n",tasks);
            }
        }
    }

}
//accept and add to the tree
void solve_the_accept(int lfd,int epfd)
{
    //the struct of cfd
    struct sockaddr_in cfd_addr;
    int cfd_addr_len = sizeof(cfd_addr);
    //accept
    int cfd = accept(lfd,(struct sockaddr*)&cfd_addr,&cfd_addr_len);
    if(cfd == -1)
        p_error_("accept error");
    //print the cfd ip port
    char ip_host_string[64] = {0};
    printf("connected cfd:%d---ip:%s---port%d\n",cfd,
                    inet_ntop(AF_INET,&cfd_addr.sin_addr.s_addr,ip_host_string,sizeof(ip_host_string)),
                    ntohs(cfd_addr.sin_port));
    //set the cfd unblock
    int flg = fcntl(cfd,F_GETFL);
    flg = flg | O_NONBLOCK;
    fcntl(cfd,F_SETFL,flg);

    //the struct of epoll_ctl
    struct epoll_event tmp;
    tmp.data.fd = cfd;
    tmp.events = EPOLLIN | EPOLLET;
    //add to the tree
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&tmp);
    if(ret == -1)
        p_error_("epoll_ctl in solve_the_accept error");
}

//sock、bind、listen、add to the tree
int init_sock_bind_listen_fd(int port,int epfd)
{
    //socket
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1)
        p_error_("socker error");
    //the struct of bind
    struct sockaddr_in lfd_addr;
    lfd_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    lfd_addr.sin_family = AF_INET;
    lfd_addr.sin_port = htons(port);
    //bind
    int ret = bind(lfd,(struct sockaddr*)&lfd_addr,sizeof(lfd_addr));
    if(ret == -1)
        p_error_("bind error");
    //listen
    ret = listen(lfd,128);
    if(ret == -1)
        p_error_("listen error");
    
    //the struct: of epoll_ctl's lfd
    struct epoll_event tmp;
    tmp.events = EPOLLIN;
    tmp.data.fd = lfd;
    ret = epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&tmp);
    if(ret == -1)
        p_error_("epoll_ctl error");
    
    //setsockopt
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    return lfd;
}

//del the node from the tree
//close cfd
void dis_connect(int cfd,int epfd)
{
    int ret = epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
    if(ret == -1)
        p_error_("epoll ctl in dis_connect error");
    
    close(cfd);
}
//read
void solve_the_read(int cfd,int epfd)
{
    char linebuf[BUFSIZ]={0};
    int len = get_line(cfd,linebuf,sizeof(linebuf));
    if(len == 0)
    {
        printf("the client is closed");
        dis_connect(cfd,epfd);
        //in this case, the linebuf is NULL, the func will not solve_http_request and will not
        //dis_connect again
    }else{
        printf("http head:\n");
        printf("%s\n",linebuf);
        //read up the http head
        while(1)
        {
            char BUF[BUFSIZ];
            int len = get_line(cfd,BUF,BUFSIZ);
            if(len == -1)
                break;
            if(BUF[0] == '\n')
                break;
        }
        printf("http head end\n");
    }


    //if the head is request head:
    if(strncasecmp("get",linebuf,3)==0)
    {
        printf("solve_http_request......\n");
        solve_http_request(linebuf,cfd);
        //the http protocol's default is disconnect when connected
        dis_connect(cfd,epfd);
    }
}


//HTTP/1.1 200 OK
// http/1.1     status disc
void send_the_error_page(int cfd,int status,char *disc,char *text)
{
    char BUF[4096]={0};
    sprintf(BUF,"%s %d %s\r\n","HTTP/1.1",status,disc);
    sprintf(BUF+strlen(BUF),"Content-Type:%s\r\n","text/html");
    sprintf(BUF+strlen(BUF),"Content-Length:%d\r\n",-1);
    sprintf(BUF+strlen(BUF),"Connection:close\r\n");
    send(cfd,BUF,strlen(BUF),0);
    send(cfd,"\r\n",2,0);

    memset(BUF,0,sizeof(BUF));

    sprintf(BUF, "<html><head><title>%d %s</title></head>\n", status, disc);
	sprintf(BUF+strlen(BUF), "<body bgcolor=\"#ffffff\"><h1 align=\"center\">%d %s</h1>\n", status, disc);
	
    //text
    sprintf(BUF+strlen(BUF), "%s\n", text);
	sprintf(BUF+strlen(BUF), "<hr>\n</body>\n</html>\n");
	send(cfd, BUF, strlen(BUF), 0);
}

//http/1.1 status disc
//Content-Type: type;
//Content-Type: len
void send_respond_head(int cfd,int no,const char *disc,const char *type,long len) //status disc type
{
    char buf[1024] = {0};
    sprintf(buf, "http/1.1 %d %s\r\n", no, disc);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf+strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    // 
    send(cfd, "\r\n", 2, 0);

}
void send_the_dir(int cfd,char *dirname)
{
    printf("send the dir \n");
    int i,ret;
     char buf[BUFSIZ] = {0};
     sprintf(buf,"<html><head><title>目录名: %s</title></head>",dirname);
     sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);

     char encode_str_[1024]={0};
     char path[1024]={0};

     struct dirent **ptr;
     int num = scandir(dirname,&ptr,NULL,alphasort);

     for(i = 0;i< num;++i)
     {
        char *name = ptr[i]->d_name;
        //
        sprintf(path,"%s/%s",dirname,name);
        printf("====path:---%s\n",path);
        struct stat st;
        stat(path,&st);
        encode_str(encode_str_,sizeof(encode_str_),name);

        if(S_ISREG(st.st_mode)) {         //if file
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    encode_str_, name, (long)st.st_size);
        } else if(S_ISDIR(st.st_mode)) {		// if dir       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    encode_str_, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
     }
    
    sprintf(buf+strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    printf("dir message send OK!!!!\n");
}


void send_the_reg(int cfd,char *filename)
{
    int fd = open(filename,O_RDONLY);
    if(fd == -1)
        p_error_("open in the send_the_reg error");
    char buf[4096]= {0};
    int len = 0;
    int ret = 0;
    while((len = read(fd,buf,4096))>0)
    {
        ret = send(cfd,buf,len,0);
        if(ret == -1)
        {
            if(errno == EAGAIN)
            {
                printf("send error(EAGAIN)\n");
                continue;
            }else if(errno == EINTR)
            {
                printf("send error(EINTR)\n");
                continue;
            }
            else{
                p_error_("send error exit");
            }
        }
    }

    if(len == -1)
        p_error_("read error");

    close(fd);
}

//the get line
//read the line like：get /hello.c http/1.1\n
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {    
        n = recv(sock, &c, 1, 0);  //flag=0 equal to the "wirte" func
        if (n > 0)
        {        
            if (c == '\r') {            
                n = recv(sock, &c, 1, MSG_PEEK); //MSG_PEEK means that message will not be deleted after been read
                if ((n > 0) && (c == '\n')) {               
                    recv(sock, &c, 1, 0);
                } else {                            
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } 
        else 
        {       
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

//get /hello http/1.1\n
void solve_http_request(const char *head,int cfd)
{
    char method[12],path[1024],protocol[12];
    sscanf(head,"%[^ ] %[^ ] %[^ ]",method,path,protocol);
    //like the '/hello' 
    //path+1 is 'hello'
    decode_str(path,path);
    char *file = path + 1;

    if(strcmp(path,"/") == 0)
        file = "./";
    struct stat file_stat;
    int ret = stat(file,&file_stat);
    if(ret == -1){
        send_the_error_page(cfd,404,"NOT FOUND","NO SUCH FILE!!!");
        return;
    }

    if(S_ISDIR(file_stat.st_mode))//dir
    {
        send_respond_head(cfd,200,"OK",get_file_type(".html"),-1);
        send_the_dir(cfd,file);
    }else if(S_ISREG(file_stat.st_mode))//file
    {
        send_respond_head(cfd,200,"OK",get_file_type(file),file_stat.st_size);
        send_the_reg(cfd,file);
    }

    
}
// 16to10
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}


 //to encode (for the utf-8)
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {      
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

//let %A9 %B1 be conversed to the num in 10
void decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  ) {     
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {       
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;                      
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}



