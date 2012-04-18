/* 
 * mpsutils.js
 *
 * This provides a javascript interface to control (start, stop, 
 * skip forward, skip backward, pause, etc) the media player using
 * the CGI interface from mps-commands. 
 *
 * These should be decoupled from any specific user interface.
 *
 */

function mpsPlay() {
    var command = '/cgi-bin/mps-player?play+current';

    AJAXdispatchGetCommand(command);
}

function mpsPause() {
    var command = '/cgi-bin/mps-player?pause';

    AJAXdispatchGetCommand(command);
}

function mpsPlayerGetSummary(callback) {
    var command = '/cgi-bin/mps-player?get+summary';

    AJAXdispatchGetCommand(command, callback);
}

function mpsPlayNext(callback) {
    var command = '/cgi-bin/mps-player?play+next';

    AJAXdispatchGetCommand(command, callback);
}

function mpsPlayPrevious(callback) {
    var command = '/cgi-bin/mps-player?play+previous';

    AJAXdispatchGetCommand(command, callback);
}

function mpsPlayAbsolute(callback, id) {
    var command = '/cgi-bin/mps-player?play+absolute+' + id;

    AJAXdispatchGetCommand(command, callback); 
}

function mpsGetPlaylist(callback, param1, param2) {
    var command = '/cgi-bin/mps-playlist?get+' + param1;
    if (param2) { command += '+' + param2; }

    AJAXdispatchGetCommand(command, callback); 
}

function mpsAddOneToPlaylist(callback, param1) {
    var command = '/cgi-bin/mps-playlist?add+end+' + param1;

    AJAXdispatchGetCommand(command, callback);
}

function mpsAddAllToPlaylist(callback, param1) {
    var command = '/cgi-bin/mps-playlist?add+end+' + param1;

    AJAXdispatchGetCommand(command, callback);
}

function mpsRemoveOneFromPlaylist(callback, param1) {
    var command = '/cgi-bin/mps-playlist?remove+' + param1;

    AJAXdispatchGetCommand(command, callback);
}

function mpsRemoveAllFromPlaylist(callback) {
    var command = '/cgi-bin/mps-playlist?remove+all';

    AJAXdispatchGetCommand(command, callback);
}

function mpsLibraryGetStatus(callback) {
    var command = '/cgi-bin/mps-library?get+status';

    AJAXdispatchGetCommand(command, callback);
}

function mpsGetLibrary(callback, param1, param2) {
    var command;

    if (param1 == "path" && !param2) {
	command = '/cgi-bin/mps-library?get+root';
    }
    else if (!param2) {
	command = '/cgi-bin/mps-library?get+' + param1;
    }
    else {
	command = '/cgi-bin/mps-library?get+' 
	    + param1 + '+' + param2;
    }

    AJAXdispatchGetCommand(command, callback);
}

function mpsSimpleSearch(searchString, callback) {
    var command;
    console.log(searchString);
    command = '/cgi-bin/mps-library?get+search+' + searchString + '+20';

    AJAXdispatchGetCommand(command, callback);
}

function mpsSimpleRandomSuggest(callback) {
    var command;
    command = '/cgi-bin/mps-library?get+random+10';

    AJAXdispatchGetCommand(command, callback);
}

function mpsGetConfig(param_list, callback) {
    var param_string = "";
    //console.log("param_list = " + param_list);
    for (i = 0; i < param_list.length; i++) {
        param_string += '+' + param_list[i];        
    }  
    var command = '/cgi-bin/mps-config?get' + param_string;
    //console.log("command = " + command);
    AJAXdispatchGetCommand(command, callback);
}

function mpsGetConstants(param_list, callback) {
    var param_string = "";
    for (i = 0; i < param_list.length; i++) {
        param_string += '+' + param_list[i];        
    }  
    var command = '/cgi-bin/mps-constants?get' + param_string;;
    
    AJAXdispatchGetCommand(command, callback);
}

function mpbSystemGetStatus(callback) {
    var command = '/cgi-bin/mpb-system?get+status';

    AJAXdispatchGetCommand(command, callback);
}


//
// Generic AJAX interaction stuff
//
function AJAXdispatchGetCommand(command, callback) {
    var request = new AJAXInteraction(command, callback);

    request.doGet();
}

function AJAXInteraction(url, callback) {
    var req = init();
    req.onreadystatechange = processRequest;

    function init() {
      if (window.XMLHttpRequest) {
        return new XMLHttpRequest();
      } else if (window.ActiveXObject) {
        return new ActiveXObject("Microsoft.XMLHTTP");
      }
    }

    function processRequest () {
      if (req.readyState == 4) {
        if (req.status == 200) {
          if (callback) callback(req.responseXML);
        }
      }
    }

    this.doGet = function() {
      req.open("GET", url, true);
      req.send(null);
    }

    this.doPost = function(body) {
      req.open("POST", url, true);
      req.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      req.send(body);
    }
}
