#include <IQWebSocketServer.hpp>



using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using json = nlohmann::json;

bool IQWebSocketServer::checkQueue(boost::asio::ip::address ip_addr) {
                queue_mutex.lock();
                bool returnbool = (std::find(connection_queue.begin(), connection_queue.end(), ip_addr) == connection_queue.end());
                queue_mutex.unlock();
                return returnbool;
            }

IQWebSocketServer::IQWebSocketServer(unsigned short port, std::string endpoint) {
	try {
        fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 8192);
        fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 8192);
        fft_plan = fftw_plan_dft_1d(8192, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);
		server.reset(new WsServer());
        socket_counter.store(0);
		if (server) {
            printf("INIT SERVER\n");
			server->config.port = port;
            server->config.timeout_request = 30;
            server->config.thread_pool_size = 3;

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
                bool queued = false;
                while ((status_ == 1)) {
                    queue_mutex.lock();
                    uint16_t counter = socket_counter.load();
                    if (counter > 5) {
                        j.clear();
                        j["Served"] = false;                     
                        if (!queued) {
                            connection_queue.push_back(connection->remote_endpoint().address());
                            j["Queue Position"] = connection_queue.size();
                            queued = true;
                        }
                        else {
                            std::vector<boost::asio::ip::address>::iterator itr = std::find(connection_queue.begin(), connection_queue.end(), connection->remote_endpoint().address());
                            j["Queue Position"] = std::distance(connection_queue.begin(), itr) + 1;
                        }       
                    }
                    else if (queued) {
                        boost::asio::ip::address front_ip = connection_queue.front();
                        if (front_ip == connection->remote_endpoint().address()) {
                            connection_queue.erase(connection_queue.begin());
                            queued = false;
                        }
                    }
                    if (counter <= 5 && !queued /*checkQueue(connection->remote_endpoint().address())*/) {
                        
                            status_ = 0;
                            socket_counter.fetch_add(1);
                            j.clear();
                            j["Duplicate"] = false;
                        
                    }
                    queue_mutex.unlock();
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
                socket_counter.fetch_sub(1);
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
        printf("GOT EXCEPTION\n");
		server.release();
	}
}

void IQWebSocketServer::writeToBuffer(const double * buffer ) {
    bool send = false;
    {
        std::unique_lock<std::shared_mutex> lock(buffer_mutex);
        for (int i = buffer_index; i < (buffer_index + 1000); i++) {
            i_buffer[i] = buffer[2*(i-buffer_index)*5];
            q_buffer[i] = buffer[2*(i-buffer_index)*5 + 1];
        }
        buffer_index = buffer_index + 1000;
        if (buffer_index >= 8192) {
            for (int i = 0; i<8192; i++) {
                fft_in[i][0] = i_buffer[i] * hann8192[i];
                fft_in[i][1] = 1 * q_buffer[i] * hann8192[i];
            }
            fftw_execute(fft_plan);

            for (int i = 0; i<8192; i++) {
                std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
                double modulus = std::abs(bin);
                double normalize = modulus/2048/hann8192Sum;
                if (normalize <= 0) {
                    normalize = 0.2048;
                }
                double dbFSMagnitude = 20 * log10(normalize);
                if (dbFSMagnitude > 0) {
                    dbFSMagnitude = 0;
                }
                magnitude_buffer[(i+4096)%8192] =abs(round((-80-dbFSMagnitude)*255/-80));
            }

            memcpy(i_buffer, &(i_buffer[4096]),9100);
            memcpy(q_buffer, &(q_buffer[4096]),9100);
            buffer_index = buffer_index - 4096;
            sample_json.clear();
            sample_json["Served"] = true;
            sample_json["Magnitudes"] = magnitude_buffer;
            send = true;
        }
    }
    if (send)
        buffer_cv.notify_all();
}

IQWebSocketServer::~IQWebSocketServer() {
	server->stop();
    fftw_destroy_plan(fft_plan);
    fftw_free(fft_in);
    fftw_free(fft_out);
	if (serverThread.joinable())
		serverThread.join();
}

void IQWebSocketServer::run() {
    std::promise<unsigned short> server_port;
	serverThread = std::thread { [&] 
        {
            server->start([&server_port](unsigned short port) {
                server_port.set_value(port);
            }
            );
        } 
    };
    std::cout << "Server listening on port" << server_port.get_future().get() << std::endl << std::endl;
}