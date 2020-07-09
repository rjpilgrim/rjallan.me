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
#include <hann8192.hpp>
#include <math.h>

#include <nlohmann/json.hpp>
#include <server_ws.hpp>

using json = nlohmann::json;

class IQWebSocketServer {
public:

	IQWebSocketServer() = delete;
	IQWebSocketServer(const IQWebSocketServer&) = delete;
	IQWebSocketServer& operator=(const IQWebSocketServer &) = delete;
	IQWebSocketServer(IQWebSocketServer &&) = delete;
	IQWebSocketServer & operator=(IQWebSocketServer &&) = delete;


	IQWebSocketServer(unsigned short port, std::string endpoint);

	bool checkQueue(boost::asio::ip::address ip_addr);

	virtual ~IQWebSocketServer();

	void writeToBuffer(const double * buffer );
	
	void run();

protected:
	double i_buffer[13196];
	double q_buffer[13196];
	int magnitude_buffer[8196];
	fftw_complex *fft_in;
	fftw_complex *fft_out;
	fftw_plan fft_plan;
	unsigned int buffer_index = 0;
	json sample_json;
	std::shared_mutex buffer_mutex;
	std::condition_variable_any buffer_cv;
	std::atomic<uint16_t> socket_counter;
	std::vector<boost::asio::ip::address> connection_queue;
	std::mutex queue_mutex;
	std::thread serverThread;
	std::unique_ptr<SimpleWeb::SocketServer<SimpleWeb::WS>> server; 
};
