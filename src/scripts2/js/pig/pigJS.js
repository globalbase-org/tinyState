//:IMPORT pig/pigPrimitive.js

function pigJS(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigJS");
}

piggybackTurtle.top.bind(
    null,
    "js",
    new pigData("pn:primitive",pigJS));


stdObject.defClass
(
    "pigJS",
    pigJS,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigJS STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    var param = pigActivate.normalize(this.param);
	    var str = "";
	    for ( i = 1 ; i < param.length ; i ++ )
		str += this.param[i];
	    var orgstr = str;
	    var pattern = /\<\<\$([^\<\>]+)\>\>/;
	    var strpt = /\<\<([^\<\>]*)\>\>/;
	    var result1 = "";
	    var ano = 0;
	    var target = [];
	    for ( ; ; ) {
		var ret = pattern.exec(str);
		if ( !ret )
		    break;
		var prev = str.substr(0,ret.index);
		str = str.substr(ret.index+ret[0].length);
		target[ano] = ret[1];
		result1 += prev+"targetResult["+ano+"]";
		ano ++;
	    }
	    result1 += str;

	    str = result1;
	    result1 = "";
	    for ( ; ; ) {
		var ret = strpt.exec(str);
		if ( !ret )
		    break;
		var prev = str.substr(0,ret.index);
		str = str.substr(ret.index+ret[0].length);
		result1 += prev+'"'+ret[1]+'"';
	    }
	    result1 += str;
	    var targetResult = [];
	    var _ix;
	    for ( _ix in target ) {
		var v = target[_ix];
		var dt = this.env.refer(this,v);
		if ( dt.getType() == "error" ) {
		    this.i_sendResult(dt);
		    return "doFIN_START";
		}
		targetResult[_ix] = dt;
	    }
	    
	    targetResult = pigActivate.normalize(targetResult);
	    var res;
	    try {
		if ( result1.indexOf("{") == 0 )
		    eval(result1);
		else
		    res = eval(result1+";");
	    }
	    catch ( err ) {
this.dump("JS ERROR ... "+err+" ("+orgstr+" "+target.length+")");
		res = new pigData("error",err+" ("+orgstr+" "+target.length+")");
	    }
	    this.i_sendResult(res);
	    return "doFIN_START";
	},

    }
);


