//:IMPORT gt/gtObject.js
//;IMPORT pig/pigEval.js
//:EXPORT gtPane

function gtPane(gtScope, parameters) {
    if (arguments.length == 0)
	return;
    gtObject.apply(this, [gtScope, gtScope, gtScope.gtWindow,parameters]);
    this.initial("gtPane");
}

stdObject.defClass
(
    "gtPane",
    gtPane,
    gtObject,
    {


	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_gtObject_END : function(ev) {
	    return "doINI_gtPane_END";
	},
	INI_gtPane_END : function(ev) {
	    return "doINI_TINYSTATE_START";
	},

	FIN_START : function(ev) {
	    return "doFIN_gtPane_START";
	},
	FIN_gtPane_START : function(ev) {
	    return "doFIN_gtObject_START";
	},
    }
);
