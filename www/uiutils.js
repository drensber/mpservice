/* 
 * uiutils.js
 *
 * Javascript functions that are specific to the mpservice
 * web user interface.. 
 *
 */

// Global state variables (better organized as objects?...
// after I learn JS a little better, maybe).

// Variables representing basic UI attributes
var haveTouchScreen = true;
var smallScreen = false;

var parentFolderId;
var currentFolderName;
var currentFolderId;

var lastElementVisibilityTimeout = null;
var lastElementMadeVisible = null;

var mostRecentPlaylistXmlResponse = null;
var navigatorStack = [];
var currentBrowserScrollPosition = 0;

var current_id = 0;
var library_change_counter = 0;
var playlist_change_counter = 0;
var current_paused = false;
var back_button_last_pressed = 0;
var browserDiv;
var mpsCookie;

var platformGlobal = "unknown";
var versionStringGlobal = "";

var configStatusDivBuilder;

var mostRecentRandomXmlResponse = null;

// Triggered by UI commands.

// Player control
function doPlay() {
    updatePausedDisplay(false);
    updatePlaylistDisplay(mostRecentPlaylistXmlResponse);
    mpsPlay();
}

function doPause() {
    updatePausedDisplay(true);
    updatePlaylistDisplay(mostRecentPlaylistXmlResponse);
    mpsPause();
}

function doForward() {
    mpsPlayNext( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');  
	} );
}

function doBackward() {
    var date = new Date();
    var new_time_ms = date.getTime();
    var time_diff_ms = new_time_ms - back_button_last_pressed; 
    back_button_last_pressed = new_time_ms;

    if (time_diff_ms < 2000 ) {
        mpsPlayPrevious( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	} );
    }
    else {
        mpsPlayAbsolute( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	},
        current_id);
    }
}

function doPlayAbsolute(param1) {
    mpsPlayAbsolute( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	},
	param1);
}

// Browser Navigation
function doNavigateInit() {
    currentBrowserScrollPosition = 0;    
    navigatorStack = [];

    if (!mpsCookie.navigationType || mpsCookie.navigationType == "tag") {
        document.getElementById("navigationTypeRadioButtonTag").checked = true;
        mpsGetLibrary(updateBrowserDisplay, 'artists');
    }
    else {
        document.getElementById("navigationTypeRadioButtonFilename").checked = true;
        mpsGetLibrary(updateBrowserDisplay, 'root');
    }
}

function doNavigateForward(parentId, id) {

    parentScrollPosition = browserDiv.scrollTop;
    navigatorStack.push([parentId, parentScrollPosition]);
    mpsGetLibrary(updateBrowserDisplay, 'path', id);
}

function doNavigateBack() {
    parent = navigatorStack.pop();
    parentId = parent[0];
    currentBrowserScrollPosition = parent[1];

    if ((parentId == 0) &&
       document.getElementById('navigationTypeRadioButtonSearch')
        .checked == true) {
        doSimpleSearch();
    }
    else if ((parentId == 0) &&
             document.getElementById('navigationTypeRadioButtonRandom')
             .checked == true) {
        doShowSimpleRandomSuggest();
    }
    else {
        mpsGetLibrary(updateBrowserDisplay, 'path', parentId);
    }    
}


// Playlist control
function doAddOneToPlaylist(param1) {
    mpsAddOneToPlaylist( function() {
	    mpsGetPlaylist(updatePlaylistDisplayAndScrollToEnd, 'all');
	},
	param1);
}

function doAddAllToPlaylist(param1) {
    mpsAddAllToPlaylist( function() {
	    mpsGetPlaylist(updatePlaylistDisplayAndScrollToEnd, 'all');
	},
	param1);
}

function doRemoveOneFromPlaylist(param1) {
    mpsRemoveOneFromPlaylist( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	},
	param1);
}

function doRemoveAllFromPlaylist() {
    mpsRemoveAllFromPlaylist( function() {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	} );	    	    
}

// Search
function doInitSimpleSearch() {
    currentBrowserScrollPosition = 0;
    navigatorStack = [];

    doSimpleSearch();
}

function doSimpleSearch() {
    var searchText = document.getElementById("navigationTypeSearchText");
    var searchString = encodeURI(searchText.value);
    
    mpsSimpleSearch(searchString, updateBrowserDisplay);
}

function selectSearchText() {
    var searchText = document.getElementById("navigationTypeSearchText");
    document.getElementById('navigationTypeRadioButtonSearch')
        .checked = true; 
    searchText.focus();
    searchText.select();
}

// Random
function doInitShowSimpleRandomSuggest() {
    currentBrowserScrollPosition = 0;
    navigatorStack = [];

    doShowSimpleRandomSuggest();
}

function doShowSimpleRandomSuggest() {
    document.getElementById("navigationTypeRadioButtonRandom").checked = true;
    if (mostRecentRandomXmlResponse == null) {
        mpsSimpleRandomSuggest(updateBrowserDisplay);
    }
    else {
        updateBrowserDisplay(mostRecentRandomXmlResponse); 
    }
}

function doNewSimpleRandomSuggest() {
    mostRecentRandomXmlResponse = null;
    doShowSimpleRandomSuggest();
}

// Periodic
function doPeriodicUiUpdate() {
    mpsPlayerGetSummary( function(xml_response) {
        configParams = xml_response.getElementsByTagName("param");       

        var nameValuePairs = {};
        
        for (i = 0; i < configParams.length; i++) {
            nameValuePairs[configParams[i].attributes[0].nodeValue] =
                configParams[i].attributes[1].nodeValue;
        }


	    /* Play/Pause button */
	var state = nameValuePairs["state"];
	var paused = (state=="STATE_PAUSED" ? true : false);

	if (paused != current_paused) {
	    updatePausedDisplay(paused);
	    updatePlaylistDisplay(mostRecentPlaylistXmlResponse);
	}
		
	/* Playlist */
	var id = nameValuePairs["current_playlist_entry_id"];

	var old_pcc = playlist_change_counter;
	playlist_change_counter=nameValuePairs["playlist_change_counter"];
	    
	// only request and update the playlist if necessary
	if ((id != current_id) ||
	    (old_pcc != playlist_change_counter)) {
	    mpsGetPlaylist(updatePlaylistDisplay, 'all');
	}
	    
	/* Browser */
	var old_mcc = library_change_counter;
	library_change_counter = nameValuePairs["library_change_counter"];

        librarydbd_state = nameValuePairs["librarydbd_state"];
	if (old_mcc != library_change_counter ||
            librarydbd_state == "LIBRARYDBD_STATE_CREATINGDB") {
            currentBrowserScrollPosition = browserDiv.scrollTop;
	    mpsGetLibrary(updateBrowserDisplay, 'path', 
			  currentFolderId);
	}
	} );
}


function updatePlaylistDisplayAndScrollToEnd(xml_response) {
    playlistDiv = document.getElementById("playlistDiv");
    updatePlaylistDisplay(xml_response);
    playlistDiv.scrollTop = playlistDiv.scrollHeight;
}

function updatePlaylistDisplay(xml_response) {
    var currently_html = "";
    var html = "";
    var playlist_header = xml_response.getElementsByTagName("entries");
    var playlist_entries = xml_response.getElementsByTagName("entry");
    var playlistRemoveAllButton = 
	document.getElementById("playlistRemoveAllButton");

    current_id = playlist_header[0].attributes[1].nodeValue;
    
    mostRecentPlaylistXmlResponse = xml_response;

    currently_html += '<font size +=2><center><b>CURRENTLY '
	+ (current_paused ? 'PAUSED' : 'PLAYING')
	+ '</b><br>';

    if (current_id == -102) {
	currently_html += 
	    '<br><font size +=1><center>'
	    + '<i>NOTHING (END OF PLAYLIST)</i>'
	    + '</center></font>';
    }

    if (playlist_entries.length > 0) {

	for (var i = 0; i < playlist_entries.length; i++) {
	    var entryName = playlist_entries[i].attributes[4].nodeValue;
	    var entryId = playlist_entries[i].attributes[0].nodeValue;
	    
	    if (entryId == current_id) {
		// Should probably really have a separate 
		// updateCurrently(xml_resp) function rather than 
		// tying it together with this.
		var currentArtist = playlist_entries[i].attributes[2].nodeValue;
		var currentAlbum = playlist_entries[i].attributes[3].nodeValue;
		
		currently_html += '<i>"' + entryName + '"</i><br>'
		    + '<b>' + currentArtist + '</b><br>'
		    + currentAlbum + '<br>'
		    + '<br></center></font>';
	    }

	    html +="<span id=\"playlistSpan"+i+"\" ";
	    if (!haveTouchScreen && !(entryId == current_id)) {
		html +=
		    "onMouseOver="
		    + "\"changeVisibility('playlistButtonSpan" + i
		    + "', 'visible'); \" "
		    + "onMouseOut="
		    + "\"delayedChangeVisibility('playlistButtonSpan" + i
		    + "', 'hidden'); \" ";
	    }	    
	    html +=">";

	    if (entryId == current_id) {
		html += "<b>" +  entryName + " (PLAYING)</b>";
	    }
	    else {
		html += "<span id=\"playlistButtonSpan" + i + "\"";

		if (!haveTouchScreen) {
		    html += "style = \"visibility:hidden\"";
		}

		html += ">";
 
		html += "<button onClick="
		    + "\"doPlayAbsolute(" + entryId + "); \" "
                    + "style = \" position:relative; top:2; \"> "
                    + "<img src='play.png' height=\"13\" width=\"13\"/> </button>";

		html += "<input type=\"button\" onClick="
		    + "\"doRemoveOneFromPlaylist(\'" + entryId + "\'); \" "
		    + "value=\"-\"> ";

		html += "</span>";

		html += entryName;
	    }

	    html += "</span><br>\n";
	}
	playlistRemoveAllButton.style.visibility = "visible";
    }
    else {
	currently_html += '<br><font size +=1><center>' 
	    + '<i>NOTHING (PLAYLIST IS EMPTY)</i>'
	    + '</center></font>';
        html += "<i>PLAYLIST IS EMPTY</i>";
	playlistRemoveAllButton.style.visibility = "hidden";
    }

    document.getElementById("currentlyPlayingDiv").innerHTML = currently_html;
    document.getElementById("playlistDiv").innerHTML = html;
}

function updatePausedDisplay(state) {
    var button = document.getElementById("playPauseButton");
    var img = document.getElementById("playPauseButtonImg");

    // Update the global state variable
    current_paused = state;
    
    // If state is true, then we're paused, so make
    // the button say and do "play".
    if (state == true) {
	img.src = 'play.png';
	button.onclick = function() { doPlay(); };
    }
    else {
	img.src = 'pause.png';
	button.onclick = function() { doPause(); };	
    }
}

function delayedChangeVisibility(element_id, visibility)  {
    lastElementVisibilityTimeout
	= setTimeout( function() {
		changeVisibility(element_id, visibility);
	    },
	    500);
}

function changeVisibility(element_id, visibility) {
    var element = document.getElementById(element_id);

    if (visibility == "visible") {
	if (lastElementMadeVisible != null) {
	    lastElementMadeVisible.style.visibility = "hidden";
	    clearTimeout(lastElementVisibilityTimeout);
	}
	lastElementMadeVisible = element;
    }

    element.style.visibility = visibility;
}

function updateBrowserDisplay(xml_response) {
    var trackbrowser_html = "";
    var trackbrowser_elements = xml_response.getElementsByTagName("element");
    var header = xml_response.getElementsByTagName("elements");
    var folderCount = 0, trackCount = 0;    
    
    browserDiv = document.getElementById("trackBrowserDiv");
    parentFolderId = (header[0].attributes[1].nodeValue);
    currentFolderName = (header[0].attributes[2].nodeValue);
    currentFolderId = (header[0].attributes[3].nodeValue);

    if (currentFolderId == 0 && (currentFolderName.match(/.*random.*/) != null)) {
        mostRecentRandomXmlResponse = xml_response;
    }

    if (trackbrowser_elements.length == 0) {
        if (document.getElementById('navigationTypeRadioButtonSearch')
            .checked == true)  {
            search = document.getElementById("navigationTypeSearchText").value;
            //console.log("search = \"" + search + "\"");
            if (search == "") {
                trackbrowser_html = 
                    "<i>TYPE SOMETHING IN THE BOX LABELED \"Search\"</i>";
            }
            else {
                trackbrowser_html = 
                    "<i>NO SEARCH RESULTS FOUND FOR \"" + search + "\"</i>";
            }
        }  
    }
    else {
        for (var i = 0; i < trackbrowser_elements.length; i++) {
	    var type = trackbrowser_elements[i].attributes[0].nodeValue;	
	    var name = trackbrowser_elements[i].attributes[1].nodeValue;
	    var id = trackbrowser_elements[i].attributes[2].nodeValue;

            trackbrowser_html += "<span id=\"browserButtonSpan" + i + "\"";

	    if (!haveTouchScreen) {
	        trackbrowser_html += "style = \"" +
                    "visibility:hidden; " +
                    "\" ";
	    }

            trackbrowser_html += ">" + "<button onClick=";

            if (type == "FOLDER") {
	        trackbrowser_html +=
                "\"doNavigateForward('" + currentFolderId + "', '" + id + "'); \" "
                    + "style = \" position:relative; top:-1; \"> "
                    + "<img src='navigateforward.png' height=\"13\" width=\"13\"/> "
                    + "</button>";
	        folderCount++;
	    }
	    else {
	        trackbrowser_html +=
		"\"doAddOneToPlaylist(\'" + id + "\'); \">" + " + </button>";
	        trackCount++;
	    }
	    trackbrowser_html += "</span>\n";

            
	    trackbrowser_html +=
	    "<span id=\"browserSpan"+i+"\" ";
	    if (!haveTouchScreen) {
	        trackbrowser_html +=
		"onMouseOver="
		    + "\"changeVisibility('browserButtonSpan" + i
		    + "', 'visible'); \" "
		    + "onMouseOut="
		    + "\"delayedChangeVisibility('browserButtonSpan" + i 
		    + "', 'hidden'); \" ";
	    }

	    trackbrowser_html += "style = \"" +
                "display: inline-block; width: 85%; overflow: hidden; " +
                "\">";
 
	    if (type == "FOLDER") {
	        trackbrowser_html += name;
	    }
	    else {
	        trackbrowser_html += "<i>" + name + "</i>";
	    }
	    trackbrowser_html += "</span><br>";
 

        }
        
        var folderNavigatorDiv = document.getElementById("folderNavigatorDiv");
        var folderNavigatorTextSpan = 
	    document.getElementById("folderNavigatorText");
        var folderNavigatorAddAllButton =
	    document.getElementById("folderNavigatorAddAllButton");
        
        // Set up the folder navigator header
        folderNavigatorTextSpan.innerHTML =
	    "<i><b>" + currentFolderName + "</b></i>";
        
        folderNavigatorBackButton.style.visibility =
	    ((currentFolderId == 3000000 || currentFolderId == 1000000
              || currentFolderId == 0) 
	     ? "hidden" : "visible");
        folderNavigatorText.style.visibility =
	    ((currentFolderId == 3000000 || currentFolderId == 1000000
              || currentFolderId == 0) 
	     ? "hidden" : "visible");
        folderNavigatorAddAllButton.style.visibility =
	    ((trackCount == 0 || currentFolderId == 0) ? "hidden" : "visible");
        
        if (trackCount > 0) {
	    folderNavigatorAddAllButton.onclick = function() 
	    { doAddAllToPlaylist
	      (currentFolderId); };
        }
    }
    
    browserDiv.innerHTML = trackbrowser_html;
    //console.log("navigatorStack = " + navigatorStack);
    //console.log("currentBrowserScrollPosition = " + currentBrowserScrollPosition);


    browserDiv.scrollTop = currentBrowserScrollPosition;
}

function updateNavigationType() {
    currentBrowserScrollPosition = 0;
    navigatorStack = [];

    tagButton = document.getElementById("navigationTypeRadioButtonTag");
    fnButton = document.getElementById("navigationTypeRadioButtonFilename");
    if (tagButton.checked == true) {
        mpsGetLibrary(updateBrowserDisplay, 'artists');
        mpsCookie.navigationType="tag";
    }
    else if (fnButton.checked == true) {
        mpsGetLibrary(updateBrowserDisplay, 'root');
        mpsCookie.navigationType="filesystem";
    }
    mpsCookie.store(100);
}

function mainView() {
    browserDiv = document.getElementById("trackBrowserDiv");

    mainDiv.style.display = "block";
    configStatusDiv.style.display = "none";
    browserDiv.scrollTop = currentBrowserScrollPosition;
}

function configView() {
    browserDiv = document.getElementById("trackBrowserDiv");

    configStatusDivBuilder.init();

    currentBrowserScrollPosition = browserDiv.scrollTop;
    mainDiv.style.display = "none";
    configStatusDiv.style.display = "block";

    // to get number of artists/albums/songs
    mpsLibraryGetStatus(updateStatusDisplay);

    //Really should be conditionalized for platform != standalone
    mpbSystemGetStatus(updateStatusDisplay);

    if (platformGlobal == "standalone") {
        mpsGetConfig(["webserver_port", "media_dir__0"], 
                     updateConfigDisplay);
    }    
}



function buildConfigDivSection(section) {
    divName = section[0];
    header = section[1];
    content = section[2];

    containerDiv = document.getElementById(divName);

    containerDiv.innerHTML 
        += '<span id="' + divName + header[0] + 'Label"' +
        'style="position: absolute; left: ' + header[2] + '%">' +
        header[1] +
        '</span> <br>';

    for (i=0; i < content.length; i++) {
        param = content[i];
        containerDiv.innerHTML 
            += '<span id="' + divName + '_' + param[0] + 'Label"' +
            'style="position: absolute; left: ' + param[2] + '%">' +
            param[1] +
            '</span>' +
            '<span id="' + divName + '_' + param[0] + 'Value"' +
            'style="position: absolute; left: ' + param[3] + '%">' +
            '</span> <br>';        
    }
}

function ConfigStatusDivBuilder() {
    var initialized = false;

    this.init=init;
    function init() {
        if (! initialized) {
            buildConfigStatusDiv();
            initialized=true;
        }
    }

    function buildConfigStatusDiv() {
        configDivConfig = document.getElementById("configDivConfig");
        /* status */
        section = ['configDivStatus',
                   ['software', '<b><u>Software:</u></b>', 3],
                   [['platformType', '<b>Platform:</b>', 5, 55],
                    ['version', '<b>Version:</b>', 5, 55]]];
        buildConfigDivSection(section);
        
        section = ['configDivStatus',
                   ['library', '<b><u>Library:</u></b>', 3],
                   [['numSongs', '<b>Number of Songs:</b>', 5, 55],
                    ['numArtists', '<b>Number of Artists:</b>', 5, 55],
                    ['numAlbums', '<b>Number of Albums:</b>', 5, 55]]];
        buildConfigDivSection(section);

        if (platformGlobal != "standalone") {        
            section = ['configDivStatus',
                       ['ethernet', '<b><u>Network:</u></b>', 3],
                       [['ethernetIpAddress', '<b>IP Address:</b>', 5, 55],
                        ['ethernetBroadcast', '<b>IP Broadcast:</b>', 5, 55],
                        ['ethernetNetmask', '<b>IP Netmask:</b>', 5, 55],
                        ['ethernetGateway', '<b>IP Gateway:</b>', 5, 55],
                        ['hostname', '<b>Hostname:</b>', 5, 55]]];
            buildConfigDivSection(section);
        }
        /* configuration */
        if (platformGlobal == "standalone") {
            section = ['configDivConfig',
                       ['library', '<b><u>Library:</u></b>', 3],
                       [['mediaDir', '<b>Music source:</b>', 5, 55]]];
            buildConfigDivSection(section);
            
            section = ['configDivConfig',
                       ['network', '<b><u>HTTP Server:</u></b>', 3],
                       [['serverPort', '<b>Port:</b>', 5, 55]]];
            buildConfigDivSection(section);
        }
    }
}

function updateStatusDisplay(xml_response) {
    numArtistsValue = document.getElementById("configDivStatus_numArtistsValue");
    numAlbumsValue = document.getElementById("configDivStatus_numAlbumsValue");
    numSongsValue = document.getElementById("configDivStatus_numSongsValue");
    platformTypeValue = document.getElementById("configDivStatus_platformTypeValue");
    versionValue = document.getElementById("configDivStatus_versionValue");
    configParams = xml_response.getElementsByTagName("param");

    var nameValuePairs = {};
    var value, v1, v2, v3;
    
    for (i = 0; i < configParams.length; i++) {
        nameValuePairs[configParams[i].attributes[0].nodeValue] =
            configParams[i].attributes[1].nodeValue;
    }
    
    if (value = nameValuePairs["artist_count"]) 
        numArtistsValue.innerHTML = value;
    if (value = nameValuePairs["album_count"]) 
        numAlbumsValue.innerHTML = value;
    if (value = nameValuePairs["title_count"]) 
        numSongsValue.innerHTML = value;

    platformTypeValue.innerHTML = platformGlobal;
    versionValue.innerHTML = versionStringGlobal;

    if (platformGlobal != "standalone") {        
        ethernetIpAddressValue 
            = document.getElementById("configDivStatus_ethernetIpAddressValue");
        ethernetBroadcastValue 
            = document.getElementById("configDivStatus_ethernetBroadcastValue");
        ethernetNetmaskValue 
            = document.getElementById("configDivStatus_ethernetNetmaskValue");
        ethernetGatewayValue 
            = document.getElementById("configDivStatus_ethernetGatewayValue");
        hostnameValue 
            = document.getElementById("configDivStatus_hostnameValue");
        
        if (value = nameValuePairs["ip_address_eth0"])
            ethernetIpAddressValue.innerHTML= value;
        if (value = nameValuePairs["ip_broadcast_eth0"])
            ethernetBroadcastValue.innerHTML= value;
        if (value = nameValuePairs["ip_netmask_eth0"])
            ethernetNetmaskValue.innerHTML= value;
        if (value = nameValuePairs["ip_gateway_eth0"])
            ethernetGatewayValue.innerHTML= value;
        if (value = nameValuePairs["hostname"])
            hostnameValue.innerHTML= value;
    }
}

function updatePlatformInfo(xml_response) {
    params = xml_response.getElementsByTagName("param");
    var value, v1, v2, v3;
    var nameValuePairs = {};

    for (i = 0; i < params.length; i++) {
        nameValuePairs[params[i].attributes[0].nodeValue] =
            params[i].attributes[1].nodeValue;
    }
    if ((v1 = nameValuePairs["version__major__"]) &&
        (v2 = nameValuePairs["version__minor__"]) &&
        (v3 = nameValuePairs["version__release__"]))
        versionStringGlobal = v1 + "." + v2 + "." + v3;

    if (value = nameValuePairs["platform"]) 
        platformGlobal = value;
}

function updateConfigDisplay(xml_response) {
    mediaDirValue = document.getElementById("configDivConfig_mediaDirValue");
    serverPortValue = document.getElementById("configDivConfig_serverPortValue");
    configParams = xml_response.getElementsByTagName("param");

    var nameValuePairs = {};
    var value;

    for (i = 0; i < configParams.length; i++) {
        nameValuePairs[configParams[i].attributes[0].nodeValue] =
            configParams[i].attributes[1].nodeValue;
    }
    
    if (value = nameValuePairs["media_dir__0"])
        mediaDirValue.innerHTML = value;
    if (value = nameValuePairs["webserver_port"])
        serverPortValue.innerHTML = value;
}