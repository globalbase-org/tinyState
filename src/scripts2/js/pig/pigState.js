//:IMPORT pig/pigPrimitive.js

function pigState(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigState");
}

piggybackTurtle.top.bind(
    null,
    "state",
    new pigData("pn:primitive",pigState));

stdObject.defClass
(
    "pigState",
    pigState,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigState STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    this.i_sm_state = "";
	    return "INI_RET";
	},
	INI_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( ev.msg.getType() != "string" ) {
		this.i_sendResult("pigState: type mismatch arg 1 (string is required)","error");
		return "doFIN_START";
	    }
	    if ( ev.msg.d.indexOf("*") == 0 ) {
		this.i_sm_state = ev.msg.d.substr(1);
		this.i_sm_trace = true;
	    }
	    else {
		this.i_sm_state = ev.msg.d;
		this.i_sm_trace = false;
	    }
	    this.i_machine = this.param[2];
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    var target = this.i_machine[this.i_sm_state];
	    if ( !target ) {
		this.i_sendResult("pigState: "+this.i_sm_state+" is not defined!!","error");
		return "doFIN_START";
	    }
	    if ( this.i_sm_trace )
		this.dump("STATE : "+this.i_sm_state);
	    this.callFunc = new pigEval(this,this.env,target);
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		var got = "control:goto:";
		if ( ev.msg.d.indexOf(got) != 0 ) {
		    ev.source = this;
		    this.parent.eventHandler(ev);
		    return "doFIN_START";
		}
		var state = ev.msg.d.substr(got.length);
		if ( state == "sink" ) {
		    this.i_sendResult(ev.msg.sub);
		    return "doFIN_START";
		}
		if ( state != "" )
		    this.i_sm_state = state;
		return "doACT_START";
	    }
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},
    }
);


