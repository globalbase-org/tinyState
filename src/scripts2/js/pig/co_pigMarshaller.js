//:IMPORT pig/pigEval.js

function co_pigMarshaller(parent,index,variable,func) {
    if (arguments.length == 0)
	return;
    this.i_index = index;
    this.i_variable = variable;
    this.i_func = func;
    this.i_reboot = false;
    tinyState.apply(this, [parent]);

    this.parent.env.bind(this,this.i_variable,"run");
    this.initial("co_pigMarshaller");
}


stdObject.defClass
(
    "co_pigMarshaller",
    co_pigMarshaller,
    tinyState,
    {

	set : function(func) {
	    if ( this.i_func === func )
		return false;
	    this.i_func = func;
	    if ( this.i_reboot )
		return true;
	    this.i_reboot = true;
	    this.wakeup();
	},

	/*=====================================================
	 *
	 *    co_pigMarshaller STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    if ( Array.isArray(this.i_func) && this.i_func.length == 0 )
		return "doFIN_START";
	    this.callFunc = new pigEval(this,this.parent.env,this.i_func);
	    return "ACT_RET";
	},
	ACT_RET : function(ev) {
	    if ( this.i_reboot )
		return "doACT_REBOOT";
	    if ( ev.type != "return" )
		return;
	    return "doFIN_START";
	},
	ACT_REBOOT : function(ev) {
	    this.i_reboot = false;
	    this.callFunc.abort();
	    return "ACT_REBOOT_RET";
	},
	ACT_REBOOT_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    return "doACT_START";
	},

	FIN_START : function(ev) {
	    return this.application.shared("marshaller").sem
		.get(this,"doFIN_SEM_OK");
	},
	FIN_SEM_OK : function(ev) {
	    this.parent.env.bind(this,this.i_variable,"sus");
	    this.parent.delRunning(this.i_index);
	    this.application.shared("marshaller").sem.release();

	    this.i_func = null;
	    this.i_variable = null;
	    this.i_index = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);


