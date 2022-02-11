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
#include <nlohmann/json.hpp>
#include <server_ws.hpp>
#include <time.h>
#include <sys/types.h>
#include <unistd.h> 
#include <stdlib.h>
#include <errno.h>  
#include <sys/wait.h>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using json = nlohmann::json;

double *fft_in[10];
fftw_complex *fft_out[10];
fftw_plan fft_plan[10];
SimpleWeb::SocketServer<SimpleWeb::WS> server = WsServer(); 
json sample_json;
std::shared_mutex socket_mutex;
std::condition_variable_any socket_cv;
JSAMPROW row_pointers[10][1];

std::vector<std::thread> fftThreads;

struct jpeg_compress_struct jpeg_structs[10];
struct jpeg_error_mgr jpeg_errors[10];

const std::string endpoint = "^/socket/?$";
const int port = 8079;

const int tenSecondsOfBytes = 1764000;//7056000;
const double logNormalizer = 85;//87.2986987426;//90.3089986992;//4.81647993062;

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;

	for (int i = 0; i < 10; i++) {
		fft_in[i] = (double*) fftw_malloc(sizeof(double) * 2048);
	    fft_out[i] = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 1025);
	    fft_plan[i] = fftw_plan_dft_r2c_1d(2048, fft_in[i], fft_out[i], FFTW_MEASURE);
	    
    }

	
    printf("INIT SERVER\n");
	server.config.port = port;
    server.config.timeout_request = 30;
    server.config.thread_pool_size = 3;

	auto &echo = server.endpoint[endpoint];

	echo.on_message = [&](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> message) {
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

        

	echo.on_open = [&](std::shared_ptr<WsServer::Connection> connection) {
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

	

	for (int index = 0; index < 10; index++) {
		int i = index;
		fftThreads.push_back(
		std::thread{ [&, i]
		{
		std::cout << "THREAD STARTING, here is i: " << i << std::endl;
		int16_t fileBuffer[4096];
		double monoBuffer[2048];
		double spectrum[1024];
		uint8_t colors[1024*3];
		while (true) {
			auto now = std::chrono::system_clock::now();
			std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
			struct tm timeInfo;
			struct tm local_tm = *localtime_r(&nowTime, &timeInfo);
			//printf("GOIN IN: %d:%d and i: %d\n", local_tm.tm_min, local_tm.tm_sec, i);
			if ((local_tm.tm_sec) >= (40)) {
				local_tm.tm_min = (local_tm.tm_min + 1) % 60;
				local_tm.tm_sec = i*2;
			}
			else if ((local_tm.tm_sec) >= (20)) {
				local_tm.tm_sec = 40 + i*2;
			}
			else {
				local_tm.tm_sec = 20 + i*2;
			}
			//printf("DELAYING UNTIL: %d:%d and i: %d\n", local_tm.tm_min, local_tm.tm_sec, i);
			std::time_t scheduleTime = mktime(&local_tm);
			auto schedule = std::chrono::system_clock::from_time_t(scheduleTime);
			std::this_thread::sleep_until(schedule);
			pid_t pid;

			
			int status;
			pid = fork();
			if (pid == -1) {
				printf("FAILED PID\n");
				break;
			}
			else if (pid == 0) {
				char iAsInt[6];
				strcpy(iAsInt, (std::to_string(i) + ".mp3").c_str());
				iAsInt[5] = 0;
				std::string curlCommand = "curl --no-progress-meter --speed-time 11 --speed-limit 100000000000  --output " 
				+  std::to_string(i) + ".mp3" 
				/*+ " --trace myTrace.txt --trace-time"*/ 
				+ " https://stream-relay-geo.ntslive.net/stream2";

				execl("/bin/sh", "/bin/sh", "-c", curlCommand.c_str(), NULL);
				break;
			}
			else {
				if (waitpid(pid, &status, 0) > 0) {
					//printf("FINISHED CURL for: %d\n", i);
					if (WIFEXITED(status)) {
						int es = WEXITSTATUS(status);
						if (es != 28) {
							continue;
						}						
					}
					
					std::string filePath = std::to_string(i) + ".wav";
					std::this_thread::sleep_for(1ms);
					std::string mpgCommand = "mpg123 -w " + filePath + " " + std::to_string(i) + ".mp3" + " > /dev/null";
					system(mpgCommand.c_str());
					FILE * myFile;
					myFile = fopen(filePath.c_str(), "rb");
					if (myFile == NULL) {
						continue;
					}
					fseek(myFile, 44, SEEK_SET);
					int firstNonzeroSample = 44;
					bool firstNonzeroFound = false;
					while (fread(fileBuffer, 2, 4096, myFile) == 4096) {
						for (int j = 0; j < 4096; j++) {
							//printf("Here is filebuffer at %d: %d\n", j, fileBuffer[j]);
							if (fileBuffer[j] != 0) {
								firstNonzeroSample += (j*2);
								firstNonzeroFound = true;
								break;
							}
						}
						if (firstNonzeroSample) {
							break;
						}
						firstNonzeroSample += (4096*2);
					}
					fseek(myFile, firstNonzeroSample, SEEK_SET);
					int currentByte = 0;
					FILE * imageFile;
					std::string imageFilePath = std::to_string(i) + ".jpg";
					imageFile = fopen(imageFilePath.c_str(), "wb");
					jpeg_create_compress(&jpeg_structs[i]);
					jpeg_stdio_dest(&jpeg_structs[i], imageFile);
					jpeg_structs[i].image_width = 1024;
			        jpeg_structs[i].image_height = 431;
			        jpeg_structs[i].input_components = 3;	
			        jpeg_structs[i].in_color_space = JCS_RGB;
			        jpeg_structs[i].err = jpeg_std_error(&(jpeg_errors[i]));
					jpeg_set_defaults(&jpeg_structs[i]);
					jpeg_set_quality(&jpeg_structs[i], 100, TRUE);
					jpeg_start_compress(& jpeg_structs[i], TRUE);

					int scanLineCounter = 0;
					while ((currentByte < tenSecondsOfBytes) && (fread(fileBuffer, 2, 4096, myFile) == 4096)) {
						//printf("4\n");
						double minMono = 1000000000000;
						double maxMono = -1000000000000;
						for (int j = 0; j < 2048; j++) {
							monoBuffer[j] = fileBuffer[j*2];
							monoBuffer[j] += fileBuffer[j*2+1];
							monoBuffer[j] /= 2;
							//printf("Here is monoBuffer before mod at %d: %f\n", j, monoBuffer[j]);
							//monoBuffer[j] += 32767;
							//monoBuffer[j] /= 65535;
							
							if (monoBuffer[j] > 0) {
								monoBuffer[j] /= 32767;
							}
							else {
								monoBuffer[j] /= 32768;
							}
							if (monoBuffer[j] > maxMono) {
								maxMono = monoBuffer[j];
							}
							if (monoBuffer[j] < minMono) {
								minMono = monoBuffer[j];
							}
							//monoBuffer[j] /= 32768;
							//printf("Here is monoBuffer at %d: %f\n", j, monoBuffer[j]);
						}
						//printf("Here is minMono: %f\n", minMono);
						//printf("here is maxMono: %f\n", maxMono);
						for (int j = 0; j<2048; j++) {
		            		fft_in[i][j] = monoBuffer[j] * hann2048[j];
		            		//fft_in[i][j] = monoBuffer[j];
		        		}
		        		fftw_execute(fft_plan[i]);
		        		double spectrumAccum = 0;
		        		double minFFT = 100000000;
		        		double maxFFT = -100000000;
		        		double maxSpectrum = -1000000;
		        		double minSpectrum = 1000000;
		        		for (int j = 0; j<1024; j++) {
		        			
		        			fft_out[i][j][0] /= 2048;
		        			fft_out[i][j][1] /= 2048;
		        			if (fft_out[i][j][0]  > maxFFT) {
								maxFFT = fft_out[i][j][0];
							}
							if (fft_out[i][j][0]  < minFFT) {
								minFFT = fft_out[i][j][1];
							}

		
	        				spectrum[j] = std::sqrt((fft_out[i][j][0] * fft_out[i][j][0]) + (fft_out[i][j][1] * fft_out[i][j][1]));
	        				//spectrum[j] /= 2048;
	        				spectrum[j] =  10.0 * log10(spectrum[j] == 0 ? 1/32768 : spectrum[j]);
	        				spectrum[j] = spectrum[j] / logNormalizer;
	        				spectrum[j] += 1.0;
		        			//printf("Here is spectrum: %f\n", spectrum[j]);
		        			spectrumAccum += spectrum[j];
		        			if (spectrum[j] > maxSpectrum) {
		        				//printf("NEW MAX AT J: %d\n", j);
		        				maxSpectrum = spectrum[j];
		        			}
		        			if (spectrum[j] < minSpectrum) {
		        				//printf("NEW MIN AT J: %d\n", j);
		        				minSpectrum = spectrum[j];
		        			}
		        			if (spectrum[j] > 1.0) {
		        				spectrum[j] = 1.0;
		        			}
		        			else if (spectrum[j] < 0) {
		        				spectrum[j] = 0;
		        			}
		        			double redShifter = (spectrum[j] * 510);
		        			redShifter = redShifter > 255 ? 255 : redShifter;
		        			colors[j*3] = 255 - redShifter;
		        			double greenMultiplier = spectrum[j] - 0.5;
		        			greenMultiplier = (greenMultiplier < 0) ? 0 : greenMultiplier;
		    				greenMultiplier = (greenMultiplier / 0.5);
		        			colors[j*3 + 1] = 255 - (greenMultiplier * 255);
		        			colors[j*3 + 2] = 255;
		    			}

		    			/*
		    			spectrumAccum /= 1024;
		    			printf("Here is max fft: %f\n", maxFFT);
		    			printf("Here is min ftt: %f\n", minFFT);
		    			printf("here is logNormalizer: %f \n", logNormalizer);
		    			printf("Here is averrage spectrum: %f \n", spectrumAccum);
		    			printf("Here is max spectrum: %f\n", maxSpectrum);
		    			printf("Here is min spectrun: %f\n", minSpectrum);
						*/
		    			
						row_pointers[i][0] = (unsigned char *) colors;
						(void) jpeg_write_scanlines(&jpeg_structs[i], row_pointers[i], 1);

						scanLineCounter++;
						//printf("Here is scanLineCounter: %d\n", scanLineCounter);
						fseek(myFile, -4096, SEEK_CUR);
						currentByte += 4096;
					}
					if (scanLineCounter == 431) {
						jpeg_finish_compress(& jpeg_structs[i]);
						fclose(imageFile);
						jpeg_destroy_compress(&jpeg_structs[i]);
					}
					else {
						fclose(imageFile);
						jpeg_destroy_compress(&jpeg_structs[i]);
						continue;
					}

					std::string mv_call = std::string("mv ") + std::to_string(i) + ".jpg" + std::string(" ") + std::to_string(i) + ".jpg.old";
					std::string flip_call = std::string("jpegtran -flip vertical -outfile ") + std::to_string(i) + ".jpg" + std::string(" ") + std::to_string(i) + ".jpg.old";
					std::string rm_call = std::string("rm ") + std::to_string(i) + ".jpg.old";
					std::future<int> f2 = std::async(std::launch::async, [mv_call, flip_call, rm_call]{ system(mv_call.c_str()); system(flip_call.c_str()); return system(rm_call.c_str()); });
					f2.wait();
					{
					    std::unique_lock<std::shared_mutex> lock(socket_mutex);
					    sample_json.clear();
					    sample_json["Image Name"] = std::to_string(i) + ".jpg";
					    std::ostringstream oss;
					    oss << std::put_time(std::localtime(&scheduleTime), "%Y-%m-%d %H:%M:%S");
					    auto str = oss.str();
					    //printf("HEre is date time: %s\n", str.c_str());
					    sample_json["Image Time"] = str;
					    sample_json["Served"] = true;
					}
					//printf("FLIPPED FILE\n");
        			socket_cv.notify_all();

				}
			}
			
		}
		}});
	}
/*
	//This will extract 10 seconds of audio from icecast
	system("curl --speed-time 10 --speed-limit 100000000000  --output speedlimit.mp3 --trace myTrace.txt --trace-time  https://stream-relay-geo.ntslive.net/stream2");

	//convet to mp3
	system("mpg123 -w output.wav speedlimit.mp3");
*/

	std::promise<unsigned short> server_port;

	server.start([&server_port](unsigned short port) {
                server_port.set_value(port);
            }
    );

    std::cout << "Server listening on port" << server_port.get_future().get() << std::endl << std::endl;

	while (true) {
		std::this_thread::sleep_for(1000000s);
	}

	return 0;
}