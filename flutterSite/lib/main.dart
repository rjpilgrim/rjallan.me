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


Future<dart_ui.Image> fetchImage(String url) async {
  var response = await http.get(url);
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
      home: MyHomePage(title: 'RJ Allan'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  MyHomePage({Key key, this.title}) : super(key: key);

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

  ScrollController _aboutController;
  ScrollController _whatController;

  static var url = window.location.hostname;
  var channel = HtmlWebSocketChannel.connect("ws://"+url+"/socket");

  StreamController<Tuple2<String, DateTime>> imageController = StreamController<Tuple2<String, DateTime>>.broadcast(); 

  void _playAudio() {
    setState(() {
      if (!_playing) {
        _playing = true;
        js.context.callMethod('playAudio', ["http://"+url+"/play/hls/index.m3u8"]);
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

  @override
  void initState() {
    _aboutController = ScrollController();
    _whatController = ScrollController();
    channel.stream.listen((data) {
      Future<Map<String, dynamic>> messageFuture = compute(jsonDecodeIsolate, data);
      messageFuture.then(( Map<String, dynamic> message) {
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
            imageController.add(Tuple2<String,DateTime>(imageName, imageTime));
          }
      }
      );
    });
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    var  _mediaQuery = MediaQuery.of(context);
    double _width = _mediaQuery.size.width;  //(_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? _mediaQuery.size.height :  _mediaQuery.size.width;
    double _height = _mediaQuery.size.height; //(_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? _mediaQuery.size.width :  _mediaQuery.size.height;
    return Scaffold(
      backgroundColor: Colors.black,
      body: Container (
        decoration: const BoxDecoration(
          border: Border(
            top: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
            left: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
            right: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
            bottom: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
          ),
        ),
        width: _width,
        height: _height,
        child: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.start,
          children: <Widget>[
            SizedBox(height: 10),
            SizedBox(  
              height:(_width < 450) ? 60 :70,
              child: Row(  
                children: [  
                  Text("RJ Allan", style: TextStyle(fontSize: (_width < 450) ? 31 :41, color: Colors.white)),
                  SizedBox(width: 10),
                  Ink(
                    decoration: const ShapeDecoration(
                      color: Colors.white,
                      shape: CircleBorder(),
                    ),
                    height: (_width < 450) ? 40 :50,
                    child: IconButton(
                      icon: _playing ? Icon(Icons.pause) : Icon(Icons.play_arrow),
                      color: Colors.black,
                      tooltip: _playing ? 'Stop digitally demodulated audio' : 'Play digitally demodulated audio',
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
                    color: _pageSelect == 0 ? Colors.white60: _homeHover ? Colors.white24 : Colors.black, 
                    border: Border( 
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      left: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("Home", style: TextStyle(fontSize: (_width < 450) ? 14 :21, color: _pageSelect == 0 ? Colors.black87 : _homeHover ? Colors.white: Colors.white))
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
                  width: (_width < 450) ? _width/3.5 :150,  
                  alignment: Alignment.center, 
                  decoration: BoxDecoration(  
                    color: _pageSelect == 1 ? Colors.white60 : _aboutHover ? Colors.white24 : Colors.black,
                    border: Border(
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 2.5),
                      left: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("About me", style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: _pageSelect == 1 ? Colors.black87 : _aboutHover ? Colors.white: Colors.white))
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
                    color: _pageSelect == 2 ? Colors.white60 : _whatHover ? Colors.white24 : Colors.black,
                    border: Border( 
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 5.0),
                      left: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("What is this?", style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: _pageSelect == 02 ? Colors.black87 : _whatHover ? Colors.white: Colors.white))
                ),
                ) 
                ),
              ],mainAxisAlignment: MainAxisAlignment.center)
            ),
            /*Divider(  
              color: Colors.white,
              endIndent: 60,
              indent: 60,
              height: 50,
              thickness: 3,
            ),*/
            SizedBox(height: 10),
            (_mediaQuery.size.height < 450 &&  _mediaQuery.size.width > _mediaQuery.size.height) ? Center(child: Text("Please flip your phone around to view content.", style: TextStyle(fontSize:27, color: Colors.white)))
            : _pageSelect == 0 ? RealTimeSpectrogram(height: _height, width: _width, connected: _connected, queued: _queued, queuePosition: _queuePosition, duplicate: _duplicate, served: _served, imageStream: imageController.stream)
            : _pageSelect == 1 ? Expanded(child: Padding( 
                padding: EdgeInsets.fromLTRB(22, 20, 22, 50), 
                child: Container(padding: EdgeInsets.fromLTRB(20, 10, 5, 10), decoration: BoxDecoration(color: Colors.white60, border: Border(
                    top: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    left: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    right: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    bottom: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                  ),) 
                
                , constraints: BoxConstraints( maxWidth: 1200)
                , child: LayoutBuilder(
                    builder: (BuildContext context, BoxConstraints viewportConstraints) {
                      return Scrollbar(isAlwaysShown: true, controller: _aboutController, child: SingleChildScrollView( controller: _aboutController,
                        child: ConstrainedBox(
                          constraints: BoxConstraints(
                            minHeight: viewportConstraints.maxHeight,
                          ),
                          child: IntrinsicHeight( child: Padding( padding: EdgeInsets.fromLTRB(0, 0, 10, 0), 
                
                            child: Column (crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.start,
                              children:  [
                                Text( 
                                  "I am a software engineer who loves to work on novel and challenging projects that bridge the gap between high and low level technologies. " +
                                  "I have recent experience with applications in IOT, wireless communications, graphics" +
                                  ", music production, speech analysis, and natural language processing."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                MouseRegion( 
                                cursor: SystemMouseCursors.click,
                                child: RichText(
                                  text: TextSpan(
                                    style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87),
                                    children: <TextSpan>[
                                      TextSpan(text: 'My resume is ',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("http://"+url+"/rjallan.pdf");
                                            }),
                                      
                                      TextSpan(
                                          text: 'here',
                                          style: TextStyle(color: Colors.deepPurple),
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("http://"+url+"/rjallan.pdf");
                                            }),
                                      TextSpan(
                                          text: '.',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("http://"+url+"/rjallan.pdf");
                                            }),
                                    ],
                                  ),
                                ),
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "I received my BS in Computer Science from UNC Chapel Hill, and my MS in Computer Science from UCLA. I focused my studies at UCLA in wireless communications "+ 
                                  "envisioning new protocols designed to bring together physical communities."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "Since graduating, I have had the opportunity to build my expertise in programming while on the R&D team at MTS Sensors. In addition to increasing my understanding of " +
                                  "electronics with the talented electical engineers here. I have gained valuable experience in IOT software development including UI development and embedded firmware development."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "Outside of work, I enjoy playing guitar, singing, and surfing. I enjoy following baseball and college basketball, following the Dodgers and UCLA Bruins primarily."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                              ]
                
                            )
                          )
                          )
                        )
                      ));
                    }
                  )
                )
              )
            )
            : Expanded(child: Padding( 
                padding: EdgeInsets.fromLTRB(22, 20, 22, 50), 
                child: Container(padding: EdgeInsets.fromLTRB(20, 10, 5, 10), decoration: BoxDecoration(color: Colors.white60, border: Border(
                    top: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    left: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    right: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                    bottom: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
                  ),) 
                
                , constraints: BoxConstraints( maxWidth: 1200)
                , child: LayoutBuilder(
                    builder: (BuildContext context, BoxConstraints viewportConstraints) {
                      return Scrollbar(isAlwaysShown: true, controller: _aboutController, child: SingleChildScrollView( controller: _aboutController,
                        child: ConstrainedBox(
                          constraints: BoxConstraints(
                            minHeight: viewportConstraints.maxHeight,
                          ),
                          child: IntrinsicHeight( child:  Padding( padding: EdgeInsets.fromLTRB(0, 0, 10, 0), 
                            child: Column (crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.start,
                              children:  [
                                Text( 
                                  "The home page is a spectrogram view of my favorite local FM radio station. This view is created by tuning to the station with a LimeSDR and applying some digital signal processing. " +
                                  "Press the play button above to listen to the radio station as it is demodulated on the server."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                MouseRegion( 
                                cursor: SystemMouseCursors.click,
                                child: RichText(
                                  text: TextSpan(
                                    style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87),
                                    children: <TextSpan>[
                                      TextSpan(text: 'I encourage you to visit the WCPE website over at ',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("https://theclassicalstation.org/");
                                            }),
                                      
                                      TextSpan(
                                          text: 'theclassicalstation.org',
                                          style: TextStyle(color: Colors.deepPurple),
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("https://theclassicalstation.org/");
                                            }),
                                      TextSpan(
                                          text: ' to listen to the pure audio.',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("https://theclassicalstation.org/");
                                            }),
                                    ],
                                  ),
                                ),
                                ),
                                SizedBox(height: 10),
                                MouseRegion( 
                                cursor: SystemMouseCursors.click,
                                child: RichText(
                                  text: TextSpan(
                                    style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87),
                                    children: <TextSpan>[
                                      TextSpan(text: 'The code for this site, meanwhile, is hosted ',
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
                                SizedBox(height: 10),
                                Text( 
                                  "This site uses the flutter web engine for its display. On the back end, there is an nginx server running on a computer in my house. The displayed spectrogram screenshots and audio stream" +
                                  "are generated by a process running on that computer, which has the SDR unit connected to it. The audio stream is generated by a digital low pass filter followed by a phase difference calculation" + 
                                  "performed on the samples coming in from the SDR unit. The spectrogram is created by an FFT performed after the initial low pass filter, followed by a write to a PNG pixel row, building out" +
                                  "a complete spectrogram snapshot as samples come in."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "This site i"
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                              ]
                
                            )
                          )
                          )
                        )
                      ));
                    }
                  )
                )
              )
            )
          ],
        ),
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
  RealTimeSpectrogram({Key key, this.width, this.height, this.connected, this.queued, this.queuePosition, this.duplicate, this.served, this.imageStream}) : super(key: key);
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
    double _height = height;
    return SizedBox(  
          width: _width-18,
          height: _height*0.75,
          child: !connected ? Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
                : duplicate ? Center(child: Text("Your device is already connected to this site somewhere. Sorry, I do not have the bandwidth to host you twice.", style: TextStyle(fontSize:27, color: Colors.white)))
                : queued ? Center(child: Text("You are in the queue to view the spectrogram. Your position in the queue is: $queuePosition.", style: TextStyle(fontSize:27, color: Colors.white)))
                : served ? 
                  SpectrogramWindow(imageStream: imageStream, width: _width, height: _height)
                : Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
        );
    
  }

}

class SpectrogramWindow extends StatefulWidget {
  SpectrogramWindow({Key key, /*this.gradientStream,*/ this.imageStream, this.width, this.height}) : super(key: key);
  //final Stream<LinearGradient> gradientStream;
  final Stream<Tuple2<String, DateTime>> imageStream;
  final double width;
  final double height;

  @override
  _SpectrogramWindowState createState() => _SpectrogramWindowState();
}

class _SpectrogramWindowState extends State<SpectrogramWindow> {

  dart_ui.Image myImage;
  
  bool _ready = false;
  bool _fetchingImage = false;
  DateTime myTime;
  DateTime laterTime;

  static var url = window.location.hostname;

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
  void initState() {
    widget.imageStream.listen((data) {
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
      var imageFuture = fetchImage("http://"+url+"/"+data.item1);
      imageFuture.then( (image) {
        _fetchingImage  = false;
        setState(() {
          myImage = image;
          myTime = data.item2;
          laterTime = data.item2.add(Duration(seconds:10));
          _ready = true;
        });
      });
      }
    }
    );
    super.initState();
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
              top: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
              left: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
              right: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
              bottom: BorderSide(width: 4.0, color: Color(0xFFFFFFFFFF)),
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
            _ready ? Stack(children: [Container(constraints: BoxConstraints.expand(),child:FittedBox(  
              fit: BoxFit.fill,
              child: SizedBox(  
                width: myImage.width.toDouble(),
                height: myImage.height.toDouble(),
                child: CustomPaint(  
                  painter: ImagePainter(myImage),
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
                  painter: TimePainter((_width < 450) ? true:false, "${numberFormat.format(myTime.hour)}:${numberFormat.format(myTime.minute)}:${numberFormat.format(myTime.second)} EST", "${numberFormat.format(laterTime.hour)}:${numberFormat.format(laterTime.minute)}:${numberFormat.format(laterTime.second)} EST"),
                  child: Center(child: Text(""))
                    
                )
            )
            ]): Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
          )
          ),
          Container( 
            height: 70,
            padding: EdgeInsets.fromLTRB(0, 10, 0, 10),
            child :Row(children: [
            Text("0 dBFS", style: TextStyle(fontSize:(_width < 450) ? 12 :18, color: Colors.white)),
            SizedBox(width:5),
            Expanded(child: Container(
              height: 25,
              decoration: BoxDecoration(
                borderRadius: BorderRadius.all(Radius.circular(5)),
                gradient: LinearGradient(
                  begin: Alignment.centerLeft,
                  end: Alignment.centerRight, 
                  colors: [const Color(0xFFFFFFFF), const Color(0xFF000000)], 
                  ),
                ),
              )
            ),
            SizedBox(width:5),
            Text("-80 dBFS", style: TextStyle(fontSize:(_width < 450) ? 12 :18, color: Colors.white)),
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
    var textStyle = TextStyle(color: Colors.white, fontSize: smallText ? 12 : 18);

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
    var textStyle = TextStyle(color: Colors.white, fontSize: smallText ? 12 : 18);

    var firstTickRect = Rect.fromPoints(Offset(0, size.height-8), Offset(4, size.height));
    canvas.drawRect(firstTickRect,Paint()..color = Colors.white);
    var secondTickRect = Rect.fromPoints(Offset(size.width*0.25 -2, size.height-8), Offset(size.width*0.25 +2, size.height));
    canvas.drawRect(secondTickRect,Paint()..color = Colors.white);
    var middleTickRect = Rect.fromPoints(Offset(size.width*0.5 -2, size.height-8), Offset(size.width*0.5 +2, size.height));
    canvas.drawRect(middleTickRect,Paint()..color = Colors.white);
    var fourthTickRect = Rect.fromPoints(Offset(size.width*0.75 -2, size.height-8), Offset(size.width*0.75 +2, size.height));
    canvas.drawRect(fourthTickRect,Paint()..color = Colors.white);
    var lastTickRect = Rect.fromPoints(Offset(size.width-4, size.height-8), Offset(size.width, size.height));
    canvas.drawRect(lastTickRect,Paint()..color = Colors.white);

    var firstTextSpan = TextSpan(  
      text: "89.7 MHz",
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
      text: "89.755 MHz",
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
      text: "89.81 MHz",
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
