#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <bits/stdc++.h>
#include <thread>

int handle_request(int client_fd, struct sockaddr_in client_addr)
{
  std::string client_message(1024, '\0');

  ssize_t brecvd = recv(client_fd, (void *)&client_message[0], client_message.max_size(), 0);
  if (brecvd < 0)
  {
    std::cerr << "error receiving message from client\n";
    close(client_fd);
    return 1;
  }
  
  std::cerr << "Client Message (length: " << client_message.size() << ")" << std::endl;
  std::clog << client_message << std::endl;
  std::string response;
  std::string echo_str;

  if (client_message.starts_with("GET /echo"))
  { 
    std::vector <std::string> parsed;
    std::stringstream t1(client_message);
    std::string temp;

    while(getline(t1, temp, ' '))
    {
        parsed.push_back(temp);
    }

    std::string echo_rep = parsed[1]; 
    std::stringstream t2(echo_rep);
    std::vector <std::string> parsed2;

    while(getline(t2, temp, '/'))
    {
        parsed2.push_back(temp);
    }

  echo_str = parsed2[2];
  std::stringstream num; 
  num << echo_str.size();
  response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + num.str() + "\r\n\r\n" + echo_str;
  }

  else if (client_message.starts_with("GET /user-agent"))
  {
    int found = client_message.find("User-Agent:");
    int accept_found = client_message.substr(found+12).find("\r\n");
    accept_found = found + accept_found + 12;
    std::string usr_msg = client_message.substr(found+12,accept_found-(found+12));
    std::stringstream usr_msg_size;
    usr_msg_size << usr_msg.size();

    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + usr_msg_size.str() + "\r\n\r\n" + usr_msg;
  }

  else if (client_message.starts_with("GET /file")){
    int found_file = client_message.find("HTTP");
    int found = client_message.find("/file");
    std::string filename = client_message.substr(found+2, (found_file-1)-(found-2));
    std::ifstream request_file(filename);
    std::string file_message;
    std::string temp;
    std::stringstream file_size;

    if (!request_file.is_open()){response = "HTTP/1.1 404 Not Found\r\n\r\n";}

    else
    {
      while(getline(request_file,temp)){
        file_message = file_message + temp + "\n";  
      }
      request_file.close();
      file_size << file_message.size();
      response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + file_size.str() + "\r\n\r\n" + file_message;
    }

  }

  else{response = client_message.starts_with("GET / HTTP/1.1\r\n") ? "HTTP/1.1 200 OK\r\n\r\n" : "HTTP/1.1 404 Not Found\r\n\r\n" ;}

  ssize_t bsent = send(client_fd, response.c_str(), response.size(), 0);

  if (bsent < 0)
  {
    std::cerr << "error sending response to client\n";
    close(client_fd);
    return 1;
  }
  close(client_fd);
  return 0;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  //Uncomment this block to pass the first stage
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int client_fd = 0;

  while(1){
  client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  std::cout << "Client connected\n";
  std::thread th(handle_request, client_fd, client_addr);
  th.detach();
  }

  close(server_fd);
  return 0;

}

