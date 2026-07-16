//:IMPORT ts/tinyState.js
//:IMPORT gt/gtRequest.js
//:EXPORT gtItem


function gtItem(parent,uri,prevGtItem)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_URI = uri;
/* this is not necessary because new gtItem is called in the bind
    this.application.piggybackTurtle.bind(this,"g."+uri);
*/
    this_i_prevGtItem = prevGtItem;
    this.i_connection = {};
    this.i_connection.status = {};
    this.i_connection.status.status = "INITIALIZE";
    this.i_connection.status.status2 = 0;
    
    this.i_ref = {};
    this.i_writeQueue = [];
    this.i_psHandle = null;
    this.i_submit = false;
    this.i_transaction = null;
    this.initial("gtItem");
};

gtItem.uri = function(str) {
    if ( str.indexOf(gtPolling.URIheader,0) == 0 )
	return str.substr(gtPolling.URIheader.length);
    return str;
}

gtItem.convert = function(app,data) {
    if ( data == null ) {
	return null;
    }
    else if ( typeof data == "string" ) {
	if ( data.indexOf(gtPolling.URIheader,0) == 0 ) {
	    var uri = data.substr(gtPolling.URIheader.length);
	    return app.application.getGtItem(uri);
	}
	return data;
    }
    else if ( typeof data == "boolean" ) {
	return data;
    }
    else if ( typeof data == "number" ) {
	return data;
    }
    else if ( typeof data == "boolean" ) {
	return data;
    }
    else {
	var ret;
	if ( data.isArray )
	    ret = [];
	else
	    ret = {};
	var ix;
	for ( ix in data ) {
	    var el = data[ix];
	    ret[ix] = gtItem.convert(app,el);
	}
	return ret;
    }
};

stdObject.defClass
(
    "gtItem",
    gtItem,
    tinyState,
    {

	i_URI : null,
	i_data : null,
	i_updated : false,
	i_writeQueue : null,
	i_born : null,
	i_pol : null,
	i_ref : null,

	i_info : null,

	selected : false,

	notice : function(type,el) {
	    this.i_updated = true;
	    this.wakeup();
	},
	read : function() {
	    var ret = stdObject.copyHash(this.i_data);
	    if ( !this.i_pol ) {
		ret.connection = this.i_connection;
		ret.self = this.i_connection;
		if ( this.i_connection.status.status == "INITIALIZE" )
		    ret.self.init = false;
		else
		    ret.self.init = true;
		return ret;
	    }
	    ret.connection = this.i_pol.coGetPollingInfo();
	    ret.self = this.i_connection;
	    if ( this.i_connection.status.status == "INITIALIZE" )
		ret.self.init = false;
	    else
		ret.self.init = true;
	    return ret;
	},

	i_filterHash : {
	    "data" : 1,
	    "command" : 1
	},

	i_replace_data : function(path,data,org) {
	    if ( path.length == 0 ) {
		if ( typeof(org) == typeof(data) )
		    return data;
		switch ( typeof(org) ) {
		case "string":
		    switch ( typeof(data) ) {
		    case "boolean":
		    case "number":
			return data+"";
		    default:
			return data;
		    }
		case "boolean":
		    if ( data )
			return true;
		    else
			return false;
		case "number":
		    switch ( typeof(data) ) {
		    case "boolean":
			if ( data )
			    return 1;
			return 0;
		    case "string":
			return data - 0;
		    default:
			return data;
		    }
		default:
		    return data;
		}
	    }
	    path = path.concat();
	    var top = path.shift();
	    if ( typeof(org) != "object" ) {
		var ret;
		if ( typeof(top) == "number" )
		    ret = [];
		else
		    ret = {};
		ret[top] = this.i_replace_data(path,data,"");
		return ret;
	    }
	    org = stdObject.copyHash(org);
	    if ( typeof(org[top]) == "undefiend" ) {
		org[top] = this.i_replace_data(path,data,"");
		return org;
	    }
	    org[top] = this.i_replace_data(path,data,org[top]);
	    return org;
	},
	i_replace_data_top : function(path,data,org) {
	    if ( !this.i_filterHash[path[0]] )
		return org;
	    return this.i_replace_data(path,data,org);
	},

	write : function(caller,args) {
/*
  args = [{path : , data : },...]
*/
	    var req = new stdEvent("request",caller,args);
	    this.i_writeQueue.push(req);
	    this.wakeup();
	    return req;
	},

	i_sendWriteResult : function(status,status2,post_status) {
	    for ( ; ; ) {
		var req = this.i_writeResultQueue.shift();
		if ( !req )
		    break;
		var ev = new stdEvent("return",this,{
		    "request" : req,
		    "status" : status,
		    "status2" : status2,
		    "post" : post_status
		});
		req.source.eventHandler(ev);
	    }
	},

	uri : function() {
	    return this.i_URI;

	},

	addRef : function(env) {
	    this.i_ref[env.seq()] = env;
/*
this.dump("addRef "+this.i_URI+" >> ");
for ( ix in this.i_ref ) {
    this.dump("  "+this.i_ref[ix].host.gid);
}
*/
	},
	releaseRef : function(env) {
	    delete this.i_ref[env.seq()];
	    if ( Object.keys(this.i_ref).length == 0 )
		this.wakeup();
/*
this.dump("releaseRef "+this.i_URI+" >> ");
for ( ix in this.i_ref ) {
    this.dump("  "+this.i_ref[ix].host.gid);
}
*/
	},

	priority : function() {
	    var ret;
	    ret = tinyState.defaultPriority;
	    for ( ix in this.i_ref ) {
		var pri = this.i_ref[ix].priority();
		if ( pri < ret )
		    ret = pri;
	    }
//this.dump("gtItem "+this.i_URI+" >> "+ret);
	    return ret;
	},

	i_pollingStatusHandler : function(ev) {
	    if ( this.state(["ZOM","FIN"]) )
		return;
	    this.i_invoke("updated");
	},

	i_transactionIn : function() {
	    if ( !this.i_transaction )
		this.i_transaction = this.application.refShared("transaction");
	},
	i_transactionOut : function() {
	    if ( !this.i_transaction )
		return;
	    this.i_transaction = null;
	    this.application.unrefShared("transaction");
	},


	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_writeResultQueue = [];
	    var prev = this.i_prevGtItem;
	    if ( !prev )
		return "doINI_gtItem_GET";
	    prev.listen(this,"state");
	    return "doINI_gtItem_WAIT_PREV";
	},
	INI_gtItem_WAIT_PREV : function(ev) {
	    if ( prev.state(["ZOM"]) == 0 )
		return;
	    this.i_prevGtItem = null;
	    return "doINI_gtItem_GET";
	},
	INI_gtItem_GET : function(ev) {
	    this.i_pol = this.application.getGtPolling(this.i_URI);
	    this.i_pid = 0;
	    if ( this.i_pol ) {
		var info = this.i_pol.coGetPollingInfo();
		if ( info.pollingID )
		    this.i_pid = info.pollingID;
		this.i_shrd = null;
		return "doINI_gtItem_REQUEST";
	    }
	    this.i_shrd = this.application.refShared("initAccess");
	    if ( !this.i_shrd.sem )
		this.i_shrd.sem = new stdSemaphore(1);
	    return "doINI_gtItem_INIT_POL_ACCESS_WAIT";
	},
	INI_gtItem_INIT_POL_ACCESS_WAIT : function(ev) {
	    return this.i_shrd.sem.get(this,"doINI_gtItem_REQUEST");
	},
	INI_gtItem_REQUEST : function(ev) {
	    this.i_pol = this.application.getGtPolling(this.i_URI);
	    this.i_pid = 0;
	    if ( this.i_pol ) {
		var info = this.i_pol.coGetPollingInfo();
		if ( info.pollingID )
		    this.i_pid = info.pollingID;
	    }
	    this.i_req = new gtRequest(this,"GET",this.i_URI+"?polling="+this.i_pid+":set");
	    return "INI_gtItem_RET";
	},
	INI_gtItem_RET : function(ev) {
	    if ( this.destroyFlag && this.i_req ) {
		this.i_req.destroy();
		this.i_req = null;

		this.i_shrd.sem.release();
		this.application.unrefShared("initAccess");
		this.i_shrd = null;

		return "doFIN_START";
	    }
	    if ( ev.type != "return" )
		return;
	    if ( this.i_shrd ) {
		this.i_shrd.sem.release();
		this.application.unrefShared("initAccess");
		this.i_shrd = null;
	    }

	    this.i_req = null;
	    if ( ev.msg.status == "ERROR" ) {
		var ss = ev.msg.status2;
		if ( 400 <= ss && ss <= 499 )
		    return "doFIN_START";
		return "doINI_TINYSTATE_START";
	    }
	    this.i_born = [];
	    this.i_born[0] = ev.msg.message.born[0];
	    this.i_born[1] = ev.msg.message.born[1];
	    this.i_born[2] = ev.msg.message.objId;
	    this.i_connection.status.status = ev.msg.status;
	    this.i_connection.status.status2 = ev.msg.status2;
	    return "doACT_gtItem_SETUP";
	},
	


	ACT_TINYSTATE_START : function(ev) {
	    if ( Object.keys(this.i_ref).length == 0 )
		return "doFIN_REFERENCE";
	    if ( this.i_updated )
		return "doACT_gtItem_UPDATED";
	    if ( this.i_writeQueue.length )
		return "doACT_gtItem_WRITE";
	    return "ACT_START";
	},
	ACT_gtItem_UPDATED : function(ev) {
	    this.i_updated = false;
	    new gtRequest(this,"GET",this.i_URI);
	    return "ACT_gtItem_UPDATED_RET";
	},
	ACT_gtItem_UPDATED_RET : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( ev.type != "return" )
		return;
	    // this.dump("gtItem "+this.i_URI+" RETURN "+ev.msg.statetus2+" "+ev.msg.message);
	    if ( ev.msg.status == "ERROR" ) {
		var ss = ev.msg.status2;
		if ( 400 <= ss && ss <= 499 )
		    return "doFIN_START";
		return "doACT_gtItem_UPDATED";
	    }
	    if ( !this.i_born ) {
		this.i_born = [];
		this.i_born[0] = ev.msg.message.born[0];
		this.i_born[1] = ev.msg.message.born[1];
		this.i_born[2] = ev.msg.message.objId;
	    }
	    else if ( this.i_born[0] != ev.msg.message.born[0] ||
		      this.i_born[1] != ev.msg.message.born[1] ||
		      this.i_born[2] != ev.msg.message.objId ) {
		return "doFIN_START";
	    }
	    this.i_connection.status.status = ev.msg.status;
	    this.i_connection.status.status2 = ev.msg.status2;
	    return "doACT_gtItem_SETUP";
	},
	ACT_gtItem_SETUP : function(ev) {
	    this.i_data = ev.msg.message;
	    // this.dump("READ = "+JSON.stringify(this.i_data));
	    this.i_pol = this.application.getGtPolling(
		this.i_data.polling.base.slice(gtPolling.URIheader.length));
	    if ( this.i_pol ) {
		if ( this.i_psHandle )
		    this.i_psHandle.remove();
		this.i_psHandle = this.i_pol.listen(this,"updated",this.i_pollingStatusHandler);
	    }
	    else {
		this.dump("cannot open the polling !! "+this.i_data.polling.base);
	    }
	    this.selected = ev.msg.message.selected;
	    this.i_invoke("updated");

	    var info = this.i_pol.coGetPollingInfo();
	    if ( info.pollingID == 0 || this.i_pid == 0 ||
		 info.pollingID == this.i_pid )
		return "doACT_START";
	    this.i_pid = info.pollingID;
	    this.i_req = new gtRequest(this,"GET",this.i_URI+"?polling="+this.i_pid+":set");
	    return "ACT_gtItem_UPDATED_RET";
	},

	ACT_gtItem_WRITE : function(ev) {
	    this.i_temp = this.i_data;
//this.dump("ORG = "+JSON.stringify(this.i_data));
	    for ( ; ; ) {
		var req = this.i_writeQueue.shift();
		if ( !req )
		    break;
		var args = req.msg;
		var ixxx;
		for ( ixxx in args ) {
		    var aa = args[ixxx];
		    this.i_temp = this.i_replace_data_top(aa.path,aa.data,this.i_temp);
		}
		this.i_writeResultQueue.push(req);
	    }
	    var sendData;
	    if ( this.i_temp.command )
		sendData = {
		    "data" : this.i_temp.data,
		    "command" : this.i_temp.command
		};
	    else
		sendData = {
		    "data" : this.i_temp.data,
		    "command" : "write"
		};
//this.dump("SEND DATA = "+JSON.stringify(sendData));
	    if ( sendData.command == "transaction" ) {
		this.i_transactionIn();
	    }
	    else if (sendData.command == "submit" ) {
		this.i_submit = true;
	    }
	    new gtRequest(this,"POST",this.i_URI,JSON.stringify(sendData));
	    return "ACT_gtItem_WRITE_RET";
	},
	ACT_gtItem_WRITE_RET : function(ev) {
	    if ( this.i_destroyFlag ) {
		this.i_sendWriteResult("error",404);
		return "doFIN_START";
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.status == "ERROR" ) {
		var ss = ev.msg.status2;
		if ( 400 <= ss && ss <= 499 ) {
		    this.i_sendWriteResult("error",ss);
		    return "doFIN_START";
		}
		if ( this.i_transaction )
		    this.i_transactionOut();
		this.i_submit = false;
		return "doACT_gtItem_UPDATED";
	    }
	    if ( this.i_submit ) {
		if ( ev.msg.message.post.status && 
		     ev.msg.message.post.status.indexOf("ok") == 0 )
		    if ( this.i_transaction ) {
			this.i_transactionOut();
		    }
		this.i_submit = false;
	    }

	    this.i_data = this.i_temp;
	    this.i_temp = null;
//this.dump("SET DATA = "+JSON.stringify(this.i_data));

	    if ( ev.msg.message.updated )
		this.i_data = ev.msg.message;
	    this.i_sendWriteResult("ok",200,ev.msg.message.post);

	    var cc = {};
	    var ccc = null;
	    var ixx
	    for ( ixx in this.i_writeResultQueue ) {
		var req = this.i_writeResultQueue[ixx];
		cc[req.caller.objId()] = req.caller;
		ccc = req.caller;
	    }
	    if ( (Object.keys(cc).length == 1) && (!ev.msg.message.updated) )
		this.i_invoke("updated",ccc);
	    else
		this.i_invoke("updated");
	    return "doACT_START";
	},

	FIN_REFERENCE : function(ev) {
	    if ( !this.i_pol )
		return "doFIN_START";
	    var info = this.i_pol.coGetPollingInfo();
	    if ( !info.pollingID )
		return "doFIN_START";
	    new gtRequest(this,"GET",this.i_URI+"?polling="
			  +info.pollingID+":reset");
	    return "FIN_REFERENCE_WAIT";
	},
	FIN_REFERENCE_WAIT : function(ev) {
	    if ( ev.type != "return" ) 
		return;
	    if ( Object.keys(this.i_ref).length )
		return "doINI_gtItem_GET";
	    return "doFIN_START";
	},

	FIN_START : function(ev) {
	    for ( ; ; ) {
		var req = this.i_writeQueue.shift();
		if ( !req )
		    break;
		this.i_writeResultQueue.push(req);
	    }
	    this.i_sendWriteResult("error",404);

	    if ( this.i_transaction )
		this.i_transactionOut();
	    this.i_submit = false;
	    this.application.removeGtItem(this,this.i_URI);
	    var _ix;
	    for ( _ix in this.i_ref ) {
		var env = this.i_ref[_ix];
		env.del(this,"g."+this.i_URI);
	    }
	    this.i_ref = {};
	    if ( this.i_psHandle )
		this.i_psHandle.remove();
	    this.i_psHandle = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);
