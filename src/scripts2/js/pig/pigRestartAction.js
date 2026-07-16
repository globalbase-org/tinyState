//:IMPORT pig/pigPrimitive.js

function pigRestartAction(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigRestartAction");
}

piggybackTurtle.top.bind(
    null,
    "restartAction",
    new pigData("pn:primitive",pigRestartAction));


stdObject.defClass
(
    "pigRestartAction",
    pigRestartAction,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigRestartAction STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_abort = false;
	    if ( this.param.length == 1 )
		return "doACT_EXEC";
	    this.callFunc = new pigActivate(this,["all"]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    if ( this.args.length == 1 ) {
		this.i_tState = "act";
		return "doACT_EXEC";
	    }
	    if ( this.args[1].getType() == "error" ) {
		this.i_sendResult(this.args[i]);
		return "doFIN_START";
	    }
	    this.i_tState = this.args[1].d;
	    if ( this.args.length >= 3 )
		this.i_target_gid = this.args[2].d;
	    else
		this.i_target_gid = null;
	    if ( this.i_tState == "restart" ) {
		this.i_tState = "act";
		return "doACT_RESTART";
	    }
	    if ( this.i_tState != "act" && this.i_tState != "freeze" ) {
		this.i_sendResult("invalid argument ("+this.i_tState+")","error");
		return "doFIN_START";
	    }
	    return "doACT_EXEC";
	},
	ACT_RESTART : function(ev) {
	    var top_obj = this.env.getGtObject();
	    if ( !top_obj ) {
		this.i_sendResult("this is no action on uiObject","error");
		return "doFIN_START";
	    }
	    var gt = top_obj.gtObject;
	    if ( this.i_target_gid )
		gt = gt.gtScope.getGtObjectById(this.i_target_gid);
	    if ( !gt ){
		this.i_sendResult("this is no action on gtObject","error");
		return "doFIN_START";
	    }
	    gt.set_gaction_status("freeze");
	    return "doACT_EXEC";
	},
	ACT_EXEC : function(ev) {
	    var top_obj = this.env.getGtObject();
	    if ( !top_obj ) {
		this.i_sendResult("this is no action on uiObject","error");
		return "doFIN_START";
	    }
	    var gt = top_obj.gtObject;
	    if ( this.i_target_gid )
		gt = gt.gtScope.getGtObjectById(this.i_target_gid);
	    if ( !gt ){
		this.i_sendResult("this is no action on gtObject","error");
		return "doFIN_START";
	    }
	    gt.set_gaction_status(this.i_tState);
	    return "doACT_EXEC_FINISH";
	},
	ACT_EXEC_FINISH : function(ev) {
	    this.i_sendResult("ok");
	    return "doFIN_START";
	}
    }
);


