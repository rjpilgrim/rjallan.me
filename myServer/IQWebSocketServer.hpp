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
#include <zlib.h>

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
	double i_buffer[3048];
	double q_buffer[3048];
	uint8_t magnitude_buffer[2048];
	fftw_complex *fft_in;
	fftw_complex *fft_out;
	fftw_plan fft_plan;
	unsigned int buffer_index = 0;
	json sample_json;
	std::shared_mutex socket_mutex;
	std::condition_variable_any socket_cv;
	std::mutex image_mutex;
	std::condition_variable image_cv;
	std::atomic<uint16_t> socket_counter;
	std::vector<boost::asio::ip::address> connection_queue;
	std::mutex queue_mutex;
	std::thread serverThread;
	std::thread imageThread;
	bool first_file = false;
	FILE *file_pointers[4];
	std::time_t file_times[4];
	std::string file_names[4];
	std::atomic<bool> file_atomics[4];
	unsigned long file_crcs[4];
	uint32_t file_lengths[4];
	z_stream file_deflates[4];
	int file_rows[4];
	int file_name_index = 0;
	bool runImageThread = false;
	std::unique_ptr<SimpleWeb::SocketServer<SimpleWeb::WS>> server; 

	bool closed_file = false;

	void openFile();
	void writeToFiles();
	void closeFile();
};
