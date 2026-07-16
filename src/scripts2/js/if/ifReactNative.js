
//:IMPORT std/stdEvent.js
//:IMPORT std/stdObject.js
//:IMPORT ts/tinyState.js

function ifReactNative(
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
	throw new Error("undefined uri ifReactNative");
    }
    if ( this.i_uri.indexOf(gtPolling.URIheader,0) == 0 ) {
	this.dump("ReactNative = "+uri+" "+parent.className);
    }
    this.i_send = send;


    this.initial("ifReactNative");
}

stdObject.defClass
(
    "ifReactNative",
    ifReactNative,
    tinyState,
    {

	i_recieve : null,

	recv : function(message) {
	    this.i_recieve = message;
	    this.wakeup();
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


	INI_START : function(ev) {
	    this.i_share = this.application.refShared("ifReactNative");
	    if ( this.i_share.ref == 1 ) {
		this.i_share.sem = new stdSemaphore(5);
		this.i_share.count = 0;
		this.i_share.que = [];
	    }
	    return "doINI_SEM_GET";
	},
	INI_SEM_GET : function(ev) {
	    return this.i_share.sem.get(this,"doINI_SEM_FINISH");
	},
	INI_SEM_FINISH : function(ev) {
	    var i;
	    for ( i = 0 ; i < this.i_share.que.length ; i ++ )
		if ( !this.i_share.que[i] )
		    break;
	    this.i_seq = i;
	    this.i_share.que[i] = this;
	    this.i_share.count ++;
	    return "doINI_CALL";
	},
	INI_CALL : function(ev) {
	    this.i_recieve = null;
	    window.ReactNativeWebView.postMessage(
		JSON.stringify(
		    {
			seq : this.i_seq,
			method : this.i_method,
			uri : this.i_uri,
			body : this.i_send
		    }
		));
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( this.i_recieve === null )
		return;
		/*
		  {
			    status : OK,ERROR,
			    status2 : status number of HTTP,
			    message : 
			}
		*/
	    this.parent.eventHandler(new stdEvent(
			"return",this,this.i_recieve));
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    this.i_share.count --;
	    this.i_share.que[this.i_seq] = null;
	    this.i_share.sem.release();
	    this.application.unrefShared("ifReactNative");

	    return "doFIN_TINYSTATE_START";
	},
    }
);

