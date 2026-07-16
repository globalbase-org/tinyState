
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js
//:IMPORT std/stdInterval.js
//:IMPORT gt/co_gtPolling.js
//:IMPORT gt/gtObject.js
//:EXPORT gtPolling


function gtPolling(parent) {
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_coList = [];
    this.i_latency = gtPolling.minInterval;
    this.initial("gtPolling");
};


gtPolling.maxCoList = 20;
gtPolling.minInterval = 1000;
gtPolling.maxInterval = 20*1000*1000;
gtPolling.latencyRate = 0.5;
gtPolling.lazyRate = 1.25;
gtPolling.agent = "GLOBALBASE TOOLKIT";
gtPolling.URIheader = "___GTURI:";

stdObject.defClass
(
    "gtPolling",
    gtPolling,
    tinyState,
    {
	i_baseURI : null,
	i_pollingID : 0,
	i_pollingURI : null,
	i_coList :null,

	i_latency : 0,
	i_expire : 0,
	i_request : false,

	i_born : null,

	i_pollingResult : {
	    msg : {
		status : "OK"
		    }
	},
	
	i_requestResult : function(ev) {
	    var change_flag = 0;
	    var prev;
	    if ( ev.msg.status == "OK" ) {
		if ( ev.msg.latency ) {
		    this.i_latency = (this.i_latency + ev.msg.latency)/2;
		    if ( this.i_latency < gtPolling.minInterval )
			this.i_latency = gtPolling.minInterval;
		    if ( this.i_latency > gtPolling.maxInterval )
			this.i_latency = gtPolling.maxInterval;
		    this.i_min_interval =ev.msg.message.polling.interval;
		}
		var message = ev.msg.message;
//this.dump("POLLING ---- "+JSON.stringify(message));
		if ( this.i_pollingID && 
		     message.polling.id != this.i_pollingID ) {
		    this.eventHandler(
			new stdEvent("result",this));
		    return;
		}
		prev = this.i_pollingID;
		this.i_pollingID = message.polling.id;
		if ( prev != this.i_pollingID )
		    change_flag = 1;

		prev = this.i_pollingURI;
		this.i_pollingURI = message.polling.uri
		    .slice(gtPolling.URIheader.length);
		if ( prev != this.i_pollingURI )
		    change_flag = 2;

		prev = this.i_baseURI;
		this.i_baseURI = message.polling.base
		    .slice(gtPolling.URIheader.length);
		if ( prev != this.i_baseURI )
		    change_flag = 3;
	    }
	    else if ( ev.msg.status == "ERROR" ) {
		if ( ev.msg.message == "destroyed" ) {
		    this.eventHandler(
			new stdEvent("result",this));
		    return;
		}
		if ( ev.msg.status2 == 404 ) {
		    this.i_pollingID = 0;
		    this.i_pollingURI = null;
		    change_flag = 10;
		}
	    }
	    prev = this.i_status;
	    this.i_status = {
		"status" : ev.msg.status,
		"status2" : ev.msg.status2,
	    };
	    if ( !prev )
		change_flag = 4;
	    else if ( prev.status != this.i_status.status ||
		 prev.status2 != this.i_status.status2 )
		change_flag = 5;

	    if ( change_flag != 0 )
		this.i_invoke("updated");
	    this.eventHandler(
		new stdEvent("result",this));
	},

	coGetPollingInfo : function() {
	    var ret = {};
	    ret.baseURI = this.i_baseURI;
	    ret.pollingID = this.i_pollingID;
	    ret.pollingURI = this.i_pollingURI;
	    ret.status = this.i_status;
	    return ret;
	},

	coResult : function(ev,req) {
	    this.i_request = req;
	    if ( ev.msg.status == "OK" ) {
		if ( !this.i_born ) {
		    this.i_born = [];
		    this.i_born[0] = ev.msg.message.born[0];
		    this.i_born[1] = ev.msg.message.born[1];
		    this.i_born[2] = ev.msg.message.objId;
		}
		else if ( this.i_born[0] != ev.msg.message.born[0] ||
			  this.i_born[1] != ev.msg.message.born[1] ||
			  this.i_born[2] != ev.msg.message.objId ) {
		    ev.msg.status = "ERROR";
		    ev.msg.status2 = 404;
		    ev.msg.message = "Born is not mach";
		}
	    }
	    if ( this.i_pollingResult.msg.status == "OK" )
		this.i_pollingResult = ev;
	    var i;
	    for ( i = 0 ; i < this.i_coList.length ; i ++ )
		if ( this.i_coList[i] === ev.source )
		    break;
	    if ( i == this.i_coList.length )
		return;
	    this.i_coList 
		= this.i_coList.slice(0,i).concat(
		    this.i_coList.slice(i+1));
	    this.i_requestResult(ev);
	},
	requestResult : function(ev) {
	    this.i_request = true;
	    this.i_requestResult(ev);
	    this.application.setGtPolling(this.i_baseURI,this);
	},
	getLatency : function() {
	    return this.i_latency;
	},
	priority : function() {
	    return gtObject.basePriority;
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	ACT_TINYSTATE_START : function(ev) {
	    if ( this.i_pollingID == 0 )
		return "ACT_START";
	    this.i_interval = this.i_latency * gtPolling.latencyRate;
	    return "doACT_POLLING_START";
	},
	ACT_POLLING_START : function(ev) {
	    var ll = Math.round(gtRequest.limit(this)/3);
	    if ( ll < 1 )
		ll = 1;
	    if ( this.i_coList.length >= ll )
		return;
	    this.i_coList.push(new co_gtPolling(this));
	    var min = this.i_latency*gtPolling.latencyRate;
	    if ( this.i_interval < min )
		this.i_interval = min;
	    if ( this.i_interval < this.i_min_interval )
		this.i_interval = this.i_min_interval;
	    this.i_expire = stdInterval.now() + this.i_interval;
/*
this.dump("INTERVAL = "+this.i_baseURI+" "+this.i_pollingID+" "+(this.i_interval/1000000)+" ("+(this.i_latency/1000000)+") "+min);
*/
	    stdInterval.wait(this,this.i_interval,"timer");
	    return "ACT_POLLING_WAIT";
	},
	ACT_POLLING_WAIT : function(ev) {
	    if ( this.destroyFlag )
		return "doFIN_START";
	    if ( this.i_expire - stdInterval.now() <= 0 ) {
		this.i_interval *= gtPolling.lazyRate;
		if ( this.i_interval > gtPolling.maxInterval )
		    this.i_interval = gtPolling.maxInterval;
		return "doACT_POLLING_START";
	    }
	    if ( this.i_request ) {
		this.i_request = false;
		this.i_interval = this.i_latency * gtPolling.latencyRate;
		if ( this.i_interval < this.i_min_interval )
		    this.i_interval = this.i_min_interval;
		return "doACT_POLLING_START";
	    }
	    if ( this.i_pollingResult.msg.status == "OK" )
		return;
	    for ( var i = 0 ; i < this.i_coList.length ; i ++ )
		this.i_coList[i].destroy();
	    return "doACT_ERROR_HANDLE";
	},
	ACT_POLLING_MORE_WAIT : function(ev) {
	    if ( this.i_expire - stdInterval.now() > 0 )
		return;
	    return "doACT_POLLING_START";
	},
	ACT_ERROR_HANDLE : function(ev) {
	    if ( this.i_coList.length > 0 )
		return;
	    this.i_pollingID = 0;
	    this.i_born = null;
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    new gtRequest(this,"GET",this.i_baseURI);
	    return "ACT_ERROR_HANDLE_RET";
	},
	ACT_ERROR_HANDLE_RET : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.status == "OK" ) {
		this.i_pollingResult.msg.status = "OK";
		this.application.sendNoticeToAllGtItem();
		return "doACT_START";
	    }
	    this.i_expire = stdInterval.now() + this.i_interval;
	    stdInterval.wait(this,this.i_interval,"timer");
	    return "ACT_ERROR_HANDLE_RET2";
	},
	ACT_ERROR_HANDLE_RET2 : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( this.i_expire > stdInterval.now() )
		return;
	    this.i_interval *= gtPolling.lazyRate;
	    if ( this.i_interval > gtPolling.maxInterval )
		this.i_interval = gtPolling.maxInteral;
	    return "doACT_ERROR_HANDLE";
	},

	FIN_START : function(ev) {
	    this.application.removeGtPolling(this.i_baseURI);
	    for ( var i = 0 ; i < this.i_coList.length ; i ++ )
		this.i_coList[i].destroy();
	    return "doFIN_START_WAIT";
	},
	FIN_START_WAIT : function(ev) {
	    if ( this.i_coList.length > 0 )
		return;
	    if ( this.i_pollingID == 0 )
		return "doFIN_TINYSTATE_START";
	    new gtRequest(this,"GET",this.i_pollingURI+"?polling="
			  +this.i_pollingID+":close");
	    return "FIN_START_WAIT2";
	},
	FIN_START_WAIT2 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    return "doFIN_TINYSTATE_START";
	},
    }
);

