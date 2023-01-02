#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/stat.h> // For struct stat
#include <signal.h>   // For struct sigaction
#include <sys/wait.h> // For waitpid

#define BUF_SIZE_1 4096
#define BUF_SIZE_2 100
#define MAX_PENDING 8

static void die(char *msg) {
    perror(msg);
    exit(1);
}

void perror_response(FILE *fpw, FILE *fpr, char *status_code, char *req_uri) {
    if (!strcmp("301 Moved Permanently", status_code))
        fprintf(fpw, "HTTP/1.0 %s\r\nLocation: %s/\r\n\r\n<html><boyd><h1>%s</h1><p>The document "
                "has moved <a href=\"%s/\">here</a>.</p></body></html>\n", status_code, req_uri, 
                status_code, req_uri);
    else
        fprintf(fpw,"HTTP/1.0 %s\r\n\r\n<html><body><h1>%s</h1></body></html>\n", status_code, status_code);

    fclose(fpw);
    fclose(fpr);
}

void reap_children(int sig) {
 while (waitpid(-1, NULL, WNOHANG) > 0)
     ;
}

int main(int argc, char **argv) {

    // Ignore potential SIGPIPES
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL))
        die("sigaction");

    // Manage command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-port> <web-root>\n", argv[0]);
        exit(1);
    }

    char *server_port = argv[1];
    char *web_root = argv[2];
    
    // Define hints for getaddrinfo and zero it out
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    
    // Set fields of hints for establishing TCP/IP connection as a server
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;  // Specify that this is a server
    
    // Define where getaddrinfo returns information
    struct addrinfo *info;
    
    // Call getaddrinfo, which parses hints and returns info about the TCP/IP
    // connection it was called to establish.
    int addr_err;
    if ((addr_err = getaddrinfo(NULL, server_port, &hints, &info)) != 0) {
       fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_err));
       exit(1);
    }
    
    // Create socket according to parsed info from getaddrinfo
    int serv_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (serv_fd < 0)
        die("socket");
    
    // Bind socket to a port on a server
    if (bind(serv_fd, info->ai_addr, info->ai_addrlen) < 0)
        die("bind");

    // Free info returned from getaddrinfo
    freeaddrinfo(info);

    // Start listening for connections on this port with atmost 8 pending
    // connections
    if (listen(serv_fd, 8) < 0)
        die("listen");
    
    for(;;) {

        // Define space to receive client address info
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_len = sizeof(clnt_addr);

        // Block until a client connects to server, in which case this returns a
        // new socket file descriptor
        int clnt_fd = accept(serv_fd, (struct sockaddr *) &clnt_addr, &clnt_addr_len);
        if (clnt_fd < 0)
            die("accept");
        
        // Handle reaping of children asynchronously
        struct sigaction sa2;
        memset(&sa2, 0, sizeof(sa2));         
        sigemptyset(&sa2.sa_mask);           
        sa2.sa_flags = SA_RESTART;           
        sa2.sa_handler = &reap_children;    
 
        if (sigaction(SIGCHLD, &sa2, NULL))
            die("sigaction");

        pid_t pid = fork();
        
        // Handle each client in the child process.
        if (pid == 0) {
            close(serv_fd);

            char *ip_addr = inet_ntoa(clnt_addr.sin_addr);
        
            FILE *clnt_r = fdopen(clnt_fd, "rb");
            FILE *clnt_w = fdopen(dup(clnt_fd), "wb");
        
            char req_line[BUF_SIZE_2];
            char blank_line[BUF_SIZE_2];
            blank_line[0] = '\0';

            char *method = NULL;
            char *requestURI = NULL;
            char *httpVersion = NULL;

            if (fgets(req_line, sizeof(req_line), clnt_r) == NULL) {
                fprintf(stderr, "%s (%d) \"%s %s %s\" 400 Bad Request\n", ip_addr, getpid(), method, requestURI, httpVersion);
                fclose(clnt_w);
                fclose(clnt_r);
            } else {

                char *token_separators = "\t \r\n";
                char *method = strtok(req_line, token_separators);
                char *requestURI = strtok(NULL, token_separators);
                char *httpVersion = strtok(NULL, token_separators);

                if (method == NULL || requestURI == NULL || httpVersion == NULL || 
                        strtok(NULL, token_separators) != NULL || strcmp("GET", method) || 
                        (strcmp("HTTP/1.0", httpVersion) && strcmp("HTTP/1.1", httpVersion))) {
                    perror_response(clnt_w, clnt_r, "501 Not Implemented", requestURI);
                    fprintf(stderr, "%s (%d) \"%s %s %s\" 501 Not Implemented\n", ip_addr, getpid(), method, requestURI, httpVersion);
                    exit(0);
                }

                if (requestURI[0] != '/' || strstr(requestURI, "/../") || !strcmp("/..", requestURI + 
                       strlen(requestURI) - 3)) {
                    perror_response(clnt_w, clnt_r, "400 Bad Request", requestURI);
                    fprintf(stderr, "%s (%d) \"%s %s %s\" 400 Bad Request\n", ip_addr, getpid(), method, requestURI, httpVersion);
                    exit(0);
                }
                
                while (fgets(blank_line, sizeof(blank_line), clnt_r) != NULL && strcmp(blank_line, "\r\n"))
                    ;
                    
                if (strcmp(blank_line, "\r\n")) {
                    fprintf(stderr, "%s (%d) \"%s %s %s\" 400 Bad Request\n", ip_addr, getpid(), method, requestURI, httpVersion);
                    fclose(clnt_w);
                    fclose(clnt_r);
                    exit(0);
                }
           
                char file_path[BUF_SIZE_2];
                sprintf(file_path, "%s%s", web_root, requestURI);
                int path_len = strlen(file_path);

                if (file_path[path_len - 1] == '/' && (path_len - 10) <= sizeof(file_path))
                    strcat(file_path, "index.html");
           
                struct stat st;
                if (stat(file_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                    perror_response(clnt_w, clnt_r, "301 Moved Permanently", requestURI);
                    fprintf(stderr, "%s (%d) \"%s %s %s\" 301 Moved Permanently\n", ip_addr, getpid(), method, requestURI, httpVersion);
                    exit(0);
                }

                FILE *req_file = fopen(file_path, "rb");
                if (req_file == NULL) {
                    perror_response(clnt_w, clnt_r, "404 Not Found", requestURI);
                    fprintf(stderr, "%s (%d) \"%s %s %s\" 404 Not Found\n", ip_addr, getpid(), method, requestURI, httpVersion);
                    exit(0);
                }
 
                fputs("HTTP/1.0 200 OK\r\n\r\n", clnt_w);
                fprintf(stderr, "%s (%d) \"%s %s %s\" 200 OK\n", ip_addr, getpid(), method, requestURI, httpVersion);

                char buf[BUF_SIZE_1];
                int len;
                while((len = fread(buf, 1, sizeof(buf), req_file)) > 0) 
                    fwrite(buf, 1, len, clnt_w);  
                
                fclose(req_file);
                fclose(clnt_w);
                fclose(clnt_r);        
           
            }

            exit(0);

        } else {

            close(clnt_fd);

        }
                 
    }

    // Though never reached, if server needs to be gracefully terminated, we
    // close its file descriptor here.
    close(serv_fd);

    return 0;
}
