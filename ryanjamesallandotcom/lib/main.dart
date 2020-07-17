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

/*const hann_sum = 552.5000000000002;
const min_magnitude = 0.2048;
var myHannWindow = fft.Window(fft.WindowType.HANN);

List<LinearGradient> iqToMagnitude(Map<String,List<num>> parameterMap) {
  List<LinearGradient> returnList = [];
  List<num> rawISamples = parameterMap["I"];
  List<num> rawQSamples = parameterMap["Q"];
  print("BEFORE FIRST LOOP");
  for (int i = 0; i < 429; i++) {
    List<int> rgbList = [];
    List<num> iWindow = rawISamples.getRange(i*512, i*512+1024);
    List<num> qWindow = rawQSamples.getRange(i*512, i*512+1024);
    List<num> iFilteredWindow = List(1024);
    List<num> qFilteredWindow = List(1024);
    print("BEFORE WINDOW");
    for (int j = 0; j<1024; j++) {
      iFilteredWindow[j] = iWindow[j] * hann1024[j];
      qFilteredWindow[j] = qWindow[j] * hann1024[j];
    }
    print("BEFORE FFT");
    var iFFT = new fft.FFT().Transform(iFilteredWindow);
    var qFFT = new fft.FFT().Transform(qFilteredWindow);
    print("BEFORE SECOND LOOP");
    for (int j = 512; j<1024; j++) {
      var complexFFT = iFFT[j] + (Complex.cartesian(0.0, 1.0) * qFFT[j]);
      var magnitude = complexFFT.modulus;
      if (magnitude <= 0) {
        magnitude = min_magnitude;
      }
      var normalizedMagnitude = magnitude/2048/hann_sum/2;
      var dbFSMagnitude = 20 * log(normalizedMagnitude) / log(10);
      int rgbMagnitude =((-80-dbFSMagnitude)*255/-80).round().abs();
      if (rgbMagnitude > 255) {
        rgbMagnitude = 255;
      }
      rgbList.add(rgbMagnitude);
    }
    print("BEFORE THIRD LOOP");
    for (int j = 0; j<512; j++) {
      var complexFFT = iFFT[j] + (Complex.cartesian(0.0, 1.0) * qFFT[j]);
      var magnitude = complexFFT.modulus;
      if (magnitude <= 0) {
        magnitude = min_magnitude;
      }
      var normalizedMagnitude = magnitude/2048/hann_sum/2;
      var dbFSMagnitude = 20 * log(normalizedMagnitude) / log(10);
      int rgbMagnitude =((-80-dbFSMagnitude)*255/-80).round().abs();
      if (rgbMagnitude > 255) {
        rgbMagnitude = 255;
      }
      rgbList.add(rgbMagnitude);
    }
    List<Color> colorList = [];
    print("BEFORE FOURTH LOOP");
    for (int j = 0; j<1024; j++) {
      int value = rgbList[j];
      colorList.add(Color.fromARGB(255, value, value, value));
    }
    var gradient = LinearGradient(  
      begin: Alignment.centerLeft,
      end: Alignment.centerRight, 
      colors: colorList
    );
    returnList.add(gradient);
  }
  return returnList;
}
*/
Map<String,dynamic> jsonDecodeIsolate(dynamic responseBody) {
  //print("TRY DECODE on $responseBody");
  return jsonDecode(responseBody);
}

/*LinearGradient gradientIsolate(List<dynamic> rgbMagnitudes) {
  return LinearGradient(  
              begin: Alignment.centerLeft,
              end: Alignment.centerRight, 
              colors: rgbMagnitudes.cast<int>().map((value) {return Color.fromARGB(255, value, value, value);}).toList()
            );
}

List<Tuple2<Rect, Paint>> canvasIsolate(Tuple2<List<LinearGradient>, Size> parameters) {
  List<Tuple2<Rect, Paint>> returnList = [];
  for(int i = 0; i < 538; i++) {
      double myY = (i/538) * parameters.item2.height;
      var rect = Rect.fromPoints(Offset(0, myY), Offset(parameters.item2.width, myY));
      var gradient = parameters.item1[i];
      var paint = Paint()..shader = gradient.createShader(rect)..blendMode = BlendMode.screen..strokeWidth=(parameters.item2.height/538)..style=PaintingStyle.stroke;
      returnList.add(Tuple2<Rect,Paint>(rect, paint));
  }
  return returnList;
}*/

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
      home: MyHomePage(title: 'Ryan Allan'),
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

  ScrollController _aboutController;
  ScrollController _whatController;

  static var url = window.location.hostname;
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
  void initState() {
    _aboutController = ScrollController();
    _whatController = ScrollController();

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
                  Text("Ryan Allan", style: TextStyle(fontSize: (_width < 450) ? 31 :41, color: Colors.white)),
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
            : _pageSelect == 0 ? RealTimeSpectrogram(height: _height, width: _width)
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
                                  "I have focused my studies on wireless communications since graduate school, and I also have recent experience with applications in graphics" +
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
                                              launch("http://"+url+"/ryanjamesallan.pdf");
                                            }),
                                      
                                      TextSpan(
                                          text: 'here',
                                          style: TextStyle(color: Colors.deepPurple),
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("http://"+url+"/ryanjamesallan.pdf");
                                            }),
                                      TextSpan(
                                          text: '.',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("http://"+url+"/ryanjamesallan.pdf");
                                            }),
                                    ],
                                  ),
                                ),
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "I first gained an interest in digital signal processing in a Linear Algebra Class during my undergraduate. For a programming project, I created a phase vocoder " +
                                  "implemented with FFT's and applied it to a few of my favorite songs. The vocoder was rather simple, but it appealed directly to my creative sensibilities and set me " +
                                  "on the path to UCLA to learn more. At UCLA, I challenged myself with electical engineering classes like Filter Design, Speech Processing, and Digital Arithmetic to get a more rigorous understanding of signal processing. " +
                                  "During my time there, I acquired a new interest for wireless communications, studying WiFi, Bluetooth, and LTE as well as envisioning new protocols designed to bring together physical communities, rather than drive them apart."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "Since graduating, I have had the opportunity to build my expertise in programming while on the R&D team at MTS Sensors, in addition to increasing my understanding of " +
                                  "electronics with the talented electical engineers here. I must credit one of my colleagues for introducing me to the LimeSDR platform which this site's server uses. " +
                                  "My understanding of wireless was useful when solving a crucial bug in the firmware for the wireless chip in our product. " +
                                  "Software Engineering is a craft. Like most crafts, it is honed with practice. Many concepts (asynchronous programming, generic programming, functional programming, among others) which " +
                                  "I had learned in school have now been cemented in practice."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "Outside of computing work, I enjoy playing guitar, singing, and surfing. I do in fact enjoy classical music. My favorite composer is Carl Maria von Weber, " +
                                  "and he is often played by the station to which this site's server is tuned. I spend more time listening to rock music though. Naturally, my favorite band " +
                                  "is The Beach Boys."
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
                                  "Press the play button above to listen to the radio station as it is demodulated on the server. Due to limitations in the receiver, the audio " +
                                  "is not the greatest quality. I am considering ways to filter out the noise in the output PCM, but this may make me miss my timing constraints for " +
                                  "the spectrogram display."
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
                                              launch("https://gitlab.com/RyanAllan/ryanallandotcom");
                                            }),
                                      
                                      TextSpan(
                                          text: 'here',
                                          style: TextStyle(color: Colors.deepPurple),
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("https://gitlab.com/RyanAllan/ryanallandotcom");
                                            }),
                                      TextSpan(
                                          text: '.',
                                          recognizer: TapGestureRecognizer()
                                            ..onTap = () {
                                              launch("https://gitlab.com/RyanAllan/ryanallandotcom");
                                            }),
                                    ],
                                  ),
                                ),
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "This site has been through a number of iterations. The two primary limitations in its creation have been the performance of the Flutter Javascript engine and the " +
                                  "upload speed on my home internet connection. Originally, I had the crazy idea of doing both the graphics and the signal processing for the spectrogram on the client side. " +
                                  "Though I love Flutter for it's cross platform capability, it's web implementation (written in Javascript) does not have the raw performance of say a custom WebAssembly implementation " +
                                  "written in Rust or C++."
                                  ,style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.black87)
                                ),
                                SizedBox(height: 10),
                                Text( 
                                  "On the link speed side, it was not really feasible to send raw FFT bins over my internet anyway. Though I was able to squeak out a single client, whenever my " +
                                  "roommate facetimes his girlfriend, the connection would slow down to a point where samples would pile up and a coherent display would be impossible. As such, the low cost approach at present "
                                  "is one where JPEG snapshots are sent out every 2.5 seconds."
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

class RealTimeSpectrogram extends StatefulWidget {
  RealTimeSpectrogram({Key key, this.width, this.height}) : super(key: key);

  final double width;
  final double height;

  @override
  _RealTimeSpectrogramState createState() => _RealTimeSpectrogramState();
}

class _RealTimeSpectrogramState extends State<RealTimeSpectrogram> {
  bool _connected = false;
  bool _queued = false;
  int  _queuePosition = 0;
  bool _duplicate = false;
  bool _served = false;
  //List<num> rawISamples = [];
  //List<num> rawQSamples = [];
  //Number of windows processed each cycle: 429
  //Number of samples removes from samples after processing cycle: 219648
  //Number of samples processed each cycle: 220160
  //List<int> displayedMagnitude = new List.filled(4410368,128, growable: true);

  //List<LinearGradient> gradientBuffer = [];


  //List<LinearGradient> gradientList = new List.filled(4307, gradient, growable:true);
  //StreamController<LinearGradient> gradientController = StreamController<LinearGradient>.broadcast();
  StreamController<Tuple2<String, DateTime>> imageController = StreamController<Tuple2<String, DateTime>>.broadcast(); 

  static var url = window.location.hostname;
  var channel = HtmlWebSocketChannel.connect("ws://"+url+"/socket");

  /*Future<List<LinearGradient>> _processSamples(List<num> rawISamples, List<num> rawQSamples) async {
    Map<String,List<num>> myMap = Map();
    myMap["I"] = rawISamples;
    myMap["Q"] = rawQSamples;
    return compute(iqToMagnitude, myMap);
  }*/

  @override
  void dispose() {
    //gradientController.close();
    imageController.close();
    super.dispose();
  }

  @override
  void initState() {
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
    double _width = widget.width;
    double _height = widget.height;
    return SizedBox(  
          width: _width-18,
          height: _height*0.75,
          child: !_connected ? Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
                : _duplicate ? Center(child: Text("Your device is already connected to this site somewhere. Sorry, I do not have the bandwidth to host you twice.", style: TextStyle(fontSize:27, color: Colors.white)))
                : _queued ? Center(child: Text("You are in the queue to view the spectrogram. Your position in the queue is: $_queuePosition.", style: TextStyle(fontSize:27, color: Colors.white)))
                : _served ? //Center(child: Text("Testing Compute function no graphics.", style: TextStyle(fontSize:27, color: Colors.white)))
                  SpectrogramWindow(imageStream: imageController.stream, width: _width, height: _height)
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
