Pebble.addEventListener("ready",
  function(e) {
    console.log("PebbleKit JS ready, moo!");
  }
);

Pebble.addEventListener("showConfiguration",
  function(e) {
    //Load the remote config page
    console.log("show config, moo!");
    Pebble.openURL("http://minotaurproject.co.uk/YakImages/bbc.html");
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    //Get JSON dictionary
    var configuration = JSON.parse(decodeURIComponent(e.response));
    console.log("Configuration window returned: " + JSON.stringify(configuration));
 
    //Send to Pebble, persist there
    Pebble.sendAppMessage(
      {"KEY_SECONDS": configuration.seconds,
       "KEY_ERA":configuration.era,
       "KEY_XCOL":configuration.xcol},
      function(e) {
        console.log("Sending settings data...");
      },
      function(e) {
        console.log("Settings feedback failed!");
      }
    );
  }
);                     