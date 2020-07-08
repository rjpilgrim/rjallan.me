import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'dart:html';
import 'dart:math';
import 'dart:core';
import 'dart:js' as js;
import 'package:web_socket_channel/html.dart';
import 'dart:async';
import 'dart:convert';
import 'package:fft/fft.dart' as fft;
import 'package:my_complex/my_complex.dart';
import 'package:loading/loading.dart';
import 'package:loading/indicator/line_scale_indicator.dart';
import 'hann1024.dart';

const hann_sum = 552.5000000000002;
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

Map<String,dynamic> jsonDecodeIsolate(dynamic responseBody) {
  //print("TRY DECODE on $responseBody");
  return jsonDecode(responseBody);
}

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      //title: 'Flutter Demo',
      /*theme: ThemeData(
        // This is the theme of your application.
        //
        // Try running your application with "flutter run". You'll see the
        // application has a blue toolbar. Then, without quitting the app, try
        // cha//nging the primarySwatch below to Colors.green and then invoke
        // "hot reload" (press "r" in the console where you ran "flutter run",
        // or simply save your changes to "hot reload" in a Flutter IDE).
        // Notice that the counter didn't reset back to zero; the application
        // is not restarted.
        primarySwatch: Colors.blue,
        // This makes the visual density adapt to the platform that you run
        // the app on. For desktop platforms, the controls will be smaller and
        // closer together (more dense) than on mobile platforms.
        visualDensity: VisualDensity.adaptivePlatformDensity,
      ),*/
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
    // This method is rerun every time setState is called, for instance as done
    // by the _incrementCounter method above.
    //
    // The Flutter framework has been optimized to make rerunning build methods
    // fast, so that you can just rebuild anything that needs updating rather
    // than having to individually change instances of widgets.
    var  _mediaQuery = MediaQuery.of(context);
    double _width = _mediaQuery.size.width;
    double _height = _mediaQuery.size.height;

    return Scaffold(
      /*appBar: AppBar(
        // Here we take the value from the MyHomePage object that was created by
        // the App.build method, and use it to set our appbar title.
        title: Text(widget.title),
      ),*/
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
        // Center is a layout widget. It takes a single child and positions it
        // in the middle of the parent.
        child: Column(
          // Column is also a layout widget. It takes a list of children and
          // arranges them vertically. By default, it sizes itself to fit its
          // children horizontally, and tries to be as tall as its parent.
          //f
          // Invoke "debug painting" (press "p" in the console, choose the
          // "Toggle Debug Paint" action from the Flutter Inspector in Android
          // Studio, or the "Toggle Debug Paint" command in Visual Studio Code)
          // to see the wireframe for each widget.
          //
          // Column has various properties to control how it sizes itself and
          // how it positions its children. Here we use mainAxisAlignment to
          // center the children vertically; the main axis here is the vertical
          // axis because Columns are vertical (the cross axis would be
          // horizontal).
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
  bool _connected = false;
  bool _queued = false;
  int  _queuePosition = 0;
  bool _duplicate = false;
  bool _served = false;
  bool _ready = false;
  List<num> rawISamples = [];
  List<num> rawQSamples = [];
  //Number of windows processed each cycle: 429
  //Number of samples removes from samples after processing cycle: 219648
  //Number of samples processed each cycle: 220160
  //List<int> displayedMagnitude = new List.filled(4410368,128, growable: true);

  static List<Color> colorList = new List.filled(1024,Color.fromARGB(255, 128, 128, 128));

  static var gradient = LinearGradient(  
    begin: Alignment.centerLeft,
    end: Alignment.centerRight, 
    colors: colorList
  );

  List<LinearGradient> gradientList = new List.filled(4307, gradient, growable:true);

  static var url = window.location.hostname;
  var channel = HtmlWebSocketChannel.connect("ws://"+url+"/socket");

  Future<List<LinearGradient>> _processSamples(List<num> rawISamples, List<num> rawQSamples) async {
    Map<String,List<num>> myMap = Map();
    myMap["I"] = rawISamples;
    myMap["Q"] = rawQSamples;
    return compute(iqToMagnitude, myMap);
  }

  @override
  Widget build(BuildContext context) {
    double _width = widget.width;
    double _height = widget.height;
    return StreamBuilder(  
              stream: channel.stream,
              builder: (context, snapshot) {
                if (snapshot.hasData) {
                  Future<Map<String, dynamic>> messageFuture = compute(jsonDecodeIsolate, snapshot.data);
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
                  if (message.containsKey("I Samples")) {
                    setState(() {
                      _connected = true;  
                    });
                    print("GOT SPECIAL SAMPLE MESSAGE");
                    rawISamples.addAll(message["I Samples"]);
                    rawQSamples.addAll(message["Q Samples"]);
                    if (rawISamples.length >220160) {
                      Future<List<LinearGradient>> gradientFuture = _processSamples(rawISamples, rawQSamples);
                      gradientFuture.then((List<LinearGradient> newGradients) {
                        print("here is new gradient length: ${newGradients.length}");
                        gradientList.removeRange(4307-429, 4307);
                        rawISamples.removeRange(0, 219648);
                        rawQSamples.removeRange(0,219648);
                        setState(() {
                          gradientList.insertAll(0, newGradients);
                          _ready = true;
                        });
                      });
                    }
                  }
                  });
                }
                return SizedBox(  
                  width: _width-18,
                  height: _height*0.75,
                  child: !_connected ? Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
                       : _duplicate ? Center(child: Text("Your device is already connected to this site somewhere. Sorry, I do not have the bandwidth to host you twice.", style: TextStyle(fontSize:27, color: Colors.white)))
                       : _queued ? Center(child: Text("You are in the queue to view the spectrogram. Your position in the queue is: $_queuePosition.", style: TextStyle(fontSize:27, color: Colors.white)))
                       : _served ? //Center(child: Text("Testing Compute function no graphics.", style: TextStyle(fontSize:27, color: Colors.white)))
                         SpectrogramWindow(gradientList, _width, _height)
                       : Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
                );
              }
            );
  }

}

class SpectrogramWindow extends StatelessWidget {
  final List<LinearGradient> gradientList;
  final double width;
  final double height;

  SpectrogramWindow(this.gradientList, this.width, this.height);

  @override
  Widget build(BuildContext context) {
    double _width = this.width;
    double _height = this.height;
    return Column(children: [
      Expanded( child:
      Row(children: [
      Container(  
        width: 20,
      ),
      Expanded(child:
        Column( 
        children: [
          Center(child: Text("89.7 MHz", style: TextStyle(fontSize:(_width < 450) ? 12:18, color: Colors.white)),),
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
          child: CustomPaint( 
              painter: SamplesPainter(gradientList),
              child: Center(child: Text(""))
            ) 
          
            //Center(child: Loading(indicator: LineScaleIndicator(), size: 100.0, color:Colors.white))
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
      ),
      Container( 
        width: 20,
      )
      ],
      )
      )
    ],
    );  
  }
}

class SamplesPainter extends CustomPainter {
  final List<LinearGradient> gradientList;

  SamplesPainter(this.gradientList);
  

  @override
  void paint(Canvas canvas, Size size) {
    for(int i = 0; i < 4307; i++) {
      double myY = (i/4307) * size.height;
      var rect = Rect.fromPoints(Offset(0, myY), Offset(size.width, myY));
      var gradient = gradientList[i];
      canvas.drawRect(rect,Paint()..shader = gradient.createShader(rect)..blendMode = BlendMode.screen..strokeWidth=(size.height/4307)..style=PaintingStyle.stroke);
    }
    /*double myY = size.height;
    var rect = Rect.fromPoints(Offset(0, 0), Offset(size.width, myY));
    var gradient = gradientList[0];
    canvas.drawRect(rect,Paint()..shader = gradient.createShader(rect)..style=PaintingStyle.fill);*/
  }

  @override
  bool shouldRepaint(SamplesPainter oldDelegate) => this.gradientList[0] != oldDelegate.gradientList[0];

}
