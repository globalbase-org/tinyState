//:IMPORT std/stdObject.js

function pigTerminalHandle(source,listener,handler) {
    if (arguments.length == 0)
	return;
    this.source = source;
    this.listener = listener;
    this.handler = handler;
    stdObject.apply(this);
}


stdObject.defClass
(
    "pigTerminalHandle",
    pigTerminalHandle,
    stdObject,
    {
	i_removed : false,

	remove : function() {
	    if ( this.i_removed )
		return;
	    this.i_removed = true;
	    this.source.removeListener(this.listener);
	    this.source = null;
	    this.listener = null;
	    this.handler = null;
	}
    }
);


