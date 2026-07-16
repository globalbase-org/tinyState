
//:IMPORT std/stdObject.js
//:EXPORT stdEventHandle.js


function stdEventHandle(type,source,dest,handler)
{
    if ( arguments.length == 0 )
        return;
    stdObject.apply(this);
    this.type = type;
    this.source = source;
    this.dest = dest;
    this.handler = handler;
    if ( !this.source.add_listener(this) ) {
	this.i_remove = true;
	return;
    }
    this.dest.add_eventHandleDest(this);
}

stdObject.defClass
(
    "stdEventHandle",
    stdEventHandle,
    stdObject,
    {
	i_removed : false,
	remove : function() {
	    if ( this.i_removed )
		return;
	    this.i_removed = true;
	    this.source.remove_listener(this);
	    this.dest.remove_eventHandleDest(this);
	}
    }
);
