#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include <nlohmann/json.hpp>
#include <server_ws.hpp>

using json = nlohmann::json;

template <uint16_t N = 5000>
class IQWebSocketServer {
public:

	IQWebSocketServer() = delete;
	IQWebSocketServer(const IQWebSocketServer&) = delete;
	IQWebSocketServer& operator=(const IQWebSocketServer &) = delete;
	IQWebSocketServer(IQWebSocketServer &&) = delete;
	IQWebSocketServer & operator=(IQWebSocketServer &&) = delete;


	IQWebSocketServer(unsigned short port, std::string endpoint);

	virtual ~IQWebSocketServer();

	void writeToBuffer(const double (&buffer)[N*2] );
	
	void run();

protected:
	volatile uint16_t i_buffer[N/5];
	volatile uint16_t q_buffer[N/5];
	volatile json sample_json;
	std::shared_mutex buffer_mutex;
	std::condition_variable_any buffer_cv;
	std::atomic<uint16_t> socket_counter;
	std::thread serverThread;
	std::unique_ptr<SimpleWeb::SocketServer<SimpleWeb::WS>> server; 
};