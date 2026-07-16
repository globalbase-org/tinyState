//:IMPORT pig/pigPrimitive.js

function pigEval(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigEval");
}


piggybackTurtle.top.bind(
    null,
    "eval",
    new pigData("pn:primitive",pigEval));

pigEval.traceErrorMsg = function(msg,ev) {
    if ( !ev.msg.trace )
	return;
    console.log("TRACE-ERROR ["+msg+" >> "+ev.source.className+"] "+ev.msg.d);
}


stdObject.defClass
(
    "pigEval",
    pigEval,
    pigPrimitive,
    {

	traceOn : function(msg) {
	    if ( !this.callFunc ) {
		this.dump("TRACE: UNDEFINED OR FINISH "+msg);
		return;
	    }
	    this.callFunc.trace(msg);
	    this.dump("TRACE: "+msg+" "+this.callFunc.className+" "+this.callFunc.objId()+" "+this.callFunc.state());
	},

	traceError : function(ev) {
	    if ( !ev.msg.trace )
		return;
	    this.dump("TRACE-ERROR ["+this.className+" << "+ev.source.className+"] "+ev.msg.d);
	},

	/*=====================================================
	 *
	 *    pigEval STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    if ( this.param == null ) {
		this.i_sendResult(this.param);
		return "doFIN_START";
	    }
	    switch ( typeof(this.param) ) {
	    case "object":
		if ( Array.isArray(this.param) )
		    return "doINI_ARRAY";
		if ( typeof(this.param.isClass) == "function" && 
		     this.param.isClass("pigData") ) {
		    switch ( this.param.getType() ) {
		    case "array":
			this.param = this.param.d;
			return "doINI_ARRAY";
		    case "string":
			this.param = this.param.d;
			return "doINI_STRING";
		    }
		    this.i_sendResult(this.param);
		    return "doFIN_START";
		}
		return "doINI_BIND";
	    case "string":
		return "doINI_STRING";
	    default:
		this.i_sendResult(this.param);
	    }
	    return "doFIN_START";
	},
	INI_BIND : function(ev) {
	    this.callFuncBind = {};
	    this.callFuncHash = {};
	    this.callFuncOrgSym = {};
	    var result;
	    var sym;
	    for ( sym in this.param ) {
		if ( sym.indexOf(">") == 0 ) {
		    result = sym.substr(1);
		}
		else if ( sym.indexOf("$") == 0 ) {
		    var prev = sym;
		    result = this.env.refer(this,sym.substr(1));
		    if ( result.getType() == "error" ) {
			this.i_sendResult(result);
			return "doFIN_START";
		    }
		    if ( result.getType() != "string" ) {
			var msg = "pigEval: (BIND) type missmatch ["+prev+"] "+ result.getType();
			this.i_sendResult(msg,"error");
			return "doFIN_START";
		    }
		    result = result.d;
		}
		else if ( sym.indexOf("..") == 0 ) {
		    result = sym.substr(2);
		}
		else if ( sym.indexOf("..>") == 0 ) {
		    result = sym.substr(3);
		}
		else
		    result = sym;
		ts = new pigEval(this,this.env,this.param[sym]);
		this.callFuncOrgSym[ts.objId()] = sym;
		this.callFuncBind[result] = ts;
		this.callFuncHash[ts.objId()] = result;
	    }
	    if ( Object.keys(this.callFuncHash).length == 0 )
		return "doACT_BIND_FINISH";
	    this.ret = null;
	    return "ACT_BIND";
	},
	ACT_BIND : function(ev) {
	    if ( ev.type == "abort" ) {
		for ( sym in this.callFuncBind )
		    this.callFuncBind[sym].eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    var oid = ev.source.objId();
	    var sym = this.callFuncHash[oid];
	    var org = this.callFuncOrgSym[oid];
	    delete this.callFuncHash[oid];
	    delete this.callFuncBind[sym];
	    delete this.callFuncOrgSym[oid];
	    this.traceError(ev);
	    if ( ev.msg.getType() == "error" )
		this.ret = ev;
	    else if ( org.indexOf("..") == 0 || org.indexOf("..>") == 0 ) {
		var pos = sym.indexOf("..");
		var dd;
		if ( pos >= 0 )
		    dd = this.env.refer(this,sym.substr(0,pos));
		else
		    dd = this.env.refer(this,sym);
		if ( dd.getType() == "error" ) {
		    this.i_sendResult(dd);
		    return "doFIN_START";
		}
		dd.terminal.parent.bind(this,sym,ev.msg);
	    }
	    else
		this.env.bind(this,sym,ev.msg);
	    if ( org.indexOf(">") == 0 || org.indexOf("..>") == 0 ) {
		if ( ev.msg.getType() != "string" ) {
		    var msg = "pigEval : (BIND) type missmatch ["+org+" convertion after] "+ ev.msg.getType();
		    this.i_sendResult(msg,"error");
		    return "doFIN_START";
		}
		this.env.bind(this,ev.msg.d,null);
	    }
	    if ( Object.keys(this.callFuncHash).length )
		return;
	    return "doACT_BIND_FINISH";
	},
	ACT_BIND_FINISH : function(ev) {
	    if ( this.ret ) {
		this.ret.source = this;
		this.parent.eventHandler(this.ret);
	    }
	    else
		this.i_sendResult();
	    return "doFIN_START";
	},
	INI_STRING : function(ev) {
	    var pp = this.param;
	    if ( pp.indexOf("$") == 0 )
		this.i_sendResult(this.env.refer(this,pp.substr(1)));
	    else
		this.i_sendResult(pp);
	    return "doFIN_START";
	},
	INI_ARRAY : function(ev) {
	    if ( typeof(this.param[0]) == "string" ) {
		if ( /^\$+$/.test(this.param[0]) ) {
		    this.args[0] = this.env.refer(this,"refer");
		    this.param = [this.param[0],this.param[0].length]
			.concat(this.param.slice(1));
		    return "doINI_PRIMITIVE";
		}
		else if ( this.param[0].indexOf("$") != 0 )
		    this.callFunc = new pigEval(this,this.env,"$"+this.param[0]);
		else
		    this.callFunc = new pigEval(this,this.env,this.param[0]);
	    }
	    else
		this.callFunc = new pigEval(this,this.env,this.param[0]);
	    return "INI_ARRAY_WAIT";
	},
	INI_ARRAY_WAIT : function(ev) {
	    if ( ev.type == "abort" )
		return "doFIN_ABORT";
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    this.args[0] = ev.msg;
	    if ( this.args[0].type == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( this.args[0].type == "pn:primitive" )
		return "doINI_PRIMITIVE";
	    if ( this.args[0].type == "pn:lambda" )
		return "doINI_LAMBDA";
	    var msg = "pigEval : (FUNCTION) function type is required "+this.args[0].type;
	    this.i_sendResult(msg,"error");
	    return "doFIN_START";
	},
	INI_PRIMITIVE : function(ev) {
	    var param = this.param.concat();
	    param[0] = this.args[0];
	    this.callFunc = new this.args[0].d(this,this.env,param);
	    return "ACT_PRIMITIVE";
	},
	ACT_PRIMITIVE : function(ev) {
	    if ( ev.type == "abort" )
		return "doFIN_ABORT";
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},
	INI_LAMBDA : function(ev) {
	    this.i_lambda = this.args[0];
	    this.args[0] = null;
	    if ( !this.i_lambda.d.active ) {
		this.i_sendResult("execute deleted function","error");
		return "doFIN_START";
	    }
	    this.i_lambda.lock(this);
	    this.callFunc = new pigActivate(this,["all"]);
	    return "INI_LAMBDA_ARGS";
	},
	INI_LAMBDA_ARGS : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_LAMBDA_FINISH";
	    }
	    var argsName = this.i_lambda.d.args;
	    if ( this.i_lambda.d.env )
		this.nenv = new piggybackTurtle(this.i_lambda.d.env,this);
	    else
		this.nenv = new piggybackTurtle(this.env,this);
	    this.args = pigActivate.normalize(this.args);
	    for ( i = 0 ; i < argsName.length && 
		  i + 1 < this.param.length ; i ++ ) {
		this.nenv.bind(this,argsName[i],this.args[i+1]);
	    }
	    this.callFunc = new pigEval(
		this,
		this.nenv,
		this.i_lambda.d.body);
	    return "ACT_LAMBDA";
	},
	ACT_LAMBDA : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_LAMBDA_FINISH";
	},
	FIN_LAMBDA_FINISH : function(ev) {
	    this.i_lambda.unlock(this);
	    return "doFIN_START";
	},
	FIN_ABORT : function(ev) {
	    this.callFunc.eventHandler(ev);
	    return "FIN_ABORT_WAIT";
	},
	FIN_ABORT_WAIT : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.traceError(ev);
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
	    }
	    else
		this.i_sendResult("aborted","error");
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    return "doFIN_pigPrimitive_START";
	},
    }
);


