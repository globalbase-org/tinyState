
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js

function ifAjaxDebug(
		     parent,
		     uri)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.i_caller = parent;
    this.i_uri = uri;
    this.i_now = stdInterval.now();
    this.initial("ifAjaxDebug");
}

stdObject.defClass
(
    "ifAjaxDebug",
    ifAjaxDebug,
    tinyState,
    {

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    return "doACT_WAIT";
	},
	ACT_WAIT : function(ev) {
	    stdInterval.wait(this,5000000,"timer");
	    return "ACT_START";
	},
	ACT_TINYSTATE_START : function(ev) {
	    //	    this.dump("WAITING AJAX "+this.i_uri+" ("+(stdInterval.now() - this.i_now)+")");
	    return "doACT_WAIT";
	},
    }
);

