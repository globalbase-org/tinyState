//:IMPORT ts/tinyState.js

function pigFadeInvoker(parent,cycle) {
    if (arguments.length == 0)
	return;
    this.i_cycle = cycle;
    tinyState.apply(this, [parent]);
    this.initial("pigFadeInvoker");
}

stdObject.defClass
(
    "pigFadeInvoker",
    pigFadeInvoker,
    tinyState,
    {

	/*=====================================================
	 *
	 *    pigFadeInvoker STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_shared = this.application.refShared("pigFade");
	    if ( this.i_shared.fade )
		this.i_shared.fade = [];
	    if ( this.i_shared.fade[this.i_cycle] )
		return "doFIN_START";
	    this.i_shared.fade[this.i_cycle] = this;
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    stdInterval.wait(this,this.i_cycle,"return");
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type != "return")
		return;
	    if ( this.i_invoke("updated") == 0 )
		return "doFIN_START";
	    return "doACT_START";
	},

	FIN_START : function(ev) {
	    delete this.i_shared.fade[this.i_cycle];
	    this.application.unrefShared("pigFade");
	    return "doFIN_TINYSTATE_START";
	}
    }
);


