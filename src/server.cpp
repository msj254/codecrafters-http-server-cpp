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
#include <zlib.h>

std::string gzip_compress(const std::string &data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("deflateInit2 failed while compressing.");
    }
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = data.size();
    int ret;
    char outbuffer[32768];
    std::string outstring;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Exception during zlib compression: (" + std::to_string(ret) + ") " + zs.msg);
    }
    return outstring;
}

int handle_request(int client_fd, struct sockaddr_in client_addr, std::string dir)
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
   int found = client_message.find("/echo");
   int f_http = client_message.find("HTTP");
   std::string echo_msg = client_message.substr(found+6,(f_http-1)-(found+6));
   std::stringstream echo_msg_size;
   echo_msg_size << echo_msg.size();

    //std::cout << "ENCODING: " << client_message.find("gzip") << std::endl;
    //std::cout << "INVALID: " << client_message.find("invalid") << std::endl;
    //std::cout << "SIZE: " << client_message.size() << std::endl;

   if (client_message.find("Accept-Encoding")!=std::string::npos)
   {
      if (client_message.find("gzip")!=std::string::npos) 
          {
            std::string gzip_msg = gzip_compress(echo_msg);
            std::stringstream compress_length;
            compress_length << gzip_msg.size();
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: gzip\r\nContent-Length: " + compress_length.str() +  "\r\n\r\n" + gzip_msg;
          }

      else {response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";}
   }

    else {response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + echo_msg_size.str() + "\r\n\r\n" + echo_msg;}   
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
    std::string filename = client_message.substr(found+7, (found_file-1)-(found+7));
    std::ifstream request_file(dir+filename);
    std::string file_message;
    std::string temp;
    std::stringstream file_size;

    if (request_file.good())
    {
      while(getline(request_file,temp)){
        file_message = file_message + temp;  
      }
      request_file.close();
      file_size << file_message.size();
      response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + file_size.str() + "\r\n\r\n" + file_message;
    }

    else {response = "HTTP/1.1 404 Not Found\r\n\r\n"; }

  }

  //lots of magic number
  else if (client_message.starts_with("POST /file"))
  {
    int content = client_message.find("Content-Length:");
    int type = client_message.find("Content-Type");
    int end  = client_message.substr(content).find("\r\n");
    std::string content_num = client_message.substr(content+16,end-(content+16));
    int content_length = stoi(content_num);
    std::string request_body = client_message.substr(type+42,content_length);
    
    int found_file = client_message.find("HTTP");
    int found = client_message.find("/file");
    std::string filename = client_message.substr(found+7, (found_file-1)-(found+7));
    //combine dir and filename
    std::ofstream request_file("/files/" + filename);
    std::ofstream request_file_to_disk(dir+filename);
    request_file << request_body;
    request_file_to_disk << request_body;
    request_file.close();
    request_file_to_disk.close();

    response = "HTTP/1.1 201 Created\r\n\r\n";
    //std::cout << "request body: " << request_body << std::endl;
    //std::cout << client_message;
    //
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

  std::string dir;
  if (argc == 3 && strcmp(argv[1], "--directory") == 0)
  {
  	dir = argv[2];
  }
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
  std::thread th(handle_request, client_fd, client_addr,dir);
  th.detach();
  }

  close(server_fd);
  return 0;

}

