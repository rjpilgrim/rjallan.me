#pragma once
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <future>
#include <complex>
#include <fftw3.h>
#include <hann2048.hpp>
#include <math.h>
#include <ctime>
#include <stdio.h>
#include <iomanip>
#include <jpeglib.h>
#include <stdlib.h>
#include <nlohmann/json.hpp>
#include <server_ws.hpp>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using json = nlohmann::json;

double *fft_in;
fftw_complex *fft_out;
fftw_plan fft_plan;
SimpleWeb::SocketServer<SimpleWeb::WS> server = WsServer(); 
json sample_json;
std::shared_mutex socket_mutex;
std::condition_variable_any socket_cv;

int main(int argc, char** argv)
{
	fft_in = (double*) fftw_malloc(sizeof(double) * 2048);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 1025);
    fft_plan = fftw_plan_dft_r2c_1d(2048, fft_in, fft_out, FFTW_MEASURE);

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
							"Error: " << ec << ", error message: " << ec.message() << std::endl;;
						}
					});
			
		};

            

		echo.on_open = [this](std::shared_ptr<WsServer::Connection> connection) {
			std::cout << "Server: Opened connection " << connection.get() << " to " << connection->remote_endpoint().address()
			<< ":" << connection->remote_endpoint().port();

            int status_ = 0;
            json j;

                            
            while ((status_ == 0)) {
                {
                    j.clear();
                    std::shared_lock<std::shared_mutex> lock(socket_mutex);
                    socket_cv.wait(lock);
                    if (status_ != 0) {
                        break;
                    }             
                    connection->send(sample_json.dump(), [&status_](const SimpleWeb::error_code &ec) {
							if(ec) {
								std::cout << "Server: Error sending message. " <<
								// See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
								"Error: " << ec << ", error message: " << ec.message() << std::endl;
                                status_ = -1;
							}
						});
                }
            }
            connection->send_close(1006);
		};

		// See RFC 6455 7.4.1. for status codes
		echo.on_close = [](std::shared_ptr<WsServer::Connection> connection, int status, const std::string & /*reason*/) {
			std::cout << "Server: Closed connection " << connection.get() << " with status code " << status << std::endl;
		};

		// See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
		echo.on_error = [](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec) {
			std::cout << "Server: Error in connection " << connection.get() << ". "
			<< "Error: " << ec << ", error message: " << ec.message() << std::endl;
		};

	}

	//This will extract 10 seconds of audio from icecast
	system("curl --speed-time 10 --speed-limit 100000000000  --output speedlimit.mp3  https://stream-relay-geo.ntslive.net/stream2");

	return 0;
}