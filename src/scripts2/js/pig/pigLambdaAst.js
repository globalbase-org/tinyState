//:IMPORT pig/pigPrimitive.js
//:IMPORT pig/pigLambda.js

function pigLambdaAst(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigLambda.apply(this, [parent,env,param]);
    this.initial("pigLambdaAst");
}

piggybackTurtle.top.bind(
    null,
    "lambda*",
    new pigData("pn:primitive",pigLambdaAst));


stdObject.defClass
(
    "pigLambdaAst",
    pigLambdaAst,
    pigLambda,
    {

	/*=====================================================
	 *
	 *    pigLambdaAst STATE MACHINE
	 *
	 *=====================================================*/

	ACT_START : function(ev) {
	    this.i_sendResult(
		new pigData(
		    "pn:lambda",
		    this.env,
		    this.param[1],
		    this.i_body));
	    this.i_body = null;
	    return "doFIN_START";
	}
    }
);


