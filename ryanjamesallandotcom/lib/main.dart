import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'dart:html';
import 'dart:math';
import 'dart:ui' as dart_ui;
import 'dart:core';
import 'dart:js' as js;
import 'package:web_socket_channel/html.dart';
import 'dart:async';
import 'dart:convert';
import 'package:fft/fft.dart' as fft;
import 'package:my_complex/my_complex.dart';
import 'package:loading/loading.dart';
import 'package:loading/indicator/line_scale_indicator.dart';
import 'package:tuple/tuple.dart';
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

LinearGradient gradientIsolate(List<dynamic> rgbMagnitudes) {
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
}

/*Canvas canvasIsolate(Tuple2<List<LinearGradient>, Size> parameters) {
  Canvas returnCanvas = Canvas(dart_ui.PictureRecorder());
  for(int i = 0; i < 538; i++) {
      double myY = (i/538) * parameters.item2.height;
      var rect = Rect.fromPoints(Offset(0, myY), Offset(parameters.item2.width, myY));
      var gradient = parameters.item1[i];
      var paint = Paint()..shader = gradient.createShader(rect)..blendMode = BlendMode.screen..strokeWidth=(parameters.item2.height/538)..style=PaintingStyle.stroke;
      returnCanvas.drawRect(rect, paint);
  }
  return returnCanvas;
}*/

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

  @override
  Widget build(BuildContext context) {
    var  _mediaQuery = MediaQuery.of(context);
    double _width = _mediaQuery.size.width;
    double _height = _mediaQuery.size.height;
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
                Container(
                  width: (_width < 450) ? _width/3.5 :150,  
                  alignment: Alignment.center,
                  decoration: BoxDecoration(  
                    border: Border( 
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      left: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("Home", style: TextStyle(fontSize: (_width < 450) ? 14 :21, color: Colors.white))
                ),
                Container(
                  width: (_width < 450) ? _width/3.5 :150,  
                  alignment: Alignment.center, 
                  decoration: BoxDecoration(  
                    border: Border(
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 2.5),
                      left: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("About me", style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.white))
                ),
                Container(
                  width: (_width < 450) ? _width/3.5 :150,   
                  alignment: Alignment.center,
                  decoration: BoxDecoration(  
                    border: Border( 
                      top: BorderSide(color: Colors.white, width: 5.0),
                      bottom: BorderSide(color: Colors.white, width: 5.0),
                      right: BorderSide(color: Colors.white, width: 5.0),
                      left: BorderSide(color: Colors.white, width: 2.5)
                    )
                  ),
                  child: Text("What is this?", style: TextStyle(fontSize: (_width < 450) ? 14 :21,  color: Colors.white))
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
            RealTimeSpectrogram(height: _height, width: _width),
            
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
  bool _connected = true;
  bool _queued = false;
  int  _queuePosition = 0;
  bool _duplicate = false;
  bool _served = true;
  bool _ready = false;
  //List<num> rawISamples = [];
  //List<num> rawQSamples = [];
  //Number of windows processed each cycle: 429
  //Number of samples removes from samples after processing cycle: 219648
  //Number of samples processed each cycle: 220160
  //List<int> displayedMagnitude = new List.filled(4410368,128, growable: true);

  //List<LinearGradient> gradientBuffer = [];


  //List<LinearGradient> gradientList = new List.filled(4307, gradient, growable:true);
  StreamController<LinearGradient> gradientController = StreamController<LinearGradient>.broadcast();
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
    gradientController.close();
    super.dispose();
  }

  @override
  void initState() {
    channel.stream.listen((data) {
      Future<Map<String, dynamic>> messageFuture = compute(jsonDecodeIsolate, data);
      messageFuture.then(( Map<String, dynamic> message) {
          print("FUTURE RETURN");
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
          if (message.containsKey("Magnitudes")) {
            List<dynamic> rgbMagnitudes = message["Magnitudes"];
            Future<LinearGradient> gradientFuture = compute(gradientIsolate, rgbMagnitudes);
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
            });
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
                  SpectrogramWindow(gradientStream: gradientController.stream, width: _width, height: _height)
                : Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
        );
    
  }
}

class SpectrogramWindow extends StatefulWidget {
  SpectrogramWindow({Key key, this.gradientStream, this.width, this.height}) : super(key: key);
  final Stream<LinearGradient> gradientStream;
  final double width;
  final double height;

  @override
  _SpectrogramWindowState createState() => _SpectrogramWindowState();
}

class _SpectrogramWindowState extends State<SpectrogramWindow> {
  //static List<Color> colorList = new List.filled(8192,Color.fromARGB(255, 0, 0, 0), growable: true);
  static List<Color> altColorList = new List.filled(8192,Color.fromARGB(255, 128, 128, 128), growable: true);

  static var initGradient = LinearGradient(  
    begin: Alignment.centerLeft,
    end: Alignment.centerRight, 
    colors: altColorList
  );

  //List<LinearGradient> gradientList = new List.filled(4307, gradient, growable:true);
  List<LinearGradient> gradientList = new List.filled(538, initGradient, growable:true);

  List<Widget> myChildren =   new List.filled(538,Expanded(child:Container(
          decoration: BoxDecoration(
            gradient: initGradient
          )
        ))
        , growable:true
        );
  //List<LinearGradient> gradientList = new List.filled(538, gradient, growable:true);
  
  int _repaintCounter;
  bool _repaint = false;

  SamplesPainter mySamplesPainter;

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

  }

  @override
  void initState() {
    widget.gradientStream.listen((data) {
      /*putContainer(
        Container( 
          decoration: BoxDecoration(
            gradient: data
          )
        )
      );*/
      putGradient(data);
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
          
          child:
            CustomPaint( 
              painter: SamplesPainter(gradientList, _repaint),
              child: Container( 
                child: CustomPaint( 
                  painter: TimePainter((_width < 450) ? true:false),
                  child: Center(child: Text(""))
                )
              )
            ) 
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

class TimePainter extends CustomPainter {

  final bool smallText;

  TimePainter(this.smallText);

  @override
  void paint(Canvas canvas, Size size) {
    //538 rows of 8192 bins
    var textStyle = TextStyle(color: Colors.white, fontSize: smallText ? 12 : 18);

    var firstTextSpan = TextSpan(  
      text: "Most Recent",
      style: textStyle
    );
    var firstTextPainter = TextPainter(  
      text: firstTextSpan,
      textDirection: TextDirection.ltr
    );
    firstTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    firstTextPainter.paint(canvas, Offset(2,2));

    var lastTextSpan = TextSpan(  
      text: "10s Before",
      style: textStyle
    );
    var lastTextPainter = TextPainter(  
      text: lastTextSpan,
      textDirection: TextDirection.ltr
    );
    lastTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    lastTextPainter.paint(canvas, Offset(3, size.height - 3- lastTextPainter.height));
  }

  @override
  bool shouldRepaint(TimePainter oldDelegate) =>  (this.smallText != oldDelegate.smallText);

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
      text: "89.59 MHz",
      style: textStyle
    );
    var firstTextPainter = TextPainter(  
      text: firstTextSpan,
      textDirection: TextDirection.ltr
    );
    firstTextPainter.layout(  
      minWidth: 0,
      maxWidth: size.width*0.33
    );
    firstTextPainter.paint(canvas, Offset(0, size.height - 8 - firstTextPainter.height));

    var middleTextSpan = TextSpan(  
      text: "89.7 MHz",
      style: textStyle
    );
    var middleTextPainter = TextPainter(  
      text: middleTextSpan,
      textDirection: TextDirection.ltr
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
      textDirection: TextDirection.ltr
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
