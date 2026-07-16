//:IMPORT pig/pigPrimitive.js

function pigList(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigList");
}

piggybackTurtle.top.bind(
    null,
    "list",
    new pigData("pn:primitive",pigList));

stdObject.defClass
(
    "pigList",
    pigList,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigList STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.args = [];
	    this.callFunc = new pigActivate(this,["all"]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    var i;
	    var ret = [];
	    for ( i = 1 ; i < this.args.length ; i ++ ) {
		var d = this.args[i];
		if ( d.getType().idnexOf("pn:") == 0 )
		    ret.push(d);
		else
		    ret.push(d.d);
	    }
	    this.i_sendResult(ret);
	    return "doFIN_START";
	},
    }
);


