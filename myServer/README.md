This is the primary binary which runs on my website's server.

# Dependencies

You will need to build the LimeSuite library and headers. When generating the makefile for LimeSuite using CMake, I had to pass the following command in the build directory:

```
cmake -DENABLE_HEADERS=TRUE, -DENABLE_LIBRARY=TRUE ..
```
This will allow the FIND_PACKAGE(LimeSuite REQUIRED) command to complete successfully in this project.

Furthermore, you will need the development librarys for boost (really only asio is needed), alsa, libjpeg, and fftw3. All of these are accessible in most linux package managers.

Finally you will need nlohmann's json library (https://github.com/nlohmann/json) and SimpleWebSocketServer (https://gitlab.com/eidheim/Simple-WebSocket-Server). THe easiest way to deal with these in my project's case is to issue a git clone command to their repositories from this directory. My build configuration will automatically incorporate the resulting folder into the build.

# Additional Configuration 

This binary assumes that there is a default debian nginx WebServer running with some minor reconfiguration on the sites-available. Start with this command: 

```
sudo apt update
sudo apt install nginx
```

In the file /etc/nginx/sites-available/, replace the server configuration with this:

```
server {
	listen 80 default_server;
	listen [::]:80 default_server;

	root /var/www/html;

	index index.html index.htm index.nginx-debian.html;

	server_name _;

	location / {
		if ($request_method = 'GET') {
                  add_header 'Access-Control-Allow-Origin' '*';
                  add_header 'Access-Control-Allow-Methods' 'GET, POST, OPTIONS';
                  add_header 'Access-Control-Allow-Headers' 'DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range';
                  add_header 'Access-Control-Expose-Headers' 'Content-Length,Content-Range';
                }
		try_files $uri $uri/ =404;
	}
	
	location /play/ {
                types {
                        application/x-mpegURL m3u8;
                        audio/MP2T ts;
                }
                alias /var/media/;
        }

	location = /socket {
	    proxy_read_timeout 300s;
            proxy_connect_timeout 75s;
            proxy_pass              http://127.0.0.1:8079/socket;
            proxy_http_version      1.1;
            proxy_set_header        Host $http_host;
            proxy_set_header        X-Real-IP $remote_addr;
            proxy_set_header        Upgrade $http_upgrade
            proxy_set_header        Connection "upgrade";
        }
}
```

In addition, you will need to be running an ffmpeg in a separate process to listen on a loopback device to product the HTTP Audio Livestream served on the website. First you will need to add in the loopback drivers to your system:

```
sudo modprobe snd-aloop
```

You can then display the new list of audio devices with this command:

```
aplay -l 
```

Example Output:

```
**** List of PLAYBACK Hardware Devices ****
card 0: HDMI [HDA Intel HDMI], device 3: HDMI 0 [HDMI 0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: HDMI [HDA Intel HDMI], device 7: HDMI 1 [HDMI 1]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: HDMI [HDA Intel HDMI], device 8: HDMI 2 [HDMI 2]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: HDMI [HDA Intel HDMI], device 9: HDMI 3 [HDMI 3]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 0: HDMI [HDA Intel HDMI], device 10: HDMI 4 [HDMI 4]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: PCH [HDA Intel PCH], device 0: ALC668 Analog [ALC668 Analog]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 2: Loopback [Loopback], device 0: Loopback PCM [Loopback PCM]
  Subdevices: 8/8
  Subdevice #0: subdevice #0
  Subdevice #1: subdevice #1
  Subdevice #2: subdevice #2
  Subdevice #3: subdevice #3
  Subdevice #4: subdevice #4
  Subdevice #5: subdevice #5
  Subdevice #6: subdevice #6
  Subdevice #7: subdevice #7
card 2: Loopback [Loopback], device 1: Loopback PCM [Loopback PCM]
  Subdevices: 8/8
  Subdevice #0: subdevice #0
  Subdevice #1: subdevice #1
  Subdevice #2: subdevice #2
  Subdevice #3: subdevice #3
  Subdevice #4: subdevice #4
  Subdevice #5: subdevice #5
  Subdevice #6: subdevice #6
  Subdevice #7: subdevice #7
```

Then, the following command will listen to a pcm stream (generated by the server) on the loopback and output an .m3u8 file and .ts files that compose the audio livestream:

```
ffmpeg -f alsa -ac 1 -ar 44100 -i hw:2,0,0 \
       -c:a aac -b:a 128k -ac 1 \
       -f hls -hls_time 4 -hls_flags delete_segments \
       index.m3u8
```

Run this command in the directory /var/media/hls/ so that nginx will see it.

You may also need to change what audio device the server writes to and which one ffpmeg is listening on, depending on what label your mahine gives to the loopback devices. In my case, the loopback devices are under card 2. This is specified on line 51 in main.cpp. 