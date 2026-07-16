//:IMPORT gt/gtPolling.js
//:IMPORT std/stdLimitSemaphore.js
//:IMPORT std/stdInterval.js
//:EXPORT gtRequest


function gtRequest(parent,method,uri,send)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_method = method;
    this.i_uri = uri;
    if ( this.i_uri.indexOf(gtPolling.URIheader,0) == 0 ) {
	this.dump("REQUEST = "+uri+" "+parent.className);
    }
    this.i_send = send;
    this.errorLockInterval = gtPolling.minInterval;
    this.initial("gtRequest");
}


gtRequest.limit = function(me) {
    var shared = me.application.shared("requestSem");
    if ( !shared )
	return 1;
    return shared.sem.limit;
};

stdObject.defClass
(
    "gtRequest",
    gtRequest,
    tinyState,
    {

	i_returnEvent : null,
	i_orgEvent : null,
	i_shared : null,
	errorLockInterval : 1000,

	priority : function() {
	    return this.parent.priority();
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_shared = this.application.refShared("requestSem");
	    if ( !this.i_shared.sem )
		this.i_shared.sem = new stdLimitSemaphore(1);
	    var req = this.application.getGtRequest(this.i_uri);
	    if ( !req )
		return "doINI_CALL_SETTING";
	    this.errorLockInterval = req.errorLockInterval * 2;
	    if ( this.errorLockInterval > gtPolling.maxInterval )
		this.errorLockInterval = gtPolling.maxInterval;
	    req.listen(this,"state");
	    return "doINI_WAIT_OLD_REQ";
	},
	INI_WAIT_OLD_REQ : function(ev) {
	    if ( this.application.getGtRequest(this.i_uri) )
		return;
	    return "doINI_CALL_SETTING";
	},
	INI_CALL_SETTING : function(ev) {
	    this.application.setGtRequest(this.i_uri,this);
	    this.i_pol = this.application.getGtPolling(this.i_uri);
	    if ( !this.i_pol ) {
		this.i_target_uri = this.i_uri;
		return "doACT_REQUEST";
	    }
	    if ( !this.i_pol.state("FIN") ) {
		if ( this.i_uri.indexOf("polling=") >= 0 ) {
		    this.i_target_uri = this.i_uri;
		    return "doACT_REQUEST";
		}
		else {
		    var info = this.i_pol.coGetPollingInfo();
		    if ( info.pollingID == 0 ) {
			this.i_startTime = stdInterval.now();
			this.i_target_uri = this.i_uri + "?polling=0:set";
			return "doACT_REQUEST";
		    }
		    this.i_startTime = stdInterval.now();
		    this.i_target_uri = this.i_uri + "?polling="+
			info.pollingID + ":set";
		    return "doACT_REQUEST";
		}
	    }
	    this.i_pol.listen(this,"destroy");
	    return "doINI_POL_DESTROY_WAIT";
	},
	INI_POL_DESTROY_WAIT : function(ev) {
	    if ( !this.i_pol.state("ZOM") )
		return;
	    this.i_pol = null;
	    return "doINI_CALL_SETTING";
	},
	ACT_REQUEST : function(ev) {
	    return this.i_shared.sem.get(
		this,
		"doACT_REQUEST_RET");
	},
	ACT_REQUEST_RET : function(ev) {
	    this.i_reqStart = stdInterval.now();
	    return "doACT_REQUEST_BODY";
	},
	ACT_REQUEST_BODY : function(ev) {
	    var tout = 2000*1000;
	    if ( this.i_pol ) {
		tout = this.i_pol.getLatency()*10;
		if ( tout > 20*1000*1000 )
		    tout = 20*1000*1000
		if ( tout < 2*1000*1000 )
		    tout = 2*1000*1000;
	    }
	    if ( this.i_method == "GET" )
		stdInterval.wait(this,tout,"timeout");
	    this.i_ifC = new this.application.ifClass(this,this.i_method,
						   this.i_target_uri,this.i_send);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( this.i_destroyFlag && this.i_ifC ) {
		this.i_ifC.destroy();
		this.i_ifC = null;
		return;
	    }
	    if ( ev.type != "return" ) {
		if ( ev.type == "timeout" ) {
		    this.i_ifC.destroy();
		    return "doACT_REQUEST_BODY";
		}
		return;
	    }
	    this.i_shared.sem.release();
	    this.i_shared.sem.value(
		stdInterval.now() - this.i_reqStart);
	    
	    this.i_ifC = null;
	    this.i_orgEvent = ev;
	    if ( ev.msg.status == "ERROR" ) {
		this.i_returnEvent = ev.copy();
		return "doFIN_REQUEST_START";
	    }
	    var jmsg;
	    try {
		jmsg = JSON.parse(ev.msg.message);
	    }
	    catch ( err ) {
		this.dump("gtRequest JSON ERROR "+err);
		this.dump(ev.msg.message);
		ev.msg.status = "ERROR";
		ev.msg.status2 = 401;
		ev.msg.message = "JSON PARSE ERROR "+ev.msg.message;
		this.i_returnEvent = ev.copy();
		return "doFIN_REQUEST_START";
	    }
	    ev.msg.message = jmsg;
	    if ( ev.msg.message.polling.agent.indexOf(
		gtPolling.agent,0) != 0 ) {
		this.i_returnEvent = null;
		this.i_orgEvent = ev;
		return "doFIN_REQUEST_START";
	    }
	    this.i_returnEvent = ev;
	    this.i_returnEvent.msg.latency = stdInterval.now()
		- this.i_startTime;
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    this.i_returnEvent.source = this;
	    if ( !this.i_pol ) {
		this.i_pol = this.application.getGtPolling(this.i_uri);
		if ( !this.i_pol )
		    this.i_pol = new gtPolling(this.application);
	    }
	    this.i_pol.requestResult(this.i_returnEvent);
	    return "doFIN_REQUEST_START";
	},
	FIN_REQUEST_START : function(ev) {
	    this.i_orgEvent.source = this;
	    this.parent.eventHandler(this.i_orgEvent);
	    if ( this.i_orgEvent.msg.status == "OK" )
		return "doFIN_REQUEST_REMOVE_APP";
	    return "doFIN_ERROR_LOCK";
	},
	FIN_ERROR_LOCK : function(ev) {
	    stdInterval.wait(this,this.errorLockInterval,"return");
	    return "FIN_ERROR_LOCK_RET";
	},
	FIN_ERROR_LOCK_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    return "doFIN_REQUEST_REMOVE_APP";
	},
	FIN_REQUEST_REMOVE_APP : function(ev) {
	    this.application.removeGtRequest(this.i_uri);
	    this.i_shared = null;
	    this.application.unrefShared("requestSem");
	    return "doFIN_TINYSTATE_START";
	},
    }
);
