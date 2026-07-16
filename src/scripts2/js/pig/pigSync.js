//:IMPORT pig/pigPrimitive.js

function pigSync(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.i_syncTerminal = {};
    this.i_errTimer = stdInterval.now();
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigSync");
}

piggybackTurtle.top.bind(
    null,
    "sync",
    new pigData("pn:primitive",pigSync));


stdObject.defClass
(
    "pigSync",
    pigSync,
    pigPrimitive,
    {
	i_wakeup_flag : false,
	i_abort : false,

	i_syncEventHandler : function(ev) {
	    if ( this.state(["ZOM","FIN"]) )
		return;
	    if ( this.i_wakeup_flag )
		return;
	    this.i_wakeup_flag = true;
	    this.wakeup();
	},

	set : function(term) {
	    var thash = this.i_syncTerminal[term.seq()];
	    if ( thash )
		return;
	    var penv = term.parent;
	    for ( ; penv ; ) {
		if ( penv == this.nenv )
		    return;
		penv = penv.parent;
	    }
	    var th = term.listen(this,this.i_syncEventHandler);
	    if ( !th )
		return;

	    this.i_syncTerminal[term.seq()] = {
		terminal : term,
		handle : th
	    };
	},

	i_handleCheck : function() {
	    this.dump("TH "+this.objId());
	    for ( ix in this.i_syncTerminal ) {
		var th = this.i_syncTerminal[ix];
		this.dump("  >> "+th.handle.i_removed+" "+th.handle.source.symbol);
	    }
	},

	/*=====================================================
	 *
	 *    pigSync STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.sync = this;
	    this.nenv = new piggybackTurtle(this.env,this);
	    this.args[1] = pigActivate.normalize(this.param[1]);
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    this.callFunc = new pigEval(this,this.nenv,this.args[1]);
	    return "doACT_START_WAIT";
	},
	ACT_START_WAIT : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		if ( ev.msg.d == "EOF" ) {
		    this.i_sendResult("EOF");
		    return "doFIN_START";
		}
		if ( this.i_errTimer == -1 || 
		     stdInterval.now() - this.i_errTimer >= 10000000 ) {
		    this.dump("pigSync ERROR (ignored)"+ev.msg.d);
		    this.i_errTimer = -1;
		}
	    }
	    else
		this.i_errTimer = -1;
	    this.callFunc = null;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    return "doACT_EVENT_WAIT";
	},
	ACT_EVENT_WAIT : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    if ( !this.i_wakeup_flag )
		return;
	    this.i_wakeup_flag = false;
	    return "doACT_START";
	},

	FIN_START : function(ev) {
	    for ( ix in this.i_syncTerminal )
		this.i_syncTerminal[ix].handle.remove();
	    this.i_syncTerminal = null;
	    return "doFIN_pigPrimitive_START";
	},

    }
);


