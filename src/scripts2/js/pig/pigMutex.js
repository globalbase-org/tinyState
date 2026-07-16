//:IMPORT pig/pigPrimitive.js

/*
["mutex","LOCK VARIABLE","OPTIONS",["OPTIONLIST"],....]

OPTIONS
r : read lock
w : write lock
* : same name locks in the all of parent environment
e : ignore error

OPTIONLIST
["wstate","VALUE"]
["mystate","STATEVARIABLE"]
["mystate","STATEVARIABLE","PREFIX"]
*/

function pigMutex(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.i_lockTerminal = [];
    this.i_locked_ptr = 0;
    this.i_wstate = "";
    this.i_mystateVariable = "";
    this.i_mystatePrefix = "";
    this.i_mystateStack = "";
    this.i_mystateStackEnable = false;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigMutex");
}

piggybackTurtle.top.bind(
    null,
    "mutex",
    new pigData("pn:primitive",pigMutex));


stdObject.defClass
(
    "pigMutex",
    pigMutex,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigMutex STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();

	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    return "INI_RET_1";
	},
	INI_RET_1 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    switch ( ev.msg.getType() ) {
	    case "error":
		ev.source = this;
		this.i_sendEvent = ev;
		return "doFIN_START";
	    case "string":
		break;
	    default:
		this.i_setResultEvent("type missmatch","error");
		return "doFIN_START";
	    }
	    this.args[1] = ev.msg;
	    this.callFunc = new pigEval(this,this.env,this.param[2]);
	    return "INI_RET_2";
	},
	INI_RET_2 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    switch ( ev.msg.getType() ) {
	    case "error":
		ev.source = this;
		this.i_sendEvent = ev;
		return "doFIN_START";
	    case "string":
		break;
	    default:
		this.i_setResultEvent("type missmatch","error");
		return "doFIN_START";
	    }
	    this.args[2] = ev.msg;
	    this.callFunc = new pigActivate(this,[3,"all","all"]);
	    return "INI_RET_3";
	},
	INI_RET_3 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.i_sendEvent = ev;
		return "doFIN_START";
	    }
	    this.args = pigActivate.normalize(this.args);
	    this.callFunc = null;

	    var i;
	    for ( i in this.args[3] ) {
		var cmd = this.args[3][i];
		switch ( cmd[0] ) {
		case "mystate":
		    if ( typeof(cmd[1]) != "string" ) {
			this.i_setResultEvent(
			    "type missmatch (mystate:variable)",
			    "error");
			return "doFIN_START";
		    }
		    this.i_mystateVariable = cmd[1];
		    if ( cmd.length < 3 )
			break;
		    if ( typeof(cmd[2]) != "string" ) {
			this.i_setResultEvent(
			    "type missmatch (mystate:prefix)",
			    "error");
			return "doFIN_START";
		    }
		    this.i_mystatePrefix = cmd[2];
		    break;
		case "wstate":
		    if ( typeof(cmd[1]) != "string" ) {
			this.i_setResultEvent(
			    "type missmatch (wstate)",
			    "error");
			return "doFIN_START";
		    }
		    this.i_wstate = cmd[1];
		    break;
		default:
		    this.i_setResultEvent(
			"invalid command of option list ("+cmd[0]+")",
			"error");
		    return "doFIN_START";
		}
	    }


	    var lockv = this.env.refer(this,this.args[1],"");
	    if ( lockv.getType() == "error" ) {
		if ( this.args[2].indexOf("e") < 0 ) {
		    ev.source = this;
		    this.i_sendEvent = ev;
		    return "doFIN_START";
		}
		this.i_lockTerminal = [];
		return "doINI_MYSTATE";
	    }
	    this.i_lockTerminal.push(lockv.terminal);
	    if ( this.args[2].indexOf("*") < 0 )
		return "doINI_MYSTATE";
	    for ( ; ; ) {
		var penv = lockv.terminal.parent.parent;
		if ( !penv )
		    break;
		lockv = penv.refer(this,this.args[1]);
		if ( lockv.getType() == "error" )
		    break;
		this.i_lockTerminal.push(lockv.terminal);
	    }
	    return "doINI_MYSTATE";
	},
	INI_MYSTATE : function(ev) {
	    if ( !this.i_mystateVariable )
		return "doACT_START";
	    var mysv = this.env.refer(this,this.i_mystateVariable);
	    if ( mysv.getType() == "error" ) {
		this.i_setResultEvent(mysv.d,"error");
		return "doFIN_START";
	    }
	    this.i_mystateTerminal = mysv.terminal;
	    return "doACT_START";
	},

	ACT_START : function(ev) {
	    var i;
	    var args2 = this.args[2];
	    if ( this.i_lockTerminalHandle ) {
		this.i_lockTerminalHandle.remove();
		this.i_lockTerminalHandle = null;
	    }
	    if ( this.i_abort )
		return "doFIN_ABORT";

	    for ( ; this.i_locked_ptr < this.i_lockTerminal.length ;
		  this.i_locked_ptr ++ ) {
		var term = this.i_lockTerminal[this.i_locked_ptr];
		var lvalue = term.get("");
		if ( lvalue.getType() == "error" ) {
		    this.i_setResultEvent(
			lvalue.d,"error");
		    return "doFIN_START";
		}
		if ( args2.indexOf("r") >= 0 ) {
		    if ( lvalue.getType() != "number" ) {
			this.i_lockTerminalHandle
			    = term.listen(this);
			return "doACT_LOCK_WAIT";
		    }
		    if ( lvalue.d < 0 ) {
			this.i_lockTerminalHandle
			    = term.listen(this);
			return "doACT_LOCK_WAIT";
		    }
		    term.set(this,"",new pigData("pure",lvalue.d+1));
		}
		else {
		    if ( lvalue.d != 0 ) {
			this.i_lockTerminalHandle
			    = term.listen(this);
			return "doACT_LOCK_WAIT";
		    }
		    if ( this.i_wstate != "" )
			term.set(this,"",new pigData("pure",this.i_wstate));
		    else
			term.set(this,"",new pigData("pure",-1));
		}
	    }


	    if ( this.i_mystateTerminal ) {
		if ( !this.i_mystateStackEnable ) {
		    this.i_mystateStack = this.i_mystateTerminal.get("");
		    this.i_mystateStackEnable = true;
		}
		if ( this.i_mystatePrefix )
		    this.i_mystateTerminal.set(
			this,"",
			new pigData("pure",
				    this.imystatePrefix+":ACT"));
		else
		    this.i_mystateTerminal.set(
			this,"",
			new pigData("pure",
				    "ACT"));
	    }
	    this.i_ptr = 4;
	    this.i_setResultEvent(null);
	    return "doACT_EXEC";
	},
	ACT_LOCK_WAIT : function(ev) {
	    if ( this.i_mystateTerminal ) {
		if ( !this.i_mystateStackEnable ) {
		    this.i_mystateStack = this.i_mystateTerminal.get("");
		    this.i_mystateStackEnable = true;
		}
		if ( this.i_mystatePrefix )
		    this.i_mystateTerminal.set(
			this,"",
			new pigData("pure",
				    this.imystatePrefix+":WAIT"));
		else
		    this.i_mystateTerminal.set(
			this,"",
			new pigData("pure",
				    "WAIT"));
	    }
	    return "ACT_START";
	},

	ACT_EXEC : function(ev) {
	    if ( this.i_ptr >= this.param.length )
		return "doFIN_START";
	    this.callFunc = new pigEval(this,this.env,this.param[this.i_ptr]);
	    return "ACT_EXEC_RET";
	},
	ACT_EXEC_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.i_sendEvent = ev;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.i_sendEvent = ev;
		return "doFIN_START";
	    }
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    this.i_ptr ++;
	    return "doACT_EXEC";
	},

	FIN_ABORT : function(ev) {
	    this.i_setResultEvent(
		"aborted","error");
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    var i;
	    var args2 = this.args[2];
	    for ( i = this.i_locked_ptr-1 ; i >= 0 ; i -- ) {
		var term = this.i_lockTerminal[i];
		var lvalue = term.get("");
		if ( args2.indexOf("r") >= 0 )
		    term.set(this,"",new pigData("pure",lvalue.d-1));
		else
		    term.set(this,"",new pigData("pure",0));
	    }
	    if ( this.i_mystateTerminal ) {
		if ( this.i_mystateStackEnable )
		    this.i_mystateTerminal.set(
			this,"",
			this.i_mystateStack);
		else
		    this.i_mystateTerminal.set(
			this,"",
			new pigData("pure",""));
	    }
	    this.i_mystateTerminal = null;
	    this.i_lockTerminal = null;

	    this.i_sendResultEvent();
	    return "doFIN_TINYSTATE_START";
	},
    }
);


