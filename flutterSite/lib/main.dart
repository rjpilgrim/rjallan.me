import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/gestures.dart';
import 'dart:html';
import 'package:http/http.dart' as http;
import 'dart:math';
import 'dart:ui' as dart_ui;
//import 'dart:typed_data';
import 'dart:core';
import 'dart:js' as js;
import 'package:web_socket_channel/html.dart';
import 'dart:async';
import 'dart:convert';
//import 'package:fft/fft.dart' as fft;
//import 'package:my_complex/my_complex.dart';
import 'package:loading/loading.dart';
import 'package:loading/indicator/line_scale_indicator.dart';
import 'package:tuple/tuple.dart';
import 'package:intl/intl.dart';
import 'package:url_launcher/url_launcher.dart';
//import 'hann1024.dart';

Map<String,dynamic> jsonDecodeIsolate(dynamic responseBody) {
  return jsonDecode(responseBody);
}

const Color uclaBlue = const Color.fromRGBO(40, 115, 173, 1);
const Color uclaGold = const Color(0xFFF2A900);

Future<dart_ui.Image> fetchImage(String url) async {
  var response = await http.get(Uri.parse(url));
  var codec = await dart_ui.instantiateImageCodec( response.bodyBytes); //Uint8List
  var frame = await codec.getNextFrame();
  return frame.image;
}


void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    SystemChrome.setPreferredOrientations([DeviceOrientation.portraitUp, DeviceOrientation.portraitDown]);
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData(primaryColor: uclaBlue,
        textTheme: TextTheme(
          bodyText1: TextStyle(),
          bodyText2: TextStyle(),
        ).apply(
          bodyColor: uclaBlue,
          fontFamily: "Muli",
          displayColor: Colors.blue,
        ),
      ),
      home: MyHomePage(title: 'RJ Allan'),
    );
  }

}
class WebEmojiLoaderHack extends StatelessWidget {
  const WebEmojiLoaderHack({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return const Offstage(
      // Insert invisible emoji in order to load the emoji font in CanvasKit
      // on startup.
      child: Text('✨'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  MyHomePage({Key? key, required this.title}) : super(key: key);

  final String title;

  @override
  _MyHomePageState createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  bool _playing = false;
  int _pageSelect = 0;
  bool _homeHover = false;
  bool _aboutHover = false;
  bool _whatHover = false;

  bool _connected = false;
  bool _queued = false;
  int  _queuePosition = 0;
  bool _duplicate = false;
  bool _served = false;

  late ScrollController _aboutController;
  late ScrollController _whatController;

  static var url = window.location.hostname ?? "localhost";
  HtmlWebSocketChannel? channel;

  StreamController<Tuple2<String, DateTime>> imageController = StreamController<Tuple2<String, DateTime>>.broadcast(); 

  void _playAudio() {
    setState(() {
      if (!_playing) {
        _playing = true;
        js.context.callMethod('playAudio', [_connected ? "https://stream-relay-geo.ntslive.net/stream2" /*"http://"+(url)+"/play/hls/index.m3u8"*/ : "https://stream-relay-geo.ntslive.net/stream2", false]);
      }
      else {
        _playing = false;
        js.context.callMethod('stopAudio', []);
      }
    });
  }

  void _setHomeHoverOn(PointerEvent details) {
    setState(()  {
      _homeHover = true;
    });
  }

  void _setHomeHoverOff(PointerEvent details) {
    setState(()  {
      _homeHover = false;
    });
  }
  void _setAboutHoverOn(PointerEvent details) {
    setState(()  {
      _aboutHover = true;
    });
  }

  void _setAboutHoverOff(PointerEvent details) {
    setState(()  {
      _aboutHover = false;
    });
  }
  void _setWhatHoverOn(PointerEvent details) {
    setState(()  {
      _whatHover = true;
    });
  }

  void _setWhatHoverOff(PointerEvent details) {
    setState(()  {
      _whatHover = false;
    });
  }


  void _selectHomePage() {
    setState(()  {
      _pageSelect = 0;
    });
  }

  void _selectAboutPage() {
    setState(()  {
      _pageSelect = 1;
    });
  }

  void _selectWhatPage() {
    setState(()  {
      _pageSelect = 2;
    });
  }

  @override
  void dispose() {
    //gradientController.close();
    imageController.close();
    super.dispose();
  }

  void mySocketListenFunction(dynamic data) {
      Future<Map<String, dynamic>> messageFuture = compute(
          jsonDecodeIsolate, data);
      messageFuture.then((Map<String, dynamic> message) {
        //print("FUTURE RETURN");
        if (message.containsKey("Served")) {
          setState(() {
            _connected = true;
            _queued = false;
            _duplicate = false;
            _served = message["Served"];
          });
        }
        if (message.containsKey("Queue Position")) {
          setState(() {
            _connected = true;
            _queued = true;
            _queuePosition = message["Queue Position"];
          });
        }
        if (message.containsKey("Duplicate")) {
          setState(() {
            _connected = true;
            _duplicate = message["Duplicate"];
          });
        }
        if (message.containsKey("Image Name")) {
          String imageName = message["Image Name"];
          DateTime imageTime = DateTime.parse(message["Image Time"]);
          /*Future<LinearGradient> gradientFuture = compute(gradientIsolate, rgbMagnitudes);
            gradientFuture.then((gradient) {
            /*var gradient = LinearGradient(
              begin: Alignment.centerLeft,
              end: Alignment.centerRight,
              colors: rgbMagnitudes.cast<int>().map((value) {return Color.fromARGB(255, value, value, value);}).toList()
            );*/
            try {
              gradientController.add(gradient);
            } on Exception catch(e) {
              print('error caught: $e');
            }
            });*/
          imageController.add(Tuple2<String, DateTime>(imageName, imageTime));
        }
      }
      );
  }

  void webSocketKeepAlive() {
    if (channel == null || channel?.closeCode != null) {
      channel = HtmlWebSocketChannel.connect(url == "localhost" ? "ws://" + (url) + ":8080/socket"
          : window.location.port == "80" ? "ws://" + (url) + "/socket"
          : "wss://" + (url) + "/socket");
      channel?.stream.listen(mySocketListenFunction);
    }
    Future.delayed(Duration(seconds: 3), () {
      webSocketKeepAlive();
    });
  }

  @override
  void initState() {
    //print("Heres url: $url");
    _aboutController = ScrollController();
    _whatController = ScrollController();
    try {
      channel = HtmlWebSocketChannel.connect(url == "localhost" ? "ws://" + (url) + ":8080/socket"
          : window.location.port == "80" ? "ws://" + (url) + "/socket"
          : "wss://" + (url) + "/socket");
      channel?.stream.listen(mySocketListenFunction);
    }
    catch (_) {

    }
    Future.delayed(Duration(seconds: 3), () {
      webSocketKeepAlive();
    });

    super.initState();
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    precacheImage(AssetImage('assets/me.jpg'), context);
  }
  double roundUpToNumber(double number, double roundTo) {
    if (number < roundTo) {
      return roundTo;
    }
    return number;
  }

  //nts2 - https://stream-relay-geo.ntslive.net/stream2
  @override
  Widget build(BuildContext context) {
    var  _mediaQuery = MediaQuery.of(context);
    double _width = _mediaQuery.size.width;  //(_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? _mediaQuery.size.height :  _mediaQuery.size.width;
    double _height = _mediaQuery.size.height; //(_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? _mediaQuery.size.width :  _mediaQuery.size.height;
    double _myFontSize = (_width < 450 || _height < 450) ? 14 :21;
    double _titleFont = (_width < 450 || _height < 450) ? 31 :41;
    final spectrogramWindow = RealTimeSpectrogram(height: _height, width: _width, connected: _connected, queued: _queued, queuePosition: _queuePosition, duplicate: _duplicate, served: _served, imageStream: imageController.stream);
    return Scaffold(
      backgroundColor: Colors.white,
      body: SingleChildScrollView (
        child: Column(
          mainAxisAlignment: MainAxisAlignment.start,
          children: <Widget>[
            //WebEmojiLoaderHack(),
            SizedBox(height: 10),
            SizedBox(  
              height:(_width < 450) ? 60 :70,
              child: Row(  
                children: [  
                  Text("RJ  Allan", style: TextStyle(fontFamily: "Pacifico",fontSize: _titleFont)),
                  SizedBox(width: 10),
                  Ink(
                    decoration: const ShapeDecoration(
                      color: uclaBlue,
                      shape: CircleBorder(),
                    ),
                    height: (_width < 450) ? 40 :50,
                    child: IconButton(
                      icon: _playing ? Icon(Icons.pause) : Icon(Icons.play_arrow),
                      color: Colors.white,
                      tooltip: _playing ? (/*_connected ? 'Stop digitally demodulated audio' :*/ 'Stop NTS Radio')
                          : (/*_connected ? 'Play digitally demodulated audio':*/ 'Play NTS Radio'),
                      onPressed: _playAudio
                    ),
                  ),
                ],
                crossAxisAlignment: CrossAxisAlignment.center,
                mainAxisAlignment: MainAxisAlignment.center,
              )
            ),
            SizedBox(height: 10),
            Container( 
              height:50,
              child: Row(children: [
                MouseRegion( 
                onEnter: _setHomeHoverOn,
                onExit: _setHomeHoverOff,
                cursor: SystemMouseCursors.click,
                child: GestureDetector(
                onTap: _selectHomePage,
                child: Container(
                  width: (_width < 450) ? _width/3.5 :150,
                  alignment: Alignment.center,
                  decoration: BoxDecoration( 
                    color: _pageSelect == 0 ? uclaBlue : _homeHover ? Colors.white : Colors.white,
                    border: Border( 
                      top: BorderSide(color: uclaBlue, width: 5.0),
                      bottom: BorderSide(color: uclaBlue, width: 0),
                      left: BorderSide(color: uclaBlue, width: 5.0),
                      right: BorderSide(color: uclaBlue, width: 2.5)
                    )
                  ),
                  child: Text("Home", style: TextStyle(fontSize: _myFontSize,
                      color: _pageSelect == 0 ? Colors.white : uclaBlue))
                ),
                ),
                ),
                MouseRegion( 
                onEnter: _setAboutHoverOn,
                onExit: _setAboutHoverOff,
                cursor: SystemMouseCursors.click,
                child: GestureDetector(
                onTap: _selectAboutPage,
                child: Container(
                  width: (_width < 450 ) ? _width/3.5 :150,
                  alignment: Alignment.center, 
                  decoration: BoxDecoration(  
                    color: _pageSelect == 1 ? uclaBlue : _homeHover ? Colors.white : Colors.white,
                    border: Border(
                      top: BorderSide(color: uclaBlue, width: 5.0),
                      bottom: BorderSide(color: uclaBlue, width: 0),
                      right: BorderSide(color: uclaBlue, width: 2.5),
                      left: BorderSide(color: uclaBlue, width: 2.5)
                    )
                  ),
                  child: Text("About me", style: TextStyle(fontSize: _myFontSize,
                      color: _pageSelect == 1 ? Colors.white : uclaBlue))
                ),
                )
                ),
                MouseRegion( 
                onEnter: _setWhatHoverOn,
                onExit: _setWhatHoverOff,
                cursor: SystemMouseCursors.click,
                child: GestureDetector(
                onTap: _selectWhatPage,
                child: Container(
                  width: (_width < 450) ? _width/3.5 :150,   
                  alignment: Alignment.center,   
                  decoration: BoxDecoration(  
                    color: _pageSelect == 2 ? uclaBlue : _homeHover ? Colors.white : Colors.white,
                    border: Border( 
                      top: BorderSide(color: uclaBlue, width: 5.0),
                      bottom: BorderSide(color: uclaBlue, width: 0),
                      right: BorderSide(color: uclaBlue, width: 5.0),
                      left: BorderSide(color: uclaBlue, width: 2.5)
                    )
                  ),
                  child: Text("What is this?", style: TextStyle(fontSize: _myFontSize,
                      color: _pageSelect == 2 ? Colors.white : uclaBlue))
                ),
                ) 
                ),
              ],mainAxisAlignment: MainAxisAlignment.center)
            ),
            Divider(
              color: uclaBlue,
              endIndent: _width <= 530 ? roundUpToNumber((_width - 450)/2, 0) :40,
              indent: _width <= 530 ? roundUpToNumber((_width - 450)/2, 0)  :40,
              height: 5,
              thickness: 5,
            ),
            SizedBox(height: 10),
            /*(_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? Center(child: Text("Please flip your phone around to view content.", style: TextStyle(fontSize:27, color: Colors.white)))
            :*/ _pageSelect == 0 ?
            //Column( children: [Text("Under Construction :)"), Spacer(), Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:uclaBlue)), Spacer()])
            spectrogramWindow
            : _pageSelect == 1 ?
            Padding(
              padding: EdgeInsets.fromLTRB(50, 0, 50, 0),
              child: Column (crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.start,
                children:  [
                  /*MouseRegion(
                  cursor: SystemMouseCursors.click,
                  child: */RichText(
                    text: TextSpan(
                      style: TextStyle(fontSize: _myFontSize,  color: Colors.black87, fontFamily: "Muli"),
                      children: <TextSpan>[
                        TextSpan(text:
                            "I am a software engineer who loves to work on novel and challenging projects that bridge the gap between high and low level technologies. " +
                            "I have recent experience with applications in IOT, wireless communications, graphics" +
                            ", music production, speech analysis, and natural language processing. "
                        ),
                        TextSpan(text: 'My resume is ',
                            recognizer: TapGestureRecognizer()
                              ..onTap = () {
                                launch("https://"+url+"/rjallan.pdf");
                              }),

                        TextSpan(
                            text: 'here',
                            style: TextStyle(color: Colors.deepPurple),
                            recognizer: TapGestureRecognizer()
                              ..onTap = () {
                                launch("https://"+url+"/rjallan.pdf");
                              }),
                        TextSpan(
                            text: '.',
                        ),
                      ],
                    ),
                  //),
                  ),
                  SizedBox(height: 10),
                  Text(
                    "I received my BS in Computer Science from UNC Chapel Hill, and my MS in Computer Science from UCLA. I focused my studies at UCLA in wireless communications "+
                    "and novel applications for radio protocols to increase connectivity among physical communities."
                    ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                  ),
                  SizedBox(height: 10),
                  Text(
                    "Since graduating, I have had the opportunity to build my expertise in programming while on the R&D team at Temposonics (formerly MTS Sensors). In addition to increasing my understanding of " +
                    "hardware with the talented electical engineers here, I have gained valuable experience in IOT software development including user interface and embedded firmware development."
                    ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                  ),
                  SizedBox(height: 10),
                  Text(
                    "Outside of work, I enjoy playing guitar, singing, and surfing."
                    ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                  ),
                  SizedBox(height: 20),
                  Center(
                    child: ClipRRect(
                        borderRadius: BorderRadius.circular(8.0),
                        child: Image(image: AssetImage('assets/me.jpg'), width: roundUpToNumber(_width* 0.3, 400) )
                    )
                  ),
                  SizedBox(height: 20),
                ]
              )
            )
            : Padding(
                padding: EdgeInsets.fromLTRB(50, 0, 50, 0),
                child: Column (crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.start,
                  children:  [
                    Text(
                        "The home page shows a real time spectrogram feed for the NTS Live 2 Radio Stream. The corresponding audio can be played with the play button above."
                        ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                    ),
                    SizedBox(height: 10),
                    Text(
                        "The spectrogram is created by performing a curl request to the radio stream with a timeout of ten seconds. " +
                        "After the timeout, the resulting .mp3 file is converted to .wav, and then a series of windowed FFT's are performed upon the PCM samples in the .wav file. " +
                        "Each FFT produces the values used to generate a line in the images shown. Deeper shades of blue indicate a greater magnitude at that frequency. " +
                        "The spectrogram uses a length 2048 FFT with a 50% overlap and a Hann window."
                        ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                    ),
                    /*Text(
                      "I am currently rebuilding the site, hence the loading screen on the home page. "
                      ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                    ),
                    SizedBox(height: 10),
                    Text(
                      "From August 2020 to May 2021, this site was served directly from a laptop in my house, showing a real time spectrogram feed of a local FM radio station. " +
                      "The displayed spectrogram screenshots and audio stream were generated by a process running on that computer, which had an SDR unit connected to it. " +
                      "The audio stream was generated by a digital low pass filter followed by a phase difference calculation performed on the samples coming in from the SDR unit. " +
                      "The spectrogram was created by an FFT performed after the initial low pass filter, followed by a write to a PNG pixel row, building out a complete spectrogram snapshot as samples came in. "
                      ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                    ),*/
                    SizedBox(height: 10),
                    Text(
                      "This site is hosted on an Ubuntu machine running in Digital Ocean. " +
                      "In addition to showing the spectrogram for NTS Radio on this site, I am working to show radio spectrum data from a local SDR."
                        ,style: TextStyle(fontSize: _myFontSize,  color: Colors.black87)
                    ),
                    SizedBox(height: 10),
                    MouseRegion(
                      cursor: SystemMouseCursors.click,
                      child: RichText(
                        text: TextSpan(
                          style: TextStyle(fontSize: _myFontSize,  color: Colors.black87, fontFamily: "Muli"),
                          children: <TextSpan>[
                            TextSpan(text: 'You can follow along with the code for this site over ',
                                recognizer: TapGestureRecognizer()
                                  ..onTap = () {
                                    launch("https://github.com/rjpilgrim/rjallan.me");
                                  }),

                            TextSpan(
                                text: 'here',
                                style: TextStyle(color: Colors.deepPurple),
                                recognizer: TapGestureRecognizer()
                                  ..onTap = () {
                                    launch("https://github.com/rjpilgrim/rjallan.me");
                                  }),
                            TextSpan(
                                text: '.',
                                recognizer: TapGestureRecognizer()
                                  ..onTap = () {
                                    launch("https://github.com/rjpilgrim/rjallan.me");
                                  }),
                          ],
                        ),
                      ),
                    ),
                    SizedBox(height: 20),
                  ]
                )
            )
          ],
        ),
      ),
      /*floatingActionButton: FloatingActionButton(
        onPressed: _playAudio,
        tooltip: 'Increment',
        backgroundColor: Colors.white,
        foregroundColor: Colors.black,
        child: Icon(Icons.play_arrow),
        elevation: 3,
      ),*/ // This trailing comma makes auto-formatting nicer for build methods.
      );
    
  }
}

class RealTimeSpectrogram extends StatelessWidget {
  RealTimeSpectrogram({Key? key, required this.width, required this.height, required this.connected, required this.queued, required this.queuePosition,
    required this.duplicate, required this.served, required this.imageStream}) : super(key: key);
  final double width;
  final double height;
  final bool connected;
  final bool queued;
  final int  queuePosition;
  final bool duplicate;
  final bool served;
  final Stream<Tuple2<String, DateTime>> imageStream;

  @override
  Widget build(BuildContext context) {
    double _width = width;
    double _height = height > 600 ? height : 600;
    return SizedBox(  
          width: _width-18,
          height: _height*0.75,
          child: !connected ? Column( children: [Spacer(), Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:uclaBlue)), Spacer()])
                : served ? 
                  SpectrogramWindow(imageStream: imageStream, width: _width, height: _height)
                : Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
        );
    
  }

}

class SpectrogramWindow extends StatefulWidget {
  SpectrogramWindow({Key? key, /*this.gradientStream,*/ required this.imageStream, required this.width, required this.height}) : super(key: key);
  //final Stream<LinearGradient> gradientStream;
  final Stream<Tuple2<String, DateTime>> imageStream;
  final double width;
  final double height;

  @override
  _SpectrogramWindowState createState() => _SpectrogramWindowState();
}

class _SpectrogramWindowState extends State<SpectrogramWindow> {

  dart_ui.Image? myImage;
  
  bool _fetchingImage = false;
  DateTime? myTime;
  DateTime? laterTime;
  StreamSubscription? mySubscription;

  static var url = window.location.hostname ?? "localhost";

  static NumberFormat numberFormat = new NumberFormat("00");

  /*
  void putContainer(Container newContainer) {
    setState(() {
      myChildren.insert(0, Expanded(child:newContainer));
      myChildren.removeLast();
    });
  }
  void putGradient(LinearGradient myGradient) {
    
    gradientList.insert(0, myGradient);
    gradientList.removeLast();
    _repaintCounter++;
    
      if (_repaintCounter > 200) {
        setState(() {
        _repaintCounter = 0;
        _repaint = true;
        });
      }
      else if (_repaintCounter == 0) {
        setState(() {
          _repaintCounter = 0;
          _repaint = false;
        });
      }
      else {
        _repaint = false;
      }    

  }*/

  @override
  void dispose() {
    //gradientController.close();
    mySubscription?.cancel();
    super.dispose();
  }

  @override
  void initState() {
    mySubscription = widget.imageStream.listen((data) {
      if (!mounted) {
        return;
      }
      /*putContainer(
        Container( 
          decoration: BoxDecoration(
            gradient: data
          )
        )
      );*/
      //putGradient(data);
      //print("http://"+url+"/"+data.item1);
      if (!_fetchingImage ) {
      _fetchingImage = true;
      var imageFuture = fetchImage(url == "localhost" ? "http://"+url+":8080/"+data.item1
        : window.location.port == "80" ? "http://"+url+"/"+data.item1
        : "https://"+url+"/"+data.item1
        );
      imageFuture.then( (image) {
        _fetchingImage  = false;
        setState(() {
          myImage = image;
          myTime = data.item2.subtract(Duration(hours: 5));
          //print(getFormatTime(myTime!));
          laterTime = data.item2.subtract(Duration(hours:4, minutes: 59, seconds: 50));
          //print(getFormatTime(laterTime!));
        });
      });
      }
    }
    );
    super.initState();
  }

  String getFormatTime(DateTime time) {
    return "${numberFormat.format(time.hour)}:${numberFormat.format(time.minute)}:${numberFormat.format(time.second)}";
  }

  @override
  Widget build(BuildContext context) {
    double _width = widget.width;
    double _height = widget.height;
    return Padding(
      padding: EdgeInsets.fromLTRB(20, 0, 20, 0),
      child:Column( 
        children: [
          Container( 
            height: 30,
            child:CustomPaint( 
              painter: TickPainter((_width < 450) ? true:false),
              child: Center(child: Text(""))
            ), 
          ),
          Expanded( 
          child: Container(
          decoration: const BoxDecoration(
            border: Border(
              top: BorderSide(width: 4.0, color: Colors.black),
              left: BorderSide(width: 4.0, color: Colors.black),
              right: BorderSide(width: 4.0, color: Colors.black),
              bottom: BorderSide(width: 4.0, color: Colors.black),
            ),
          ),
          constraints: BoxConstraints.expand(),
          child:
            /*CustomPaint( 
              painter: SamplesPainter(gradientList, _repaint),
              child: Container( 
                child: CustomPaint( 
                  painter: TimePainter((_width < 450) ? true:false),
                  child: Center(child: Text(""))
                )
              )
            )*/
          (myImage != null && myTime != null && laterTime != null) ? Stack(children: [Container(constraints: BoxConstraints.expand(),child:FittedBox(
              fit: BoxFit.fill,
              child: SizedBox(  
                width: myImage!.width.toDouble(),
                height: myImage!.height.toDouble(),
                child: CustomPaint(  
                  painter: ImagePainter(myImage!),
                  child: Center(child: Text(""))/* Container( 
                    child: CustomPaint( 
                      painter: TimePainter((_width < 450) ? true:false),
                      child: Center(child: Text(""))
                    )*/
                )
                )
              ),
            ),
            Container(constraints: BoxConstraints.expand(),child:
              CustomPaint( 
                  painter: TimePainter((_width < 450) ? true:false, "${getFormatTime(laterTime!)} EST", "${getFormatTime(myTime!)} EST"),
                  child: Center(child: Text(""))
                    
                )
            )
            ]): Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:uclaBlue))
          )
          ),
          Container( 
            height: 70,
            padding: EdgeInsets.fromLTRB(0, 10, 0, 10),
            child :Row(children: [
            Text("0 dBFS", style: TextStyle(fontSize:(_width < 450) ? 12 :18, color: Colors.black)),
            SizedBox(width:5),
            Expanded(child: Container(
              height: 25,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.all(Radius.circular(5)),
                gradient: LinearGradient(
                  begin: Alignment.centerLeft,
                  end: Alignment.centerRight, 
                  colors: [const Color(0xFF0000FF), const Color(0xFF00FFFF), const Color(0xFFFFFFFF)],
                  ),
                ),
              )
            ),
            SizedBox(width:5),
            Text("-96 dBFS", style: TextStyle(fontSize:(_width < 450) ? 12 :18, color: Colors.black)),
            ],
            ),
          ),
        ]
      ),
    );
  }
}

class SamplesPainter extends CustomPainter {
  final List<LinearGradient> gradientList;
  final bool repaint;

  SamplesPainter(this.gradientList, this.repaint);

  @override
  void paint(Canvas canvas, Size size) {

    
    //538 rows of 8192 bins
    for(int i = 0; i < 538; i++) {
      double myY = (i/538) * size.height;
      var rect = Rect.fromPoints(Offset(0, myY), Offset(size.width, myY));
      var gradient = gradientList[i];
      canvas.drawRect(rect,Paint()..shader = gradient.createShader(rect)..blendMode = BlendMode.screen..strokeWidth=(size.height/538)..style=PaintingStyle.stroke);
    }
    /*print("Painting cnavs");
     Future<List<Tuple2<Rect,Paint>>> canvasFuture = compute(canvasIsolate, Tuple2<List<LinearGradient>,Size>(gradientList, size));
    canvasFuture.then((canvasList) {
      print("painting future canvas");
      for(int i = 0; i < 538; i++) {
        canvas.drawRect(canvasList[i].item1, canvasList[i].item2);
      }
    });*/
    /*Future<Canvas> canvasFuture =compute(canvasIsolate, Tuple2<List<LinearGradient>,Size>(gradientList, size));
    canvasFuture.then((myCanvas) {
      canvas = myCanvas;
    });*/
  }

  @override
  bool shouldRepaint(SamplesPainter oldDelegate) => repaint;

}

class ImagePainter extends CustomPainter {
  final dart_ui.Image myImage;

  ImagePainter(this.myImage);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawImage(myImage, Offset(0,0), Paint());
  }

  @override
  bool shouldRepaint(ImagePainter oldDelegate) => this.myImage != oldDelegate.myImage;
}

class TimePainter extends CustomPainter {

  final bool smallText;
  final String present;
  final String tenSeconds;

  TimePainter(this.smallText, this.present, this.tenSeconds);

  @override
  void paint(Canvas canvas, Size size) {
    //538 rows of 8192 bins
    var textStyle = TextStyle(color: Colors.black, fontSize: smallText ? 12 : 18);

    var firstTextSpan = TextSpan(  
      text: present,
      style: textStyle
    );
    var firstTextPainter = TextPainter(  
      text: firstTextSpan,
      textDirection: dart_ui.TextDirection.ltr
    );
    firstTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    firstTextPainter.paint(canvas, Offset(size.width - 3 - firstTextPainter.width,3));

    var lastTextSpan = TextSpan(  
      text: tenSeconds,
      style: textStyle
    );
    var lastTextPainter = TextPainter(  
      text: lastTextSpan,
      textDirection: dart_ui.TextDirection.ltr
    );
    lastTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    lastTextPainter.paint(canvas, Offset( size.width - 3 - lastTextPainter.width, size.height - 3- lastTextPainter.height));
  }

  @override
  bool shouldRepaint(TimePainter oldDelegate) =>  (this.smallText != oldDelegate.smallText) || (this.present != oldDelegate.present);

}

class TickPainter extends CustomPainter {
  final bool smallText;

  TickPainter(this.smallText);

  @override
  void paint(Canvas canvas, Size size) {
    var textStyle = TextStyle(color: Colors.black, fontSize: smallText ? 12 : 18);

    var firstTickRect = Rect.fromPoints(Offset(0, size.height-8), Offset(4, size.height));
    canvas.drawRect(firstTickRect,Paint()..color = Colors.black);
    var secondTickRect = Rect.fromPoints(Offset(size.width*0.25 -2, size.height-8), Offset(size.width*0.25 +2, size.height));
    canvas.drawRect(secondTickRect,Paint()..color = Colors.black);
    var middleTickRect = Rect.fromPoints(Offset(size.width*0.5 -2, size.height-8), Offset(size.width*0.5 +2, size.height));
    canvas.drawRect(middleTickRect,Paint()..color = Colors.black);
    var fourthTickRect = Rect.fromPoints(Offset(size.width*0.75 -2, size.height-8), Offset(size.width*0.75 +2, size.height));
    canvas.drawRect(fourthTickRect,Paint()..color = Colors.black);
    var lastTickRect = Rect.fromPoints(Offset(size.width-4, size.height-8), Offset(size.width, size.height));
    canvas.drawRect(lastTickRect,Paint()..color = Colors.black);

    var firstTextSpan = TextSpan(  
      text: "0 kHz",
      style: textStyle
    );
    var firstTextPainter = TextPainter(  
      text: firstTextSpan,
      textDirection: dart_ui.TextDirection.ltr
    );
    firstTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    firstTextPainter.paint(canvas, Offset(0, size.height - 8 - firstTextPainter.height));

    var middleTextSpan = TextSpan(  
      text: "11.025 kHz",
      style: textStyle
    );
    var middleTextPainter = TextPainter(  
      text: middleTextSpan,
      textDirection: dart_ui.TextDirection.ltr
    );
    middleTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    middleTextPainter.paint(canvas, Offset(size.width * 0.5 - middleTextPainter.size.width*0.5, size.height - 8 - middleTextPainter.height ));

    var lastTextSpan = TextSpan(  
      text: "22.05 kHz",
      style: textStyle
    );
    var lastTextPainter = TextPainter(  
      text: lastTextSpan,
      textDirection: dart_ui.TextDirection.ltr
    );
    lastTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    lastTextPainter.paint(canvas, Offset(size.width - lastTextPainter.size.width, size.height - 8 - lastTextPainter.height ));

  }

  @override
  bool shouldRepaint(TickPainter oldDelegate) => this.smallText != oldDelegate.smallText;

}
