//:IMPORT gt/gtScope.js
//:EXPORT gtScopeNode

function gtScopeNode(gtScp,parameters) {
    if ( arguments.length == 0 )
	return;
    gtScope.apply(
	this,
	[gtScp,
	 this.getUiObjectsList(parameters.uiObject),
	 false,
	 parameters]);
    this.initial("gtScopeNode");
}

stdObject.defClass
(
    "gtScopeNode",
    gtScopeNode,
    gtScope,
    {


	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


    }
);
