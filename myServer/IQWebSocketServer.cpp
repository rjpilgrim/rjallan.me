#include <IQWebSocketServer.hpp>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using json = nlohmann::json;

template <uint16_t N>
IQWebSocketServer<N>::IQWebSocketServer(unsigned short port, std::string endpoint) {
	try {
		server.reset(new WsServer());
        socket_counter.store(0);
		if (server) {
			server->config.port = port;

			auto &echo = server->endpoint[endpoint];

			echo.on_message = [this](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> message) {
				auto message_str = message->string();
				auto send_stream = std::make_shared<WsServer::OutMessage>();
				
					*send_stream << message_str;
					// connection->send is an asynchronous function
					connection->send(send_stream, [](const SimpleWeb::error_code &ec) {
								if(ec) {
									std::cout << "Server: Error sending message. " <<
									// See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
									"Error: " << ec << ", error message: " << ec.message();
								}
							});
				
			};

			echo.on_open = [this](std::shared_ptr<WsServer::Connection> connection) {
				std::cout << "Server: Opened connection " << connection.get() << " to " << connection->remote_endpoint().address()
				<< ":" << connection->remote_endpoint().port();
                int status_ = 1;
                json j;
                while ((status_ == 1)) {
                    uint16_t counter = socket_counter.load();
                    if (counter > 5) {
                        j.clear();
                        j["Served"] = false;
                        j["Queue Position"] = counter - 5;
                        
                    }
                    else {
                        uint16_t connections_from_this_ip = 0;
                        std::unordered_set<std::shared_ptr<WsServer::Connection>> my_connections;
                        my_connections = server->get_connections();
                        for (const auto& elem : my_connections) {
                            if (connection->remote_endpoint().address() == elem->remote_endpoint().address()) {
                                connections_from_this_ip++;
                            }
                        }
                        if (connections_from_this_ip > 1) {
                            j.clear();
                            j["Served"] = false;
                            j["Duplicate"] = true;
                        }
                        else {
                            status_ = 0;
                            j.clear();
                            j["Duplicate"] = false;
                        }
                    }
                    connection->send(j.dump(), [&status_](const SimpleWeb::error_code &ec) {
                            if(ec) {
                                std::cout << "Server: Error sending message. " <<
                                // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                                "Error: " << ec << ", error message: " << ec.message();
                                status_ = -1;
                            }
                        });
                    if (status_ != 0) {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(1s);
                    }
                }                
                while ((status_ == 0)) {
                    {
                        j.clear();
                        std::shared_lock<std::shared_mutex> lock(buffer_mutex);
                        buffer_cv.wait(lock);
                        /*j["Served"] = true;
                        j["I Samples"] = i_buffer;
                        j["Q Samples"] = q_buffer;*/
                        if (status_ != 0) {
                            break;
                        }             
                        connection->send(sample_json.dump(), [&status_](const SimpleWeb::error_code &ec) {
								if(ec) {
									std::cout << "Server: Error sending message. " <<
									// See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
									"Error: " << ec << ", error message: " << ec.message();
                                    status_ = -1;
								}
							});
                    }
                }
                connection->send_close(1006);
			};

			// See RFC 6455 7.4.1. for status codes
			echo.on_close = [](std::shared_ptr<WsServer::Connection> connection, int status, const std::string & /*reason*/) {
				std::cout << "Server: Closed connection " << connection.get() << " with status code " << status;
			};

			// See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
			echo.on_error = [](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
				std::cout << "Server: Error in connection " << connection.get() << ". "
				<< "Error: " << ec << ", error message: " << ec.message();
			};

		}
	} catch (...) {
		server.release();
	}
}
template <uint16_t N>
void IQWebSocketServer<N>::writeToBuffer(const double (&buffer)[N*2] ) {
    {
        std::unique_lock<std::shared_mutex> lock(buffer_mutex);
        for (int i = 0; i < N; i++) {
            i_buffer[i] = buffer[2*i*5];
            q_buffer[i] = buffer[2*i*5 + 1];
        }
        sample_json.clear();
        sample_json["Served"] = true;
        sample_json["I Samples"] = i_buffer;
        sample_json["Q Samples"] = q_buffer;
    }
    buffer_cv.notify_all();
}

template <uint16_t N>
IQWebSocketServer<N>::~IQWebSocketServer() {
	server->stop();
	if (serverThread.joinable())
		serverThread.join();
}

template <uint16_t N>
void IQWebSocketServer<N>::run() {
	serverThread = std::thread { [&] {server->start();} };
}