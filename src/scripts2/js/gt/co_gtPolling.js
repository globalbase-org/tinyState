
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js
//:EXPORT co_gtPolling


function co_gtPolling(parent) {
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_latency = gtPolling.minInterval;
    this.initial("co_gtPolling");
};



stdObject.defClass
(
    "co_gtPolling",
    co_gtPolling,
    tinyState,
    {

	i_req : false,
	i_returnEvent : null,
	i_shared : null,
	i_reqStart : 0,

	i_pollingInvoke : function(ev) {
	    var data = ev.msg.message;
	    var targetList = data.data.updated;
	    var req = false;

	    var info = this.parent.coGetPollingInfo();


	    for ( ix in targetList ) {
		var el = targetList[ix];
		var target = this.application.getGtItem(gtItem.uri(el.uri),true);
		if ( target )
		    target.notice("updated",el);
		req = true;
	    }
	    var targetList = data.data.destroyed;
	    for ( ix in targetList ) {
		var el = targetList[ix];
                var target = this.application.getGtItem(gtItem.uri(el.uri),true);
		if ( target )
		    target.destroy();
		req = true;
	    }
	    return req;
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

	    this.i_info = this.parent.coGetPollingInfo();
	    if ( this.i_info.pollingID == 0 ) {
		this.i_returnEvent = new stdEvent(
		    "return",
		    this,
		    {
			status : "ERROR",
			message : "not setup pollingID"
		    });
		return "doFIN_START_SEND_EVENT";
	    }
	    this.i_startTime = stdInterval.now();
	    return "doINI_LOCK_REQUEST";
	},
	INI_LOCK_REQUEST : function(ev) {
	    return this.i_shared.sem.get(
		this,
		"doINI_LOCK_OK");
	},
	INI_LOCK_OK : function(ev) {
	    this.i_reqStart = stdInterval.now();
	    new this.application.ifClass(this,"GET",this.i_info.pollingURI);
	    this.i_info = null;
	    return "ACT_START";
	},
	ACT_TINYSTATE_START : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.i_shared.sem.release();
	    this.i_shared.sem.value(
		stdInterval.now() - this.i_reqStart);

	    if ( ev.msg.status == "ERROR" ) {
		this.i_returnEvent = ev;
		return "doFIN_START_SEND_EVENT";
	    }
	    try {
		ev.msg.message = JSON.parse(ev.msg.message);
	    }
	    catch ( err ) {
		ev.msg.status = "ERROR";
		ev.msg.status2 = 401;
		ev.msg.message = "JSON parse error";
		this.i_returnEvent = ev;
		return "doFIN_START_SEND_EVENT";
	    }
	    if ( ev.msg.message.polling.agent.indexOf(
		gtPolling.agent,0) != 0 ) {
		this.i_returnEvent = ev;
		this.i_returnEvent.msg.status = "ERROR";
		this.i_returnEvent.msg.status2 = 404;
		this.i_returnEvent.msg.message = "different agent";
		return "doFIN_START_SEND_EVENT";
	    }
	    this.i_req = this.i_pollingInvoke(ev);
	    this.i_returnEvent = ev;
	    this.i_returnEvent.msg.latency = stdInterval.now()
		- this.i_startTime;
	    return "doFIN_START_SEND_EVENT";
	},
	FIN_START : function(ev) {
	    this.i_returnEvent = new stdEvent(
		"return",
		this,
		{
		    status : "ERROR",
		    message : "destroyed"
		});
	    return "doFIN_START_SEND_EVENT";
	},
	FIN_START_SEND_EVENT : function(ev) {
	    if ( this.i_returnEvent ) {
		this.i_returnEvent.source = this;
		this.parent.coResult(this.i_returnEvent,this.i_req);
	    }
	    this.i_req = false;

	    this.i_shared = null;
	    this.application.unrefShared("requestSem");
	    return "doFIN_TINYSTATE_START";
	},
    }
);


