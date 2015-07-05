# bbc
This is my BBC watchface with simple JS to do settings.

There is also an HTML component that runs on my website.  If you extend or modify the settings
then you'll need to create your own companion HTML page and hosy it on your site, and then
modify the JS so it points to your settings page and not mine!

In this readme I'll include the code of my HTML settings page so you can work from that if you do
modify or extend the settings.

HTML code follows }:-)

<!DOCTYPE html>
<html>
  <head>
    <title>BBC Configuration</title>
  </head>
  <body>
    <h1>BBC</h1>
    <p>Choose watchface settings</p>
 
    <p>Second hand display:
    <select id="seconds_select">
      <option value="off">Off</option>
      <option value="on">On</option>
    </select>
    </p>

    <p>Era:
    <select id="era_select">
      <option value="1970">1970</option>
      <option value="1978">1978</option>
      <option value="1981">1981</option>
    </select>
    </p>
 
    <p>
    <button id="save_button">Save</button>
    </p>

  <script>
    //Setup to allow easy adding more options later
    function saveOptions() {
    var secondsSelect = document.getElementById("seconds_select");
    var eraSelect=document.getElementById("era_select");
    var options = {
      "seconds": secondsSelect.options[secondsSelect.selectedIndex].value,
      "era":eraSelect.options[eraSelect.selectedIndex].value
    }
     
    return options;
  };
 
  var submitButton = document.getElementById("save_button");
  submitButton.addEventListener("click", 
    function() {
      console.log("Submit");
 
      var options = saveOptions();
      var location = "pebblejs://close#" + encodeURIComponent(JSON.stringify(options));
       
      document.location = location;
    }, 
  false);
</script>

  </body>
</html>

