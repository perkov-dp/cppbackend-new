#include <iostream>
#include <string>
#include <string_view>

#include <boost/asio.hpp>


namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

int main() {
    //	Порт можем выбрать любым, главное, чтобы он не был занят другим процессом
    //	Рекомендуется брать порты, начиная от 1024 — меньшие номера зарезервированы за стандартными протоколами. 
    //	Кроме того, под Linux программе нужны специальные права (например, права суперпользователя), чтобы открыть порт 1023 или меньший.
    static const int port = 3333;

    net::io_context io_context;	// контекст ввода-вывода

    /**
     * чтобы сервер мог принимать подключения клиентов, он должен создать акцептор.
     * Акцептор слушает порт и может принимает входящие соединения.
     */ 
    // используем конструктор tcp::v4 по умолчанию для адреса 0.0.0.0
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    
    //	ждем подключения клиента
    std::cout << "Waiting for connection..."sv << std::endl;
    boost::system::error_code ec;	// объект ec для сохранения кода ошибки
    tcp::socket socket{io_context};	// сокет
    acceptor.accept(socket, ec);

    if (ec) {
    	std::cout << "Can't accept connection"sv << std::endl;
    	return 1;
    }
    
    /**
     * После принятия соединения сокет можно использовать для получения и отправления данных. 
     * Он привязан к установленному соединению и становится для нас интерфейсом этого соединения. 
     * Вызов метода accept заставит программу ждать, пока кто-то не подключится к серверу по указанному порту.
     */
     
     // Прочтём из сокета одну строку вплоть до символа \n. Это синхронная операция.
     net::streambuf stream_buf;
     net::read_until(socket, stream_buf, '\n', ec);
     std::string client_data{
     	std::istreambuf_iterator<char>(&stream_buf),
        std::istreambuf_iterator<char>()
     };
     if (ec) {
     	std::cout << "Error reading data"sv << std::endl;
     	return 1;
     }
     std::cout << "Client said: "sv << client_data << std::endl; 
     
     //	Посылаем ответ клиенту
     socket.write_some(net::buffer("Hello, I'm server!\n"sv), ec);
     if (ec) {
        std::cout << "Error sending data"sv << std::endl;
        return 1;
     }
}
