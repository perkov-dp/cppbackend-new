#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

using namespace std;

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    //  принимает поле игрока с уже расставленными кораблями
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    //  содержит всю логику игры
    void StartGame(tcp::socket& socket, bool my_initiative) {
        // TODO: реализуйте самостоятельно
        boost::system::error_code ec;	// объект ec для сохранения кода ошибки

        while (!IsGameEnded()) {
            //  сервер
            if (my_initiative == false) {                                
                //  отметим результат попадания по своему полю
                SeabattleField::ShotResult shoot_res = ReadMove(socket); 
                if (shoot_res == SeabattleField::ShotResult::MISS) {
                    my_initiative = true;
                } else {
                    //  выводим свое поле и поле соперника
                    PrintFields();
                    cout << "Waiting for turn..." << endl;                    
                }

                //  отсылаем результат попадания
                SendResult(socket, shoot_res);
            } else {
                //  выводим свое поле и поле соперника
                PrintFields();

                //  отправка результата выстрела через сокет
                string shoot_coords = SendMove(socket);
                
                //  анализируем статус попадания и определяем статус корабля
                //  мимо -> стреляет соперник, иначе -> мы                
                SeabattleField::ShotResult shoot_res = ReadResult(socket);
                if (shoot_res == SeabattleField::ShotResult::MISS) {
                    cout << "Miss!" << endl;
                    other_field_.MarkMiss(ParseMove(shoot_coords).value().second, ParseMove(shoot_coords).value().first);

                    //  выводим свое поле и поле соперника
                    PrintFields();
                    cout << "Waiting for turn..." << endl;

                    //  ход за соперником -> переходим в режим ожидания
                    my_initiative = false;
                } else if (shoot_res == SeabattleField::ShotResult::HIT) {
                    cout << "Hit!" << endl;
                    other_field_.MarkHit(ParseMove(shoot_coords).value().second, ParseMove(shoot_coords).value().first);
                } else if (shoot_res == SeabattleField::ShotResult::KILL) {
                    cout << "Kill!" << endl;
                    other_field_.MarkKill(ParseMove(shoot_coords).value().second, ParseMove(shoot_coords).value().first);
                }
            }        
        }
    }

private:
    //  преобразует текстовое представление клетки (например, B6) в координаты
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return {{p1, p2}};
    }

    //  преобразует координаты в текстовое представление клетки
    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first) + 'A', static_cast<char>(move.second) + '1'};
        return {buff, 2};
    }

    //  выводит в cout два поля: игрока и соперника
    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    //  возвращает true, если игра завершена
    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    // TODO: добавьте методы по вашему желанию
    //  отправка результата выстрела через сокет
    string SendMove(tcp::socket& socket) {
        //  считываем координаты выстрела
        std::string shoot_coords;
        cout << "Your turn: ";
        getline(cin, shoot_coords);

        //	отправим сообщение серверу
        //	write_some ждёт, пока хотя бы один байт передан. Это даёт гаран что в момент вызова функции соединение ещё активно.
        boost::system::error_code ec;	// объект ec для сохранения кода ошибки
        socket.write_some(net::buffer(shoot_coords + '\n'), ec);
        if (ec) {
            std::cout << "Client::Error sending data"sv << std::endl;
            return {};
        }

        return shoot_coords;
    }

    //  получения результата выстрела через сокет
    SeabattleField::ShotResult ReadResult(tcp::socket& socket) {
        // Прочтём из сокета один байт char - результат выстрела. Это синхронная операция.
        net::streambuf stream_buf;
        boost::system::error_code ec;	// объект ec для сохранения кода ошибки
        net::read_until(socket, stream_buf, '\n', ec);
        if (ec) {
            std::cout << "Client::Error reading data"sv << std::endl;
        }

        //  получили статус попадания
        std::string server_data{
            std::istreambuf_iterator<char>(&stream_buf),
            std::istreambuf_iterator<char>()
        };
        //  удаляем символ конца строки
        server_data.pop_back();
                              
        return static_cast<SeabattleField::ShotResult>(stoi(server_data)); 
    }

    //  получения результата хода через сокет
    SeabattleField::ShotResult ReadMove(tcp::socket& socket) {
        boost::system::error_code ec;	// объект ec для сохранения кода ошибки

        // Прочтём из сокета один байт char - результат выстрела. Это синхронная операция.
        net::streambuf stream_buf;
        net::read_until(socket, stream_buf, '\n', ec);
        if (ec) {
            std::cout << "Server::Error reading data"sv << std::endl;
        }

        std::string client_data{
            std::istreambuf_iterator<char>(&stream_buf),
            std::istreambuf_iterator<char>()
        };
        client_data.pop_back();

        //  отметим результат попадания по своему полю
        return my_field_.Shoot(ParseMove(client_data).value().second, ParseMove(client_data).value().first);
    }

    //  отсылаем результат выстрела через сокет
    void SendResult(tcp::socket& socket, SeabattleField::ShotResult shoot_res) {
        //	write_some ждёт, пока хотя бы один байт передан. Это даёт гарантию, что в момент вызова функции соединение ещё активно.
        boost::system::error_code ec;	// объект ec для сохранения кода ошибки
        socket.write_some(net::buffer(to_string(static_cast<int>(shoot_res)) + '\n'), ec);
        if (ec) {
            std::cout << "Client::Error sending data"sv << std::endl;
        }
    }
private:
    SeabattleField my_field_;       //  поле игрока
    SeabattleField other_field_;    //  поле соперника
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);

    // TODO: реализуйте самостоятельно
    //  инициализация подключения
    net::io_context io_context;	// контекст ввода-вывода
    tcp::socket socket{io_context};	// сокет

    /**
     * чтобы сервер мог принимать подключения клиентов, он должен создать акцептор.
     * Акцептор слушает порт и может принимает входящие соединения.
     */ 
    // используем конструктор tcp::v4 по умолчанию для адреса 0.0.0.0
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    //	ждем подключения клиента    
    cout << "Server::Waiting for connection..."sv << endl;
    boost::system::error_code ec;	// объект ec для сохранения кода ошибки
    acceptor.accept(socket, ec);
    if (ec) {
    	cout << "Server::Can't accept connection"sv << endl;
    	return;
    }

    /**
     * После принятия соединения сокет можно использовать для получения и отправления данных. 
     * Он привязан к установленному соединению и становится для нас интерфейсом этого соединения. 
     * Вызов метода accept заставит программу ждать, пока кто-то не подключится к серверу по указанному порту.
     */
    agent.StartGame(socket, false);
};

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);

    // TODO: реализуйте самостоятельно
    //  инициализация подключения

    // Создадим endpoint - объект с информацией об адресе и порте.
    // Для разбора IP-адреса пользуемся функцией net::ip::address_v4::from_string.
    boost::system::error_code ec;
    auto endpoint = tcp::endpoint(net::ip::make_address(ip_str, ec), port);
    if (ec) {
        std::cout << "Wrong IP format"sv << std::endl;
        return;
    }

    // Теперь подключаемся к серверу. Клиент отправляет запрос — и для этого используется сокет, инициализированный контекстом
    net::io_context io_context;
    tcp::socket socket{io_context};
    socket.connect(endpoint, ec);
    if (ec) {
    	std::cout << "Can't connect to server"sv << std::endl;
    	return;
    }

    agent.StartGame(socket, true);
};

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
