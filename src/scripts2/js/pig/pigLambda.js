//:IMPORT pig/pigPrimitive.js
//:IMPORT pig/pigActivate.js

function pigLambda(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigLambda");
}

piggybackTurtle.top.bind(
    null,
    "lambda",
    new pigData("pn:primitive",pigLambda));


stdObject.defClass
(
    "pigLambda",
    pigLambda,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigLambda STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    var body;
	    if ( this.param.length > 3 ) {
		body = this.param.concat();
		body.shift();
		body.shift();
		body.unshift("sq");
	    }
	    else {
		body = this.param[2];
	    }
	    var args = this.param[1];
	    if ( !Array.isArray(args) ) {
		this.i_sendResult("type missmatch (2nd arg) arguments array is required","error");
		return "doFIN_START";
	    }
	    var i = args.length - 1;
	    for ( ; i >= 0 ; i -- )
		if ( typeof(args[i]) != "string" ) {
		    var post = "th";
		    switch ( i ) {
		    case 0:
			post = "st";
			break;
		    case 1:
			post = "nd";
			break;
		    case 2:
			post = "rd";
			break;
		    }
		    this.i_sendResult("type missmatch (2nd arg list, "+(i+1)+post+") string type is required for arguments","error");
		    return "doFIN_START"
		}
	    this.i_body = body;
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    this.i_sendResult(
		new pigData(
		    "pn:lambda",
		    null,
		    this.param[1],
		    this.i_body));
	    this.i_body = null;
	    return "doFIN_START";
	}
    }
);


