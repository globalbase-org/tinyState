
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js
//:IMPORT if/ifAjaxDebug.js
//:IMPORT std/stdSystem.js

function ifAjax(
    parent,
    method,
    uri,
    send)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_httpObj = null;
    this.i_method = method;
    this.i_uri = uri;
    if ( !this.i_uri ) {
	throw new Error("undefined uri ifAjax");
    }
    if ( this.i_uri.indexOf(gtPolling.URIheader,0) == 0 ) {
	this.dump("AJAX = "+uri+" "+parent.className);
    }
    this.i_send = send;


    this.initial("ifAjax");
}

stdObject.defClass
(
    "ifAjax",
    ifAjax,
    tinyState,
    {

	i_version : {},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


	INI_START : function(ev) {
	    this.i_version = stdSystem.version();
	    if ( this.i_version.browser != "-" )
		return "doINI_CALL";
	    var share = this.application.refShared("ifAjax");
	    if ( share.ref == 1 ) {
		share.sem = new stdSemaphore(5);
		share.count = 0;
	    }
	    return "doINI_SEM_GET";
	},
	INI_SEM_GET : function(ev) {
	    return this.application.refShared("ifAjax").sem.get(this,"doINI_SEM_FINISH");
	},
	INI_SEM_FINISH : function(ev) {
	    var share = this.application.refShared("ifAjax");
	    share.count ++;
	    return "doINI_CALL";
	},

	INI_CALL : function(ev) {
	    try {
		this.i_httpObj = new XMLHttpRequest();
	    } catch(e) {
		try {
		    this.i_httpObj = new ActiveXObject("Msxml2.XMLHTTP");
		} catch(e) {
		    try {
			this.i_httpObj = new ActiveXObject("Microsoft.XMLHTTP");
		    } catch(e) {
			parent.eventHandler(
			    new stdEvent(
				"return",
				this,
				{
				    status : "ERROR",
				    status2 : 401,
				    message : "ajax is not supported(1)..." + e
				}));
			return "doFIN_START";
		    }
		}
	    }
	    if ( this.i_httpObj == null ) {
		parent.eventHandler(
		    new stdEvent(
			"return",
			this,
			{
			    status : "ERROR",
			    message : "ajax is not supported(2)..."
			}));
		return "doFIN_START";
	    }
	    var thisObj = this;
	    this.i_httpObj.onreadystatechange = function() {
		if ( thisObj.i_httpObj == null )
		    return;
		if ( thisObj.i_httpObj.readyState != 4 )
		    return;
		var status = "OK";
		var status2 = thisObj.i_httpObj.status;
		if ( thisObj.i_httpObj.status != 200 ) {
		    status = "ERROR";
		    thisObj.dump("Ajax ERROR = "+thisObj.i_uri+" => "+
				 thisObj.i_httpObj.status);
		    if ( thisObj.i_httpObj.status == 0 ) 
			status2 = 401;
		}
/*
if ( thisObj.i_uri.indexOf("/soi/gt/polling/") != 0 )
		  thisObj.dump("Ajax Handler = "+thisObj.i_uri+" => "+
			     status+" "+status2);
*/

		thisObj.eventHandler(
		    new stdEvent(
			"return",
			this,
			{
			    status : status,
			    status2 : status2,
			    message : thisObj.i_httpObj.responseText
			}));
	    }
	    try {
//thisObj.dump("Ajax SEND = "+thisObj.i_uri);
		this.i_debug = new ifAjaxDebug(this,this.i_uri);
		this.i_httpObj.open(this.i_method,this.i_uri,true);
		this.i_httpObj.send(this.i_send);
	    }
	    catch ( err ) {
		this.parent.eventHandler(
		    new stdEvent(
			"return",
			this,
			{
			    status : "ERROR",
			    status2 : 401,
			    message : err
			}));
		return "doFIN_START";
	    }
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( this.destroyFlag ) {
		this.i_httpObj.abort();
		this.i_httpObj = null;
		this.parent.eventHandler(
		    new stdEvent(
			"return",
			this,
			{
			    status : "ERROR",
			    status2 : 401,
			    message : "aborted",
			}));
		return "doFIN_START";
	    }
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    if ( this.i_debug ) {
		this.i_debug.destroy();
		this.i_debug = null;
	    }
	    if ( this.i_version.browser != "-" )
		return "doFIN_TINYSTATE_START";
	    var share = this.application.refShared("ifAjax");
	    share.count --;
	    share.sem.release();
	    this.application.unrefShared("ifAjax");

	    return "doFIN_TINYSTATE_START";
	},
    }
);

