#pragma once
#include <boost/asio.hpp>
#include <iostream>

namespace net = boost::asio;
using net::ip::udp;

using namespace std;

class UDP_client {
public:
    UDP_client(net::io_context& io_context, uint16_t port) 
        : port_(port), 
          socket_(io_context, udp::v4()
    ) {
        // Перед отправкой данных нужно открыть сокет. 
        // При открытии указываем протокол (IPv4 или IPv6) вместо endpoint.
    }
    void ClientSend(const char *data, size_t size, const std::string& ip_address) {
        try {
            boost::system::error_code ec;
            auto endpoint = udp::endpoint(net::ip::address_v4::from_string(ip_address, ec), port_);
            socket_.send_to(boost::asio::buffer(data, size), endpoint);            
        } catch (std::exception& e) {
            cerr << e.what() << endl;
        }
    }
private:
    uint16_t port_;
    udp::socket socket_;
};
