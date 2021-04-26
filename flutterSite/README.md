This is client which runs in the browser, built with Flutter Web.

Please visit https://flutter.dev/web for instructions on building flutter for web.

Originally, this client was intended to do all of the signal processing for the spectrogram, as well as drawing the computed samples using a linear gradient for each fourier window and Flutter's Canvas API. The signal processing immedietly crashed the Flutter javascript engine. I then refactored to only doing the drawing in the client. Flutter was sort of able to do this, but drawing all of the fourier windows proved to very inefficient with the Canvas API. As such, I decided to create jpegs on the server side and simply fetch them in the client.

In order for the play button to work as intended, the following code must be added to the index.html file generated by flutter in the body tag but before the script tag with main.dart.js in it.

```
<script src="https://cdn.jsdelivr.net/npm/hls.js@latest"></script>
  <audio id="audio"></audio>
  <script type="application/javascript">
    var audio = document.getElementById('audio');
    function playAudio(path) {
      if (Hls.isSupported()) {
         console.log(path);
        Hls.DefaultConfig.debug = true;
        var hls = new Hls();
        hls.loadSource(path);
        hls.attachMedia(audio)
        hls.on(Hls.Events.MANIFEST_PARSED, function() {
           console.log("Hello play!");
          audio.play();
        });
      }
      else if (audio.canPlayType('application/vnd.apple.mpegurl')) {
        audio.src = path;
        audio.addEventListener('loadedmetadata', function () {
          audio.play();
        });
      }
    }
    function stopAudio() {
      audio.pause();
    }
  </script>
```
