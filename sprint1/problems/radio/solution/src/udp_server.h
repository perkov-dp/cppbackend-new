#pragma once
#include <boost/asio.hpp>
#include <vector>

using namespace std;

namespace net = boost::asio;
using net::ip::udp;

class UDP_server {
public:
    UDP_server(
        net::io_context& io_context,
        uint16_t port, size_t max_buffer_size) 
            : max_buffer_size_(max_buffer_size), 
              socket_(io_context, udp::endpoint(udp::v4(), port)
    ) {
    }

    std::vector<char> ServerRecv() {
        vector<char> recv_buf(max_buffer_size_);
        udp::endpoint remote_endpoint;

        // Получаем не только данные, но и endpoint клиента
        size_t size = socket_.receive_from(boost::asio::buffer(recv_buf), remote_endpoint);
        recv_buf.resize(size);

        return recv_buf;
    }
private:
    // Создаём буфер достаточного размера, чтобы вместить датаграмму.
    const size_t max_buffer_size_;
    udp::socket socket_;
};
