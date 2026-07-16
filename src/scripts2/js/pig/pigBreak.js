//:IMPORT pig/pigPrimitive.js

function pigBreak(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigBreak");
}


piggybackTurtle.top.bind(
    null,
    "break",
    new pigData("pn:primitive",pigBreak));

stdObject.defClass
(
    "pigBreak",
    pigBreak,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigBreak STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.i_sendResult(
		new pigData("error","control:break"));
	    return "doFIN_START";
	},

    }
);


