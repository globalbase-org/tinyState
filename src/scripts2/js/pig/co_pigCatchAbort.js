//:IMPORT pig/pigSync.js

function co_pigCatchAbort(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigSync.apply(this, [parent,env,param]);
    this.initial("co_pigCatchAbort");
}


stdObject.defClass
(
    "co_pigCatchAbort",
    co_pigCatchAbort,
    pigSync,
    {

	/*=====================================================
	 *
	 *    co_pigCatchAbort STATE MACHINE
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
		this.parent.i_coRet = ev;
		return "doFIN_START";
	    }
	    if ( ev.msg.getType().indexOf("pn:") == 0 ||
		 ev.msg.d ) {
		this.parent.i_condition = true;
	    }
	    else {
		this.parent.i_condition = false;
		this.parent.wakeup();
	    }
	    this.callFunc = null;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    return "doACT_EVENT_WAIT";
	},

    }
);


