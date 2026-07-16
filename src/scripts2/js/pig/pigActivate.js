//:IMPORT ts/tinyState.js


function pigActivate(parent,ix) {
    if (arguments.length == 0)
	return;
    if ( parent.sync )
	this.sync = parent.sync;
    else
	this.sync = null;
    this.ix = ix;
    this.waitCall = {};
    tinyState.apply(this, [parent]);
    this.initial("pigActivate");
}


pigActivate.normalize = function(args) {
    if ( args == null )
	return args;
    if ( typeof(args) != "object" )
	return args;
    if ( args.isClass && args.isClass("pigData") ) {
	return args.d;
    }
    if ( Array.isArray(args) ) {
	var ret = [];
	var ix;
	for ( ix in args )
	    ret[ix] = pigActivate.normalize(args[ix]);
	return ret;
    }
    var ret = {};
    var ix;
    for ( ix in args )
	ret[ix] = pigActivate.normalize(args[ix]);
    return ret;
}

stdObject.defClass
(
    "pigActivate",
    pigActivate,
    tinyState,
    {

	i_get : function(ix) {
	    var ptr = this.parent.param;
	    var i;
 	    for ( i = 0 ; i < ix.length ; i ++ ) {
		if ( !Array.isArray(ptr) ) {
		    return ["notArray",i];
		}
		if ( ix[i] >= ptr.length )
		    return ["length",null];
		ptr = ptr[ix[i]];
	    }
	    return ["ok",ptr];
	},
	i_getArgs : function(ix) {
	    var ptr = this.parent.args;
	    var i;
 	    for ( i = 0 ; i < ix.length ; i ++ ) {
		if ( !Array.isArray(ptr) ) {
		    return ["notArray",i];
		}
		if ( ix[i] >= ptr.length )
		    return ["length",null];
		ptr = ptr[ix[i]];
	    }
	    return ["ok",ptr];
	},
	i_set : function(ix,data) {
	    if ( this.parent.args == null )
		this.parent.args = [];
	    ptr = this.parent.args;
	    var i;
	    for ( i = 0 ; i < ix.length-1 ; i ++ ) {
		if ( !Array.isArray(ptr[ix[i]]) )
		    ptr[ix[i]] = [];
		ptr = ptr[ix[i]];
	    }
	    ptr[ix[i]] = data;
	},

	i_call : function(param,args,ix,ptr,ix_path) {
	    if ( ix.length <= ptr ) {
		if ( args )
		    return;
		var ts = new pigEval(this,this.parent.env,param);
		this.waitCall[ts.objId()] = {
		    "ts" : ts,
		    "ix" : ix_path
		};
		return;
	    }
	    if ( !Array.isArray(param) )
		return;
	    var new_args = null;
	    var args_ary = false;
	    if ( args ) {
		if ( Array.isArray(args) ) {
		    new_args = args;
		    args_ary = true;
		}
		else
		    new_args = "*";
	    }
	    if ( ix[ptr] != "all" ) {
		if ( args_ary )
		    new_args = new_args[ix[ptr]];
		var new_ix_path = ix_path.concat();
		new_ix_path.push(ix[ptr]);
		this.i_call(
		    param[ix[ptr]],
		    new_args,
		    ix,
		    ptr+1,
		    new_ix_path);
		return;
	    }
	    var i;
	    for ( i = 0 ; i < param.length ; i ++ ) {
		var nargs;
		if ( args_ary )
		    nargs = new_args[i];
		else
		    nargs = new_args;
		var new_ix_path = ix_path.concat();
		new_ix_path.push(i);
		this.i_call(
		    param[i],
		    nargs,
		    ix,
		    ptr+1,
		    new_ix_path);
	    }
	},

	traceError : function(ev) {
	    if ( !ev.msg.trace )
		return;
	    this.dump("TRACE-ERROR ["+this.className+" << "+ev.source.className+"] "+ev.msg.d);
	},

	/*=====================================================
	 *
	 *    pigActivate STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.i_call(this.parent.param,this.parent.args,this.ix,0,[]);
	    if ( Object.keys(this.waitCall).length == 0 )
		return "doFIN_START";
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" )
		return "doFIN_ABORT";
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    var ix = this.waitCall[ev.source.objId()].ix;
	    if ( ix ) {
		this.i_set(ix,ev.msg);
		delete this.waitCall[ev.source.objId()];
	    }
	    if ( Object.keys(this.waitCall).length )
		return;
	    return "doFIN_START";
	},
	FIN_ABORT : function(ev) {
	    for ( ix in this.waitCall )
		this.waitCall[ix].ts.eventHandler(
		    new stdEvent("abort",this));
	    return "FIN_ABORT_WAIT";
	},
	FIN_ABORT_WAIT : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    delete this.waitCall[ev.source.objId()];
	    if ( Object.keys(this.waitCall).length )
		return;
	    this.parent.eventHandler(
		new stdEvent("return",this,
			    new pigData("error","aborted")));
	    return "doFIN_ACTIVATE_FINISH";
	},
	FIN_START : function(ev) {
	    this.parent.eventHandler(
		new stdEvent("return",this,
			    new pigData("ok",null)));
	    return "doFIN_ACTIVATE_FINISH";
	},
	FIN_ACTIVATE_FINISH : function(ev) {
	    this.waitCall = null;
	    this.ix = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);




