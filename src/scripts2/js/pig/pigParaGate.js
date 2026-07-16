//:IMPORT pig/pigPrimitive.js

function pigParaGate(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigParaGate");
}

piggybackTurtle.top.bind(
    null,
    "->",
    new pigData("pn:primitive",pigParaGate));

stdObject.defClass
(
    "pigParaGate",
    pigParaGate,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigParaGate STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    var ret = pigPara.paraGate(this);
	    if ( ret < 0 )
		this.i_sendResult("PARA GATE (->) not in the para function","error");
	    else if ( ret > 0 )
		this.i_sendResult("aborted","error");
	    else
		this.i_sendResult();
	    return "doFIN_START";
	},
    }
);


