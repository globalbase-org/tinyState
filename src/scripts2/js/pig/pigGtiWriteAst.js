//:IMPORT pig/pigGtiWrite.js

/*
["gtw","gtValue"
  ["...","...",....,data],
  ["...","...",....,data],
  ....]
*/


function pigGtiWriteAst(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigGtiWrite.apply(this, [parent,env,param]);
    this.initial("pigGtiWriteAst");
}

piggybackTurtle.top.bind(
    null,
    "gtiWrite*",
    new pigData("pn:primitive",pigGtiWriteAst));

stdObject.defClass
(
    "pigGtiWriteAst",
    pigGtiWriteAst,
    pigGtiWrite,
    {

	/*=====================================================
	 *
	 *    pigGtiWriteAst STATE MACHINE
	 *
	 *=====================================================*/
	ACT_PREV_RET : function(ev) {
	    this.i_sendResult();
	    return "doFIN_START";
	}
    }
);


