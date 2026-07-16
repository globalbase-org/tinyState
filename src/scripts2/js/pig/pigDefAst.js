//:IMPORT pig/pigDef.js

function pigDefAst(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigDef.apply(this, [parent,env,param]);
    this.initial("pigDefAst");
}

piggybackTurtle.top.bind(
    null,
    "def*?",
    new pigData("pn:primitive",pigDefAst));


stdObject.defClass
(
    "pigDefAst",
    pigDefAst,
    pigDef,
    {

	/*=====================================================
	 *
	 *    pigDefAst STATE MACHINE
	 *
	 *=====================================================*/

	ACT_AST_TYPE : function(ev) {
	    if ( this.i_ret.getType() == "error" )
		return "doFIN_START";
	    var pos = this.i_ret.terminal.parent.parent;
	    for ( ; pos ; ) {
		var berr = pos.bind(this,this.i_trigger);
		var ret = pos.refer(this,this.i_trigger);
		if ( ret.getType() == "error" )
		    return "doFIN_START";
		pos = pos.parent;
	    }
	    return "doFIN_START";
	},
    }
);


