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
        fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
        fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
        fft_plan = fftw_plan_dft_1d(2048, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);
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
                        std::shared_lock<std::shared_mutex> lock(socket_mutex);
                        socket_cv.wait(lock);
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

unsigned long crc_table[256];
   
int crc_table_computed = 0;
   
void make_crc_table(void)
{
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (unsigned long) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}
   
   
unsigned long update_crc(unsigned long crc, const unsigned char *buf,
                        int len)
{
    unsigned long c = crc;
    int n;

    if (!crc_table_computed)
        make_crc_table();
    for (n = 0; n < len; n++) {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void IQWebSocketServer::writeToBuffer(const double * buffer ) {
    
    for (int i = buffer_index; i < (buffer_index + 1000); i++) {
        i_buffer[i] = buffer[2*(i-buffer_index)*5];
        q_buffer[i] = buffer[2*(i-buffer_index)*5 + 1];
    }
    buffer_index = buffer_index + 1000;
    if (buffer_index >= 2048) {
        //printf("DOING FFT\n");
        for (int i = 0; i<2048; i++) {
            fft_in[i][0] = i_buffer[i] * hann2048[i];
            fft_in[i][1] = 1 * q_buffer[i] * hann2048[i];
        }
        fftw_execute(fft_plan);
        uint8_t local_buffer[1024];
        for (int i = 0; i<1024; i++) {
            std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
            double modulus = std::abs(bin);
            double normalize = modulus/2048/hann2048Sum;
            if (normalize <= 0) {
                normalize = 0.2048;
            }
            double dbFSMagnitude = 20 * log10(normalize);
            if (dbFSMagnitude > 0) {
                dbFSMagnitude = 0;
            }
            local_buffer[i] =abs(round((-80-dbFSMagnitude)*255/-80));
        }

        {
            std::unique_lock<std::mutex> lock(image_mutex);
            memcpy(&(magnitude_buffer[1]), local_buffer, 1024);
        }
        image_cv.notify_all();

        memcpy(i_buffer, &(i_buffer[2048]),1000);
        memcpy(q_buffer, &(q_buffer[2048]),1000);
        buffer_index = buffer_index - 1024;
    }
}

const uint8_t pngHeader[] = {0x89, 0x50 , 0x4E , 0x47 
                            , 0x0D , 0x0A , 0x1A , 0x0A 
                            , 0x00 , 0x00 , 0x00 , 0x0D};
                            
                              
const uint8_t pngParam[]   =  {0x49 , 0x48 , 0x44 , 0x52
                            , 0x00, 0x00, 0x04, 0x00 //width
                            , 0x00, 0x00, 0x08, 0x69 //height
                            , 0x08, 0x00, 0x00, 0x00, 0x00}; //simple 8 bit grayscale
                            
                            
const uint8_t pngParamCrc[] = { 0xB2, 0x2D, 0x40, 0xB9}; //crc32 0xB22D40B9

const uint8_t pngDataLength[] = {0x00, 0x21, 0xA4, 0x00};
const uint8_t pngIDAT[] = {0x49 ,0x44 ,0x41 ,0x54};

const uint8_t pngFooter[] = {0x00 , 0x00 , 0x00 , 0x00 
                            , 0x49 , 0x45 , 0x4E , 0x44 
                            , 0xAE , 0x42 , 0x60 , 0x82};


IQWebSocketServer::~IQWebSocketServer() {
	server->stop();
    fftw_destroy_plan(fft_plan);
    fftw_free(fft_in);
    fftw_free(fft_out);
	if (serverThread.joinable())
		serverThread.join();
    runImageThread = false;
    image_cv.notify_all();
    if (imageThread.joinable())
        imageThread.join();
}

void IQWebSocketServer::openFile() {
    int i = 0;
    for (i; i < 4; i++) {
        if (file_pointers[i] == nullptr) {
            int index = i == 0 ? 3 : i - 1;
            if (first_file && file_rows[index] < 538 ) {
            }
            else  {
                first_file = true;
                break;
            }
        }
    }   
    if (i != 4) {
        std::string file_name = std::to_string(file_name_index);
        file_name.append(".png");
        file_names[i] = file_name;
        file_times[i] = std::time(nullptr);
        file_lengths[i] = 0;
        file_rows[i] = 0;
        std::string full_path = "/var/www/html/";
        full_path.append(file_name);
        file_pointers[i] = fopen(full_path.c_str(), "w");
        fwrite(pngHeader, 1, 12, file_pointers[i]);
        fwrite(pngParam, 1, 17, file_pointers[i]);
        fwrite(pngParamCrc, 1, 4, file_pointers[i]);
        fwrite(pngDataLength, 1, 4, file_pointers[i]);
        fwrite(pngIDAT, 1, 4, file_pointers[i]);
        file_crcs[i] = update_crc(0xffffffffL, pngIDAT, 4);
        file_name_index++;
        if (file_name_index >= 40) {
            file_name_index = 0;
        }
        deflateInit2(&(file_deflates[i]), Z_BEST_COMPRESSION, Z_DEFLATED, 15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY); 
        file_atomics[i].store(true);
    }
}

void IQWebSocketServer::closeFile() {
    int i = 0;
    for (i; i < 4; i++) {
        if (file_pointers[i] != nullptr ) {
            if (file_rows[i] >= 2153) 
                break;
        }
    }
    if (i != 4) {
        file_atomics[i].store(false);
        deflateEnd(&(file_deflates[i]));
        file_crcs[i] = file_crcs[i]  ^ 0xffffffffL;
        uint8_t crc_byte_array[4] = {(uint8_t)(file_crcs[i] >> 24 & 0x000000ff),(uint8_t) (file_crcs[i] >> 16 & 0x000000ff),(uint8_t)(file_crcs[i] >> 8 & 0x000000ff),(uint8_t)(file_crcs[i] >> 0 & 0x000000ff)};
        fwrite(crc_byte_array, 1, 4, file_pointers[i]);
        fwrite(pngFooter, 1, 12, file_pointers[i]);
        fseek(file_pointers[i], 33, SEEK_SET);
        uint8_t length_byte_array[4] = {(uint8_t)(file_lengths[i] >> 24 & 0x000000ff), (uint8_t)(file_lengths[i] >> 16 & 0x000000ff),(uint8_t)(file_lengths[i] >> 8 & 0x000000ff),(uint8_t)(file_lengths[i] >> 0 & 0x000000ff)};
        fwrite(length_byte_array, 1, 4, file_pointers[i]);
        fclose(file_pointers[i]);
        file_pointers[i] = nullptr;
        file_rows[i] = 0;
        {
            std::unique_lock<std::shared_mutex> lock(socket_mutex);
            sample_json.clear();
            sample_json["Image Name"] = file_names[i];
            std::ostringstream oss;
            oss << std::put_time(std::localtime(&(file_times[i])), "%d-%m-%Y %H-%M-%S");
            auto str = oss.str();
            sample_json["Image Time"] = str;
        }
        socket_cv.notify_all();
    
    }
}

const uint8_t filter_byte = 0;

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
    runImageThread = true;

    imageThread = std::thread{ [&]
    {
        uint8_t deflate_buffer[1025];

        for (int i = 0; i < 4; i++) {
            file_deflates[i].zalloc = Z_NULL;
            file_deflates[i].zfree = Z_NULL;
            file_deflates[i].opaque = Z_NULL;  
        }
        
        while(runImageThread) {  
        //IMAGE DIMENSIONS: 1024 * 2153 = 2204672
        //2204672 / 4 = 551168
        { 
            std::unique_lock<std::mutex> lock(image_mutex);
            image_cv.wait(lock);
            openFile();
            magnitude_buffer[0] = filter_byte;          
            //deflateEnd(&strm);
            for (int i = 0; i < 4; i++) {
                if (file_atomics[i].load()) {
                    file_deflates[i].avail_in = 1025;
                    file_deflates[i].next_in = magnitude_buffer;
                    file_rows[i]++;
                    do {
                        file_deflates[i].avail_out = 1025;
                        file_deflates[i].next_out = deflate_buffer;
                        if (file_rows[i] >=2153) {
                            deflate(&(file_deflates[i]), Z_FINISH);
                        }
                        else
                            deflate(&(file_deflates[i]), Z_NO_FLUSH);
                        uint32_t new_length = 1025 - file_deflates[i].avail_out;
                        fwrite(deflate_buffer, 1, new_length, file_pointers[i]);
                        file_lengths[i] = file_lengths[i] + new_length;
                        file_crcs[i] = update_crc(file_crcs[i], deflate_buffer, new_length);
                    } while (file_deflates[i].avail_out == 0);
                }
            }
            closeFile();
        }
        }    
    }
    };


}