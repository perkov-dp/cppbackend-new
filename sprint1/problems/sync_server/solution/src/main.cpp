// Подключаем заголовочный файл <sdkddkver.h> в системе Windows,
// чтобы избежать предупреждения о неизвестной версии Platform SDK,
// когда используем заголовочные файлы библиотеки Boost.Asio
#ifdef WIN32
#include <sdkddkver.h>
#endif

// Boost.Beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
// Библиотеки для работы с http
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp> 
#include <iostream>
#include <optional>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

namespace beast = boost::beast;
namespace http = beast::http;

// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>; 

// Структура ContentType задаёт область видимости для констант,
// задающий значения HTTP-заголовка Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    // При необходимости внутрь ContentType можно добавить и другие типы контента
    constexpr static std::string_view ALLOW = "GET, HEAD"sv;
};

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    // Считываем из socket запрос req, используя buffer для хранения данных.
    // В ec функция запишет код ошибки.
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }
    return req;
}

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    // Выводим заголовки запроса
    for (const auto& header : req) {
        std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
    }
}

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, 
                                  unsigned http_version, bool keep_alive,
                                  std::string_view allow = "",
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    // Формируем ответ со статусом status и версией равной http_version
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.set(http::field::allow, allow);
    // Формируем тело ответа
    response.body() = body;
    // Формируем заголовок Content-Length, сообщающий длину тела ответа
    response.content_length(body.size());
    // Формируем заголовок Connection в зависимости от значения заголовка в запросе
    response.keep_alive(keep_alive);
    return response;
}

// Обрабатываем запрос и формируем ответ
StringResponse HandleRequest(StringRequest&& req) {
    const auto text_response = [&req](http::status status, std::string_view text, 
        std::string_view allow = "") {
        return MakeStringResponse(status, text, req.version(), req.keep_alive(), allow);
    };

    if(req.method() == http::verb::get) {
        std::string target(std::string(req.target()));
        target.erase(target.begin());
        std::string responce("Hello, " + target);

        return text_response(http::status::ok, responce);
    } else if (req.method() == http::verb::head) {
        return text_response(http::status::ok, "");
    } else {
        return text_response(http::status::method_not_allowed, "Invalid method", ContentType::ALLOW);
    }
}

// Работа с HTTP-сессией будет располагаться в функции HandleConnection
// Функция настраиваемая. Принимает функию-стратегию в качестве параметра
template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        // Буфер для чтения данных в рамках текущей сессии.
        beast::flat_buffer buffer;

        // Продолжаем обработку запросов, пока клиент их отправляет
        while (auto request = ReadRequest(socket, buffer)) {
           // Обрабатываем запрос и формируем ответ сервера
            
           /**
            * В цикле обработки запроса выведите информацию о запросе функцией DumpRequest и отправьте ответ сервера. 
            * Цикл обработки запроса продолжается до тех пор, пока ReadRequest не вернёт std::nullopt, 
            * либо метод need_eof объекта http::response не вернёт true
            */
            
            DumpRequest(*request);

            // Делегируем обработку запроса функции handle_request
            StringResponse response = handle_request(*std::move(request));

            // Отправляем ответ сервера клиенту. 
            // write преобразует ответ сервера в последовательность байт в соответствии с протоколом HTTP и отправляет их через сокет.
            http::write(socket, response);

            // Прекращаем обработку запросов, если семантика ответа требует это
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    beast::error_code ec;
    // Запрещаем дальнейшую отправку данных через сокет — HTTP-сессия завершена
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    // Контекст для выполнения синхронных и асинхронных операций ввода/вывода
    net::io_context ioc;
    
    //	сервер будет прослушивать порт на всех сетевых интерфейсах этого компьютера, то есть на всех связанных с ним IP адресах
    //	порт 8080, тк на порт 80 может потребоваться право суперпользователя
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr unsigned short port = 8080;
    
    // Первое, что должен сделать ваш HTTP-сервер — открыть порт и начать принимать подключения клиентов к этому порту
    // Объект, позволяющий принимать tcp-подключения к сокету
    tcp::acceptor acceptor(ioc, {address, port});
    std::cout << "Server has started..."sv << std::endl;
    
    while (true) {
    	// Дождаться подключения клиента к сокету, переданному в качестве параметра метода
    	std::cout << "Waiting for socket connection"sv << std::endl;
        tcp::socket socket(ioc);
        // Метод acceptor::accept не возвращает управление, пока клиент не подключится к порту, с которым был проинициализирован acceptor
        acceptor.accept(socket);
        std::cout << "Connection received"sv << std::endl;

        /**
         * Область видимости переменной socket — тело цикла while. 
         * Чтобы фоновый поток мог работать с сокетом, лямбда-функция потока принимает его по значению, 
         * а сам сокет передаётся в конструктор thread как rvalue-ссылка. 
         * Так сокет будет перемещён из главного потока сервера в фоновый поток.
        */

        // Запускаем обработку взаимодействия с клиентом в отдельном потоке
        std::thread t(
            // Лямбда-функция будет выполняться в отдельном потоке
            [](tcp::socket socket) {
                // Вызываем HandleConnection, передавая ей функцию-обработчик запроса
                HandleConnection(socket, HandleRequest);
            },
            std::move(socket)
        );  // Сокет нельзя скопировать, но можно переместить

        // После вызова detach поток продолжит выполняться независимо от объекта t
        t.detach();
    }
       
    return 0;
}