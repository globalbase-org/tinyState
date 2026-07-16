//:IMPORT std/stdObject.js
//:EXPORT stdSystem


function stdSystem()
{
    stdObject.apply(this);
}



stdSystem.version = function()
{
	var userAgent = window.navigator.userAgent.toLowerCase();
	var br = "";
	var vs = "";

	if (userAgent.indexOf('opera') != -1) {
	  br = 'opera';
	} else if (userAgent.indexOf('msie') != -1) {
	  br = 'ie';
	  if (appVersion.indexOf("msie 6.") != -1) {
	    vs = 'ie6';
	  } else if (appVersion.indexOf("msie 7.") != -1) {
	    vs = 'ie7';
	  } else if (appVersion.indexOf("msie 8.") != -1) {
	    vs = 'ie8';
	  } else if (appVersion.indexOf("msie 9.") != -1) {
	    vs = 'ie9';
	  }
	} else if (userAgent.indexOf('chrome') != -1) {
	  br = 'chrome';
	} else if (userAgent.indexOf('safari') != -1) {
	  br = 'safari';
	} else if (userAgent.indexOf('gecko') != -1) {
	  br = 'gecko';
	}
	var ret = {
		"browser" : br,
		"version" : vs
	};
	return ret;
}


stdObject.defClass
(
    "stdSystem",
    stdSystem,
    stdObject
);


