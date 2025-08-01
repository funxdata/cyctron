#ifndef SERVER_H
#define SERVER_H

int setup_server_socket(unsigned short port);
int run_event_loop(int listen_fd);

#endif // SERVER_H
