//:IMPORT pig/pigSync.js

function pigWait(parent,env,param,opp) {
    if (arguments.length == 0)
	return;
    this.i_syncTerminal = {};
    this.i_opp = opp;
    pigSync.apply(this, [parent,env,param]);
    this.initial("pigWait");
}

piggybackTurtle.top.bind(
    null,
    "wait",
    new pigData("pn:primitive",pigWait));


stdObject.defClass
(
    "pigWait",
    pigWait,
    pigSync,
    {

	/*=====================================================
	 *
	 *    pigWait STATE MACHINE
	 *
	 *=====================================================*/

	ACT_START_WAIT : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( !this.i_opp ) {
		if ( ev.msg.getType().indexOf("pn:") == 0 ) {
		    ev.source = this;
		    this.parent.eventHandler(ev);
		    return"doFIN_START";
		}
		if ( ev.msg.d ) {
		    ev.source = this;
		    this.parent.eventHandler(ev);
		    return "doFIN_START";
		}
	    }
	    else {
		if ( ev.msg.getType().indexOf("pn:") && !ev.msg.d ) {
		    ev.source = this;
		    this.parent.eventHandler(ev);
		    return "doFIN_START";
		}
	    }
	    this.callFunc = null;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    return "doACT_EVENT_WAIT";
	}

    }
);


