//:IMPORT pig/pigPrimitive.js

function pigContinue(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigContinue");
}


piggybackTurtle.top.bind(
    null,
    "continue",
    new pigData("pn:primitive",pigContinue));

stdObject.defClass
(
    "pigContinue",
    pigContinue,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigContinue STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.i_sendResult(
		new pigData("error","control:continue"));
	    return "doFIN_START";
	},

    }
);


