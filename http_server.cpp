/*
 *  Author: Rada Berar
 *  email: ujagaga@gmail.com
 *
 *  HTTP server which generates the web browser pages.
 */

#include <ESP8266WebServer.h>
#include <pgmspace.h>
#include "wifi_connection.h"
#include "config.h"
#include "pinctrl.h"
#include "freqmeter.h"
#include "esp8266_IEC104_emulator.h"


/* If we were writing HTML files, this would be the content. Here we use char arrays. */
static const char HTML_BEGIN[] PROGMEM = R"(
<!DOCTYPE HTML>
<html>
  <head>
    <meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
    <title>WiFi Gate</title>
    <style>
      body { background-color: white; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }
      .contain{width: 100%;}
      .center_div{margin:0 auto; max-width: 400px;position:relative;}
    </style>
  </head>
  <body>
)";

static const char HTML_END[] PROGMEM = "</body></html>";

static const char INDEX_HTML_0[] PROGMEM = R"(
<style>
  .btn_led{border:0;border-radius:50%;width:6rem;height:6rem;margin:2rem auto;display:block;cursor:pointer;}
  .btn_led_on{background-color:#2ecc71;}
  .btn_led_off{background-color:#888888;}
  .btn_cfg{border:0;border-radius:0.3rem;color:#fff;line-height:1.4rem;font-size:0.8rem;margin:1ch;height:2rem;width:10rem;background-color:#ff3300;}
</style>
<div class="contain">
  <div class="center_div">
)";

const char INDEX_HTML_1[] PROGMEM = R"(
  </div>
  <p id='freqVal'></p>
  <hr>
  <button class="btn_cfg" type="button" onclick="location.href='/selectap';">Configure wifi</button>
  <span id='status'></span>
  <br/>
</div>
<script>
  function checkIp(){
    fetch('/constatus').then(function(r){ return r.text(); }).then(function(ip){
      if(ip){
        document.getElementById('status').innerHTML = 'IP: ' + ip;
      }else{
        setTimeout(checkIp, 2000);
      }
    }).catch(function(){
      setTimeout(checkIp, 2000);
    });
  }
  checkIp();

  var ledOn = document.getElementById('ledBtn').className.indexOf('btn_led_on') !== -1;
  function toggleLed(){
    location.href = '/led?state=' + (ledOn ? 0 : 1);
  }
  function refreshLed(){
    fetch('/ledstate').then(function(r){ return r.text(); }).then(function(s){
      ledOn = (s === '1');
      document.getElementById('ledBtn').className = 'btn_led ' + (ledOn ? 'btn_led_on' : 'btn_led_off');
    });
  }
  setInterval(refreshLed, 1000);

  function refreshFreq(){
    fetch('/freq').then(function(r){ return r.text(); }).then(function(f){
      document.getElementById('freqVal').innerHTML = f ? ('Frequency: ' + f + ' Hz') : '';
    });
  }
  setInterval(refreshFreq, 2000);
</script>
)";

static const char APLIST_HTML_0[] PROGMEM = R"(
<style>
  .c{text-align: center;}
  div,input{padding:5px;font-size:1em;}
  input{width:95%;}
  body{text-align: left;}
  button{width:100%;border:0;border-radius:0.3rem;color:#fff;line-height:2.4rem;font-size:1.2rem;height:40px;background-color:#1fa3ec;}
  .q{float: right;width: 64px;text-align: right;}
  .radio{width:2em;}
  #vm{width:100%;height:50vh;overflow-y:auto;margin-bottom:1em;}
</style>
</head><body>
  <div class="contain">
    <div class="center_div">
)";

/* Placeholder for the wifi list */
static const char APLIST_HTML_1[] PROGMEM = R"(
      <h1 id='ttl'>Networks found:</h1>
      <div id='vm'>
)";

static const char APLIST_HTML_2[] PROGMEM = R"(
      </div>
      <form method='get' action='wifisave'>
        <button type='button' onclick='refresh();'>Rescan</button><br/><br/>
        <input id='s' name='s' length=32 placeholder='SSID (Leave blank for AP mode)'><br>
        <input id='p' name='p' length=32 placeholder='password'><br>
        <br><button type='submit'>save</button>
      </form>
     </div>
  </div>
<script>
  function c(l){
    document.getElementById('s').value=l.innerText||l.textContent;
    document.getElementById('p').focus();
  }

  function refresh(){
    document.getElementById('vm').innerHTML='Please wait...';
    fetch('/aplist').then(function(r){ return r.text(); }).then(function(data){
      var rsp = data.split('|');
      var vm = document.getElementById('vm');
      vm.innerHTML = '';
      for(var i = 0; i < rsp.length; i++){
        vm.innerHTML += '<span>' + (i + 1) + ": </span><a href='#p' onclick='c(this)'>" + rsp[i] + '</a><br>';
      }
      if(!vm.innerHTML.replace(/\s/g,'').length){
        document.getElementById('ttl').innerHTML = 'No networks found.';
      }
    });
  }
  window.onload = refresh;
</script>
)";

static const char REDIRECT_HTML[] PROGMEM = R"(
<p id="tmr"></p>
<script>
  var c=3;
  function count(){
    var tmr=document.getElementById('tmr');
    if(c>0){
      c--;
      tmr.innerHTML="You will be redirected to home page in "+c+" seconds.";
      setTimeout('count()',1000);
    }else{
      window.location.href="/";
    }
  }
  count();
</script>
)";


/* Declaring a web server object. */
ESP8266WebServer* webServer = nullptr;

void showStartPage() {
  bool ledOn = PINCTRL_getLed();
  String response = FPSTR(HTML_BEGIN);
  response += FPSTR(INDEX_HTML_0);
  response += "<button id=\"ledBtn\" class=\"btn_led ";
  response += String(ledOn ? "btn_led_on" : "btn_led_off");
  response += "\" type=\"button\" onclick=\"toggleLed()\"></button>";
  response += FPSTR(INDEX_HTML_1);
  response += FPSTR(HTML_END);
  webServer->send(200, "text/html", response);
}

static void setLed(void){
  if (webServer->hasArg("state")) {
    PINCTRL_setLed(webServer->arg("state").toInt() != 0);
  }
  showStartPage();
}

static void showNotFound(void){
  webServer->send(404, "text/html; charset=iso-8859-1","<html><head> <title>404 Not Found</title></head><body><h1>Not Found</h1></body></html>");
}

static void showStatusPage(bool goToHome = false) {
  Serial.println("showStatusPage");
  String response = FPSTR(HTML_BEGIN);
  response += "<h1>Connection Status</h1><p>";
  response += MAIN_getStatusMsg() + "</p>";
  if(goToHome){
    /* Add redirect timer. */
    response += FPSTR(REDIRECT_HTML);
  }
  response += FPSTR(HTML_END);
  webServer->send(200, "text/html", response);
}

static void showConStatus(void){
  String ip = WIFIC_isStaConnected() ? WIFIC_getStIP().toString() : "";
  webServer->send(200, "text/plain", ip);
}

static void showLedState(void){
  webServer->send(200, "text/plain", PINCTRL_getLed() ? "1" : "0");
}

static void showFreq(void){
  float freq = FREQMETER_getFrequency();
  webServer->send(200, "text/plain", freq > 0 ? String(freq, 2) : "");
}


static void selectAP(void) {
  Serial.println("selectAP");
  String response = FPSTR(HTML_BEGIN);
  response += FPSTR(APLIST_HTML_0);
  response += FPSTR(APLIST_HTML_1);
  response += "Please wait...";
  response += FPSTR(APLIST_HTML_2);
  response += FPSTR(HTML_END);
  webServer->send(200, "text/html", response);
}

static void showApList(void){
  webServer->send(200, "text/plain", WIFIC_getApList());
}

static void saveWiFi(void){
  String ssid = webServer->arg("s");
  String pass = webServer->arg("p");

  if((ssid.length() > 63) || (pass.length() > 63)){
      MAIN_setStatusMsg("Sorry, this module can only remember SSID and a PASSWORD up to 63 bytes long.");
      showStatusPage();
      return;
  }

  String st_ssid = WIFIC_getStSSID();
  String st_pass = WIFIC_getStPass();

  if(st_ssid.equals(ssid) && st_pass.equals(pass)){
      MAIN_setStatusMsg("All parameters are already set as requested.");
      showStatusPage();
      return;
  }

  WIFIC_setStSSID(ssid);
  WIFIC_setStPass(pass);

  String http_statusMessage;

  if(ssid.length() > 3){
    http_statusMessage = "Saving settings and connecting to SSID: ";
    http_statusMessage += ssid;
    http_statusMessage += "<br>Stay connected to this WiFi Access Point, the new IP will appear on the home page once connected.";
  }else{
    http_statusMessage = "Saving settings and switching to AP mode only.";
  }

  MAIN_setStatusMsg(http_statusMessage);
  showStatusPage(true);

  volatile int i;

  /* Keep serving http to display the status page*/
  for(i = 0; i < 100000; i++){
    webServer->handleClient();
    ESP.wdtFeed();
  }

  /* WiFI config changed. Restart to apply.
   Note: ESP.restart is buggy after programming the chip.
   Just reset once after programming to get stable results. */

  ESP.restart();
}

void HTTP_SERVER_process(void){
  webServer->handleClient();
}

void HTTP_SERVER_init(void){
  if (webServer != nullptr) {
    delete webServer; // Clean up old one
  }
  webServer = new ESP8266WebServer(80);

  webServer->on("/", showStartPage);
  webServer->on("/favicon.ico", showNotFound);
  webServer->on("/selectap", selectAP);
  webServer->on("/aplist", showApList);
  webServer->on("/led", setLed);
  webServer->on("/wifisave", saveWiFi);
  webServer->on("/constatus", showConStatus);
  webServer->on("/ledstate", showLedState);
  webServer->on("/freq", showFreq);
  webServer->onNotFound(showStartPage);

  webServer->begin();
}
