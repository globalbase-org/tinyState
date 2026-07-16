//:IMPORT pig/pigPrimitive.js

function pigQuote(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigQuote");
}

piggybackTurtle.top.bind(
    null,
    "quote",
    new pigData("pn:primitive",pigQuote));


stdObject.defClass
(
    "pigQuote",
    pigQuote,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigQuote STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
this.dump("E-S "+JSON.stringify(this.param[1]));
	    this.i_sendResult(this.param[1]);
	    return "doFIN_START";
	}
    }
);


