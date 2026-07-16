
//:IMPORT std/stdObject.js
//:IMPORT std/stdEvent.js
//:IMPORT std/stdEventHandle.js
//:EXPORT tinyState

function tinyState(parent)
{
    if ( arguments.length == 0 )
	return;
    stdObject.apply(this,[]);
    this.parent = parent;
    this.i_eventQueue = [];
    this.i_eventHandleSource = [];
    this.i_eventHandleDest = [];
    if ( parent ) {
	this.application = parent.application;
    }
    this.initial("tinyState");
}

tinyState.i_tsList = {};
tinyState.i_waitIndicate = "-\|/";
tinyState.i_waitCount = 0;
tinyState.defaultPriority = 10000;

stdObject.defClass
(
    "tinyState",
    tinyState,
    stdObject,
    {
	parent : null,
	application : null,
	i_state : "INI_START",
	i_processed : 0,
	i_eventQueue : null,
	i_stateChange : false,

	initial : function(className) {
	    if ( this.className != className )
		return;
	    for ( ; tinyState.i_tsList[this.i_seq] ; )
		this.i_getSeq();
	    tinyState.i_tsList[this.i_seq] = this;
	    this.eventHandler(
		new stdEvent("initial",this));
	},
	eventHandler : function(ev) {
	    if ( this.i_state.indexOf("ZOM",0) == 0 )
		return 0;
	    this.i_eventQueue.push(ev);
	    if ( this.i_processed == 1 )
		return 0;
	    this.i_processed = 1;
	    for ( ; ; ) {
		for ( ; ; ) {
		    var evCurrent = this.i_eventQueue.shift();
		    if ( !evCurrent )
		    break;
		    evCurrent = this.i_eventFilter(evCurrent);
		    if ( !evCurrent )
			break;
		    var loop_flag = true;
		    for ( ; loop_flag ; ) {
			loop_flag = false;
			var func = this.i_state;
			var newState;
			this.i_trace("IN");
			try {
			    newState = this[func](evCurrent);
			} catch ( err ) {
			    newState = "doERR_START";
			    evCurrent = new stdEvent("error",this,err);
			}
			if ( !newState )
			    newState = this.i_state;
			if ( newState.indexOf("do",0) == 0 ) {
			    loop_flag = true;
			    newState = newState.slice(2);
			}
			this.i_trace("OUT",newState);
			this.i_setState(newState);
		    }
		    if ( this.i_state == "ZOM" )
			break;
		}
		this.i_invokeState();
		if ( !this.i_eventQueue ||
		     this.i_eventQueue.length == 0 )
		    break;
	    }
	    this.i_processed = 0;
	    return 0;
	},

	objId : function() {
	    return this.i_seq;
	},


	setEventFilter : function(func) {
	    var oldfunc = this.i_eventFilter;
	    this.i_eventFilter = function(ev) {
		ev = func.apply(this,[ev]);
		if ( !ev  )
		    return ev;
		return oldfunc.apply(this,[ev]);
	    }
	},
	i_eventFilter : function(ev) {
	    return ev;
	},

	i_traceMsg : "",
	trace : function(msg) {
	    this.i_traceMsg = msg;
	    this.dump("["+this.className+":"+this.seq()+" "+this.i_traceMsg+"] TRACE-START "+this.i_state);
	},
	i_trace : function(mode,newState) {
	    if ( this.i_traceMsg == "" )
		return;
	    if ( mode == "IN" ) {
		this.dump("["+this.className+":"+this.seq()+":("+tinyState.i_waitIndicate.substr(tinyState.i_waitCount,1)+"):"+this.i_traceMsg
			  +"] >>> "+this.i_state);
	    }
	    else {
		this.dump("["+this.className+":"+this.seq()+":("+tinyState.i_waitIndicate.substr(tinyState.i_waitCount,1)+"):"+this.i_traceMsg
			  +"]       <<< "+newState);
		tinyState.i_waitCount ++;
		if ( tinyState.i_waitCount >= tinyState.i_waitIndicate.length )
		    tinyState.i_waitCount = 0;
	    }
	},
	i_setState : function(newState) {
	    if ( this.i_state != newState )
		this.i_stateChange = true;
	    this.i_prevState = this.i_state;
	    this.i_state = newState;
	},
	i_invokeState : function() {
	    if ( this.i_stateChange )
		this.i_invoke("state");
	    this.i_stateChange = false;
	},

	i_destroyFlag : false,
	destroy : function() {
	    this.i_destroyFlag = true;
	    this.eventHandler(new stdEvent("destroy",this));
	},
	wakeup : function() {
	    this.eventHandler(new stdEvent("wakeup",this));
	},
	state : function(name) {
	    if ( !name )
		return this.i_state;
	    if ( typeof name == "string" ) {
		if ( this.i_state.indexOf(name,0) == 0 )
		    return true;
		return false;
	    }
	    for ( ix in name ) {
		if ( this.i_state.indexOf(name[ix],0) == 0 )
		    return true;
	    }
	    return false;
	},

	i_eventHandleSource : null,
	i_eventHandleDest : null,
	listen : function(subject,type,handler) {
	    return new stdEventHandle(type,this,subject,handler);
	},
	add_listener : function(evHandle) {
	    if ( !this.i_eventHandleSource )
		return false;
	    if ( this.i_eventHandleSource[evHandle.type] ) {
		this.i_eventHandleSource[evHandle.type].push(evHandle);
	    }
	    else {
		this.i_eventHandleSource[evHandle.type] = [evHandle];
	    }
	    return true;
	},
	add_eventHandleDest : function(evHandle) {
	    this.i_eventHandleDest.push(evHandle);
	},
	remove_listener : function(evHandle) {
	    var i;
	    var type = evHandle.type;
	    if ( !this.i_eventHandleSource[type] )
		return;
	    var list = this.i_eventHandleSource[type]; 
	    var len = list.length;
	    for ( i = 0 ; i < len ; i ++ )
		if ( list[i] === evHandle )
		    break;
	    if ( i == len )
		return;
	    this.i_eventHandleSource[type]
		= list.slice(0,i).concat(list.slice(i+1));
	    if ( this.i_eventHandleSource[type].length == 0 ) {
		this.i_eventHandleSource[type] = null;
		delete this.i_eventHandleSource[type];
	    }
	},
	remove_eventHandleDest : function(evHandle) {
	    var i;
	    var len = this.i_eventHandleDest.length;
	    for ( i = 0 ; i < len ; i ++ )
		if ( this.i_eventHandleDest[i] === evHandle )
		    break;
	    if ( i == len )
		return;
	    this.i_eventHandleDest
		= this.i_eventHandleDest.slice(0,i).
		concat(this.i_eventHandleDest.slice(i+1));
	},
	i_invoke : function(type,escaped) {
	    if ( !this.i_eventHandleSource )
		return 0;
	    if ( !this.i_eventHandleSource[type] )
		return 0;
	    var i;
	    var list = this.i_eventHandleSource[type].concat();
	    var len = list.length;
	    var ev = new stdEvent(type,this);
	    var ret = 0;
	    for ( i = 0 ; i < len ; i ++ ) {
		if ( list[i].dest === escaped )
		    continue;
		if ( list[i].handler )
		    list[i].handler.apply(list[i].dest,[ev]);
		else
		    list[i].dest.eventHandler(ev);
		ret ++;
	    }
	    return ret;
	},
	priority : function() {
	    return tinyState.defaultPriority;
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_START : function(ev) {
	    return "doINI_TINYSTATE_START";
	},
	INI_TINYSTATE_START : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    return "doACT_TINYSTATE_CHECK1";
	},
	ACT_TINYSTATE_CHECK1 : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    return "doACT_TINYSTATE_START";
	},
	ACT_TINYSTATE_START : function(ev) {
	    return "ACT_START";
	},
	FIN_START : function(ev) {
	    return "doFIN_TINYSTATE_START";
	},
	FIN_TINYSTATE_START : function(ev) {
	    return "doZOM_START";
	},
	ZOM_START : function(ev) {
	    this.i_invoke("state");
	    this.i_invoke("destroyed");
	    var typeHash = stdObject.copyHash(this.i_eventHandleSource);
	    for ( ix in typeHash ) {
		var list = typeHash[ix];
		for ( ix2 in list ) {
		    list[ix2].remove();
		}
	    }
	    var list = this.i_eventHandleDest.concat();
	    var i;
	    for ( i = 0 ; i < list.length ; i ++ )
		list[i].remove();
	    tinyState.i_tsList[this.i_seq] = null;
	    delete tinyState.i_tsList[this.i_seq];
	    this.i_eventQueue = null;

	    var ix;
	    for ( ix in this ) {
		if ( typeof(this[ix]) != "object" )
		    continue;
		delete this[ix];
	    }
	    return "ZOM";
	},
	ZOM : function(ev) {
	    return;
	},
	ERR_START : function(ev) {
	    if ( !this.i_errPrevState ) {
		this.i_errPrevState = this.i_prevState;
		this.i_errMessage = ev.msg;
	    }
	    this.dump("tinyState ERROR ("+this.className+":"+this.seq()+") "+this.i_errMessage);
	    if ( this.i_errMessage.fileName && this.i_errMessage.lineNumber )
		this.dump("FILE : "+this.i_errMessage.fileName+
			  " LINE : "+this.i_errMessage.lineNumber);
	    this.dump("LAST STATE = "+this.i_errPrevState);
	    return;
	}
    }
);
