//:IMPORT gt/gtPolling.js
//:EXPORT gtRequestSimple



function gtRequestSimple(parent,method,uri,send)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_method = method;
    this.i_uri = uri;
    this.i_send = send;
    this.i_data = null;
    this.i_getQueue = [];
    this.initial("gtRequestSimple");
}

gtRequestSimple.timeout = 10000000;

stdObject.defClass
(
    "gtRequestSimple",
    gtRequestSimple,
    tinyState,
    {

	get : function(caller) {
	    var ev = new stdEvent(
		"request",
		caller);
	    this.i_getQueue.push(ev);
	    if ( this.i_getQueue.length == 1 )
		this.eventHandler(ev);
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    if ( this.i_method != "GET" || this.i_send )
		return "doINI_REQUEST";
	    var req = this.application.getGtRequest(this.i_uri);
	    if ( !req ) {
		this.application.setGtRequest(this.i_uri,this);
		return "doINI_REQUEST";
	    }
	    req.get(this,this.i_uri);
	    return "doINI_WAIT_GET";
	},
	INI_WAIT_GET: function(ev) {
	    if ( ev.type != "return" )
		return;
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},
	INI_REQUEST : function(ev) {
	    this.callFunc = new this.application.ifClass(
		this,
		this.i_method,
		this.i_uri,
		this.i_send);
	    return "INI_REQUEST_WAIT";
	},
	INI_REQUEST_WAIT : function(ev) {
	    if ( this.i_destroyFlag ) {
		this.callFunc.destroy();
		return "doFIN_START";
	    }
	    if ( ev.type != "return" )
		return;
	    this.i_data = ev.msg;
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    stdInterval.wait(this,gtRequestSimple.timeout,"timer");
	    this.i_expire = stdInterval.now() 
		+ gtRequestSimple.timeout;
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    for ( ; this.i_getQueue.length > 0 ; ) {
		var req = this.i_getQueue.shift();
		req.source.eventHandler(
		    new stdEvent(
			"return",
			this,
			this.i_data));
	    }
	    return "doACT_TINYSTATE_CHECK1";
	},
	ACT_TINYSTATE_START : function(ev) {
	    if ( this.i_expire - stdInterval.now() <= 0 )
		return "doFIN_START";
	    return "ACT_START";
	},
	FIN_START : function(ev) {
	    this.application.removeGtRequest(this.i_uri);
	    this.i_data = null;
	    this.i_getQueue = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);
