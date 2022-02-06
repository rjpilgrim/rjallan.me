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
#include <time.h>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using json = nlohmann::json;

double *fft_in[10];
fftw_complex *fft_out[10];
fftw_plan fft_plan[10];
SimpleWeb::SocketServer<SimpleWeb::WS> server = WsServer(); 
json sample_json;
std::shared_mutex socket_mutex;
std::condition_variable_any socket_cv;

std::vector<std::thread> fftThreads;

const int tenSecondsOfBytes = 7056000;

int main(int argc, char** argv)
{
	for (int i = 0; i < 10; i++) {
		fft_in[i] = (double*) fftw_malloc(sizeof(double) * 2048);
	    fft_out[i] = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 1025);
	    fft_plan[i] = fftw_plan_dft_r2c_1d(2048, fft_in, fft_out, FFTW_MEASURE);
    }

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

	for (int i = 0; i < 10; i++) {
		fftThreads.push_back(
		std::thread{ [&]
		{
		int16_t fileBuffer[4096];
		double monoBuffer[2048];
		double spectrum[1024];
		while (true) {
			auto now = std::chrono::system_clock::now();
			std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
			struct tm timeinfo;
			struct tm local_tm = *localtime_r(&nowTime, &timeInfo);
			if ((local_tm.tm_sec - i) >= (40) || (i >= 5 && (local_tm.tm_sec - i) < 0)) {
				local_tm.tm_min = (local_tm.tm_min + 1) % 60;
				local_tm.tm_sec = i*2;
			}
			else if ((local_tm.tm_sec - i) >= (20)) {
				local_tm.tm_sec = 40 + i*2;
			}
			else {
				local_tm.tm_sec = 20 + i*2;
			}
			std::time_t scheduleTime = mktime(&local_tm);
			auto schedule = std::chrono::system_clock::from_time_t(scheduleTime);
			std::this_thread::sleep_until(schedule);
			std::string curlCommand = "curl --speed-time 11 --speed-limit 100000000000  --output " 
				+  std::to_string(i) + ".mp3" 
				/*+ " --trace myTrace.txt --trace-time"*/ 
				+ " https://stream-relay-geo.ntslive.net/stream2";
			system(curlCommand.c_str());
			std::string filePath = std::to_string(i) + ".wav";
			std::string mpgCommand = "mpg123 -w " + filePath + " " + std::to_string(i) + ".mp3";
			system(mpgCommand.c_str());
			FILE * myFile;
			myFile = fopen(filePath.c_str(), "rb");
			fseek(myFile, 44, SEEK_SET);
			int firstNonzeroSample = 44;
			while (fread(fileBuffer, 2, 4096, myFile) == 4096) {
				for (int j = 0; j < 4096; j++) {
					if (fileBuffer[j] != 0) {
						firstNonzeroSample += j;
					}
				}
				firstNonzeroSample += 4096;
			}
			fseek(myFile, firstNonzeroSample, SEEK_SET);
			int currentByte = 0;
			while ((currentByte < tenSecondsOfBytes) && fread(fileBuffer, 2, 4096, myFile) == 4096) {
				for (int j = 0; j < 2048; j++) {
					monoBuffer[j*2] = fileBuffer[j*2];
					monoBuffer[j*2] += fileBuffer[j*2+1];
					monoBuffer[j*2] /= 2;
					monoBuffer[j*2] += 65535;

				}
				for (int j = 0; j<2048; j++) {
            		fft_in[i][j] = monoBuffer[j] * hann2048[j];
        		}
        		fftw_execute(fft_plan[i]);
        		for (int j = 0; j<2048; j++) {
        			fft_out[i][j][0] /= 2048;
        			fft_out[i][j][1] /= 2048;
    			}
    			for (int j = 0; j<1024; j++) {
    				spectrum[j] = ((fft_out[i][j][0] * fft_out[i][j][0]) + (fft_out[i][j][1] * fft_out[i][j][1]))/2
    			}
    			//GENERATE IMAGE LINE HERE

    			fseek(myFile, -2048, SEEK_CUR);
			}

			//OUTPUT IMAGE HERE
		}
		}});
	}
/*
	//This will extract 10 seconds of audio from icecast
	system("curl --speed-time 10 --speed-limit 100000000000  --output speedlimit.mp3 --trace myTrace.txt --trace-time  https://stream-relay-geo.ntslive.net/stream2");

	//convet to mp3
	system("mpg123 -w output.wav speedlimit.mp3");
*/

	return 0;
}