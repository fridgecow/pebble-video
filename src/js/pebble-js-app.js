var transferInProgress = false;
var downloadsInProgress = false;
var frame = 1;
var chunk = 0;
var timeSinceLast = Date.now();
var urls = [
	"http://www.fridgecow.com/images/pblvid/otr7/",
	"http://www.fridgecow.com/images/pblvid/bbb/"
]
var frames = [];
var url;
var failures=0;

Pebble.addEventListener("ready", function(e) {
  console.log("NetDownload JS Ready");
  //Pick which URL
  url = urls[Math.round(Math.random()*urls.length) % urls.length];
  console.log("Streaming from "+url);
});

function pad(num, size){ return ('000' + num).substr(-size); }

function transferNxt(){
  //Start transferring next frame, if available
  if(frames.length > 0){
    var bytes = frames.shift();
    transferImageBytes(bytes, chunk, transferNxt,
      function(e){ 
        console.log("Failed to transfer! Aborting."); 
        failures++;
      }
    );
    if(!downloadsInProgress){
      loadFrame(); //Restart downloads if they're not happening
    }
  }else{
    transferInProgress = false;
  }
              
}
function loadFrame(){
  downloadsInProgress = true
  downloadBinaryResource(url+pad(frame, 3)+".png", 
    function(bytes){
      frames.push(bytes); //Leave frame for different thread
      console.log("Frame buffer size: "+frames.length);
     
      if(!transferInProgress){ //If there's no sequence consuming frames
        transferInProgress = true;
        transferNxt();
      }

      //Load next frame
      if(frames.length < 50){ //If there's not too many frames anyway
        frame++;
        loadFrame();
      }else{
        downloadsInProgress = false;
      }
    },

		function(e) {
			if(failures > 100) return;

			failures++;
			console.log("Download failed. Trying next frame." ); 
			frame++; 
			loadFrame();
		}
	);
}

Pebble.addEventListener("appmessage", function(e) {
  console.log("Got message: " + JSON.stringify(e));
  if ('NETDL_URL' in e.payload) {
	chunk = e.payload['NETDL_CHUNK_SIZE'];
	loadFrame(e);
  }
});

function downloadBinaryResource(imageURL, callback, errorCallback) {
    var req = new XMLHttpRequest();
    console.log("Downloading "+imageURL);
    req.open("GET", imageURL,true);
    req.responseType = "arraybuffer";
    req.onload = function(e) {
        console.log("loaded");
        var buf = req.response;
        if(req.status == 200 && buf) {
            var byteArray = new Uint8Array(buf);
            var arr = [];
            for(var i=0; i<byteArray.byteLength; i++) {
                arr.push(byteArray[i]);
            }

            //console.log("Downloaded file with " + byteArray.length + " bytes.");
            callback(arr);
        }
        else {
          errorCallback("Request status is " + req.status);
        }
    }
    req.onerror = function(e) {
      errorCallback(e);
    }
    req.send(null);
}

function transferImageBytes(bytes, chunkSize, successCb, failureCb) {
  var retries = 0;

  success = function() {
    //console.log("Success cb=" + successCb);
    if (successCb != undefined) {
      successCb();
    }
  };
  failure = function(e) {
    //console.log("Failure cb=" + failureCb);
    if (failureCb != undefined) {
      failureCb(e);
    }
  };

  // This function sends chunks of data.
  sendChunk = function(start) {
    var txbuf = bytes.slice(start, start + chunkSize);
    //console.log("Sending " + txbuf.length + " bytes - starting at offset " + start);
    Pebble.sendAppMessage({ "NETDL_DATA": txbuf },
      function(e) {
        // If there is more data to send - send it.
        if (bytes.length > start + chunkSize) {
          sendChunk(start + chunkSize);
        }
        // Otherwise we are done sending. Send closing message.
        else {
          Pebble.sendAppMessage({"NETDL_END": "done" }, success, failure);
        }
      },
      // Failed to send message - Retry a few times.
      function (e) {
        if (retries++ < 3) {
          console.log("Got a nack for chunk #" + start + " - Retry...");
          sendChunk(start);
        }
        else {
          failure(e);
        }
      }
    );
  };

  // Let the pebble app know how much data we want to send.
  Pebble.sendAppMessage({"NETDL_BEGIN": bytes.length },
    function (e) {
      // success - start sending
      sendChunk(0);
    }, failure);

}
