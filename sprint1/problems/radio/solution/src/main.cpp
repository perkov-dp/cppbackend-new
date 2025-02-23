#include "audio.h"
#include <string_view>

#include "udp_server.h"
#include "udp_client.h"

void StartServer(uint16_t port) {
    // Создаём буфер достаточного размера, чтобы вместить датаграмму.
    Player player(ma_format_u8, 1);
    const size_t max_buffer_size = 65000 * player.GetFrameSize();

    net::io_context io_context_;
    UDP_server server(io_context_, port, max_buffer_size);

    try {
        vector<char> recv_data;

        // Запускаем сервер в цикле, чтобы можно было работать со многими клиентами
        for (;;) {
            recv_data = server.ServerRecv();
            cout << "Server::client message is..."sv
                 << string_view(recv_data.data(), recv_data.size()) 
                 << endl;
            player.PlayBuffer(recv_data.data(), recv_data.size(), 1.5s);
            cout << "Playing done" << endl;
        }
    } catch (std::exception& e) {
        cerr << e.what() << std::endl;
    }
}

void StartClient(uint16_t port) {
    net::io_context io_context;
    UDP_client client(io_context, port);
    Recorder recorder(ma_format_u8, 1);

    try {
        while (true) {
            std::string ip_address;

            cout << "Enter client`s IP-address..." << endl;
            getline(cin, ip_address);

            /**
             * Вычислите количество байт, которые нужно передать: 
             * умножьте количество фреймов, полученное от класса Recorder, на размер одного фрейма, 
             * полученный методом GetFrameSize() этого же класса.
             */
            auto rec_result = recorder.Record(65000, 1.5s);
            cout << "Recording done: " << string_view(rec_result.data.data(), rec_result.frames * recorder.GetFrameSize()) << endl;

            client.ClientSend(rec_result.data.data(), rec_result.frames * recorder.GetFrameSize(), ip_address);
        }            
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
}

int main(int argc, char** argv) {
    // 1) читаем параметры командной строки: определяем клиент я или сервер и считываем номер порта
    if (argc != 3) {
        cout << "Usage: "sv << argv[0] << " <server/client, port>"sv << endl;
        return 1;
    }
    string who_am_i = string(argv[1]);
    uint16_t port = stoi(string(argv[2]));

    // 2) Если клиент - требуем ввести IP-адрес в консоли
    if (who_am_i == "client") {
        StartClient(port);
    } else if (who_am_i == "server") {
        StartServer(port);
    } else {
        return 1;
    }  

    /*while (true) {
        std::string str;

        std::cout << "Press Enter to record message..." << std::endl;
        std::getline(std::cin, str);

        auto rec_result = recorder.Record(65000, 1.5s);
        std::cout << "Recording done" << std::endl;

        player.PlayBuffer(rec_result.data.data(), rec_result.frames, 1.5s);
        std::cout << "Playing done" << std::endl;
    }*/

    return 0;
}
